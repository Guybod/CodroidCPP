# Codroid C++ SDK

Codroid C++ SDK 提供一套 C++ 接口，用于连接 Codroid 控制器并完成上电、运动、IO、寄存器、实时状态读取和 CRI 实时轨迹下发。

首页按实际接入顺序组织：准备网络，编译 SDK，跑通第一个示例，再接入业务程序。

## 文档

- **SDK 手册（中文）**：[docs/CodroidCPP-SDK-Manual-v2.1.11-zh.md](docs/CodroidCPP-SDK-Manual-v2.1.11-zh.md)
- **SDK Manual (English)**：[docs/CodroidCPP-SDK-Manual-v2.1.11-en.md](docs/CodroidCPP-SDK-Manual-v2.1.11-en.md)

## 1. 准备控制器网络

**固件要求**：本 SDK 全部对外接口要求控制器固件 **≥ 2.3.3.43**。

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
- 安装 "使用 C++ 的桌面开发"
- 目标平台使用 x64

注意事项（编码）：

- Windows 下请使用 UTF-8 编码（源码和项目建议统一为 UTF-8）。
- 在 Visual Studio 中可通过"项目属性 -> C/C++ -> 命令行"添加 `/utf-8`，强制按 UTF-8 编译。
- 使用其他 IDE（如 CLion、VS Code、Qt Creator）但底层仍是 MSVC 时，同样需要传递 `/utf-8`。
- 使用 MinGW（GCC/Clang）时，建议添加 `-finput-charset=UTF-8 -fexec-charset=UTF-8`。

本仓库的 `CMakeLists.txt` 已内置上述编码参数（MSVC/MinGW/GCC/Clang 自动处理）。

**控制台中文乱码**（在 Ubuntu 上写 UTF-8 源码，在 Windows cmd 里运行示例时）：

1. **程序入口**（推荐）：本仓库 **所有** `examples_client/*.cpp` 与 `examples/*.cpp`（除空桩 `05_rs485`）已在 `main` 开头调用 `InitConsoleUtf8()`。业务工程可复制：

```cpp
#include "Codroid/console_utf8.hpp"

int main() {
    Codroid::InitConsoleUtf8();  // Windows 下将控制台设为 UTF-8，Linux 无操作
    // ...
}
```

2. **运行前改代码页**（cmd / 批处理）：

```bat
chcp 65001
client_04_move.exe
```

3. **终端**：优先用 **Windows Terminal** 或 **PowerShell 7+**；或在「区域设置」中开启 **Beta: 使用 Unicode UTF-8 提供全球语言支持」。

说明：乱码是**运行时控制台代码页**与 UTF-8 输出不一致，与源码 `/utf-8` 编译选项是两回事。

编译：

```bat
build_msvc.bat
```

脚本会提示选择 Visual Studio 版本，并构建 Debug / Release。产物在 `build_msvc/`。

### Windows (MinGW)

准备环境：

- CMake 已加入 PATH
- **同一套** MinGW-w64 工具链（`gcc`、`g++`、`mingw32-make` 须在**同一 `bin` 目录**）
- MSYS2 推荐：`C:\msys64\mingw64\bin`（勿把 `ucrt64\bin` 与 `mingw64\bin` 混在 PATH 里）
- 可选：设置环境变量 `MINGW_BIN=C:\msys64\mingw64\bin` 指定工具链根目录
- 目标平台使用 x64

编译：

```bat
build_mingw.bat
```

脚本会提示选择 Debug / Release / Both。产物在 `build_mingw/`。

## 3. 生成客户交付包

构建目录只用于本地开发。交付客户时使用 `package/` 下的整理包。

### Linux x64

```bash
chmod +x package_linux.sh
./package_linux.sh
```

生成目录：

```text
package/CodroidSDK-Linux-x64/
├── include/
├── lib/
├── examples/
├── docs/
└── README_PACKAGE.md
```

### Windows x64

```bat
package_msvc.bat
```

生成目录：

```text
package\CodroidSDK-Windows-x64\
├── include\
├── bin\
├── lib\
├── examples\
├── docs\
└── README_PACKAGE.md
```

### Windows MinGW x64

```bat
package_mingw.bat
```

生成目录：

```text
package\CodroidSDK-Windows-MinGW-x64\
├── include\
├── bin\
├── lib\
├── examples\
├── docs\
└── README_PACKAGE.md
```

客户集成时只需要交付包，不需要整个源码仓库。

交付包中的 `Codroid/client.hpp` 使用 PImpl 封装，不暴露 Asio、nlohmann/json 或内部控制器头文件。业务工程只需要配置 SDK 的 `include/` 和库路径。

## 4. 跑通第一个 TCP 示例

打开 `examples_client/01_connect.cpp`，修改控制器 IP 和本机 IP：

```cpp
const std::string robot_ip = "192.168.8.136";
const std::string local_ip = "192.168.8.150";
```

