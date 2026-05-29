# CodroidClient API 参考

`CodroidClient` 是 SDK 的主入口类，提供连接、IO、寄存器、运动、CRI、主题订阅等功能。

```cpp
#include "Codroid/client.hpp"

Codroid::CodroidClient robot;
```

---

## 构造与析构

### CodroidClient()

默认构造函数，不建立连接。

```cpp
Codroid::CodroidClient robot;
```

### ~CodroidClient()

析构函数。如果未调用 `Disconnect()`，析构时会自动清理资源。

---

## 连接管理

### Connect

```cpp
bool Connect(const std::string& ip, int port = 9001);
```

建立 TCP 连接，不切模式、不自动上电。

**参数：**
- `ip` — 控制器 IP 地址
- `port` — TCP 端口，默认 9001

**返回：** 连接成功返回 `true`

---

### ConnectRemoteAndSwitchOn

```cpp
bool ConnectRemoteAndSwitchOn(const std::string& ip, int port = 9001, std::string local_ip = {});
```

连接并执行远程上电/模式切换。`local_ip` 非空时用于 `StartCriDataPush` 绑定本机 UDP。

**参数：**
- `ip` — 控制器 IP 地址
- `port` — TCP 端口，默认 9001
- `local_ip` — 本机 IP，用于 CRI UDP 绑定

**返回：** 成功返回 `true`

**推荐：** 大多数场景使用此方法。

---

### Disconnect

```cpp
void Disconnect();
```

断开 TCP 连接，停止 CRI 相关线程与缓存。

**必须调用：** 在程序结束前调用，确保资源释放。

---

### NextRequestId

```cpp
int NextRequestId();
```

生成下一个单调递增的请求 ID。多线程发令时避免重复。

---

## 模式控制

### SwitchOn

```cpp
CommandResult SwitchOn(int id = 1);
```

上电使能。

---

### SwitchOff

```cpp
CommandResult SwitchOff(int id = 1);
```

下电。

---

### ToManual

```cpp
CommandResult ToManual(int id = 1);
```

切换到手动模式。

---

### ToAuto

```cpp
CommandResult ToAuto(int id = 1);
```

切换到自动模式。

---

### ToRemote

```cpp
CommandResult ToRemote(int id = 1);
```

切换到远程模式。

---

### ClearSystemError

```cpp
CommandResult ClearSystemError(int id = 1);
```

清除系统错误。

---

### EnterManualModeViaAuto

```cpp
CommandResult EnterManualModeViaAuto(int id = 1);
```

先切自动再切手动（Auto → Manual）。

---

### EnterRemoteModeViaAuto

```cpp
CommandResult EnterRemoteModeViaAuto(int id = 1);
```

先切自动再切远程（Auto → Remote）。

---

### ToSimulation

```cpp
CommandResult ToSimulation(int id = 1);
```

进入仿真模式。

---

### ToActual

```cpp
CommandResult ToActual(int id = 1);
```

进入实机模式。

---

### StartDrag

```cpp
CommandResult StartDrag(int id = 1);
```

进入拖拽示教模式。

---

### StopDrag

```cpp
CommandResult StopDrag(int id = 1);
```

退出拖拽示教模式。

---

## IO 操作

### GetDi

```cpp
int GetDi(int port, int id = 1);
```

读取数字量输入。

**参数：**
- `port` — 端口号

**返回：** DI 值（0 或 1）

---

### GetDo

```cpp
int GetDo(int port, int id = 1);
```

读取数字量输出。

**参数：**
- `port` — 端口号

**返回：** DO 值（0 或 1）

---

### GetAi

```cpp
double GetAi(int port, int id = 1);
```

读取模拟量输入。

**参数：**
- `port` — 端口号

**返回：** AI 值

---

### GetAo

```cpp
double GetAo(int port, int id = 1);
```

读取模拟量输出。

**参数：**
- `port` — 端口号

**返回：** AO 值

---

### SetDo

```cpp
CommandResult SetDo(int port, int value, int id = 1);
```

设置数字量输出。

**参数：**
- `port` — 端口号
- `value` — 值（0 或 1）

---

### SetAo

```cpp
CommandResult SetAo(int port, double value, int id = 1);
```

设置模拟量输出。

**参数：**
- `port` — 端口号
- `value` — 值

---

## 寄存器操作

### GetRegisterValue

```cpp
double GetRegisterValue(int address, int id = 1);
```

读取单个寄存器值。

**参数：**
- `address` — 寄存器地址

**返回：** 寄存器值

---

### GetRegisterValues

```cpp
std::vector<ClientRegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1);
```

批量读取寄存器。

**参数：**
- `addresses` — 地址列表

**返回：** `ClientRegisterInfo` 列表（包含 address 和 value）

---

### SetRegisterValue

```cpp
CommandResult SetRegisterValue(int address, double value, int id = 1);
```

