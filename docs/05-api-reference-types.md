# 数据类型与枚举

本章介绍 SDK 中使用的数据结构、枚举和异常类。

---

## 通信相关

### CommandResult

单次 TCP 指令结果。

```cpp
struct CommandResult {
    int id = 0;                 // 请求 id（与下发一致）
    std::string ty;             // 响应类型 / 路由
    std::string error_msg;      // 控制器 `err` 或本地错误说明；空表示成功
    std::string raw_json;       // 最近一次完整响应 JSON

    bool Ok() const noexcept;   // error_msg 为空返回 true
};
```

**使用示例：**
```cpp
auto result = robot.SetDo(10, 1);
if (result.Ok()) {
    std::cout << "成功" << std::endl;
} else {
    std::cerr << "失败: " << result.error_msg << std::endl;
    std::cerr << "原始响应: " << result.raw_json << std::endl;
}
```

---

### Response（内部类型）

`Response`（含 `nlohmann::json db`）仅用于 SDK 内部 TCP 解析，**不在公开头中**。客户侧请使用 `CommandResult`（成功/失败 + `raw_json` 字符串）。

---

## 实时数据

### ClientRealtimeState / RobotRealtimeState

CRI 实时快照：关节 **度**，TCP **mm+度**，速度 mm/s 与 °/s。

```cpp
struct ClientRealtimeState {
    int64_t timestamp_ms = 0;
    bool data_valid = false;

    uint16_t status1_raw = 0;
    uint16_t status2_raw = 0;

    std::vector<double> joint_position;       // 关节位置，度
    std::vector<double> joint_velocity;       // 关节角速度，度/s
    std::vector<double> tcp_pose;             // [x,y,z,rx,ry,rz]，mm + 度
    std::vector<double> tcp_velocity;         // 线 mm/s，角 °/s
    double tcp_linear_velocity_mm_s = 0.0;
    std::vector<double> joint_output_torque;
    std::vector<double> joint_external_force;
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
```

**字段说明：**

| 字段 | 类型 | 单位 | 说明 |
|------|------|------|------|
| `timestamp_ms` | `int64_t` | ms | CRI 帧时间戳 |
| `data_valid` | `bool` | - | 数据有效标志（首帧到达前为 false） |
| `joint_position` | `vector<double>` | deg | 六轴关节位置 |
| `joint_velocity` | `vector<double>` | deg/s | 六轴关节速度 |
| `tcp_pose` | `vector<double>` | mm+deg | TCP 位姿 [x,y,z,rx,ry,rz] |
| `tcp_velocity` | `vector<double>` | mm/s, °/s | TCP 速度 |
| `tcp_linear_velocity_mm_s` | `double` | mm/s | TCP 线速度 |
| `joint_output_torque` | `vector<double>` | - | 关节输出力矩 |
| `joint_external_force` | `vector<double>` | - | 关节外力 |
| `in_motion` | `bool` | - | 运动中标志 |
| `realtime_control_mode` | `bool` | - | 实时控制模式（`StartCriControl` 后为 true） |

**状态标志：**

| 标志 | 说明 |
|------|------|
| `project_running` | 工程运行中 |
| `project_stopped` | 工程已停止 |
| `project_paused` | 工程已暂停 |
| `enabling` | 使能中 |
| `not_enabled` | 未使能 |
| `manual_mode` | 手动模式 |
| `auto_mode` | 自动模式 |
| `remote_mode` | 远程模式 |
| `dragging` | 拖拽示教中 |
| `in_motion` | 运动中 |
| `collision_stopped` | 碰撞停止 |
| `in_safety_position` | 在安全位置 |
| `has_alarm` | 有报警 |
| `simulation_mode` | 仿真模式 |
| `emergency_stop_pressed` | 急停按下 |
| `rescue_mode` | 救援模式 |
| `realtime_control_mode` | 实时控制模式 |

**使用示例：**
```cpp
auto state = robot.GetRobotRealtimeState();

if (state.data_valid) {
    std::cout << "关节位置: ";
    for (auto j : state.joint_position) std::cout << j << " ";
    std::cout << std::endl;

    std::cout << "TCP 位姿: ";
    for (auto p : state.tcp_pose) std::cout << p << " ";
    std::cout << std::endl;

    std::cout << "运动中: " << (state.in_motion ? "是" : "否") << std::endl;
    std::cout << "实时控制: " << (state.realtime_control_mode ? "是" : "否") << std::endl;
}
```

---

## IO 相关

### ClientIoInfo / IOInfo

IO 读回信息。

