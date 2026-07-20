# CRI 实时控制 API

`TrajectoryGenerator` / `CriRealtimeDispatcher` 已由 `#include "Codroid/client.hpp"` 引入，无需再单独 include。

本章介绍 CRI（Controller Realtime Interface）实时数据接收和轨迹下发。

---

## 概述

CRI 实时控制包括两个部分：

1. **CRI 数据推送** — 控制器通过 UDP 向 SDK 推送实时状态（308 字节/帧）
2. **CRI 实时控制** — SDK 通过 UDP 向控制器下发轨迹指令（64 字节/帧）

---

## CRI 数据推送

### 启动推送

```cpp
// 1. 连接
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");

// 2. 启动 CRI 数据推送（绑定本机 IP 和端口）
robot.StartCriDataPush("192.168.1.150", 9030);

// 3. 等待首帧数据
robot.WaitForCriData(5.0);
```

### StartCriDataPush

```cpp
CommandResult StartCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

请求 CRI 状态 UDP 推送。

**参数：**
- `udpIp` — 本机 IP（用于绑定 UDP 监听）
- `udpPort` — 本机 UDP 端口

---

### StopCriDataPush

```cpp
CommandResult StopCriDataPush(int id = 1);
CommandResult StopCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

停止 CRI 数据推送。

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

### GetRobotRealtimeState

```cpp
ClientRealtimeState GetRobotRealtimeState() const;
```

获取线程安全的 CRI 数据快照。

**返回：** `ClientRealtimeState` 结构体（详见 [数据类型](05-api-reference-types.md)）

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
    std::cout << "实时控制模式: " << (state.realtime_control_mode ? "是" : "否") << std::endl;
}
```

---

### SetCriDataReceived

```cpp
void SetCriDataReceived(std::function<void(const ClientRealtimeState&)> cb);
```

设置 CRI 数据回调。每次收到 CRI 帧时触发。

**参数：**
- `cb` — 回调函数，接收 `ClientRealtimeState` 引用

**注意：** 回调在内部接收线程上执行，避免长时间阻塞。

**使用示例：**
```cpp
robot.SetCriDataReceived([](const Codroid::ClientRealtimeState& data) {
    if (data.data_valid) {
        std::cout << "时间戳: " << data.timestamp_ms << " ms" << std::endl;
        std::cout << "关节: ";
        for (auto j : data.joint_position) std::cout << j << " ";
        std::cout << std::endl;
    }
});
```

---

### GetCriUdpListenPort

```cpp
int GetCriUdpListenPort() const;
```

获取 CRI UDP 监听端口。

**返回：** 端口号，未启动推送时可能为 0

---

## CRI 实时控制

### 启动实时控制

```cpp
// 1. 连接并启动 CRI 推送
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");
robot.StartCriDataPush("192.168.1.150", 9030);
robot.WaitForCriData(5.0);

// 2. 启动实时控制
// filterType: 滤波类型（0~3）
// durationMs: 控制周期（ms），须与 SendTrajectory 的 period 一致
// startBuffer: 起始缓冲
robot.StartCriControl(1, 4, 5);