Linux 运行：

```bash
./build_linux/client_01_connect
```

看到连接成功、控制器收到上电/下电命令，说明 TCP 指令通道可用。后续 IO、寄存器、运动指令都基于同一条 TCP 通道。

## 5. 在业务程序里使用 SDK

业务代码优先包含：

```cpp
#include "Codroid/client.hpp"
```

最小程序：

```cpp
#include "Codroid/client.hpp"

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

## 6. 常用调用

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

### 脚本与工程

```cpp
// 运行 Lua 脚本（含子线程、子程序、中断）
std::unordered_map<std::string, std::string> threads = {
    {"thread1", "while true do print('sub') sleep(1) end"}
};
robot.RunScript("print('main')", threads);

// 运行工程
robot.Run("project_id");
robot.PauseProject();
robot.ResumeProject();
robot.StopProject();
```

### 模式控制

```cpp
robot.EnterRemoteModeViaAuto();  // Auto → Remote
robot.EnterManualModeViaAuto();  // Auto → Manual
robot.ToSimulation();            // 仿真模式
robot.ToActual();                // 实机模式
robot.StartDrag();               // 拖拽示教
robot.StopDrag();
```

### 运动学

```cpp
auto fk_result = robot.ForwardKinematics(Codroid::FKParams({0, 0, 90, 0, 90, 0}));
auto ik_result = robot.InverseKinematics(Codroid::IKParams({400, 200, 500, 180, 0, 90}));
```

### 坐标系转换（v2.1.7+）

```cpp
// 将 TCP 位姿从坐标系1+工具1 转换到坐标系2+工具2
auto result = robot.CposToCpos({400,200,500,180,0,90},
                               {0,0,0,0,0,0}, {0,0,0,0,0,0},  // 源坐标系+工具
                               {100,0,0,0,0,0}, {0,0,100,0,0,0}); // 目标坐标系+工具
// 或返回 CartesianPoint
auto pose = robot.CposToCposPose(CartesianPoint::MmDeg({400,200,500,180,0,90}), ...);
```

### RS485 通信（v2.1.7+）

```cpp
robot.Rs485Init(115200, Codroid::RS485StopBits::One, Codroid::RS485Parity::None);
robot.Rs485Flush();
auto data = robot.Rs485Read(7, 1000);  // 读 7 字节，超时 1 秒
robot.Rs485Write({0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A});
```

### 运动控制

运动目标使用 **`JointPoint`（关节，度）** 与 **`CartesianPoint`（TCP，mm+度）** 区分，**不要**传裸 `std::vector<double>`。类型说明见 `include/Codroid/CodroidDefine.h`。

**笛卡尔工厂**：

| 工厂 | 说明 |
|------|------|
| `CartesianPoint::MmDeg({x,y,z,rx,ry,rz})` | 只设 TCP |
| `CartesianPoint::MmDegWithRef(tcp, ref_joints_deg)` | TCP + 逆解参考关节；`ref_joints_deg` 建议用 `GetRobotRealtimeState().joint_position`，避免 movJ/movL 到 cp 时跳解 |

**运动方式 × 点位**（单点 `MovJ`/`MovL` 与路径 `ClientMoveInstruction::MovJ`/`MovL` 均支持）：

| 组合 | 参数类型 | 协议 |
|------|----------|------|
| 关节运动到关节 | `JointPoint` | movJ + jp |
| 关节运动到 TCP | `CartesianPoint`（建议 `MmDegWithRef`） | movJ + cp |
| 直线到 TCP | `CartesianPoint` | movL + cp |
| 直线到关节 | `JointPoint` | movL + jp |

**类型分层**：

| 类型 | 作用 |
|------|------|
| `JointPoint` / `CartesianPoint` | 业务层：声明点位语义 |
| `MovePoint` | 协议层：JSON 里一个 `targetPoint` / `middlePoint`（一般由工厂从上面两种转换，用户少直接碰） |
| `ClientMoveInstruction` | 路径一段；用 `ClientMoveInstruction::MovJ(jp)` 等工厂构建 |
| `Move` / `MovePath` | 一次下发多段路径（`Robot/move`） |

```cpp
#include "Codroid/client.hpp"

auto home = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
auto pose = Codroid::CartesianPoint::MmDeg({927.5, 214.5, 899.0, 180.0, 0.0, -90.0});

// 从 CRI 取参考关节再发 movJ(cp)
auto st = robot.GetRobotRealtimeState();
auto pose_ik = Codroid::CartesianPoint::MmDegWithRef(
    {927.5, 214.5, 899.0, 180.0, 0.0, -90.0}, st.joint_position);

robot.MovJ(home, 40, 100, robot.NextRequestId());
robot.MovJ(pose_ik, 40, 100, robot.NextRequestId());
robot.MovL(pose, 150, 500, {}, {}, robot.NextRequestId());
robot.MovL(home, 150, 500, {}, {}, robot.NextRequestId());

