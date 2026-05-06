/**
 * @file CodroidDefine.h
 * @brief 定义文件
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
     * @brief @~english Standard SDK Response @~chinese SDK 标准响应结构体
     */
    struct Response {
        int id;               ///< @~english Request ID @~chinese 请求 ID
        std::string ty;       ///< @~english Request type @~chinese 请求类型
        json db;              ///< @~english Return data @~chinese 返回数据内容
        std::string error_msg; ///< @~english Error message (empty if success) @~chinese 错误信息（成功则为空）
        Response() : id(0), db(json::object()) {}
    };

    /**
     * @brief CRI 实时推送的语义化快照（关节用 vector、状态用 bool）。
     * @note 需在 CodroidController::connect(ip, port, local_ip) 中传入非空 local_ip 开启 UDP；
     *       未收到数据前 data_valid 为 false，各 vector 为空。
     */
    struct RobotRealtimeState {
        int64_t timestamp_ms = 0;
        /** 是否已收到过至少一帧有效 CRI 数据 */
        bool data_valid = false;

        /** 关节位置，单位 rad */
        std::vector<double> joint_position_rad;
        /** 关节速度，单位 rad/s */
        std::vector<double> joint_velocity_rad_s;
        /**
         * 末端位姿 [x,y,z, rx,ry,rz]：x,y,z 单位 m；rx,ry,rz 单位 rad。
         */
        std::vector<double> tcp_pose;
        /** 末端速度（线速度 m/s，角速度 rad/s，与协议一致） */
        std::vector<double> tcp_velocity;
        /** TCP 线速度标量，单位 m/s */
        double tcp_line_speed_m_s = 0;
        /** 关节输出力矩，单位 Nm */
        std::vector<double> joint_torque_nm;
        /** 关节外力矩，单位 Nm */
        std::vector<double> joint_external_torque_nm;

        // --- 状态数据 1：低 8 位（协议位 0~7）---
        bool project_running = false;
        bool project_stopped = false;
        bool project_paused = false;
        bool servo_on = false;
        bool servo_off = false;
        bool manual_mode = false;
        bool dragging = false;
        bool moving = false;

        // --- 状态数据 1：高 8 位（协议位 8~15）---
        bool collision_stop = false;
        bool in_safe_position = false;
        bool alarm = false;
        bool simulation_mode = false;
        bool emergency_stop = false;
        bool rescue_mode = false;
        bool auto_mode = false;
        bool remote_mode = false;

        // --- 状态数据 2 ---
        bool realtime_control_mode = false;
        /** 高 8 位：实时控制接口错误码 */
        uint8_t cri_error_code = 0;
    };

    /**
     * @brief @~english Codroid SDK Exception class @~chinese Codroid SDK 专用异常类
     */
    class CodroidException : public std::runtime_error {
    public:
        explicit CodroidException(const std::string& message) 
            : std::runtime_error(message) {}
    };

    // ========================================================================
    // 1. 通用枚举定义 (Common Enums)
    // ========================================================================

    /** @brief @~english RS485 Parity @~chinese RS485 校验位 */
    enum class RS485Parity : int { None = 0, Odd = 1, Even = 2 };

    /** @brief @~english RS485 Stop Bits @~chinese RS485 停止位 */
    enum class RS485StopBits : int { One = 1, Two = 2 };

    /** @brief @~english Coordinate System Type @~chinese 坐标系类型 */
    enum class CoorType { Tool, User };
    NLOHMANN_JSON_SERIALIZE_ENUM(CoorType, {
        {CoorType::Tool, "tool"}, {CoorType::User, "user"}
    })

    /** @brief @~english Motion Interpolation Type @~chinese 运动插补类型 */
    enum class MoveType { movJ, movL, movC, movCircle };
    NLOHMANN_JSON_SERIALIZE_ENUM(MoveType, {
        {MoveType::movJ, "movJ"}, {MoveType::movL, "movL"},
        {MoveType::movC, "movC"}, {MoveType::movCircle, "movCircle"}
    })

    /**
     * @brief @~english MoveTo Type @~chinese 运动类型
     */
    enum class MoveToType : int {
        Home = 0,           ///< @~english Home position @~chinese Home 位置
        Safe = 1,           ///< @~english Safe position @~chinese 安全位置
        Candle = 2,         ///< @~english Candle position @~chinese 蜡烛位
        Packing = 3,        ///< @~english Packing position @~chinese 打包位
        Joint = 4,          ///< @~english Joint planning to position @~chinese 关节规划到指定位置
        Line = 5,          ///< @~english Line planning to position @~chinese 直线规划到指定位置
        ResumePoint = 6     ///< @~english Program resume point @~chinese 程序恢复点
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(MoveToType, {
        {MoveToType::Home, 0}, {MoveToType::Safe, 1}, {MoveToType::Candle, 2}, 
        {MoveToType::Packing, 3}, {MoveToType::Joint, 4}, {MoveToType::Line, 5}, {MoveToType::ResumePoint, 6}
    })

    /**
     * @brief @~english Extend array supported data types @~chinese 扩展数组支持的数据类型
     */
    enum class ExtendArrayType {
        Bool,UInt8,Int8,UInt16,
        Int16,UInt32,Int32,Float32
    };
    NLOHMANN_JSON_SERIALIZE_ENUM(ExtendArrayType, {
        {ExtendArrayType::Bool, "Bool"}, {ExtendArrayType::UInt8, "UInt8"}, {ExtendArrayType::Int8, "Int8"},
        {ExtendArrayType::UInt16, "UInt16"}, {ExtendArrayType::Int16, "Int16"}, {ExtendArrayType::UInt32, "UInt32"},
        {ExtendArrayType::Int32, "Int32"}, {ExtendArrayType::Float32, "Float32"}
    })

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
     * @brief @~english Pose point definition @~chinese 运动位姿点定义
     */
    struct MovePoint {
        std::vector<double> jp; ///< @~english Joint positions (deg) @~chinese 关节角 (单位:度)
        std::vector<double> cp; ///< @~english Cartesian position (mm, deg) @~chinese 笛卡尔坐标 (单位:mm, 度)
        std::vector<double> rj; ///< @~english Reference joints for IK @~chinese 逆解参考关节角
        std::vector<double> ep; ///< @~english External axes @~chinese 附加轴位置

        MovePoint() = default;
        static MovePoint Joint(const std::vector<double>& v) { MovePoint p; p.jp = v; return p; }
        static MovePoint Cartesian(const std::vector<double>& v) { MovePoint p; p.cp = v; return p; }
    };

    /**
     * @brief @~english Move instruction detail @~chinese 单条运动指令详情
     */
    struct MoveInstruction {
        MoveType type = MoveType::movJ;  ///< @~english Motion type @~chinese 运动类型
        double speed = 60.0;            ///< @~english Speed (mm/s or deg/s) @~chinese 运动速度
        double acc = 150.0;             ///< @~english Accel (mm/s^2 or deg/s^2) @~chinese 加速度
        double blend = -1.0;            ///< @~english Transition radius (mm) @~chinese 过渡半径
        double relativeBlend = -1.0;    ///< @~english Relative transition (%) @~chinese 相对过渡百分比
        int circleNum = 1;              ///< @~english Circle count for movCircle @~chinese 圆周运动圈数
        MovePoint targetPoint;          ///< @~english Target position @~chinese 目标点
        MovePoint middlePoint;          ///< @~english Intermediate point (for movC) @~chinese 中间点
        std::vector<double> coor;       ///< @~english User coordinate [x,y,z,a,b,c] @~chinese 用户坐标系
        std::vector<double> tool;       ///< @~english Tool coordinate [x,y,z,a,b,c] @~chinese 工具坐标系
    };

    /** @brief @~english Jog Parameters @~chinese 点动参数结构体 */
    struct JogParams {
        JogMode mode = JogMode::Line;   ///< @~english 1:Joint, 2:Line @~chinese 1:关节点动 2:直线点动
        double speed = 0.0;             ///< @~english Range -1 to 1 @~chinese 速度范围 -1~1
        int index = 1;                  ///< @~english Axis/Joint index @~chinese 轴/关节序号
        CoorType coorType = CoorType::User; ///< @~english User, Tool @~chinese 用户系 工具系
        int coorId = 1;                 ///< @~english Coordinate ID @~chinese 坐标系 ID
        JogParams() = default;
        JogParams(JogMode m, double s, int i, CoorType ct = CoorType::User, int cid = 1) 
            : mode(m), speed(s), index(i), coorType(ct), coorId(cid) {}
    };

    /**
     * @brief @~english Relative pose calculation parameters @~chinese 相对位姿计算参数
     */
    struct RelativePoseParams {
        std::vector<double> pos;      ///< @~english [Required] Current position (Cartesian) @~chinese 当前末端TCP坐标 [x,y,z,a,b,c]
        std::vector<double> offset;   ///< @~english [Required] Desired offset in Cartesian coordinates @~chinese 偏移量 [x,y,z,a,b,c]
        CoorType coorType = CoorType::Tool; ///< @~english [Optional] Coordinate system type for the offset (Tool or User) @~chinese [可选] 坐标系类型
        std::vector<double> posCoor;  ///< @~english [Optional] Coordinate of the current position @~chinese [可选] 当前末端TCP坐标系，默认世界坐标系
        std::vector<double> coor;     ///< @~english [Optional] coorType is valid when set to user; offset coordinate system is the default, world coordinate system @~chinese [可选] coorType为user时有效，偏移坐标系，默认世界坐标系
        RelativePoseParams(const std::vector<double>& p, const std::vector<double>& o, CoorType type)
            : pos(p), offset(o), coorType(type) {}
            
        RelativePoseParams() = default;
    };


    /**
     * @brief @~english Global variable information @~chinese 全局变量信息结构
     */
    struct Variable {
        std::string val;  ///< @~english Variable value (JSON string format) @~chinese 变量值 (JSON字符串格式)
        std::string nm;   ///< @~english Variable remark/note @~chinese 变量备注
        template<typename T>
        Variable(const T& value, const std::string& note = "") : nm(note) {
            if constexpr (std::is_same_v<T, std::string>) {val = value; } 
            else {nlohmann::json j = value;val = j.dump();}
        }
        Variable() = default;
    };
    
    /**
     * @brief @~english Forward Kinematics Params @~chinese 正解参数结构体
     */
    struct FKParams {
        std::vector<double> jp;      ///< @~english [Required] joint angle @~chinese [必填] 关节角 [j1...j6], 单位: deg
        std::vector<double> coor;    ///< @~english [Optional] user coordinate system @~chinese [可选] 用户坐标系, 不传则不处理
        std::vector<double> tool;    ///< @~english [Optional] tool coordinate system @~chinese [可选] 工具坐标系, 不传则不处理
        std::vector<double> ep;      ///< @~english [Optional] extra axes @~chinese [可选] 附加轴位置
        explicit FKParams(const std::vector<double>& jointPos) : jp(jointPos) {}
        FKParams() = default;
    };

    /**
     * @brief @~english Inverse Kinematics Params @~chinese 逆解参数结构体
     */
    struct IKParams {
        std::vector<double> cp;      ///< @~english [Required] Cartesian position,mm, deg @~chinese [必填] 笛卡尔末端位置, 单位: mm, deg
        std::vector<double> rj;      ///< @~english [Optional] Reference joint angle, default [20,20,20,20,20,20] @~chinese [可选] 参考关节角, 默认 [20,20,20,20,20,20]
        std::vector<double> ep;      ///< @~english [Optional] extra axes @~chinese [可选] 附加轴位置
        explicit IKParams(const std::vector<double>& cartesianPos) : cp(cartesianPos) {}
        IKParams() = default;
    };

    
    /**
     * @brief @~english MoveTo Target Position @~chinese 运动目标位置
     */
    struct MoveToTarget {
        std::vector<double> cp; ///< @~english [Optional] End effector position [x,y,z,a,b,c] @~chinese [可选] 末端位置 [x,y,z,a,b,c]
        std::vector<double> jp; ///@~english [Optional] Reference joint angle @~chinese [可选] 关节位置 [j1..j6]
        MoveToTarget() = default;
        static MoveToTarget Joint(const std::vector<double>& j) {
            MoveToTarget t; t.jp = j; return t;
        }
        static MoveToTarget Cartesian(const std::vector<double>& c) {
            MoveToTarget t; t.cp = c; return t;
        }
    };

    /**
     * @brief @~english MoveTo Parameters @~chinese 运动参数
     */
    struct MoveToParams {
        MoveToType type = MoveToType::Home;
        MoveToTarget target; ///< @~english [Optional] Target position @~chinese [可选] 目标位置

        MoveToParams() = default;
        // 预定义位置构造 (Home, Safe 等)
        MoveToParams(MoveToType t) : type(t) {}
        // 规划位置构造 (Joint/Line Planning)
        MoveToParams(MoveToType t, const MoveToTarget& tgt) : type(t), target(tgt) {}
    };
    
    // ========================================================================
    // 3. 状态推送相关 (Subscription & Status)
    // ========================================================================

    /**
     * @brief @~english Robot runtime status @~chinese 机器人运行状态
     */
    struct RobotStatus {
        int mode;            ///< @~english 0:Manual, 1:Auto, 2:Remote @~chinese 0:手动, 1:自动, 2:远程
        int state;           ///< @~english 0:Not Enabled, 1:Enabling, 2:Idle, 3:Teaching, 4:Running, 5:Dragging @~chinese 0:未使能, 1:使能中, 2:空闲, 3:点动中, 4:RunTo, 5:拖动中
        int isMoving;        ///< @~english 0:Stopped, 1:Moving @~chinese 0:停止, 1:运动
        double moveRate;     ///< @~english Auto speed rate @~chinese 自动速度倍率  
        double manualMoveRate;///< @~english Manual speed rate @~chinese 手动速度倍率
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

    /**
     * @brief @~english Robot real-time posture @~chinese 机器人实时位姿
     */
    struct RobotPosture {
        std::vector<double> joint; ///< @~english Joints (deg) @~chinese 关节角 (度)
        std::vector<double> cart;  ///< @~english Cartesian [x,y,z,a,b,c] @~chinese 笛卡尔坐标 [x,y,z,a,b,c]
    };

    /**
     * @brief @~english Project execution state @~chinese 工程执行状态
     */
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

    /** @brief @~english IO information @~chinese IO 信息结构体 */
    struct IOInfo {
        std::string type;         ///< @~english DI, DO, AI, AO @~chinese IO 类型
        int port;                 ///< @~english Port number @~chinese 端口号
        double value;             ///< @~english IO value @~chinese IO 数值
        IOInfo(const std::string& t, int p) : type(t), port(p), value(0.0) {}
        IOInfo() = default;
    };

    /** @brief @~english Register information @~chinese 寄存器信息结构体 */
    struct RegisterInfo {
        int address;              ///< @~english Register address @~chinese 寄存器地址
        double value;             ///< @~english Register value @~chinese 寄存器数值
        RegisterInfo(int addr, double val) : address(addr), value(val) {}
        RegisterInfo() = default;
    };

    // 回调函数定义：参数1是主题名称(ty)，参数2是具体数据内容(db)
    using TopicCallback = std::function<void(const std::string&, const nlohmann::json&)>;

    // ========================================================================
    // 5. 脚本参数 (script)
    // ========================================================================
    struct RunScriptParams {
        std::string mainCode;                                     // 主程序代码 (必填)
        std::unordered_map<std::string, std::string> subThreads;  // 子线程 (可选)
        std::unordered_map<std::string, std::string> subPrograms; // 子程序 (可选)
        std::unordered_map<std::string, std::string> interrupts;  // 中断程序 (可选)
        json vars = json::object();                               // 全局变量 (可选，支持混合类型)

        // 构造函数，强制要求传入主程序代码
        explicit RunScriptParams(const std::string& main) : mainCode(main) {}
    };


}

#endif