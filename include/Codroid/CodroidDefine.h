/**
 * @file CodroidDefine.h
 * @brief Codroid TCP SDK 共用数据类型（DTO）、枚举与异常定义。
 *
 * 主要内容：
 * - 通信：`Response`、主题推送 `PublishNotification`
 * - 实时：`RobotRealtimeState`（CRI UDP 解析后，mm+度）
 * - 运动：`JointPoint` / `CartesianPoint`（业务点位）→ `MovePoint`（协议路点）→ `MoveInstruction`（路径段）
 * - 规划：`MoveToParams`（RunTo，需心跳）
 * - 运动学：`FKParams` / `IKParams`
 * - 机器人设置：`RobotParameters`（固件 ≥ MinControllerFirmware）
 *
 * @note 客户程序通常只 `#include "Codroid/client.hpp"`；本头由 client 间接包含。
 *       业务层点位请用 `JointPoint` / `CartesianPoint`，不要与裸 `vector<double>` 混用。
 */

#ifndef CODROID_DEFINE_H
#define CODROID_DEFINE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "CodroidExport.h"
#include <stdexcept>
#include <functional>

namespace Codroid {
    using json = nlohmann::json;

    /**
     * @brief 单条 TCP JSON 指令的解析结果（与控制器下行帧字段对应）。
     *
     * - 成功：`error_msg` 为空，业务数据在 `db`（可能为 null / 对象 / 数组）。
     * - 失败：`error_msg` 为控制器 `err` 文本；`raw_json` 保留整帧便于现场排查。
     */
    struct Response {
        int id;                ///< 与请求一致的序号
        std::string ty;        ///< 路由类型，如 `Robot/move`、`IO/getValues`
        json db;               ///< 业务载荷 `db` 字段（已解析为 JSON）
        std::string error_msg; ///< 非空表示失败（来自控制器 `err`）
        std::string raw_json;  ///< 最近一次完整响应 JSON 字符串
        Response() : id(0), db(json::object()) {}
    };

    /**
     * @brief CRI 实时快照（与 **`AGENTS.md` §2.3.4** / C# `CriRealTimeData`）：关节 **度**，末端 **mm+度**，速度毫米/秒与度/秒；
     *        布尔位语义与 **`CriRealtimePacketParser`**（C#）一致。
     * @note 需在 CodroidController::connect(ip, port, local_ip) 中传入非空 local_ip 开启 UDP；未收到帧前 data_valid 为 false。
     */
    struct RobotRealtimeState {
        int64_t timestamp_ms = 0;
        bool data_valid = false;

        uint16_t status1_raw = 0;
        uint16_t status2_raw = 0;

        /** 关节位置（度），六轴。 */
        std::vector<double> joint_position;
        /** 关节速度（度/秒）。 */
        std::vector<double> joint_velocity;
        /** TCP [x,y,z,rx,ry,rz]，前三 mm，后三度（固定欧拉 XYZ 外旋）。 */
        std::vector<double> tcp_pose;
        /** 线分量 mm/s，角分量 °/s。 */
        std::vector<double> tcp_velocity;
        double tcp_linear_velocity_mm_s = 0.0;
        /** 关节输出力矩（协议原始数值）。 */
        std::vector<double> joint_output_torque;
        /** 关节外力（协议原始数值）。 */
        std::vector<double> joint_external_force;

        /** 附加轴位置；当前固定解析策略下恒为空。 */
        std::vector<double> external_axis_position;

        bool project_running = false;
        bool project_stopped = false;
        bool project_paused = false;
        bool enabling = false;
        bool not_enabled = false;
        bool manual_mode = false;
        bool dragging = false;
        bool in_motion = false;

        bool collision_stopped = false;
        bool in_safety_position = false;
        bool has_alarm = false;
        bool simulation_mode = false;
        bool emergency_stop_pressed = false;
        bool rescue_mode = false;
        bool auto_mode = false;
        bool remote_mode = false;

