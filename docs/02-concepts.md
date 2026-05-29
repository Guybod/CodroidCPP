# 核心概念

## CodroidClient 生命周期

```
CodroidClient robot;
        │
        ▼
   Connect()  ──或──  ConnectRemoteAndSwitchOn()
        │
        ▼
   [ IO / Register / Motion / CRI ... ]
        │
        ▼
    Disconnect()
```

```cpp
Codroid::CodroidClient robot;

// 连接
if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
    return 1;
}

// ... 使用 robot ...

// 断开（必须调用）
robot.Disconnect();
```

### 构造函数

```cpp
Codroid::CodroidClient robot;
```

- 默认构造，不连接
- 调用 `Connect` 或 `ConnectRemoteAndSwitchOn` 建立连接
- TCP 端口默认 **9001**

### 连接方法

| 方法 | 说明 |
|------|------|
| `Connect(ip, port)` | 仅建立 TCP 连接，不切模式、不上电 |
| `ConnectRemoteAndSwitchOn(ip, port, local_ip)` | 连接 + 远程模式 + 上电，`local_ip` 用于 CRI UDP 绑定 |
| `Disconnect()` | 断开 TCP，停止 CRI 线程 |

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `GetRobotRealtimeState()` | `ClientRealtimeState` | 线程安全的 CRI 数据快照 |
| `GetCriUdpListenPort()` | `int` | CRI UDP 监听端口 |

### 回调

```cpp
robot.SetCriDataReceived([](const Codroid::ClientRealtimeState& data) {
    std::cout << "关节: ";
    for (auto j : data.joint_position) std::cout << j << " ";
    std::cout << std::endl;
});
```

每次解析完一个有效的 CRI UDP 帧后触发。回调在内部接收线程上执行，避免长时间阻塞。

---

## TCP 指令模型

每个与控制器通信的 SDK 方法都遵循以下模式：

1. SDK 分配唯一的 `id`
2. SDK 序列化 `{ id, ty, db }` 为 JSON 并通过 TCP 发送
3. 控制器响应 `{ id, ty, db, err }`
4. SDK 通过 `id` 匹配响应
5. 如果 `err` 非空 → `CommandResult::error_msg` 非空（或抛异常）
6. 如果 10 秒内无响应 → 超时

### CommandResult

```cpp
struct CommandResult {
    int id = 0;                 // 请求 id
    std::string ty;             // 响应类型
    std::string error_msg;      // 错误信息（空表示成功）
    std::string raw_json;       // 完整响应 JSON

    bool Ok() const noexcept;   // error_msg 为空返回 true
};
```

大多数方法返回 `CommandResult`。检查 `Ok()` 判断是否成功。

---

## 单位约定

SDK 公共 API 使用 **毫米** 和 **度**。这与 TCP JSON 协议一致。

| 上下文 | 线性 | 角度 |
|--------|------|------|
| SDK API, TCP JSON | **mm** | **deg** |
| CRI UDP 线上格式 | **m** | **rad** |
| `ClientRealtimeState` (解析后) | **mm** | **deg** |

**重要：** CRI UDP 二进制载荷使用米和弧度。SDK 在内部自动转换为 mm/deg。不要假设原始 UDP 浮点数是 mm/deg。

---

## 命名约定

### API 命名

所有公共方法使用 **PascalCase**，与 C# 和 Python SDK 保持一致。

```cpp
robot.ConnectRemoteAndSwitchOn("192.168.8.136");
int di = robot.GetDi(0);
robot.MovJ(joints, 40, 100);
robot.SetDo(10, 1);
```

### 类型命名

- 结构体：`PascalCase`（`CommandResult`、`JointPoint`）
- 枚举：`PascalCase`（`MoveType`、`JogMode`）
- 枚举值：`PascalCase`（`MoveType::movJ`、`JogMode::Joint`）
- 方法：`PascalCase`（`GetDi`、`SetDo`）
- 私有成员：`snake_case_`（`impl_`、`error_msg`）

---

## 线程安全