const std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(home, 40, 100),    // movJ + jp
    Codroid::ClientMoveInstruction::MovJ(pose_ik, 40, 100), // movJ + cp
    Codroid::ClientMoveInstruction::MovL(pose, 150, 500),   // movL + cp
    Codroid::ClientMoveInstruction::MovL(home, 150, 500),   // movL + jp
};
robot.Move(path, robot.NextRequestId());
```

### 阻塞式运动（Sync Motion）

`*Sync` 方法发送运动指令后自动轮询 CRI 数据，直到机器人稳定到达目标。需要 CRI 数据已开始推送。

```cpp
#include "Codroid/client.hpp"

robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip);
robot.WaitForCriData();  // 等待首帧 CRI 到达

// 阻塞式关节运动（默认容差）
robot.MovJSync(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100);

// 阻塞式直线运动
robot.MovLSync(Codroid::CartesianPoint::MmDeg({400, 200, 500, 180, 0, 90}), 150, 500);

// 自定义等待参数
Codroid::MotionWaitOptions opts;
opts.timeout_s = 30.0;
robot.MovJSync(target, 40, 100, opts);

// 阻塞式多段路径
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(jp, 40, 100),
    Codroid::ClientMoveInstruction::MovL(cp, 150, 500),
};
robot.MoveSync(path);
```

`MotionWaitOptions` 参数说明：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `timeout_s` | 60.0 | 整体等待超时（秒） |
| `poll_interval_s` | 0.05 | CRI 轮询间隔（秒） |
| `cri_stale_timeout_s` | 0.5 | CRI 数据过期判定（秒） |
| `settled_samples` | 3 | 连续稳定采样数 |

另有一套 **`moveTo` / `moveToHeartbeat`**（RunTo 规划，非 `Robot/move`），目标同样用 `MoveToTarget::Joint` / `Cartesian`，示例见 `examples/07_move_To.cpp`。

- 客户示例：`examples_client/04_move.cpp`（点到点 + 四组合路径）
- 阻塞运动示例：`examples_client/07_sync_motion.cpp`（Sync 阻塞运动）
- 底层直连：`examples/08_move.cpp`（`CodroidController`）

## 7. 跑 CRI 实时轨迹

CRI 实时轨迹使用两条 UDP 路径：

- 控制器到本机：CRI 状态推送，SDK 解析为 `ClientRealtimeState`
- 本机到控制器：`CriRealtimeDispatcher` 周期下发实时控制点

修改 `examples_client/05_cri_trajectory.cpp` 中的地址：

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

## 8. 单位规则

业务层统一使用：

- 位置：`mm`
- 角度：`deg`
- TCP 位姿：`[x, y, z, rx, ry, rz] = mm + deg`

CRI UDP 线上协议使用：

- 位置：`m`
- 角度：`rad`

`CriRealtimeDispatcher` 默认会把 `mm/deg` 转成 `m/rad` 后发送。业务层不要再次转换。

## 9. 通信通道

### TCP JSON 指令

默认端口 `9001`。用于工程控制、模式切换、IO、寄存器、运动、运动学等普通指令。

### TCP publish 推送

用于控制器主动推送主题消息：

```cpp
robot.SubscribePublishTopic("publish/RobotStatus", [](const Codroid::ClientPublishNotification& msg) {
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
robot.SetCriDataReceived([](const Codroid::ClientRealtimeState& state) {
    // handle state
});
```

### CRI UDP 控制

实时控制下发由 `Codroid::CriRealtimeDispatcher` 完成，输入为 `TrajectoryGenerator` 生成的轨迹点。

## 10. 示例目录

- `examples_client/01_connect.cpp`：最小连接与实时状态读取
- `examples_client/02_io_register.cpp`：IO 与寄存器
- `examples_client/03_cri_state.cpp`：读取 CRI 状态快照
- `examples_client/04_move.cpp`：`JointPoint` / `CartesianPoint`、点到点 API 与 `Move` 多段路径
- `examples_client/05_cri_trajectory.cpp`：CRI 实时控制最小轨迹
- `examples_client/06_robot_parameters.cpp`：机器人设置（工具/负载/坐标系）
- `examples_client/07_sync_motion.cpp`：阻塞式运动 API（`*Sync` + `MotionWaitOptions`）
- `examples_client/08_force_control.cpp`：力控接口测试（零力校准、导纳初始化、在线调参、接触检测、状态读取）
- `examples/`：SDK 内部/兼容示例，包含旧接口用法

## 11. 常见问题

### 程序打印到 path 后长时间没有新输出

`SendTrajectory` 会按周期逐点发送。几千个点、4ms 周期时，发送阶段会持续十几秒。可在业务代码中打印发送前后的耗时日志判断是否仍在发送。

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