        bool realtime_control_mode = false;
        uint8_t cri_error_code = 0;
    };

    /** @brief 通用运行时错误。 */
    class CodroidException : public std::runtime_error {
    public:
        explicit CodroidException(const std::string& message)
            : std::runtime_error(message) {}
    };

    /**
     * @brief TCP 指令失败：**err 非空**、超时类错误；对齐 C# **`CodroidCommandException`**。
     * @note 默认 **`CodroidController::sendCommand`** 返回 **`Response.error_msg`**；调用 **`setThrowOnCommandError(true)`**（经 CodroidClient 暴露）时改为抛异常。
     */
    class CODROID_API CodroidCommandException : public CodroidException {
    public:
        CodroidCommandException(int request_id, std::string command_ty, std::string controller_error,
                                std::string raw_response_json);

        int request_id() const noexcept { return request_id_; }
        const std::string& command_ty() const noexcept { return command_ty_; }
        const std::string& controller_error() const noexcept { return controller_error_; }
        const std::string& raw_response_json() const noexcept { return raw_response_json_; }

    private:
        int request_id_{};
        std::string command_ty_;
        std::string controller_error_;
        std::string raw_response_json_;
    };

    // ========================================================================
    // 1. 通用枚举定义 (Common Enums)
    // ========================================================================

    /** @brief RS485 串口校验位（IO/通讯配置用）。 */
    enum class RS485Parity : int { None = 0, Odd = 1, Even = 2 };

    /** @brief RS485 停止位。 */
    enum class RS485StopBits : int { One = 1, Two = 2 };

    /** @brief 坐标系类型：工具系 / 用户系（点动、相对位姿等）。 */
    enum class CoorType { Tool, User };
    NLOHMANN_JSON_SERIALIZE_ENUM(CoorType, {
        {CoorType::Tool, "tool"}, {CoorType::User, "user"}
    })

    /**
     * @brief `Robot/move` 单段插补类型（写入 JSON 字段 `type`）。
     * - movJ：关节空间插补（目标可为 jp 或 cp）
     * - movL：笛卡尔直线（目标可为 cp 或 jp）
     * - movC / movCircle：圆弧 / 整圆，需 middlePoint + targetPoint（一般为 cp）
     */
    enum class MoveType { movJ, movL, movC, movCircle };
    NLOHMANN_JSON_SERIALIZE_ENUM(MoveType, {
        {MoveType::movJ, "movJ"}, {MoveType::movL, "movL"},
        {MoveType::movC, "movC"}, {MoveType::movCircle, "movCircle"}
    })

    /**
     * @brief `Robot/moveTo` 运动类别（与 C# `MoveToType` 一致）。
     *
     * -1：停止 MoveTo；0~3：控制器内置位（Home/Safe/Candle/Packing），无需 target。
     * 4/5：规划到用户点，须 `MoveToTarget`（jp 或 cp）；须周期性 `moveToHeartbeat()`。
     */
    enum class MoveToType : int {
        Stop = -1,       ///< 停止 MoveTo 运动
        Home = 0,
        Safe = 1,
        Candle = 2,
        Packing = 3,
        Joint = 4,       ///< 关节规划到 target（target 可 jp 或 cp）
        Line = 5,        ///< 直线规划到 target
        ResumePoint = 6
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(MoveToType, {
        {MoveToType::Stop, -1}, {MoveToType::Home, 0}, {MoveToType::Safe, 1}, {MoveToType::Candle, 2},
        {MoveToType::Packing, 3}, {MoveToType::Joint, 4}, {MoveToType::Line, 5}, {MoveToType::ResumePoint, 6}
    })