- `GetRobotRealtimeState()` — 线程安全（返回副本）
- `SetCriDataReceived(callback)` — 线程安全
- 所有 TCP 方法 — 可从任意线程调用，但不要在同一 `CodroidClient` 上并发调用
- `CriRealtimeDispatcher::SendCommand` / `SendTrajectory` — 非线程安全

---

## 异常处理

### 异常类型

| 异常 | 触发条件 | 来源 |
|------|----------|------|
| `CodroidCommandException` | 控制器返回 `err` 字段 | TCP 响应 |
| `CodroidException` | 通用运行时错误 | SDK 内部 |
| `std::runtime_error` | 标准库异常 | 标准库 |

### CodroidCommandException 属性

```cpp
class CodroidCommandException : public CodroidException {
public:
    int request_id() const noexcept;           // 协议请求 ID
    const std::string& command_ty() const noexcept;  // 如 "Robot/move"
    const std::string& controller_error() const noexcept;  // 控制器的 err 字段
    const std::string& raw_response_json() const noexcept; // 完整响应
};
```

### 错误处理模式

#### 模式 1：检查返回值（默认）

```cpp
auto result = robot.SetDo(999, 1);
if (!result.Ok()) {
    std::cerr << "错误: " << result.error_msg << std::endl;
    std::cerr << "原始响应: " << result.raw_json << std::endl;
}
```

#### 模式 2：抛异常

```cpp
robot.SetThrowOnCommandError(true);

try {
    robot.SetDo(999, 1);
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "错误: " << ex.controller_error() << std::endl;
}
```

---

## CRI 实时数据

### 启动 CRI 推送

```cpp
// 1. 连接
robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150");

// 2. 启动 CRI 数据推送
robot.StartCriDataPush("192.168.8.150", 9030);

// 3. 等待首帧数据
robot.WaitForCriData(5.0);

// 4. 获取实时状态
auto state = robot.GetRobotRealtimeState();
if (state.data_valid) {
    std::cout << "关节位置: ";
    for (auto j : state.joint_position) std::cout << j << " ";
    std::cout << std::endl;
}
```

### ClientRealtimeState 字段

| 字段 | 类型 | 单位 | 说明 |
|------|------|------|------|
| `timestamp_ms` | `int64_t` | ms | 时间戳 |
| `data_valid` | `bool` | - | 数据有效标志 |
| `joint_position` | `vector<double>` | deg | 关节位置 |
| `joint_velocity` | `vector<double>` | deg/s | 关节速度 |
| `tcp_pose` | `vector<double>` | mm+deg | TCP 位姿 [x,y,z,rx,ry,rz] |
| `tcp_velocity` | `vector<double>` | mm/s, °/s | TCP 速度 |
| `in_motion` | `bool` | - | 运动中标志 |
| `realtime_control_mode` | `bool` | - | 实时控制模式 |

---

## 主题订阅

订阅控制器推送的主题：

```cpp
auto subscription = robot.SubscribePublishTopic(
    "publish/RobotStatus",
    [](const Codroid::ClientPublishNotification& notification) {
        std::cout << "主题: " << notification.ty << std::endl;
        std::cout << "数据: " << notification.db_json << std::endl;
    },
    100  // 推送周期 ms
);

// 订阅自动在 subscription 生命周期内有效
// 析构或调用 Dispose() 时停止
```

### 可用主题

| 主题 | 说明 |
|------|------|
| `publish/ProjectState` | 工程运行状态 |
| `publish/VarUpdate` | 变量更新 |
| `publish/RobotStatus` | 机器人状态 |
| `publish/RobotPosture` | 机器人位姿 |
| `publish/RobotCoordinate` | 坐标系 |
| `publish/Log` | 日志 |
| `publish/Error` | 错误 |

---

## 下一步

- [CodroidClient API](03-api-reference-codroidclient.md) — 完整 API 参考
- [运动控制](04-api-reference-motion.md) — 运动指令详解
- [CRI 实时控制](06-api-reference-cri.md) — 实时轨迹下发
