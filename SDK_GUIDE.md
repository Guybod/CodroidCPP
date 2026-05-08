# Codroid C++ SDK 使用文档

本文面向使用 `CodroidCPP` 的开发者，提供一份从构建到联调的快速指南。  
协议细节与跨语言契约请同时参考 `AGENTS.md`、`SDK_API_AND_DESIGN.md`、`PROTOCOL_LINE_BY_LINE.md`。

## 1. SDK 能力概览

- TCP JSON 指令通道（默认端口 `9001`）
- CRI UDP 实时数据接收与解析（对外单位统一为 `mm/deg`）
- CRI UDP 实时轨迹下发（SDK 内部转换为控制器线上 `m/rad`）
- 运动控制（Jog、Move、MoveTo、倍率、负载、碰撞敏感度）
- IO、寄存器、全局变量、主题订阅
- 轨迹离线生成（Joint / Cartesian，Cubic / Trapezoidal）

## 2. 目录结构（关键）

- `include/Codroid/`：底层控制器接口
- `include/codroid/client.hpp`：对齐 C# 命名的主入口 `CodroidClient`
- `src/`：SDK 实现
- `examples/`：官方示例（推荐先跑）
- `kinematics/`：运动学动态库（Linux）
- `build_linux.sh` / `build_msvc.bat`：Linux / Windows 构建脚本

## 3. 构建

### Linux

```bash
chmod +x build_linux.sh
./build_linux.sh
```

产物在 `build_linux/`（包含 `libCodroid.so` 与示例程序）。

### Windows (MSVC)

```bat
build_msvc.bat
```

脚本支持：
- `1`：Visual Studio 2019
- `2`：Visual Studio 2022
- `3`：Visual Studio 2026

产物在 `build_msvc/Debug` 与 `build_msvc/Release`。

## 4. 快速上手（推荐调用顺序）

```cpp
#include "codroid/client.hpp"

int main() {
    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        return 1;
    }

    // ... 业务逻辑 ...

    robot.Disconnect();
    return 0;
}
```

建议流程：
1. `ConnectRemoteAndSwitchOn`
2. 执行运动 / IO / 寄存器 / CRI 业务
3. `Disconnect`（收尾 TCP/UDP 与线程）

## 5. 核心 API（`CodroidClient`）

### 连接与会话
- `Connect(ip, port)`
- `ConnectRemoteAndSwitchOn(ip, port, local_ip)`
- `Disconnect()`
- `NextRequestId()`

### 运动控制
- `Move(...)`、`MoveTo(...)`
- `StartJog(...)` / `StopJog()`
- `PauseRobotMotion()` / `ResumeRobotMotion()` / `StopRobotMove()`

### IO / 寄存器 / 变量
- `GetDi/GetDo/GetAi/GetAo`、`SetDo/SetAo`
- `GetRegisterValue(s)`、`SetRegisterValue`
- `GetGlobalVars`、`SaveGlobalVar(s)`、`RemoveGlobalVars`

### CRI 实时
- `StartCriDataPush(local_ip, local_port)`
- `StartCriControl(filterType, durationMs, startBuffer)`
- `GetRobotRealtimeState()`
- `SetCriDataReceived(callback)`
- `StopCriControl()`、`StopCriDataPush(...)`

## 6. 单位约定（非常重要）

- SDK 对外：默认 `mm/deg`
- CRI UDP 线上：`m/rad`
- `CriRealtimeDispatcher` 下发时默认会把 `mm/deg` 转换到 `m/rad`

如果业务中混用了两套单位，会表现为轨迹比例或姿态角异常。

## 7. CRI 实时控制建议时序

1. `ConnectRemoteAndSwitchOn`
2. `StartCriDataPush`
3. 等首帧 CRI（`timestamp_ms > 0`）
4. `StartCriControl(1, 4, 5)`
5. 等 `realtime_control_mode == true`
6. 用 `TrajectoryGenerator` 生成轨迹
7. 用 `CriRealtimeDispatcher::SendTrajectory(..., period_ms=4)` 下发
8. `StopCriControl`
9. `StopCriDataPush`
10. `Disconnect`

注意：`SendTrajectory` 在发送期间可能长时间无输出，建议在业务层打印分段耗时日志。

## 8. 常见问题

### Q1: 轨迹跑完后程序看起来没退出
- 先确认是否真的发完（打印 `send begin/send done`）
- 再确认是否走到清理阶段（`StopCriControl` / `StopCriDataPush` / `Disconnect`）
- 使用最新二进制，不要混用旧 build 目录程序

### Q2: `Disconnect()` 是 `void`，怎么判断返回？
- 在调用前后打印日志
- 或记录耗时，确认函数是否返回

### Q3: 为什么 CRI 数据和控制器显示单位不一致？
- 核对是否把 CRI 原始 `m/rad` 当成 `mm/deg` 直接使用
- 按本 SDK 的默认约定，应用层统一使用 `mm/deg`

## 9. 推荐先跑的示例

- `examples/01_basic_usage.cpp`：基础连接与调用
- `examples/08_move.cpp`：运动指令
- `examples/09_io_test.cpp`：IO
- `examples/14_cri_trajectory.cpp`：CRI 全流程与轨迹下发

## 10. 版本与文档同步建议

当你修改以下任一项时，建议同步更新本文：
- 构建脚本参数与产物目录
- `CodroidClient` 对外函数命名
- CRI 默认参数（如 `durationMs`、推送周期）
- 示例文件名或推荐流程