    /** @brief 寄存器扩展数组元素类型（`Register/setExtendArrayType`）。 */
    enum class ExtendArrayType {
        Bool,UInt8,Int8,UInt16,
        Int16,UInt32,Int32,Float32
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(ExtendArrayType, {
        {ExtendArrayType::Bool, "Bool"}, {ExtendArrayType::UInt8, "UInt8"}, {ExtendArrayType::Int8, "Int8"},
        {ExtendArrayType::UInt16, "UInt16"}, {ExtendArrayType::Int16, "Int16"}, {ExtendArrayType::UInt32, "UInt32"},
        {ExtendArrayType::Int32, "Int32"}, {ExtendArrayType::Float32, "Float32"}
    })

    /** @brief 点动模式：1=关节点动，2=直线点动（`JogParams`）。 */
    enum class JogMode {
        Joint = 1, Line = 2
     };
     NLOHMANN_JSON_SERIALIZE_ENUM(JogMode, {
        {JogMode::Joint, 1}, {JogMode::Line, 2}
    })

    // ========================================================================
    // 2. 运动控制相关 (Motion Control)
    // ========================================================================

    /**
     * @brief 关节空间目标点（六轴角，单位：度）。
     *
     * 用于声明「这是一个关节目标」，例如：
     * - `movJ(JointPoint)` / `movL(JointPoint)` 单点 API
     * - `MoveInstruction::MovJ(jp)` / `MovL(jp)` 路径段
     * - `MoveToTarget::Joint(...)`（RunTo 规划）
     *
     * @see JointPoint::Degrees 推荐工厂；长度为 6 的 [j1..j6]。
     */
    struct JointPoint {
        /** 六轴关节角（度）。 */
        std::vector<double> jp;

        /** @brief 由六轴关节角（度）构造。 */
        static JointPoint Degrees(std::vector<double> joints_deg) {
            JointPoint p;
            p.jp = std::move(joints_deg);
            return p;
        }
    };

    /**
     * @brief 笛卡尔末端目标点（TCP 位姿：mm + 度）。
     *
     * 用于声明「这是一个 TCP 目标」，例如：
     * - `movL(CartesianPoint)` / `movJ(CartesianPoint)`（关节运动到笛卡尔点时控制器做逆解）
     * - `MoveInstruction::MovL(cp)` / `MovJ(cp)` 路径段
     *
     * 字段：
     * - `cp`：[x, y, z, rx, ry, rz]，前三毫米、后三度（固定欧拉 XYZ 外旋，与 CRI/协议一致）。
     * - `rj`：逆解**参考关节角**（度，六轴）。多组关节解时控制器据此选解，避免跳解。
     *
     * 工厂：
     * - `MmDeg`：只设 TCP；下发 movJ/movL+cp 时若 `rj` 为空，SDK 填默认 [20,20,20,20,20,20]。
     * - `MmDegWithRef`：同时设 TCP + 参考关节；**建议**用 CRI `joint_position` 当前值作 `rj`（与 AGENTS.md movL 约定一致）。
     */
    struct CartesianPoint {
        std::vector<double> cp;
        std::vector<double> rj;

        /** @brief 仅 TCP 位姿（mm+度），不指定参考关节。 */
        static CartesianPoint MmDeg(std::vector<double> pose_mm_deg) {
            CartesianPoint p;
            p.cp = std::move(pose_mm_deg);
            return p;
        }

        /**
         * @brief TCP 位姿 + 逆解参考关节（度）。
         * @param pose_mm_deg  [x,y,z,rx,ry,rz]
         * @param ref_joints_deg  当前或期望的六轴参考角，通常取自 `GetRobotRealtimeState().joint_position`
         */
        static CartesianPoint MmDegWithRef(std::vector<double> pose_mm_deg, std::vector<double> ref_joints_deg) {
            CartesianPoint p;
            p.cp = std::move(pose_mm_deg);
            p.rj = std::move(ref_joints_deg);
            return p;
        }
    };