设置寄存器值。

**参数：**
- `address` — 寄存器地址
- `value` — 值

---

## 运动控制

### MovJ

```cpp
CommandResult MovJ(const ClientJointPoint& target, double speed, double acceleration, int id = 1);
CommandResult MovJ(const ClientCartesianPoint& target, double speed, double acceleration, int id = 1);
CommandResult MovJ(const ClientMovePoint& target, double speed, double acceleration, int id = 1);
```

关节运动。

**参数：**
- `target` — 目标点（关节或笛卡尔）
- `speed` — 速度
- `acceleration` — 加速度

---

### MovL

```cpp
CommandResult MovL(const ClientCartesianPoint& target, double speed, double acceleration,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
CommandResult MovL(const ClientJointPoint& target, double speed, double acceleration,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
CommandResult MovL(const ClientMovePoint& target, double speed, double acceleration,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

直线运动。

**参数：**
- `target` — 目标点
- `speed` — 速度（mm/s）
- `acceleration` — 加速度
- `coor` — 可选用户坐标系 [x,y,z,a,b,c]
- `tool` — 可选工具坐标系

---

### MovC

```cpp
CommandResult MovC(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   double speed, double acceleration, int id = 1);
```

圆弧运动。

**参数：**
- `middle` — 中间点
- `target` — 目标点
- `speed` — 速度
- `acceleration` — 加速度

---

### MovCircle

```cpp
CommandResult MovCircle(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                        int circle_num, double speed, double acceleration, int id = 1);
```

整圆/多圈运动。

**参数：**
- `middle` — 中间点
- `target` — 目标点
- `circle_num` — 圈数
- `speed` — 速度
- `acceleration` — 加速度

---

### Move / MovePath

```cpp
CommandResult Move(const std::vector<ClientMoveInstruction>& path, int id = 1);
CommandResult MovePath(const std::vector<ClientMoveInstruction>& path, int id = 1);
```

多段路径执行。

**参数：**
- `path` — 路径段列表

**示例：**
```cpp
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(
        Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100),
    Codroid::ClientMoveInstruction::MovL(
        Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}), 150, 500)
};
robot.Move(path);
```

---

### 阻塞运动（Sync）

```cpp
bool MoveSync(const std::vector<ClientMoveInstruction>& path, const MotionWaitOptions& wait = {});
bool MovJSync(const ClientJointPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovJSync(const ClientCartesianPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovLSync(const ClientCartesianPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovLSync(const ClientJointPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovCSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
              double speed, double acc, const MotionWaitOptions& wait = {});
bool MovCircleSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   int circle_num, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式运动，等待 CRI 确认到达目标。

**参数：**
- `target` — 目标点
- `speed` — 速度
- `acc` — 加速度
- `wait` — 等待选项（超时、容差等）

**返回：** 到达目标返回 `true`，超时返回 `false`

**MotionWaitOptions 默认值：**
```cpp
struct MotionWaitOptions {
    double timeout_s = 60.0;                        // 整体等待超时（秒）
    double poll_interval_s = 0.05;                  // CRI 轮询间隔（秒）
    double cri_stale_timeout_s = 0.5;               // CRI 数据过期判定（秒）
    int settled_samples = 3;                        // InMotion=false 连续稳定采样数
    double joint_tolerance_deg = 0.2;               // 关节目标容差（度）
    double cartesian_position_tolerance_mm = 1.0;   // 笛卡尔位置容差（mm）
    double cartesian_orientation_tolerance_deg = 1.0; // 笛卡尔姿态容差（度）
};
```

**使用前须知：** 调用 `*Sync` 方法前，必须确保 CRI 数据已开始推送（调用 `StartCriDataPush` 并等待首帧）。

---

### PauseRobotMotion / ResumeRobotMotion / StopRobotMove

```cpp
CommandResult PauseRobotMotion(int id = 1);
CommandResult ResumeRobotMotion(int id = 1);
CommandResult StopRobotMove(int id = 1);
```

暂停、恢复、停止运动。

---

## MoveTo（规划运动）

### MoveTo

```cpp
CommandResult MoveTo(const MoveToParams& params, int id = 1);
```

MoveTo 预设/规划运动。

**MoveToParams：**
```cpp
struct MoveToParams {
    MoveToType type = MoveToType::Home;  // 运动类型
    MoveToTarget target;                 // 目标点（仅 Joint/Line 需要）
};
```

**MoveToType 枚举：**
- `Stop` (-1) — 停止 MoveTo
- `Home` (0) — 回 Home
- `Safe` (1) — 回安全位
- `Candle` (2) — 回 Candle 位
- `Packing` (3) — 回 Packing 位
- `Joint` (4) — 关节规划到目标
- `Line` (5) — 直线规划到目标

**示例：**
```cpp
// 回 Home
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Home));

// 关节规划到目标
auto target = Codroid::MoveToTarget::Joint(
    Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Joint, target));
```

---

### MoveToHeartbeat

```cpp
CommandResult MoveToHeartbeat(int id = 1);
```

MoveTo 心跳。使用 `MoveToType::Joint` 或 `Line` 时，每 ≥500ms 调用一次，否则运动会停止。

---

### StopMoveTo

```cpp
CommandResult StopMoveTo(int id = 1);
```

停止 MoveTo 运动。

---

## Jog（点动）

### Jog

```cpp
CommandResult Jog(const JogParams& params, int id = 1);
```

点动。

**JogParams：**
```cpp
struct JogParams {
    JogMode mode = JogMode::Line;  // Joint=1, Line=2
    double speed = 0.0;            // -1~1 的比例
    int index = 1;                 // 轴号（Joint 模式）或方向（Line 模式）
    CoorType coorType = CoorType::User;  // 坐标系类型
    int coorId = 1;                // 坐标系 ID
};
```

**示例：**
```cpp
// 关节 1 正向点动
robot.Jog(Codroid::JogParams(Codroid::JogMode::Joint, 0.5, 1));

// 笛卡尔 X 正向点动
robot.Jog(Codroid::JogParams(Codroid::JogMode::Line, 0.3, 1));
```

---

### StopJog

```cpp
CommandResult StopJog(int id = 1);
```

停止点动。

---

### JogHeartbeat

```cpp
CommandResult JogHeartbeat(int id = 1);
```

点动心跳。点动期间每 ≥500ms 调用一次。

---

## CRI 实时控制

### StartCriDataPush

```cpp
CommandResult StartCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

请求 CRI 状态 UDP 推送。

**参数：**
- `udpIp` — 本机 IP
- `udpPort` — 本机 UDP 端口

---

### StopCriDataPush

```cpp
CommandResult StopCriDataPush(int id = 1);
CommandResult StopCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

停止 CRI 数据推送。

---

### StartCriControl

```cpp
CommandResult StartCriControl(int filterType, int durationMs, int startBuffer, int id = 1);
```

启动实时控制会话。

**参数：**
- `filterType` — 滤波类型（0~3）
- `durationMs` — 控制周期（ms），须与 `CriRealtimeDispatcher::SendTrajectory` 的 period 一致
- `startBuffer` — 起始缓冲

---

### StopCriControl

```cpp
CommandResult StopCriControl(int id = 1);
```

停止实时控制会话。

---

### GetCriUdpListenPort

```cpp
int GetCriUdpListenPort() const;
```

获取 CRI UDP 监听端口。

---

### GetRobotRealtimeState

```cpp
ClientRealtimeState GetRobotRealtimeState() const;
```

获取线程安全的 CRI 数据快照。

---

### SetCriDataReceived

```cpp
void SetCriDataReceived(std::function<void(const ClientRealtimeState&)> cb);
```

设置 CRI 数据回调。每次收到 CRI 帧时触发。

---

### WaitForCriData

```cpp
void WaitForCriData(double timeout_s = 5.0);
```

阻塞等待第一个 CRI 数据帧到达。

**参数：**
- `timeout_s` — 最大等待秒数，默认 5.0

**使用场景：** 调用 `*Sync` 阻塞运动方法前，确保 CRI 数据已开始推送。

---

## 运动学

### ForwardKinematics

```cpp
std::vector<double> ForwardKinematics(const FKParams& params, int id = 1);
```

正解：关节角 → TCP 位姿。

**FKParams：**
```cpp
struct FKParams {
    std::vector<double> jp;   // 关节角（度）
    std::vector<double> coor; // 可选用户坐标系
    std::vector<double> tool; // 可选工具坐标系
    std::vector<double> ep;   // 可选附加轴
};
```

**返回：** TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）

---

### InverseKinematics

```cpp
std::vector<double> InverseKinematics(const IKParams& params, int id = 1);
```

逆解：TCP 位姿 → 关节角。

**IKParams：**
```cpp
struct IKParams {
    std::vector<double> cp;   // TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;   // 参考关节角（度）
    std::vector<double> ep;   // 可选附加轴
};
```

**返回：** 关节角（度）

---

### CalculateRelativePose

```cpp
std::vector<double> CalculateRelativePose(const RelativePoseParams& params, int id = 1);
```

笛卡尔相对位姿计算。

**RelativePoseParams：**
```cpp
struct RelativePoseParams {
    std::vector<double> pos;      // 当前位姿
    std::vector<double> offset;   // 偏移量
    CoorType coorType = CoorType::Tool;  // 坐标系类型
    std::vector<double> posCoor;  // 可选位置坐标系
    std::vector<double> coor;     // 可选坐标系
};
```

---

## 全局变量

### GetGlobalVars

```cpp
nlohmann::json GetGlobalVars(int id = 1);
```

获取所有全局变量（原始 JSON 响应）。

---

### SaveGlobalVars

```cpp
CommandResult SaveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1);
```

保存全局变量。

**Variable：**
```cpp
struct Variable {
    std::string val;  // JSON 字符串
    std::string nm;   // 备注
};
```

**示例：**
```cpp
std::map<std::string, Codroid::Variable> vars = {
    {"counter", Codroid::Variable(42, "计数器")},
    {"name", Codroid::Variable(std::string("test"), "名称")}
};
robot.SaveGlobalVars(vars);
```

---

### RemoveGlobalVars

```cpp
CommandResult RemoveGlobalVars(const std::vector<std::string>& names, int id = 1);
```

删除全局变量。

---

## 主题订阅

### SubscribePublishTopic

```cpp
ClientPublishSubscription SubscribePublishTopic(
    std::string topicTy,
    std::function<void(const ClientPublishNotification&)> handler,
    int tc_milliseconds = 100);
```

订阅推送主题。

**参数：**
- `topicTy` — 主题字符串（如 `"publish/RobotStatus"`）
- `handler` — 回调函数
- `tc_milliseconds` — 推送周期，默认 100 ms

**返回：** 订阅句柄，析构时自动停止订阅

**示例：**
```cpp
auto sub = robot.SubscribePublishTopic(
    "publish/RobotStatus",
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "状态更新: " << n.db_json << std::endl;
    }
);
```

---

## 工程/脚本

### RunScript

```cpp
CommandResult RunScript(const std::string& mainScript,
                        const std::unordered_map<std::string, std::string>& subThreads = {},
                        const std::unordered_map<std::string, std::string>& subPrograms = {},
                        const std::unordered_map<std::string, std::string>& interrupts = {},
                        const nlohmann::json& vars = {},
                        int id = 1);
```

运行远程 Lua 脚本。

---

### EnterRemoteScriptMode

```cpp
CommandResult EnterRemoteScriptMode(int id = 1);
```

进入远程脚本模式。

---

### Run / RunByIndex / RunStep

```cpp
CommandResult Run(const std::string& projectId, int id = 1);
CommandResult RunByIndex(int index, int id = 1);
CommandResult RunStep(const std::string& projectId, int id = 1);
```

运行工程。

---

### PauseProject / ResumeProject / StopProject

```cpp
CommandResult PauseProject(int id = 1);
CommandResult ResumeProject(int id = 1);
CommandResult StopProject(int id = 1);
```

暂停、恢复、停止工程。

---

## 机器人设置参数

### GetRobotParameters

```cpp
ClientRobotParameters GetRobotParameters(int id = 1);
```

获取设置界面参数。

**ClientRobotParameters：**
```cpp
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
```

---

### SetDefaultPayloadId / SetDefaultToolId / SetDefaultUserCoordinateId

```cpp
CommandResult SetDefaultPayloadId(int payloadId, int id = 1);
CommandResult SetDefaultToolId(int toolId, int id = 1);
CommandResult SetDefaultUserCoordinateId(int coordinateId, int id = 1);
```

设置默认负载、工具、用户坐标系编号（1~15）。

---

### SaveToolFrames / SetToolFrame

```cpp
CommandResult SaveToolFrames(const std::vector<ClientRobotFrame>& frames, int id = 1);
CommandResult SetToolFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);
```

保存/修改工具坐标系。

---

### SavePayloadFrames / SetPayloadFrame

```cpp
CommandResult SavePayloadFrames(const std::vector<ClientRobotPayload>& frames, int id = 1);
CommandResult SetPayloadFrame(int frame_id, const ClientRobotPayload& frame, int id = 1);
```

保存/修改负载坐标系。

---

### SetUserCoordinateFrame

```cpp
CommandResult SetUserCoordinateFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);
```

修改用户坐标系。

---

### SetPayload

```cpp
CommandResult SetPayload(int payloadId, int id = 1);
```

运行时切换当前负载。

---

### SetManualMoveRate / SetAutoMoveRate

```cpp
CommandResult SetManualMoveRate(int pct, int id = 1);
CommandResult SetAutoMoveRate(int pct, int id = 1);
```

设置手动/自动倍率（0~100）。

---

### SetCollisionSensitivity

```cpp
CommandResult SetCollisionSensitivity(int sensitivity, int id = 1);
```

设置碰撞敏感度。

---

## 错误处理设置

### SetThrowOnCommandError

```cpp
void SetThrowOnCommandError(bool enable);
```

设置为 `true` 时，指令失败抛 `CodroidCommandException`；默认为 `false`，通过 `CommandResult::error_msg` 返回。

---

### ThrowOnCommandError

```cpp
bool ThrowOnCommandError() const noexcept;
```

获取当前错误处理模式。