```cpp
struct ClientIoInfo {
    std::string type;   // 类型字符串（DI/DO/AI/AO）
    int port = 0;       // 端口号
    double value = 0.0; // 值
};
```

---

### ClientRegisterInfo / RegisterInfo

寄存器信息。

```cpp
struct ClientRegisterInfo {
    int address = 0;    // 地址
    double value = 0.0; // 值
};
```

---

## 运动相关

### ClientMoveType / MoveType

运动类型枚举。

```cpp
enum class ClientMoveType {
    MovJ,       // 关节运动
    MovL,       // 直线（笛卡尔）
    MovC,       // 圆弧（经由中间点）
    MovCircle   // 整圆/多圈
};
```

---

### MoveToType

MoveTo 运动类别。

```cpp
enum class MoveToType : int {
    Stop = -1,       // 停止 MoveTo
    Home = 0,        // 回 Home
    Safe = 1,        // 回安全位
    Candle = 2,      // 回 Candle 位
    Packing = 3,     // 回 Packing 位
    Joint = 4,       // 关节规划到目标
    Line = 5,        // 直线规划到目标
    ResumePoint = 6
};
```

---

### JogMode

点动模式。

```cpp
enum class JogMode {
    Joint = 1,  // 关节点动
    Line = 2    // 直线点动
};
```

---

### CoorType

坐标系类型。

```cpp
enum class CoorType {
    Tool,  // 工具系
    User   // 用户系
};
```

---

## 机器人设置

### ClientRobotFrame / RobotFrameEntry

工具坐标系或用户坐标系表中的一帧。

```cpp
struct ClientRobotFrame {
    int id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
};
```

**说明：**
- `id` — 序号（1~15），0 号槽位只读且恒为 0
- `x, y, z` — 位置（mm）
- `a, b, c` — 姿态（度）

---

### ClientRobotPayload / RobotPayloadEntry

负载坐标系表中的一帧。

```cpp
struct ClientRobotPayload {
    int id = 0;
    double m = 0.0;   // 质量（kg）
    double mx = 0.0;  // 质心 X（mm）
    double my = 0.0;  // 质心 Y（mm）
    double mz = 0.0;  // 质心 Z（mm）
};
```

---

### ClientRobotParameters / RobotParameters

机器人设置参数快照。

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

**使用示例：**
```cpp
auto params = robot.GetRobotParameters();
if (params.valid) {
    std::cout << "默认工具: " << params.default_tool_id << std::endl;
    std::cout << "默认负载: " << params.default_payload_id << std::endl;
    std::cout << "最大负载: " << params.max_payload << " kg" << std::endl;

    std::cout << "工具坐标系:" << std::endl;
    for (const auto& t : params.tool) {
        std::cout << "  " << t.id << ": " << t.x << ", " << t.y << ", " << t.z << std::endl;
    }
}
```

---

## 主题推送

### ClientPublishNotification / PublishNotification

主题推送一帧。

```cpp
struct ClientPublishNotification {
    std::string ty;        // 主题类型
    std::string db_json;   // db 子树的 JSON 字符串
    std::string raw_json;  // 整帧 JSON
};
```

---

### ClientPublishSubscription / PublishTopicSubscription

订阅句柄。析构或 `Dispose()` 后停止本地分发。

```cpp
class ClientPublishSubscription {
public:
    ClientPublishSubscription();
    ~ClientPublishSubscription();

    // 不可复制
    ClientPublishSubscription(const ClientPublishSubscription&) = delete;
    ClientPublishSubscription& operator=(const ClientPublishSubscription&) = delete;

    // 可移动
    ClientPublishSubscription(ClientPublishSubscription&&) noexcept;
    ClientPublishSubscription& operator=(ClientPublishSubscription&&) noexcept;

    void Dispose();           // 提前结束订阅
    bool IsValid() const noexcept;  // 是否仍绑定有效内部资源
};
```

**使用示例：**
```cpp
{
    auto sub = robot.SubscribePublishTopic(
        "publish/RobotStatus",
        [](const Codroid::ClientPublishNotification& n) {
            std::cout << n.db_json << std::endl;
        }
    );

    // 订阅在此作用域内有效
    std::this_thread::sleep_for(std::chrono::seconds(5));
}
// sub 析构，订阅自动停止
```

---

### PublishTopics

协议 15.x 主题字面量。