    /**
     * @brief @~english One waypoint inside a path segment (protocol payload) @~chinese 路径段中的单个目标点
     *
     * 用于 `MoveInstruction.targetPoint` / `middlePoint`，对应 JSON `targetPoint` / `middlePoint` 字段。
     * 对外请用 `JointPoint` / `CartesianPoint` 表达语义，再通过 `MovePoint::Joint` / `Cartesian` 填入本结构。
     * 每条路径点只应填 **jp 或 cp 之一**（打包时 jp 优先）。
     */
    struct MovePoint {
        std::vector<double> jp; ///< 关节角（度），与 cp 二选一
        std::vector<double> cp; ///< TCP [x,y,z,rx,ry,rz]（mm+度）
        std::vector<double> rj; ///< 仅 cp 有效时：逆解参考关节（度）
        std::vector<double> ep; ///< 附加轴（可选，多数机型为空）

        MovePoint() = default;

        /** 从 JointPoint 生成协议路点（只填 jp）。 */
        static MovePoint Joint(JointPoint joint) {
            MovePoint p;
            p.jp = std::move(joint.jp);
            return p;
        }

        /** 从 CartesianPoint 生成协议路点（填 cp、rj）。 */
        static MovePoint Cartesian(CartesianPoint cart) {
            MovePoint p;
            p.cp = std::move(cart.cp);
            p.rj = std::move(cart.rj);
            return p;
        }
    };

    /**
     * @brief @~english One segment of `Robot/move` path @~chinese 路径中的一段运动指令
     *
     * `move({inst1, inst2, ...})` 将多段 `MoveInstruction` 组成一条路径下发。
     * 每段包含：运动类型（movJ/movL/movC…）、速度/加速度/过渡、目标点（及圆弧中间点）。
     */
    struct MoveInstruction {
        MoveType type = MoveType::movJ;
        double speed = 60.0;         ///< 速度（关节段常用 deg/s 标度，直线段常用 mm/s 标度，与控制器约定一致）
        double acc = 150.0;          ///< 加速度
        double blend = -1.0;         ///< 过渡半径 mm；<0 表示不下发，用控制器默认
        double relativeBlend = -1.0; ///< 相对过渡 %；<0 表示不下发
        int circleNum = 1;           ///< movCircle 圈数
        MovePoint targetPoint;       ///< 本段终点
        MovePoint middlePoint;       ///< movC/movCircle 圆弧中间点
        std::vector<double> coor;    ///< 可选用户坐标系 [x,y,z,a,b,c]
        std::vector<double> tool;    ///< 可选工具坐标系

        /** 路径段：movJ，目标为关节角。 */
        static MoveInstruction MovJ(JointPoint target, double speed, double acc, double blend = -1.0) {
            MoveInstruction inst;
            inst.type = MoveType::movJ;
            inst.speed = speed;
            inst.acc = acc;
            inst.blend = blend;
            inst.targetPoint = MovePoint::Joint(std::move(target));
            return inst;
        }

        /** 路径段：movJ，目标为 TCP（控制器逆解）；target.rj 建议用 MmDegWithRef 填写。 */
        static MoveInstruction MovJ(CartesianPoint target, double speed, double acc, double blend = -1.0) {
            MoveInstruction inst;
            inst.type = MoveType::movJ;
            inst.speed = speed;
            inst.acc = acc;
            inst.blend = blend;
            inst.targetPoint = MovePoint::Cartesian(std::move(target));
            return inst;
        }

        /** 路径段：movL，目标为 TCP。 */
        static MoveInstruction MovL(CartesianPoint target, double speed, double acc, double blend = -1.0) {
            MoveInstruction inst;
            inst.type = MoveType::movL;
            inst.speed = speed;
            inst.acc = acc;
            inst.blend = blend;
            inst.targetPoint = MovePoint::Cartesian(std::move(target));
            return inst;
        }

        /** 路径段：movL，目标为关节角。 */
        static MoveInstruction MovL(JointPoint target, double speed, double acc, double blend = -1.0) {
            MoveInstruction inst;
            inst.type = MoveType::movL;
            inst.speed = speed;
            inst.acc = acc;
            inst.blend = blend;
            inst.targetPoint = MovePoint::Joint(std::move(target));
            return inst;
        }

