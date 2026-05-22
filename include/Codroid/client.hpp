/**
 * @file client.hpp
 * @brief 客户侧主入口 `CodroidClient`：TCP JSON 指令、IO/寄存器、点到点/路径运动、CRI 推送与实时控制会话。
 *
 * @par 单位与惯例（与 AGENTS.md 一致）
 * - 关节角：**度**；笛卡尔位姿 `[x,y,z,rx,ry,rz]`：**mm + 度**（固定欧拉 XYZ 外旋）。
 * - CRI 上行 UDP（308 字节）线上为 SI；本 SDK 在内部解析后，`ClientRealtimeState` 已为 **mm/度**。
 * - CRI **实时控制**下发（`StartCriControl` + `CriRealtimeDispatcher`）：周期须与 `durationMs` 一致（见 `cri_realtime_dispatcher.hpp`、`AGENTS.md` §6）。
 *
 * @par 固件要求
 * - 本 SDK **所有对外接口**均要求控制器固件 **≥ 2.3.3.43**（见 `MinControllerFirmware`）。
 *
 * @note 本头仅依赖标准库与 `CodroidExport.h`；Asio / nlohmann 留在实现编译单元，客户工程无需引入。
 */

#ifndef CODROID_SDK_CLIENT_HPP
#define CODROID_SDK_CLIENT_HPP

#include "Codroid/CodroidExport.h"
#include "Codroid/CodroidDefine.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Codroid {

/** @brief 单次 TCP 指令结果：`error_msg` 非空表示失败；`raw_json` 为完整响应 JSON 便于排查。 */
struct CommandResult {
    int id = 0;                 ///< 请求 id（与下发一致）
    std::string ty;             ///< 响应类型 / 路由
    std::string error_msg;      ///< 控制器 `err` 或本地错误说明；空表示成功
    std::string raw_json;       ///< 最近一次完整响应 JSON

    /** @brief 是否成功（`error_msg` 为空）。 */
    bool Ok() const noexcept { return error_msg.empty(); }
};

/**
 * @brief CRI 实时快照（与 `AGENTS.md` §2.3.4 对齐）：关节 **度**，TCP **mm+度**，速度 mm/s 与 °/s。
 * @note 须先 `Connect`/`ConnectRemoteAndSwitchOn` 并 `StartCriDataPush`；首帧到达前 `data_valid` 可能为 false。
 */
struct ClientRealtimeState {
    int64_t timestamp_ms = 0;
    bool data_valid = false;
    uint16_t status1_raw = 0;   ///< status1 原始 16 位（位语义见 AGENTS.md §2.3.2）
    uint16_t status2_raw = 0;   ///< 含 CRI 错误码高 8 位与实时控制模式位 0

    std::vector<double> joint_position;       ///< 关节位置，度
    std::vector<double> joint_velocity;       ///< 关节角速度，度/s
    std::vector<double> tcp_pose;             ///< [x,y,z,rx,ry,rz]，mm + 度
    std::vector<double> tcp_velocity;         ///< 线 mm/s，角 °/s
    double tcp_linear_velocity_mm_s = 0.0;
    std::vector<double> joint_output_torque;
    std::vector<double> joint_external_force;
    std::vector<double> external_axis_position; ///< 当前解析策略下可能为空

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
    bool realtime_control_mode = false; ///< `StartCriControl` 生效后为 true（建议下发前轮询）
    uint8_t cri_error_code = 0;
};

/** @brief 单路 IO 读回：`type` 为控制器返回的类型字符串，`value` 为解析后的数值。 */
struct ClientIoInfo {
    std::string type;
    int port = 0;
    double value = 0.0;
};

/** @brief 寄存器一项：地址与值。 */
struct ClientRegisterInfo {
    int address = 0;
    double value = 0.0;
};

/** @brief 路径指令中的运动种类（与内部 `MoveType` 对应）。 */
enum class ClientMoveType {
    MovJ,       ///< 关节运动
    MovL,       ///< 直线（笛卡尔）
    MovC,       ///< 圆弧（经由中间点）
    MovCircle   ///< 整圆/多圈（`circle_num`）
};

using ClientJointPoint = JointPoint;
using ClientCartesianPoint = CartesianPoint;
/** @brief 路径段目标点，与 `MovePoint` 相同（`Robot/move` 的 targetPoint / middlePoint）。 */
using ClientMovePoint = MovePoint;

