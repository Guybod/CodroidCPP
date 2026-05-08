# Codroid C++ SDK

Codroid C++ SDK 是用于控制 Codroid 机械臂的 C++ 开发包。SDK 通过 TCP JSON 指令通道和 CRI UDP 实时通道完成连接、上电、运动、IO、寄存器、实时状态读取和实时轨迹下发。

按本文顺序完成环境准备、编译、示例运行和业务代码接入。

## 资料入口

- API 在线手册：[金山文档](https://www.kdocs.cn/l/cqlm2DOsjGRp)
- 本仓库手册：`SDK_GUIDE.md`
- 主要入口头文件：`include/codroid/client.hpp`
- CRI 完整示例：`examples/14_cri_trajectory.cpp`

## 准备工作

### 控制器与网络

- 控制器 IP，例如 `192.168.8.136`
- 本机网卡 IP，例如 `192.168.8.150`
- TCP 指令端口默认是 `9001`
- CRI 实时控制下发端口默认是控制器 UDP `9030`

确认本机与控制器能互通。示例中的 IP 需要按现场网络修改。

### Linux 环境

```bash
sudo apt update
sudo apt install -y build-essential cmake git libasio-dev nlohmann-json3-dev
```

### Windows 环境

- 安装 CMake，并加入 PATH
- 安装 Visual Studio 2019 / 2022 / 2026
- 安装 “使用 C++ 的桌面开发”
- 使用 x64 目标平台

## 1. 编译 SDK

### Linux

```bash
chmod +x build_linux.sh
./build_linux.sh
```

编译完成后，产物在 `build_linux/`：

- `libCodroid.so`
- 所有 `examples/*.cpp` 对应的可执行程序

### Windows (MSVC)

```bat
build_msvc.bat
```

脚本会提示选择 Visual Studio 版本，并同时构建 Debug / Release。产物在 `build_msvc/`。

## 2. 运行第一个示例

先打开 `examples/01_basic_usage.cpp`，将机器人 IP 改为控制器 IP。

Linux 下运行：

```bash
./build_linux/01_basic_usage
```

如果能看到连接成功，并且控制器收到上电/下电命令，说明 TCP 指令通道已经正常。

## 3. 编写第一个程序

业务代码优先使用 `Codroid::CodroidClient`，命名与 C# SDK 对齐。

```cpp
#include "codroid/client.hpp"

#include <iostream>

int main() {
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    auto state = robot.GetRobotRealtimeState();
    if (state.data_valid) {
        std::cout << "CRI timestamp: " << state.timestamp_ms << "\n";
    }

    robot.Disconnect();
    return 0;
}
```

最常用的连接流程是：

1. `ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)`
2. 执行业务命令
3. `Disconnect()`

`ConnectRemoteAndSwitchOn` 会完成 TCP 连接、切自动/远程、启动 CRI 数据推送和上电等常用准备动作。

## 4. 常用功能

### IO

```cpp
int di0 = robot.GetDi(0, robot.NextRequestId());
robot.SetDo(0, 1, robot.NextRequestId());
```

### 寄存器

```cpp
double value = robot.GetRegisterValue(49100, robot.NextRequestId());
robot.SetRegisterValue(49100, 520.0, robot.NextRequestId());
```

### 运动控制

```cpp
robot.SetAutoMoveRate(40, robot.NextRequestId());
robot.StopRobotMove(robot.NextRequestId());
```

多段 `Move`、`MoveTo`、Jog 等完整用法见 `examples/08_move.cpp` 和 API 在线手册。

## 5. CRI 实时控制

CRI 实时控制由两条 UDP 路径组成：

- 控制器 -> 本机：`StartCriDataPush` 接收 308 字节实时状态
- 本机 -> 控制器：`CriRealtimeDispatcher` 下发 64 字节实时控制数据

先运行完整示例：

```bash
./build_linux/14_cri_trajectory
```

运行前修改 `examples/14_cri_trajectory.cpp` 中的地址：

```cpp
const std::string robot_ip = "192.168.8.136";
const std::string local_ip = "192.168.8.150";
```

实时控制标准流程：

1. `ConnectRemoteAndSwitchOn`
2. 等首帧 CRI：`data_valid && timestamp_ms > 0`
3. `StartCriControl(1, 4, 5, id)`
4. 等 `realtime_control_mode == true`
5. `TrajectoryGenerator::GenerateMultiSegment(...)` 离线生成轨迹
6. `CriRealtimeDispatcher::SendTrajectory(..., period_ms=4)` 周期下发
7. `StopCriControl`
8. `StopCriDataPush`
9. `Disconnect`

`StartCriControl` 的 `durationMs` 必须和 `SendTrajectory` 的 `period_ms` 一致。例如 `durationMs=4`，下发周期也必须是 `4ms`。

## 6. 单位约定

SDK 对业务层统一使用：

- 位置：`mm`
- 角度：`deg`
- TCP 位姿：`[x, y, z, rx, ry, rz] = mm + deg`

CRI UDP 线上协议使用：

- 位置：`m`
- 角度：`rad`

`CriRealtimeDispatcher` 默认会在发送前把 `mm/deg` 转成 `m/rad`。业务层不要重复转换，否则轨迹尺寸和姿态会出错。

## 7. 协议分层

### TCP JSON 指令通道

用于工程控制、模式切换、IO、寄存器、运动、运动学等普通指令。默认端口是 `9001`。

### TCP publish 推送

用于控制器主动推送主题消息。使用：

```cpp
robot.SubscribePublishTopic("publish/RobotStatus", [](const Codroid::PublishNotification& msg) {
    // handle msg
});
```

### CRI UDP 状态推送

用于控制器高频发送实时状态，SDK 解析后通过：

```cpp
auto state = robot.GetRobotRealtimeState();
```

或回调：

```cpp
robot.SetCriDataReceived([](const Codroid::RobotRealtimeState& state) {
    // handle CRI state
});
```

### CRI UDP 实时控制下发

用于实时控制模式下按周期发送关节或笛卡尔轨迹点。对应类是 `Codroid::CriRealtimeDispatcher`。

## 8. 示例索引

- `examples/01_basic_usage.cpp`：最小连接和基础指令
- `examples/08_move.cpp`：运动控制
- `examples/09_io_test.cpp`：IO 测试
- `examples/14_cri_trajectory.cpp`：CRI 实时控制完整流程

## 9. 常见问题

### 为什么程序打印到 path 后像卡住？

`SendTrajectory` 会按周期逐点发送。几千个点、4ms 周期时，发送阶段会持续十几秒甚至更久。参考 `examples/14_cri_trajectory.cpp` 的 `send begin/send done` 日志确认是否还在发送。

### `Disconnect()` 是 void，如何判断是否返回？

在调用前后打印日志：

```cpp
std::cout << "Disconnect begin\n";
robot.Disconnect();
std::cout << "Disconnect end\n";
```

看到后一行就说明已经返回。

### Windows 能不能用？

Windows 下可以使用 TCP 指令、CRI、IO、寄存器和运动控制。当前仓库里的运动学第三方库是 Linux `.so`，Windows 运动学能力取决于是否提供对应 Windows 动态库。

### 应包含哪个头文件？

业务代码优先包含：

```cpp
#include "codroid/client.hpp"
```

底层兼容接口仍在 `include/Codroid/CodroidController.h`，但新代码建议优先使用 `CodroidClient`。