```cpp
struct PublishTopics {
    static constexpr const char* ProjectState = "publish/ProjectState";
    static constexpr const char* VarUpdate = "publish/VarUpdate";
    static constexpr const char* RobotStatus = "publish/RobotStatus";
    static constexpr const char* RobotPosture = "publish/RobotPosture";
    static constexpr const char* RobotCoordinate = "publish/RobotCoordinate";
    static constexpr const char* Log = "publish/Log";
    static constexpr const char* Error = "publish/Error";
};
```

---

## 全局变量

### Variable

全局变量一项。

```cpp
struct Variable {
    std::string val;  // JSON 字符串
    std::string nm;   // 备注

    template<typename T>
    Variable(const T& value, const std::string& note = "");
    Variable() = default;
};
```

**构造示例：**
```cpp
// 整数
Codroid::Variable v1(42, "计数器");

// 浮点数
Codroid::Variable v2(3.14, "PI");

// 字符串
Codroid::Variable v3(std::string("hello"), "问候");

// 布尔
Codroid::Variable v4(true, "标志");

// 数组（通过 JSON）
nlohmann::json arr = {1, 2, 3};
Codroid::Variable v5(arr.dump(), "数组");
```

---

## 运动学

### FKParams

正解请求参数。

```cpp
struct FKParams {
    std::vector<double> jp;    // 关节角（度）
    std::vector<double> coor;  // 可选用户坐标系
    std::vector<double> tool;  // 可选工具坐标系
    std::vector<double> ep;    // 可选附加轴

    explicit FKParams(const std::vector<double>& jointPos);
    FKParams() = default;
};
```

**使用示例：**
```cpp
Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
auto tcp = robot.ForwardKinematics(fk);
// tcp = [x, y, z, rx, ry, rz]（mm+度）
```

---

### IKParams

逆解请求参数。

```cpp
struct IKParams {
    std::vector<double> cp;   // TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;   // 参考关节角（度）
    std::vector<double> ep;   // 可选附加轴

    explicit IKParams(const std::vector<double>& cartesianPos);
    IKParams() = default;
};
```

**使用示例：**
```cpp
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
ik.rj = {0, 0, 90, 0, 90, 0};  // 参考关节
auto joints = robot.InverseKinematics(ik);
// joints = [j1, j2, j3, j4, j5, j6]（度）
```

---

### RelativePoseParams

相对位姿计算参数。

```cpp
struct RelativePoseParams {
    std::vector<double> pos;      // 当前位姿
    std::vector<double> offset;   // 偏移量
    CoorType coorType = CoorType::Tool;
    std::vector<double> posCoor;  // 可选位置坐标系
    std::vector<double> coor;     // 可选坐标系

    RelativePoseParams(const std::vector<double>& p, const std::vector<double>& o, CoorType type);
    RelativePoseParams() = default;
};
```

---

## 其他类型

### RS485Parity / RS485StopBits

RS485 串口配置。

```cpp
enum class RS485Parity : int { None = 0, Odd = 1, Even = 2 };
enum class RS485StopBits : int { One = 1, Two = 2 };
```

---

### ExtendArrayType

寄存器扩展数组元素类型。

```cpp
enum class ExtendArrayType {
    Bool, UInt8, Int8, UInt16,
    Int16, UInt32, Int32, Float32
};
```

---

## 异常类

### CodroidException

通用运行时错误。

```cpp
class CodroidException : public std::runtime_error {
public:
    explicit CodroidException(const std::string& message);
};
```

---

### CodroidCommandException

TCP 指令失败异常。

```cpp
class CodroidCommandException : public CodroidException {
public:
    CodroidCommandException(int request_id, std::string command_ty,
                           std::string controller_error, std::string raw_response_json);

    int request_id() const noexcept;                    // 协议请求 ID
    const std::string& command_ty() const noexcept;     // 如 "Robot/move"
    const std::string& controller_error() const noexcept;  // 控制器的 err 字段
    const std::string& raw_response_json() const noexcept; // 完整响应
};
```

**使用示例：**
```cpp
robot.SetThrowOnCommandError(true);

try {
    robot.SetDo(999, 1);  // 无效端口
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "请求 ID: " << ex.request_id() << std::endl;
    std::cerr << "命令类型: " << ex.command_ty() << std::endl;
    std::cerr << "控制器错误: " << ex.controller_error() << std::endl;
    std::cerr << "原始响应: " << ex.raw_response_json() << std::endl;
}
```

---

## 固件版本常量

```cpp
inline constexpr const char* MinControllerFirmware = "2.3.3.43";
inline constexpr const char* RobotParameterMinFirmware = MinControllerFirmware;
```

本 SDK 所有对外接口均要求控制器固件 **≥ 2.3.3.43**。
