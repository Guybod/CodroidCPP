/**
 * @file client.hpp
 * @brief 客户侧主入口 `CodroidClient`：TCP JSON 指令、IO/寄存器、点到点/路径运动、CRI 推送与实时控制会话。
 *
 * @par 单位与惯例（与 AGENTS.md 一致）
 * - 关节角：**度**；笛卡尔位姿 `[x,y,z,rx,ry,rz]`：**mm + 度**（固定欧拉 XYZ 外旋）。
 * - CRI 上行 UDP（308 字节）线上为 SI；本 SDK 在内部解析后，`ClientRealtimeState` 已为 **mm/度**。
 * - CRI **实时控制**下发（`StartCriControl` + `CriRealtimeDispatcher`）：周期须与 `durationMs` 一致（见 `cri_realtime_dispatcher.hpp`、`AGENTS.md` §6）。
 *
 * @note 本头仅依赖标准库与 `CodroidExport.h`；Asio / nlohmann 留在实现编译单元，客户工程无需引入。
 */

#ifndef CODROID_SDK_CLIENT_HPP
#define CODROID_SDK_CLIENT_HPP

#include "codroid/CodroidExport.h"

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

/**
 * @brief 运动目标点：`jp` 关节度；`cp` 笛卡尔 mm+度；`rj`/`ep` 按协议可选。
 * @note `ClientMovePoint::Joint` / `Cartesian` 为便捷工厂，填充对应字段。
 */
struct ClientMovePoint {
    std::vector<double> jp;
    std::vector<double> cp;
    std::vector<double> rj;
    std::vector<double> ep;

    /** @brief 构造关节点（度）。 */
    static ClientMovePoint Joint(std::vector<double> joints_deg);
    /** @brief 构造笛卡尔点（mm+度）。 */
    static ClientMovePoint Cartesian(std::vector<double> pose_mm_deg);
};

/**
 * @brief `MovePath` 单段：`target` 必填；`MovC`/`MovCircle` 需有效 `middle`。
 * @param speed / acceleration 含义与同类型点到点 API 一致（控制器约定百分比或内部标定，与示例对齐即可）。
 * @param blend / relative_blend 融合半径；默认 -1 表示沿用控制器默认。
 * @param circle_num 仅 `MovCircle`：圈数。
 * @param coor / tool 坐标系与工具，可选，与 `MovL` 一致。
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
    CommandResult SetPayload(int payloadId, int id = 1);

    /** @brief 关节运动：@p joints_deg 六轴度，@p speed @p acceleration 为控制器约定参数。 */
    CommandResult MovJ(const std::vector<double>& joints_deg, double speed, double acceleration, int id = 1);
    /**
     * @brief 直线运动：@p pose_mm_deg 为 mm+度；@p coor @p tool 可选。
     */
    CommandResult MovL(const std::vector<double>& pose_mm_deg, double speed, double acceleration,
                       const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
    /**
     * @brief 圆弧：经过 @p middle_pose_mm_deg，到达 @p target_pose_mm_deg（均为 mm+度）。
     */
    CommandResult MovC(const std::vector<double>& middle_pose_mm_deg, const std::vector<double>& target_pose_mm_deg,
                       double speed, double acceleration, int id = 1);
    /**
     * @brief 整圆/多圈：中间点、目标点、圈数 @p circle_num。
     */
    CommandResult MovCircle(const std::vector<double>& middle_pose_mm_deg,
                            const std::vector<double>& target_pose_mm_deg,
                            int circle_num,
                            double speed,
                            double acceleration,
                            int id = 1);
    /** @brief 多段路径，按顺序下发为一条 `Robot/move`（或等价）指令。 */
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