// 3. 等待实时控制模式生效
while (true) {
    auto state = robot.GetRobotRealtimeState();
    if (state.realtime_control_mode) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// 4. 下发轨迹
// ...（见下文）

// 5. 停止实时控制
robot.StopCriControl();
```

---

### StartCriControl

```cpp
CommandResult StartCriControl(int filterType, int durationMs, int startBuffer, int id = 1);
```

启动实时控制会话。

**参数：**
- `filterType` — 滤波类型（0~3）
- `durationMs` — 控制周期（ms），须与后续 `CriRealtimeDispatcher::SendTrajectory` 的 period 一致
- `startBuffer` — 起始缓冲

---

### StopCriControl

```cpp
CommandResult StopCriControl(int id = 1);
```

停止实时控制会话。

---

## CriRealtimeDispatcher

向控制器 CRI 实时控制 UDP 端口发送 64 字节指令帧。

```cpp

Codroid::CriRealtimeDispatcher dispatcher("192.168.1.136", 9030, true);
```

### 构造函数

```cpp
explicit CriRealtimeDispatcher(std::string controller_ip, int controller_udp_port = 9030,
                               bool convert_to_si = true);
```

**参数：**
- `controller_ip` — 控制器 IP
- `controller_udp_port` — 控制器 UDP 端口，默认 9030
- `convert_to_si` — 为 `true` 时（默认），上层传入关节 **度**、笛卡尔 **mm+度**，发送前自动转换为 **rad/m+rad**

---

### SendCommand

```cpp
void SendCommand(const std::array<double, 6>& position6, TrajectorySpace space);
```

发送单帧（阻塞直到操作系统写完或出错）。

**参数：**
- `position6` — 6 个位置值（关节角或笛卡尔位姿）
- `space` — 坐标空间类型

**TrajectorySpace：**
```cpp
enum class TrajectorySpace {
    Joint,     // 关节空间（度）
    Cartesian  // 笛卡尔空间（mm+度）
};
```

**使用示例：**
```cpp
// 关节空间
std::array<double, 6> joints = {0, 0, 90, 0, 90, 0};
dispatcher.SendCommand(joints, Codroid::TrajectorySpace::Joint);

// 笛卡尔空间
std::array<double, 6> pose = {400, 0, 300, 180, 0, 0};
dispatcher.SendCommand(pose, Codroid::TrajectorySpace::Cartesian);
```

---

### SendTrajectory

```cpp
void SendTrajectory(const std::vector<TrajectoryPoint>& trajectory, TrajectorySpace space, int period_ms,
                    const std::atomic<bool>* cancel = nullptr);
```

按周期下发整条轨迹。

**参数：**
- `trajectory` — 轨迹点列表
- `space` — 坐标空间类型
- `period_ms` — 发送周期（ms），须满足 (0, 1000]，且建议与 `StartCriControl` 的 `durationMs` 一致
- `cancel` — 非空且 `*cancel==true` 时提前结束

**TrajectoryPoint：**
```cpp
struct TrajectoryPoint {
    std::array<double, 6> position;  // 6 个位置值
    double time;                     // 时间戳（秒）
};
```

**使用示例：**
```cpp
// 生成轨迹
Codroid::TrajectoryRequest request;
request.space = Codroid::TrajectorySpace::Joint;
request.duration_s = 2.0;
request.frequency_hz = 250;  // 250Hz = 4ms 周期

std::array<double, 6> start = {0, 0, 90, 0, 90, 0};
std::array<double, 6> target = {10, 0, 90, 0, 90, 0};

auto trajectory = Codroid::TrajectoryGenerator::Generate(start, target, request);

// 下发轨迹
std::atomic<bool> cancel{false};
dispatcher.SendTrajectory(trajectory, Codroid::TrajectorySpace::Joint, 4, &cancel);
```

---

### Close

```cpp
void Close() noexcept;
```

关闭 UDP 连接。之后调用 `Send*` 将抛 `CodroidException`。

---

### IsOpen

```cpp
bool IsOpen() const noexcept;
```

检查 UDP 连接是否打开。

---

## TrajectoryGenerator

离线轨迹生成。

```cpp
```

### Generate

```cpp
static std::vector<TrajectoryPoint> Generate(const std::array<double, 6>& start,
                                             const std::array<double, 6>& target,
                                             const TrajectoryRequest& request);
```

生成单段轨迹。

**参数：**
- `start` — 起始位置
- `target` — 目标位置
- `request` — 轨迹请求参数

**返回：** 轨迹点列表

---

### GenerateMultiSegment

```cpp
static std::vector<TrajectoryPoint> GenerateMultiSegment(const std::vector<std::array<double, 6>>& waypoints,
                                                         const TrajectoryRequest& request);
```

生成多段路点轨迹。段间去重复首点。

**参数：**
- `waypoints` — 路点列表（至少 2 个）
- `request` — 轨迹请求参数

**返回：** 轨迹点列表

**使用示例：**
```cpp
std::vector<std::array<double, 6>> waypoints = {
    {0, 0, 90, 0, 90, 0},
    {10, 0, 90, 0, 90, 0},
    {10, 10, 90, 0, 90, 0},
    {0, 10, 90, 0, 90, 0}
};

Codroid::TrajectoryRequest request;
request.space = Codroid::TrajectorySpace::Joint;
request.duration_s = 4.0;
request.frequency_hz = 250;

auto trajectory = Codroid::TrajectoryGenerator::GenerateMultiSegment(waypoints, request);
```

---

## TrajectoryRequest

轨迹生成请求参数。

```cpp
struct TrajectoryRequest {
    TrajectorySpace space = TrajectorySpace::Joint;
    TrajectoryProfile profile = TrajectoryProfile::Trapezoidal;
    double duration_s = 1.0;
    double frequency_hz = 250.0;
    double max_velocity = 0.0;
    double max_acceleration = 0.0;
    double max_jerk = 0.0;
};
```

**字段说明：**

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `space` | `TrajectorySpace` | `Joint` | 坐标空间 |
| `profile` | `TrajectoryProfile` | `Trapezoidal` | 速度曲线类型 |
| `duration_s` | `double` | 1.0 | 总时长（秒） |
| `frequency_hz` | `double` | 250.0 | 采样频率（Hz） |
| `max_velocity` | `double` | 0.0 | 最大速度（0=自动） |
| `max_acceleration` | `double` | 0.0 | 最大加速度（0=自动） |
| `max_jerk` | `double` | 0.0 | 最大加加速度（0=自动） |

---

### TrajectorySpace

```cpp
enum class TrajectorySpace {
    Joint,     // 关节空间（度）
    Cartesian  // 笛卡尔空间（mm+度）
};
```

---

### TrajectoryProfile

```cpp
enum class TrajectoryProfile {
    Cubic,        // 三次多项式
    Trapezoidal   // 梯形速度曲线
};
```

---

## 完整示例：CRI 实时轨迹下发

```cpp
#include "Codroid/client.hpp"
#include <iostream>
#include <atomic>
#include <thread>

int main() {
    Codroid::CodroidClient robot;

    // 1. 连接
    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 2. 启动 CRI 数据推送
    robot.StartCriDataPush("192.168.1.150", 9030);
    robot.WaitForCriData(5.0);

    // 3. 启动实时控制
    robot.StartCriControl(1, 4, 5);

    // 4. 等待实时控制模式生效
    while (true) {
        auto state = robot.GetRobotRealtimeState();
        if (state.realtime_control_mode) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "实时控制模式已生效" << std::endl;

    // 5. 生成轨迹
    auto state = robot.GetRobotRealtimeState();
    std::array<double, 6> start;
    for (int i = 0; i < 6; i++) start[i] = state.joint_position[i];

    std::array<double, 6> target = {10, 0, 90, 0, 90, 0};

    Codroid::TrajectoryRequest request;
    request.space = Codroid::TrajectorySpace::Joint;
    request.duration_s = 2.0;
    request.frequency_hz = 250;

    auto trajectory = Codroid::TrajectoryGenerator::Generate(start, target, request);
    std::cout << "轨迹点数: " << trajectory.size() << std::endl;

    // 6. 创建 Dispatcher 并下发轨迹
    Codroid::CriRealtimeDispatcher dispatcher("192.168.1.136", 9030, true);

    std::atomic<bool> cancel{false};
    std::cout << "开始下发轨迹..." << std::endl;
    dispatcher.SendTrajectory(trajectory, Codroid::TrajectorySpace::Joint, 4, &cancel);
    std::cout << "轨迹下发完成" << std::endl;

    // 7. 停止实时控制
    robot.StopCriControl();

    // 8. 停止 CRI 推送
    robot.StopCriDataPush();

    // 9. 断开
    robot.Disconnect();

    return 0;
}
```

---

## 常见问题

### Q1: 轨迹跑完后程序看起来没退出

1. 确认是否真的发完（打印 `send begin/send done`）
2. 确认是否走到清理阶段（`StopCriControl` / `StopCriDataPush` / `Disconnect`）
3. 使用最新二进制，不要混用旧 build 目录程序

### Q2: 为什么 CRI 数据和控制器显示单位不一致？

- 核对是否把 CRI 原始 `m/rad` 当成 `mm/deg` 直接使用
- 按本 SDK 的默认约定，应用层统一使用 `mm/deg`
- `CriRealtimeDispatcher` 在 `convert_to_si=true` 时自动转换

### Q3: `SendTrajectory` 发送期间无输出

这是正常的。`SendTrajectory` 在发送期间可能长时间无输出，建议在业务层打印分段耗时日志。

### Q4: 实时控制模式未生效

1. 确认已调用 `StartCriControl`
2. 等待 `realtime_control_mode == true` 后再下发轨迹
3. 检查 `durationMs` 与 `SendTrajectory` 的 `period_ms` 是否一致

### Q5: 轨迹下发超时或失败

1. 检查网络连接
2. 确认控制器 UDP 端口（默认 9030）可访问
3. 检查 `period_ms` 是否在 (0, 1000] 范围内