/**
 * @brief `Move` / `MovePath` 路径中的单段指令（对应 `MoveInstruction` + `ClientMoveType`）。
 * @note 推荐用静态工厂 `MovJ` / `MovL` / `MovC` / `MovCircle`，目标点传 `JointPoint` / `CartesianPoint`。
 */
struct ClientMoveInstruction {
    ClientMoveType type = ClientMoveType::MovJ;
    double speed = 60.0;
    double acceleration = 150.0;
    double blend = -1.0;
    double relative_blend = -1.0;
    int circle_num = 1;
    ClientMovePoint target;
    ClientMovePoint middle;
    std::vector<double> coor;
    std::vector<double> tool;

    static ClientMoveInstruction MovJ(ClientJointPoint target, double speed, double acceleration, double blend = -1.0) {
        ClientMoveInstruction inst;
        inst.type = ClientMoveType::MovJ;
        inst.speed = speed;
        inst.acceleration = acceleration;
        inst.blend = blend;
        inst.target = ClientMovePoint::Joint(std::move(target));
        return inst;
    }

    static ClientMoveInstruction MovJ(ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0) {
        ClientMoveInstruction inst;
        inst.type = ClientMoveType::MovJ;
        inst.speed = speed;
        inst.acceleration = acceleration;
        inst.blend = blend;
        inst.target = ClientMovePoint::Cartesian(std::move(target));
        return inst;
    }

    static ClientMoveInstruction MovL(ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0) {
        ClientMoveInstruction inst;
        inst.type = ClientMoveType::MovL;
        inst.speed = speed;
        inst.acceleration = acceleration;
        inst.blend = blend;
        inst.target = ClientMovePoint::Cartesian(std::move(target));
        return inst;
    }

    static ClientMoveInstruction MovL(ClientJointPoint target, double speed, double acceleration, double blend = -1.0) {
        ClientMoveInstruction inst;
        inst.type = ClientMoveType::MovL;
        inst.speed = speed;
        inst.acceleration = acceleration;
        inst.blend = blend;
        inst.target = ClientMovePoint::Joint(std::move(target));
        return inst;
    }

    static ClientMoveInstruction MovC(ClientCartesianPoint middle, ClientCartesianPoint target, double speed,
                                    double acceleration, double blend = -1.0) {
        ClientMoveInstruction inst;
        inst.type = ClientMoveType::MovC;
        inst.speed = speed;
        inst.acceleration = acceleration;
        inst.blend = blend;
        inst.middle = ClientMovePoint::Cartesian(std::move(middle));
        inst.target = ClientMovePoint::Cartesian(std::move(target));
        return inst;
    }

    static ClientMoveInstruction MovCircle(ClientCartesianPoint middle, ClientCartesianPoint target, int circle_num,
                                           double speed, double acceleration, double blend = -1.0) {
        ClientMoveInstruction inst;
        inst.type = ClientMoveType::MovCircle;
        inst.circle_num = circle_num;
        inst.speed = speed;
        inst.acceleration = acceleration;
        inst.blend = blend;
        inst.middle = ClientMovePoint::Cartesian(std::move(middle));
        inst.target = ClientMovePoint::Cartesian(std::move(target));
        return inst;
    }
};

/** @brief 主题推送一帧：`ty` 为主题类型；`db_json` 为 `db` 子树的 JSON 字符串；`raw_json` 为整帧。 */
struct ClientPublishNotification {
    std::string ty;
    std::string db_json;
    std::string raw_json;
};

/**
 * @brief 订阅句柄：析构或 `Dispose()` 后停止本地分发（不向控制器发退订）；断线后须重连并重新订阅。
 */
class CODROID_API ClientPublishSubscription {
public:
    ClientPublishSubscription();
    ~ClientPublishSubscription();

    ClientPublishSubscription(const ClientPublishSubscription&) = delete;
    ClientPublishSubscription& operator=(const ClientPublishSubscription&) = delete;
    ClientPublishSubscription(ClientPublishSubscription&&) noexcept;
    ClientPublishSubscription& operator=(ClientPublishSubscription&&) noexcept;

    /** @brief 提前结束订阅（与析构效果类似）。 */
    void Dispose();
    /** @brief 是否仍绑定有效内部资源。 */
    bool IsValid() const noexcept;

private:
    class Impl;
    explicit ClientPublishSubscription(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;

    friend class CodroidClient;
};

/**
 * @brief 对外门面：连接、模式、IO、寄存器、运动、CRI、主题订阅。
 * @note 详见 `examples_client/` 与 README；异常模式见 `SetThrowOnCommandError`。
 */
class CODROID_API CodroidClient {
public:
    CodroidClient();
    ~CodroidClient();