        /** 路径段：movC，中间点 + 终点（均为笛卡尔）。 */
        static MoveInstruction MovC(CartesianPoint middle, CartesianPoint target, double speed, double acc,
                                  double blend = -1.0) {
            MoveInstruction inst;
            inst.type = MoveType::movC;
            inst.speed = speed;
            inst.acc = acc;
            inst.blend = blend;
            inst.middlePoint = MovePoint::Cartesian(std::move(middle));
            inst.targetPoint = MovePoint::Cartesian(std::move(target));
            return inst;
        }

        /** 路径段：movCircle，中间点 + 终点 + 圈数。 */
        static MoveInstruction MovCircle(CartesianPoint middle, CartesianPoint target, int circle_num, double speed,
                                         double acc, double blend = -1.0) {
            MoveInstruction inst;
            inst.type = MoveType::movCircle;
            inst.circleNum = circle_num;
            inst.speed = speed;
            inst.acc = acc;
            inst.blend = blend;
            inst.middlePoint = MovePoint::Cartesian(std::move(middle));
            inst.targetPoint = MovePoint::Cartesian(std::move(target));
            return inst;
        }
    };

    /**
     * @brief 点动（Jog）参数，对应 `Robot/startJog` 等。
     * @note 须配合 `jogHeartbeat()` 周期调用；`speed` 为 -1~1 的比例而非 mm/s。
     */
    struct JogParams {
        JogMode mode = JogMode::Line;
        double speed = 0.0;
        int index = 1;
        CoorType coorType = CoorType::User;
        int coorId = 1;
        JogParams() = default;
        JogParams(JogMode m, double s, int i, CoorType ct = CoorType::User, int cid = 1) 
            : mode(m), speed(s), index(i), coorType(ct), coorId(cid) {}
    };

    /**
     * @brief 相对位姿计算（`Robot/calculateRelativePose`），在工具系或用户系下对当前 TCP 施加偏移。
     */
    struct RelativePoseParams {
        std::vector<double> pos;
        std::vector<double> offset;
        CoorType coorType = CoorType::Tool;
        std::vector<double> posCoor;
        std::vector<double> coor;
        RelativePoseParams(const std::vector<double>& p, const std::vector<double>& o, CoorType type)
            : pos(p), offset(o), coorType(type) {}
            
        RelativePoseParams() = default;
    };


    /** @brief 全局变量一项：`val` 为 JSON 字符串，`nm` 为备注。 */
    struct Variable {
        std::string val;
        std::string nm;
        template<typename T>
        Variable(const T& value, const std::string& note = "") : nm(note) {
            if constexpr (std::is_same_v<T, std::string>) {val = value; } 
            else {nlohmann::json j = value;val = j.dump();}
        }
        Variable() = default;
    };
    
    /**
     * @brief 正解请求（`Robot/apostocpos`）：关节角 → TCP。
     * @note 与 `JointPoint` 单位相同（度）；返回 TCP 为 mm+度。
     */
    struct FKParams {
        std::vector<double> jp;
        std::vector<double> coor;
        std::vector<double> tool;
        std::vector<double> ep;
        explicit FKParams(const std::vector<double>& jointPos) : jp(jointPos) {}
        FKParams() = default;
    };

    /**
     * @brief 逆解请求（`Robot/cpostoapos`）：TCP → 关节角。
     * @note `cp` 为 mm+度；`rj` 为参考关节（同 `CartesianPoint::rj` / `MmDegWithRef` 语义）。
     */
    struct IKParams {
        std::vector<double> cp;
        std::vector<double> rj;
        std::vector<double> ep;
        explicit IKParams(const std::vector<double>& cartesianPos) : cp(cartesianPos) {}
        IKParams() = default;
    };

