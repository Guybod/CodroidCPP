# Codroid C++ SDK

Codroid C++ SDK 提供一套 C++ 接口，用于连接 Codroid 控制器并完成上电、运动、IO、寄存器、实时状态读取和 CRI 实时轨迹下发。

首页按实际接入顺序组织：准备网络，编译 SDK，跑通第一个示例，再接入业务程序。

## 资料入口

- API 在线手册：[金山文档](https://www.kdocs.cn/l/cqlm2DOsjGRp)
- SDK 手册：`SDK_GUIDE.md`
- 入口头文件：`include/codroid/client.hpp`
- CRI 实时控制参考：`examples/14_cri_trajectory.cpp`

## 1. 准备控制器网络

先确认三件事：

- 控制器 IP，例如 `192.168.8.136`
- 本机网卡 IP，例如 `192.168.8.150`
- TCP 指令端口，默认 `9001`

示例代码里的 IP 需要按现场网络修改。先从控制器所在网段确认本机能访问控制器，再运行 SDK 示例。

CRI 实时控制还会用到控制器 UDP `9030`。本机 CRI 状态接收端口由 SDK 启动推送时绑定，示例中会自动处理。

## 2. 编译 SDK

### Linux

安装依赖：

```bash
sudo apt update
sudo apt install -y build-essential cmake git libasio-dev nlohmann-json3-dev
```

编译：

```bash
chmod +x build_linux.sh
./build_linux.sh
```

完成后检查 `build_linux/`：

- `libCodroid.so`：SDK 动态库
- `01_basic_usage`、`08_move`、`09_io_test`、`14_cri_trajectory` 等示例程序

### Windows (MSVC)

准备环境：

- CMake 已加入 PATH
- Visual Studio 2019 / 2022 / 2026
- 安装 “使用 C++ 的桌面开发”
- 目标平台使用 x64

编译：

```bat
build_msvc.bat
```

脚本会提示选择 Visual Studio 版本，并构建 Debug / Release。产物在 `build_msvc/`。

## 3. 跑通第一个 TCP 示例

打开 `examples/01_basic_usage.cpp`，修改控制器 IP：

```cpp
std::string robot_ip = "192.168.8.136";
```

Linux 运行：

```bash
./build_linux/01_basic_usage
```

看到连接成功、控制器收到上电/下电命令，说明 TCP 指令通道可用。后续 IO、寄存器、运动指令都基于同一条 TCP 通道。

## 4. 在业务程序里使用 SDK

业务代码优先包含：

```cpp
#include "codroid/client.hpp"
```

最小程序：

```cpp
#include "codroid/client.hpp"

#include <iostream>
#include <string>

int main() {
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    const auto state = robot.GetRobotRealtimeState();
    if (state.data_valid) {
        std::cout << "CRI timestamp: " << state.timestamp_ms << "\n";
    }

    robot.Disconnect();
    return 0;
}
```

常用连接顺序：

1. `ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)`
2. 执行 IO、寄存器、运动或 CRI 业务
3. `Disconnect()`

`ConnectRemoteAndSwitchOn` 会完成常用准备动作：TCP 连接、切自动/远程、启动 CRI 数据推送、上电。

## 5. 常用调用

### IO

```cpp
const int id1 = robot.NextRequestId();
int di0 = robot.GetDi(0, id1);

const int id2 = robot.NextRequestId();
robot.SetDo(0, 1, id2);
```

### 寄存器

```cpp
const int id1 = robot.NextRequestId();
double value = robot.GetRegisterValue(49100, id1);

const int id2 = robot.NextRequestId();
robot.SetRegisterValue(49100, 520.0, id2);
```

### 运动控制

```cpp
robot.SetAutoMoveRate(40, robot.NextRequestId());
robot.StopRobotMove(robot.NextRequestId());
```

`Move`、`MoveTo`、Jog 等完整运动示例见 `examples/08_move.cpp`。

## 6. 跑 CRI 实时轨迹

CRI 实时轨迹使用两条 UDP 路径：

- 控制器到本机：CRI 状态推送，SDK 解析为 `RobotRealtimeState`
- 本机到控制器：`CriRealtimeDispatcher` 周期下发实时控制点

修改 `examples/14_cri_trajectory.cpp` 中的地址：

```cpp
const std::string robot_ip = "192.168.8.136";
const std::string local_ip = "192.168.8.150";
```

运行：

```bash
./build_linux/14_cri_trajectory
```

示例会依次执行：

1. 离线轨迹算法自检
2. TCP 连接、切远程、启动 CRI 推送、上电
3. 等首帧 CRI
4. `StartCriControl(1, 4, 5)`
5. 等 `realtime_control_mode`
6. 生成 joint / cart / path 三段轨迹
7. UDP `9030` 周期下发轨迹点
8. `StopCriControl`
9. `StopCriDataPush`
10. `Disconnect`

关键约束：`StartCriControl` 的 `durationMs` 必须等于 `SendTrajectory` 的 `period_ms`。示例使用 `4ms`，即 250Hz。

## 7. 单位规则

业务层统一使用：

- 位置：`mm`
- 角度：`deg`
- TCP 位姿：`[x, y, z, rx, ry, rz] = mm + deg`

CRI UDP 线上协议使用：

- 位置：`m`
- 角度：`rad`

`CriRealtimeDispatcher` 默认会把 `mm/deg` 转成 `m/rad` 后发送。业务层不要再次转换。

## 8. 通信通道

### TCP JSON 指令

默认端口 `9001`。用于工程控制、模式切换、IO、寄存器、运动、运动学等普通指令。

### TCP publish 推送

用于控制器主动推送主题消息：

```cpp
robot.SubscribePublishTopic("publish/RobotStatus", [](const Codroid::PublishNotification& msg) {
    // handle msg
});
```

### CRI UDP 状态

读取最新快照：

```cpp
auto state = robot.GetRobotRealtimeState();
```

注册回调：

```cpp
robot.SetCriDataReceived([](const Codroid::RobotRealtimeState& state) {
    // handle state
});
```

### CRI UDP 控制

实时控制下发由 `Codroid::CriRealtimeDispatcher` 完成，输入为 `TrajectoryGenerator` 生成的轨迹点。

## 9. 示例目录

- `examples/01_basic_usage.cpp`：最小连接与基础指令
- `examples/08_move.cpp`：运动控制
- `examples/09_io_test.cpp`：IO 读写
- `examples/14_cri_trajectory.cpp`：CRI 实时控制完整流程

## 10. 常见问题

### 程序打印到 path 后长时间没有新输出

`SendTrajectory` 会按周期逐点发送。几千个点、4ms 周期时，发送阶段会持续十几秒。`examples/14_cri_trajectory.cpp` 已打印 `send begin/send done`，可用来判断是否仍在发送。

### 判断 `Disconnect()` 是否返回

在调用前后打印日志：

```cpp
std::cout << "Disconnect begin\n";
robot.Disconnect();
std::cout << "Disconnect end\n";
```

出现 `Disconnect end` 即表示已返回。

### Windows 运动学说明

Windows 下可使用 TCP 指令、CRI、IO、寄存器和运动控制。当前仓库内运动学第三方库是 Linux `.so`，Windows 无法使用。