    CodroidClient(const CodroidClient&) = delete;
    CodroidClient& operator=(const CodroidClient&) = delete;
    CodroidClient(CodroidClient&&) noexcept;
    CodroidClient& operator=(CodroidClient&&) noexcept;

    /** @brief 生成下一个单调递增请求 id（多线程发令时勿重复）。 */
    int NextRequestId();

    /** @brief 建立 TCP（默认 9001），不切模式、不自动上电。 */
    bool Connect(const std::string& ip, int port = 9001);
    /**
     * @brief 连接并执行与 C# 一致的远程上电/模式切换；`local_ip` 非空时用于 `StartCriDataPush` 绑定本机 UDP。
     */
    bool ConnectRemoteAndSwitchOn(const std::string& ip, int port = 9001, std::string local_ip = {});
    /** @brief 断开 TCP，停止 CRI 相关线程与缓存。 */
    void Disconnect();

    /** @brief 上电使能类指令封装，@p id 为请求序号。 */
    CommandResult SwitchOn(int id = 1);
    CommandResult SwitchOff(int id = 1);
    CommandResult ToManual(int id = 1);
    CommandResult ToAuto(int id = 1);
    CommandResult ToRemote(int id = 1);
    CommandResult ClearSystemError(int id = 1);

    /** @brief 读数字量输入（端口 @p port）。 */
    int GetDi(int port, int id = 1);
    int GetDo(int port, int id = 1);
    double GetAi(int port, int id = 1);
    double GetAo(int port, int id = 1);
    CommandResult SetDo(int port, int value, int id = 1);
    CommandResult SetAo(int port, double value, int id = 1);

    double GetRegisterValue(int address, int id = 1);
    std::vector<ClientRegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1);
    CommandResult SetRegisterValue(int address, double value, int id = 1);

    CommandResult SetManualMoveRate(int pct, int id = 1);
    CommandResult SetAutoMoveRate(int pct, int id = 1);
    CommandResult SetCollisionSensitivity(int sensitivity, int id = 1);
    /** @brief 运行时切换当前负载（`Robot/setPayload`）。 */
    CommandResult SetPayload(int payloadId, int id = 1);

    // --- 19.2~19.7 机器人设置界面参数（`Robot/GetRobotParameter`、`Robot/SaveRobotParameter`）---
    // 负载/工具/用户坐标系序号及默认编号：SDK 仅接受 **1~15**。

    /** @brief 工具/负载/用户坐标系单帧（x,y,z,a,b,c）。 */
    struct ClientRobotFrame {
        int id = 0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
    };

    /** @brief 负载坐标系单帧（m, mx, my, mz）。 */
    struct ClientRobotPayload {
        int id = 0;
        double m = 0.0;
        double mx = 0.0;
        double my = 0.0;
        double mz = 0.0;
    };

    /** @brief `Robot/GetRobotParameter` 快照；失败时 `valid` 为 false（未开抛错模式）。 */
    struct ClientRobotParameters {
        bool valid = false;
        int default_tool_id = 0;
        int default_payload_id = 0;
        int default_coordinate_id = 0;
        double max_payload = 0.0;
        std::vector<ClientRobotFrame> tool;
        std::vector<ClientRobotPayload> payload;
        std::vector<ClientRobotFrame> coordinate;
    };

    /** @brief 获取设置界面参数（19.7）。 */
    ClientRobotParameters GetRobotParameters(int id = 1);

    /** @brief 设置默认负载编号（19.2，仅 `defaultPayloadId`）。 */
    CommandResult SetDefaultPayloadId(int payloadId, int id = 1);
    /** @brief 设置默认工具坐标系编号（19.3，仅 `defaultToolId`）。 */
    CommandResult SetDefaultToolId(int toolId, int id = 1);
    /** @brief 设置默认用户坐标系编号（19.6 默认编号部分，仅 `defaultCoordinateId`）。 */
    CommandResult SetDefaultUserCoordinateId(int coordinateId, int id = 1);