    /**
     * @brief `Robot/moveTo` 的目标位置（仅 type=Joint/Line 时需要）。
     * 使用 `Joint` / `Cartesian` 工厂，与 `JointPoint` / `CartesianPoint` 对齐。
     */
    struct MoveToTarget {
        std::vector<double> cp;
        std::vector<double> jp;
        MoveToTarget() = default;
        static MoveToTarget Joint(JointPoint joint) {
            MoveToTarget t;
            t.jp = std::move(joint.jp);
            t.cp.clear();
            return t;
        }
        static MoveToTarget Cartesian(CartesianPoint cart) {
            MoveToTarget t;
            t.cp = std::move(cart.cp);
            t.jp.clear();
            return t;
        }
    };

    /**
     * @brief `Robot/moveTo` 参数：内置位或规划到用户点。
     * @warning type=Joint/Line 时须每 ≥500ms 调用 `moveToHeartbeat()`，否则 RunTo 会停。
     */
    struct MoveToParams {
        MoveToType type = MoveToType::Home;
        MoveToTarget target;

        MoveToParams() = default;
        explicit MoveToParams(MoveToType t) : type(t) {}
        MoveToParams(MoveToType t, const MoveToTarget& tgt) : type(t), target(tgt) {}
    };
    
    // ========================================================================
    // 3. 状态推送相关 (Subscription & Status)
    // ========================================================================

    /**
     * @brief 主题 `publish/RobotStatus` 推送体解析结果（与 CRI 位字段不同，勿混用）。
     */
    struct RobotStatus {
        int mode;             ///< 0 手动 / 1 自动 / 2 远程
        int state;            ///< 0 未使能 … 5 拖动
        int isMoving;
        double moveRate;
        double manualMoveRate;
        int recoveryState;
        bool isSimulation;
        int teachingPendant;
        int rescueFlag;
        int modeSwitch;
        int ToolId;
        int PayloadId;
        int CoordinateId;
        int defaultToolId;
        int defaultPayloadId;
        int defaultUserCoorId;
        std::string type;       ///< @~english Model type @~chinese 机器人型号
        std::string stateName;  ///< @~english State description @~chinese 状态名称
        long long timestamp;    ///< @~english Push timestamp @~chinese 推送时间戳
    };

    /** @brief 主题 `publish/RobotPosture`：当前关节与 TCP（度 / mm+度）。 */
    struct RobotPosture {
        std::vector<double> joint;
        std::vector<double> cart;
    };

    /** @brief 主题 `publish/ProjectState`：工程运行状态。 */
    struct ProjectState {
        std::string id;           ///< @~english Project ID @~chinese 工程 ID
        int state;                ///< @~english 0:Idle, 2:Running, 3:Paused @~chinese 工程状态
        bool isStep;              ///< @~english Step mode flag @~chinese 是否单步运行
        int projectType;          ///< @~english project type @~chinese 工程类型
        std::map<std::string, int> scriptLines; ///< @~english ScriptID -> LineNum @~chinese 脚本ID对应行号
    };

    // ========================================================================
    // 4. IO 与 寄存器 (IO & Registers)
    // ========================================================================

    /** @brief IO 读写的单点描述（type 如 `DI`/`DO`/`AI`/`AO`，port 为索引）。 */
    struct IOInfo {
        std::string type;
        int port;
        double value;
        IOInfo(const std::string& t, int p) : type(t), port(p), value(0.0) {}
        IOInfo() = default;
    };

    /** @brief 寄存器地址与数值（批量读写 `Register/getValues` 等）。 */
    struct RegisterInfo {
        int address;
        double value;
        RegisterInfo(int addr, double val) : address(addr), value(val) {}
        RegisterInfo() = default;
    };

    /**
     * @brief 主题推送内容（与 C# PublishNotification 对齐：ty / db / 原始 JSON）。
     */
    struct PublishNotification {
        std::string ty;
        nlohmann::json db;
        std::string raw_json;
    };

    using PublishTopicHandler = std::function<void(const PublishNotification&)>;