    /** @brief 直接下发完整工具坐标系表（19.4）。 */
    CommandResult SaveToolFrames(const std::vector<ClientRobotFrame>& frames, int id = 1);
    /** @brief 修改单个工具坐标系（先读后改；@p frame_id 为 1~15）。 */
    CommandResult SetToolFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);

    /** @brief 直接下发完整负载坐标系表（19.5）。 */
    CommandResult SavePayloadFrames(const std::vector<ClientRobotPayload>& frames, int id = 1);
    /** @brief 修改单个负载坐标系（先读后改；@p frame_id 为 1~15）。 */
    CommandResult SetPayloadFrame(int frame_id, const ClientRobotPayload& frame, int id = 1);

    /** @brief 修改单个用户坐标系（先读后改；@p frame_id 为 1~15）。 */
    CommandResult SetUserCoordinateFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);

    /** @brief 关节运动到关节目标（度）。 */
    CommandResult MovJ(const ClientJointPoint& target, double speed, double acceleration, int id = 1);
    /** @brief 关节运动到笛卡尔目标（mm+度）；@p target.rj 建议填当前关节参考。 */
    CommandResult MovJ(const ClientCartesianPoint& target, double speed, double acceleration, int id = 1);
    /** @brief 关节运动；@p target 须为 `ClientMovePoint::Joint` 或 `Cartesian`。 */
    CommandResult MovJ(const ClientMovePoint& target, double speed, double acceleration, int id = 1);
    /** @brief 直线运动到笛卡尔目标（mm+度）。 */
    CommandResult MovL(const ClientCartesianPoint& target, double speed, double acceleration,
                       const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
    /** @brief 直线运动到关节目标（度）。 */
    CommandResult MovL(const ClientJointPoint& target, double speed, double acceleration,
                       const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
    /** @brief 直线运动；@p target 须为 `ClientMovePoint::Joint` 或 `Cartesian`。 */
    CommandResult MovL(const ClientMovePoint& target, double speed, double acceleration,
                       const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
    /** @brief 圆弧：中间点与目标点均为笛卡尔（mm+度）。 */
    CommandResult MovC(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                       double speed, double acceleration, int id = 1);
    /** @brief 整圆/多圈：中间点、目标点为笛卡尔（mm+度）。 */
    CommandResult MovCircle(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                            int circle_num, double speed, double acceleration, int id = 1);
    /**
     * @brief 多段路径 `Robot/move`：按顺序执行 @p path 中各 `ClientMoveInstruction`。
     * @note 与 C# `Move` 对齐；等价于 `MovePath`。
     */
    CommandResult Move(const std::vector<ClientMoveInstruction>& path, int id = 1);
    /** @brief 同 `Move`（保留别名）。 */
    CommandResult MovePath(const std::vector<ClientMoveInstruction>& path, int id = 1);
    CommandResult PauseRobotMotion(int id = 1);
    CommandResult ResumeRobotMotion(int id = 1);
    CommandResult StopRobotMove(int id = 1);

    /**
     * @brief 请求 CRI 状态 UDP 推送到本机 @p udpIp:@p udpPort（载荷 308 字节，周期等默认与 C# 一致）。
     */
    CommandResult StartCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
    CommandResult StopCriDataPush(int id = 1);
    CommandResult StopCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
    /**
     * @brief 启动实时控制会话：@p durationMs 为控制周期（ms），须与后续 `CriRealtimeDispatcher::SendTrajectory` 的 period 一致。
     * @param filterType 滤波类型（0~3）；@p startBuffer 起始缓冲。
     */
    CommandResult StartCriControl(int filterType, int durationMs, int startBuffer, int id = 1);
    CommandResult StopCriControl(int id = 1);

    /** @brief 本机为 CRI 推送绑定的 UDP 监听端口（未启动推送时可能为 0）。 */
    int GetCriUdpListenPort() const;
    /** @brief 线程安全快照，字段单位见 `ClientRealtimeState`。 */
    ClientRealtimeState GetRobotRealtimeState() const;
    /** @brief 每次收到 CRI 帧时回调（在内部接收路径上触发，避免长时间阻塞）。 */
    void SetCriDataReceived(std::function<void(const ClientRealtimeState&)> cb);
    /**
     * @brief 订阅协议 15.x 推送主题；首次订阅发送 `ty`+`tc`，无整数 id。
     * @param topicTy 主题字符串须与控制器一致（如 `publish/RobotStatus`）。
     * @param tc_milliseconds 推送周期协商，默认 100 ms。
     */
    ClientPublishSubscription SubscribePublishTopic(std::string topicTy,
                                                    std::function<void(const ClientPublishNotification&)> handler,
                                                    int tc_milliseconds = 100);

    /**
     * @brief true 时指令失败抛 `CodroidCommandException`；false 时通过 `CommandResult::error_msg` 返回。
     */
    void SetThrowOnCommandError(bool enable);
    bool ThrowOnCommandError() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Codroid

#endif