    /**
     * @brief 订阅句柄：析构或 Dispose 时仅移除本地回调，不向控制器发退订（与 C# 一致）。
     */
    class PublishTopicSubscription {
        std::function<void()> dispose_;

    public:
        PublishTopicSubscription() = default;
        explicit PublishTopicSubscription(std::function<void()> d) : dispose_(std::move(d)) {}

        ~PublishTopicSubscription() { reset(); }

        PublishTopicSubscription(const PublishTopicSubscription&) = delete;
        PublishTopicSubscription& operator=(const PublishTopicSubscription&) = delete;

        PublishTopicSubscription(PublishTopicSubscription&& o) noexcept : dispose_(std::move(o.dispose_)) {
            o.dispose_ = nullptr;
        }

        PublishTopicSubscription& operator=(PublishTopicSubscription&& o) noexcept {
            if (this != &o) {
                reset();
                dispose_ = std::move(o.dispose_);
                o.dispose_ = nullptr;
            }
            return *this;
        }

        void Dispose() { reset(); }

    private:
        void reset() {
            if (dispose_) {
                auto f = std::move(dispose_);
                dispose_ = nullptr;
                f();
            }
        }
    };

    /** @brief 协议 15.x 主题字面量（与 AGENTS.md 完全一致） */
    struct PublishTopics {
        static constexpr const char* ProjectState = "publish/ProjectState";
        static constexpr const char* VarUpdate = "publish/VarUpdate";
        static constexpr const char* RobotStatus = "publish/RobotStatus";
        static constexpr const char* RobotPosture = "publish/RobotPosture";
        static constexpr const char* RobotCoordinate = "publish/RobotCoordinate";
        static constexpr const char* Log = "publish/Log";
        static constexpr const char* Error = "publish/Error";
    };

    // ========================================================================
    // 5. 机器人设置参数 (Robot/GetRobotParameter & Robot/SaveRobotParameter)
    // ========================================================================

    /** @brief 本 SDK 对外 TCP/UDP 接口统一要求的控制器固件最低版本（≥，含本号）。 */
    inline constexpr const char* MinControllerFirmware = "2.3.3.43";

    /** @brief 同 `MinControllerFirmware`（机器人设置参数接口别名，便于检索）。 */
    inline constexpr const char* RobotParameterMinFirmware = MinControllerFirmware;

    /**
     * @brief 工具坐标系或用户坐标系表中的一帧（协议 19.4 / 19.6）。
     * @note 对外设置序号 1~15；控制器 0 号槽位只读且恒为 0，SDK 不提供对 0 号的写接口。
     */
    struct RobotFrameEntry {
        int id = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
    };

    /** @brief 负载坐标系表中的一帧（协议 19.5：质量 m 与质心 mx,my,mz）。 */
    struct RobotPayloadEntry {
        int id = 0;
        double m = 0.0;
        double mx = 0.0;
        double my = 0.0;
        double mz = 0.0;
    };

    /**
     * @brief `Robot/GetRobotParameter` 返回的完整机器人设置参数快照。
     * 修改后通过 `SaveRobotParameter` 或分项 `Set*` / `Save*Frames` 写回。
     */
    struct RobotParameters {
        int default_tool_id = 0;
        int default_payload_id = 0;
        int default_coordinate_id = 0;
        double max_payload = 0.0;
        std::vector<RobotFrameEntry> tool;
        std::vector<RobotPayloadEntry> payload;
        std::vector<RobotFrameEntry> coordinate;
    };

    // ========================================================================
    // 6. 脚本参数 (script)
    // ========================================================================

    /** @brief 远程脚本运行参数（`RunScript` / `runScript`）。 */
    struct RunScriptParams {
        std::string mainCode;
        std::unordered_map<std::string, std::string> subThreads;
        std::unordered_map<std::string, std::string> subPrograms;
        std::unordered_map<std::string, std::string> interrupts;
        json vars = json::object();

        explicit RunScriptParams(const std::string& main) : mainCode(main) {}
    };


}

#endif