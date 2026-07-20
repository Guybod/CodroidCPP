# CodroidCPP SDK 手册

**版本:** 3.0.0 | **命名空间:** `Codroid`

> 客户侧只需 `#include "Codroid/client.hpp"`。Asio / `CodroidController` 为内部实现，不进发布包；`nlohmann/json` 随包提供并由 `client.hpp` 引入。

---

## 目录

| # | 章节 | 说明 |
|---|------|------|
| 1 | [快速上手](#快速上手) | 构建、连接并运行第一个程序 |
| 2 | [核心概念](#核心概念) | 公开面、生命周期、TCP 模型、单位约定 |
| 3 | [CodroidClient API](#codroidclient-api-参考) | CodroidClient 完整 API 参考 |
| 4 | [运动 API](#运动-api-参考) | JointPoint、CartesianPoint、Move、Sync |
| 5 | [数据类型与枚举](#数据类型与枚举) | CommandResult、ClientRealtimeState、异常类 |
| 6 | [CRI 实时数据与控制](#cri-实时控制-api) | CriRealtimeDispatcher、TrajectoryGenerator |
| 7 | [IO 与寄存器](#io-与寄存器-api-参考) | DI/DO/AI/AO、寄存器 |
| 8 | [辅助工具](#辅助工具-api-参考) | 主题订阅、全局变量、TCP 运动学、UTF-8 |
| 9 | [力控](#c-力控接口说明) | 力控初始化、调参与示例 |

---

## 环境要求

| 平台 | 编译器 | 构建工具 |
|------|--------|----------|
| Linux | GCC 9+ / Clang 10+ | CMake 3.14+ |
| Windows | MSVC 2019/2022/2026 | CMake 3.14+ |
| Windows | MinGW-w64 (MSYS2) | CMake 3.14+ |

### 客户侧依赖

- **唯一公开头**：`Codroid/client.hpp`
- **nlohmann/json**：随 SDK 发布包提供（`include/nlohmann/`）
- **不需要**：Asio、KDL、`CodroidController`

### SDK 源码构建额外依赖（仅编译本仓库时）

构建 `libCodroid` 时实现单元使用 Asio + nlohmann（`build_linux.sh` 会同步到 `third_party/`）。

### 构建

#### Linux

```bash
chmod +x build_linux.sh
./build_linux.sh
```

产物在 `build_linux/`（`libCodroid.so` 与 `01_connect` 等示例）。

#### Windows (MSVC)

```bat
build_msvc.bat
```

#### Windows (MinGW)

```bat
build_mingw.bat
```

---

# 快速上手

**版本 3.0.0** · 业务代码只需：

```cpp
#include "Codroid/client.hpp"
```

即可使用 `CodroidClient`、公开 DTO、轨迹生成、CRI UDP 下发，以及随包提供的 `nlohmann::json`。无需再单独 include Asio 或其它内部头。

---

## 构建 SDK

### Linux

```bash
cd CodroidCPP
chmod +x build_linux.sh
./build_linux.sh
```

产物在 `build_linux/`：

- `libCodroid.so` — 动态库
- `01_connect`、`08_move`、`14_cri_trajectory` 等示例可执行文件

### Windows (MSVC)

```bat
cd CodroidCPP
build_msvc.bat
```

选择 Visual Studio 版本后，产物在 `build_msvc/Debug` 与 `build_msvc/Release`。

### Windows (MinGW)

```bat
cd CodroidCPP
build_mingw.bat
```

产物在 `build_mingw/`。

---

## 客户依赖说明

| 项 | 客户是否需要 |
|----|----------------|
| `#include "Codroid/client.hpp"` | **是**（唯一入口） |
| `nlohmann/json` | 发布包已附带 `include/nlohmann/`，由 `client.hpp` 间接引入，**不必**再写 `#include <nlohmann/json.hpp>` |
| Asio | **否**（仅 SDK 实现编译使用） |
| KDL / 本地正逆解库 | **否**（已从公开面移除；正逆解用 TCP API） |
| `CodroidController` | **否**（内部实现，不进发布包） |

发布包链接示例（Linux）：

```cmake
set(CODROID_SDK_ROOT "/path/to/CodroidSDK-Linux-x64")
target_include_directories(your_app PRIVATE ${CODROID_SDK_ROOT}/include)
target_link_directories(your_app PRIVATE ${CODROID_SDK_ROOT}/lib)
target_link_libraries(your_app PRIVATE Codroid pthread)
```

---

## 最小示例

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::InitConsoleUtf8();  // Windows 控制台 UTF-8；Linux 为空操作

    Codroid::CodroidClient robot;

    // 连接、切远程、上电；local_ip 用于 CRI UDP 绑定
    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败\n";
        return 1;
    }

    int di0 = robot.GetDi(0);
    std::cout << "DI 0 = " << di0 << "\n";
    robot.SetDo(10, di0);

    robot.Disconnect();
    return 0;
}
```

---

## 完整工作流示例

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::InitConsoleUtf8();
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败\n";
        return 1;
    }

    int di0 = robot.GetDi(0);
    robot.SetDo(10, di0);

    double regValue = robot.GetRegisterValue(49100);
    robot.SetRegisterValue(49100, regValue + 1);

    auto joints = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
    robot.MovJ(joints, 40, 100);

    auto target = Codroid::CartesianPoint::MmDegWithRef(
        {400, 0, 300, 180, 0, 0},
        robot.GetRobotRealtimeState().joint_position);
    robot.MovLSync(target, 150, 500);

    robot.Disconnect();
    return 0;
}
```

---

## 运行仓库示例

示例均在 `examples/`，目标名与文件名一致：

```bash
cd build_linux
./01_connect
./09_io_register
./08_move
./14_cri_trajectory
./15_force_control state
```

| 文件 | 内容 |
|------|------|
| `01_connect.cpp` | 连接 / 远程上电 / CRI |
| `02_run_script.cpp` | 远程脚本 |
| `03_run_project.cpp` | 工程运行 |
| `04_global_value.cpp` | 全局变量 |
| `05_rs485.cpp` | RS485 |
| `06_jog_mode.cpp` | 点动 |
| `07_move_to.cpp` | MoveTo |
| `08_move.cpp` | 点到点与多段 Move |
| `09_io_register.cpp` | IO / 寄存器 |
| `10_sync_motion.cpp` | 阻塞 Sync 运动 |
| `11_robot_parameters.cpp` | 工具/负载/坐标系 |
| `12_kinematics_tcp.cpp` | TCP 正逆解 |
| `13_cri_state.cpp` | CRI 快照 |
| `14_cri_trajectory.cpp` | CRI 实时轨迹 |
| `15_force_control.cpp` | 力控 |

现场请修改示例中的 `robot_ip` / `local_ip`。

---

## 错误处理

### 默认（不抛异常）

```cpp
auto result = robot.SetDo(999, 1);
if (!result.Ok()) {
    std::cerr << "控制器错误: " << result.error_msg << "\n";
}
```

### 抛异常模式

```cpp
robot.SetThrowOnCommandError(true);
try {
    robot.SetDo(999, 1);
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << ex.controller_error() << " id=" << ex.request_id() << "\n";
}
```

| 异常 | 条件 |
|------|------|
| `CodroidCommandException` | 控制器 `err` 非空 |
| `CodroidException` | 通用运行时错误 |

---

## 固件要求

本 SDK 对外接口要求控制器固件 **≥ 2.3.3.43**（见 `Codroid::MinControllerFirmware`，定义于公开头 `types.hpp`）。

---

## 下一步

- [核心概念](02-concepts.md)
- [CodroidClient API](03-api-reference-codroidclient.md)
- [运动控制](04-api-reference-motion.md)

---

# 核心概念

## 公开面与唯一入口

业务工程**只**需要：

```cpp
#include "Codroid/client.hpp"
```

该头聚合：`CodroidClient`、公开 DTO（`types.hpp`）、`TrajectoryGenerator`、`CriRealtimeDispatcher`、`InitConsoleUtf8`，以及随包提供的 `nlohmann::json`。

| 对客户可见 | 不对客户暴露 |
|------------|----------------|
| `Codroid/client.hpp` 及由其引入的公开类型 | `CodroidController`、内部 `CodroidDefine`、Asio |
| 发布包内 `include/nlohmann/` | KDL / 本地 `kinematicsInit` 等 |

---

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
if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
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
robot.ConnectRemoteAndSwitchOn("192.168.1.136");
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
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");

// 2. 启动 CRI 数据推送
robot.StartCriDataPush("192.168.1.150", 9030);

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

---

# CodroidClient API 参考

**类:** `CodroidClient`
**命名空间:** `Codroid`
**头文件:** `#include "Codroid/client.hpp"`（客户侧唯一入口；版本 3.0.0）

`CodroidClient` 是 SDK 的主入口类，提供连接、IO、寄存器、运动、CRI、主题订阅等功能。

```cpp
#include "Codroid/client.hpp"

Codroid::CodroidClient robot;
```

---

## 通用类型

### CommandResult

大多数 TCP 指令方法的返回类型。

```cpp
struct CommandResult {
    int id;               // 请求 id（与下发一致）
    std::string ty;       // 响应类型 / 路由
    std::string error_msg; // 控制器 err 或本地错误说明；空表示成功
    std::string raw_json;  // 最近一次完整响应 JSON

    bool Ok() const noexcept; // error_msg 为空返回 true
};
```

| 属性/方法 | 类型 | 说明 |
|-----------|------|------|
| `id` | `int` | 请求 ID |
| `ty` | `std::string` | 响应类型标识 |
| `error_msg` | `std::string` | 非空表示失败 |
| `raw_json` | `std::string` | 完整响应 JSON |
| `Ok()` | `bool` | 是否成功（`error_msg` 为空） |

### MotionWaitOptions

阻塞运动等待参数，用于 `*Sync` 方法。

```cpp
struct MotionWaitOptions {
    double timeout_s = 60.0;                        // 整体等待超时（秒）
    double poll_interval_s = 0.05;                  // CRI 轮询间隔（秒）
    double cri_stale_timeout_s = 0.5;               // CRI 数据过期判定（秒）
    int settled_samples = 3;                        // InMotion=false 连续稳定采样数
};
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `timeout_s` | `double` | 60.0 | 整体等待超时（秒） |
| `poll_interval_s` | `double` | 0.05 | CRI 轮询间隔（秒） |
| `cri_stale_timeout_s` | `double` | 0.5 | CRI 数据过期判定（秒） |
| `settled_samples` | `int` | 3 | InMotion=false 连续稳定采样数 |

### 异常类型

| 异常 | 触发条件 | 来源 |
|------|----------|------|
| `CodroidCommandException` | 控制器返回 `err` 字段 | TCP 响应 |
| `CodroidException` | 通用运行时错误 | SDK 内部 |
| `std::runtime_error` | Sync 方法在异常状态或超时时 | 阻塞运动 |

#### CodroidCommandException

```cpp
class CodroidCommandException : public CodroidException {
public:
    int request_id() const noexcept;
    const std::string& command_ty() const noexcept;
    const std::string& controller_error() const noexcept;
    const std::string& raw_response_json() const noexcept;
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `request_id()` | `int` | 协议请求 ID |
| `command_ty()` | `std::string` | 命令类型，如 `"Robot/move"` |
| `controller_error()` | `std::string` | 控制器返回的错误描述 |
| `raw_response_json()` | `std::string` | 完整响应 JSON |

---

## 构造与析构

### CodroidClient()

默认构造函数，不建立连接。

```cpp
Codroid::CodroidClient robot;
```

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参构造 |

**返回值：** 无

**异常：** 无

---

### ~CodroidClient()

析构函数。如果未调用 `Disconnect()`，析构时会自动清理资源。

**返回值：** 无

**异常：** 无

---

## 连接管理

### Connect

```cpp
bool Connect(const std::string& ip, int port = 9001);
```

建立 TCP 连接，不切模式、不自动上电。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ip` | `const std::string&` | -- | 控制器 IP 地址 |
| `port` | `int` | 9001 | TCP 端口 |

**返回值：** `bool` -- 连接成功返回 `true`

**异常：** 无（失败时返回 `false`）

```cpp
if (!robot.Connect("192.168.1.136")) {
    std::cerr << "连接失败" << std::endl;
}
```

---

### ConnectRemoteAndSwitchOn

```cpp
bool ConnectRemoteAndSwitchOn(const std::string& ip, int port = 9001, std::string local_ip = {});
```

连接并执行远程上电/模式切换。推荐的一键初始化方法。`local_ip` 非空时用于 `StartCriDataPush` 绑定本机 UDP。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ip` | `const std::string&` | -- | 控制器 IP 地址 |
| `port` | `int` | 9001 | TCP 端口 |
| `local_ip` | `std::string` | `{}` (空) | 本机 IP，用于 CRI UDP 绑定；非空时自动启动 CRI 数据推送 |

**返回值：** `bool` -- 连接、切换远程、上电均成功返回 `true`

**异常：** 无（失败时返回 `false`）

```cpp
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");
```

---

### Disconnect

```cpp
void Disconnect();
```

断开 TCP 连接，停止 CRI 相关线程与缓存。始终在程序结束前调用。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** 无

**异常：** 无

```cpp
try {
    robot.ConnectRemoteAndSwitchOn("192.168.1.136");
    // ... 操作 ...
} catch (...) {
    robot.Disconnect();
    throw;
}
robot.Disconnect();
```

---

### NextRequestId

```cpp
int NextRequestId();
```

生成下一个单调递增的请求 ID。多线程发令时避免重复。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `int` -- 下一个可用的请求 ID

**异常：** 无

---

## 模式控制

### SwitchOn

```cpp
CommandResult SwitchOn(int id = 1);
```

上电使能。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 通过 `Ok()` 判断是否成功，`error_msg` 非空表示失败

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SwitchOff

```cpp
CommandResult SwitchOff(int id = 1);
```

下电。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToManual

```cpp
CommandResult ToManual(int id = 1);
```

切换到手动模式。需要固件 2.3.3.43+。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToAuto

```cpp
CommandResult ToAuto(int id = 1);
```

切换到自动模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToRemote

```cpp
CommandResult ToRemote(int id = 1);
```

切换到远程模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ClearSystemError

```cpp
CommandResult ClearSystemError(int id = 1);
```

清除系统错误状态。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### EnterManualModeViaAuto

```cpp
CommandResult EnterManualModeViaAuto(int id = 1);
```

先切自动再切手动（Auto -> Manual）。满足控制器"必须经过自动模式"的限制。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### EnterRemoteModeViaAuto

```cpp
CommandResult EnterRemoteModeViaAuto(int id = 1);
```

先切自动再切远程（Auto -> Remote）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToSimulation

```cpp
CommandResult ToSimulation(int id = 1);
```

进入仿真模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ToActual

```cpp
CommandResult ToActual(int id = 1);
```

进入实机模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StartDrag

```cpp
CommandResult StartDrag(int id = 1);
```

进入拖拽示教模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopDrag

```cpp
CommandResult StopDrag(int id = 1);
```

退出拖拽示教模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## IO 操作

### GetDi

```cpp
int GetDi(int port, int id = 1);
```

读取数字量输入。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `int` -- DI 值（0 或 1）

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
int di0 = robot.GetDi(0);
std::cout << "DI 0 = " << di0 << std::endl;
```

---

### GetDo

```cpp
int GetDo(int port, int id = 1);
```

读取数字量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `int` -- DO 值（0 或 1）

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### GetAi

```cpp
double GetAi(int port, int id = 1);
```

读取模拟量输入。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `double` -- AI 值

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### GetAo

```cpp
double GetAo(int port, int id = 1);
```

读取模拟量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `double` -- AO 值

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetDo

```cpp
CommandResult SetDo(int port, int value, int id = 1);
```

设置数字量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `value` | `int` | -- | 写入值（0 或 1） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetDo(10, 1);  // 设置 DO 10 为高
robot.SetDo(10, 0);  // 设置 DO 10 为低
```

---

### SetAo

```cpp
CommandResult SetAo(int port, double value, int id = 1);
```

设置模拟量输出。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `port` | `int` | -- | IO 端口号 |
| `value` | `double` | -- | 写入值 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetAo(2, 3.14);
```

---

## 寄存器操作

### GetRegisterValue

```cpp
double GetRegisterValue(int address, int id = 1);
```

读取单个寄存器值。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `address` | `int` | -- | 寄存器地址 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `double` -- 寄存器值

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
double val = robot.GetRegisterValue(49100);
std::cout << "Register 49100 = " << val << std::endl;
```

---

### GetRegisterValues

```cpp
std::vector<ClientRegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1);
```

批量读取寄存器。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `addresses` | `const std::vector<int>&` | -- | 寄存器地址列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<ClientRegisterInfo>` -- 寄存器值列表，每个元素包含 `address` 和 `value`

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
auto regs = robot.GetRegisterValues({49100, 49101, 49102});
for (const auto& r : regs) {
    std::cout << "Register " << r.address << " = " << r.value << std::endl;
}
```

**ClientRegisterInfo 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `address` | `int` | 寄存器地址 |
| `value` | `double` | 寄存器值 |

---

### SetRegisterValue

```cpp
CommandResult SetRegisterValue(int address, double value, int id = 1);
```

设置寄存器值。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `address` | `int` | -- | 寄存器地址 |
| `value` | `double` | -- | 写入值 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetRegisterValue(49100, 42);
robot.SetRegisterValue(49300, 3.14);
```

---

## 运动控制

### MovJ -- 关节运动

```cpp
CommandResult MovJ(const ClientJointPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovJ(const ClientCartesianPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovJ(const ClientMovePoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

关节运动。发送指令后立即返回。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientJointPoint` / `ClientCartesianPoint` / `ClientMovePoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（关节 deg/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥 -- 同时传入时 `relativeBlend` 无效。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥 -- 同时传入时此参数无效。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 [x,y,z,a,b,c]。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
// 关节目标
robot.MovJ(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100);

// 笛卡尔目标（关节运动到 TCP 位姿，控制器做逆解）
robot.MovJ(Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}), 40, 100);
```

---

### MovL -- 直线运动

```cpp
CommandResult MovL(const ClientCartesianPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovL(const ClientJointPoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);

CommandResult MovL(const ClientMovePoint& target, double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

直线运动。发送指令后立即返回。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientCartesianPoint` / `ClientJointPoint` / `ClientMovePoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
auto state = robot.GetRobotRealtimeState();
robot.MovL(
    Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
    150, 500);
```

---

### MovC -- 圆弧运动

```cpp
CommandResult MovC(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   double speed, double acceleration,
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

圆弧运动。经过中间点到达目标点。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点（圆弧上） |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.MovC(
    Codroid::CartesianPoint::MmDeg({450, 100, 300, 180, 0, 0}),
    Codroid::CartesianPoint::MmDeg({500, 0, 300, 180, 0, 0}),
    100, 300);
```

---

### MovCircle -- 整圆运动

```cpp
CommandResult MovCircle(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                        int circle_num, double speed, double acceleration,
                        double blend = -1, double relativeBlend = -1,
                        const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
```

整圆/多圈运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点 |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `circle_num` | `int` | -- | 整圆圈数 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acceleration` | `double` | -- | 加速度 |
| `blend` | `double` | -1 | 平滑半径（mm）。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系。空时指令中不包含该字段 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系。空时指令中不包含该字段 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.MovCircle(
    Codroid::CartesianPoint::MmDeg(mid),
    Codroid::CartesianPoint::MmDeg(end),
    1, 80, 200);
```

---

### Move / MovePath -- 多段路径

```cpp
CommandResult Move(const std::vector<ClientMoveInstruction>& path, int id = 1);
CommandResult MovePath(const std::vector<ClientMoveInstruction>& path, int id = 1);
```

将一组运动指令作为单条路径命令发送。`MovePath` 是 `Move` 的别名。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | `const std::vector<ClientMoveInstruction>&` | -- | 运动指令列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

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

### PauseRobotMotion

```cpp
CommandResult PauseRobotMotion(int id = 1);
```

暂停当前运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ResumeRobotMotion

```cpp
CommandResult ResumeRobotMotion(int id = 1);
```

恢复暂停的运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopRobotMove

```cpp
CommandResult StopRobotMove(int id = 1);
```

立即停止当前运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## 阻塞运动（Sync）

`*Sync` 方法发送运动指令后**阻塞直到 CRI 确认机器人到达目标**。成功返回 `true`，错误/超时抛出 `std::runtime_error`。

**必须先调用** `StartCriDataPush` 并 `WaitForCriData` 等待首帧。

### MoveSync

```cpp
bool MoveSync(const std::vector<ClientMoveInstruction>& path, const MotionWaitOptions& wait = {});
```

发送多段路径并阻塞直到最后一段目标到达。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | `const std::vector<ClientMoveInstruction>&` | -- | 运动指令列表 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项（超时、容差等） |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或机器人处于异常状态（碰撞、急停、报警）

```cpp
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(Codroid::JointPoint::Degrees({0,0,90,0,90,0}), 40, 100),
    Codroid::ClientMoveInstruction::MovL(Codroid::CartesianPoint::MmDeg({400,0,300,180,0,0}), 150, 500),
};
robot.MoveSync(path);
```

---

### MovJSync

```cpp
bool MovJSync(const ClientJointPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});

bool MovJSync(const ClientCartesianPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式关节运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientJointPoint` / `ClientCartesianPoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（deg/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

```cpp
Codroid::MotionWaitOptions wait;
wait.timeout_s = 90.0;
robot.MovJSync(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100, wait);
```

---

### MovLSync

```cpp
bool MovLSync(const ClientCartesianPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});

bool MovLSync(const ClientJointPoint& target, double speed, double acc,
              const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式直线运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | `ClientCartesianPoint` / `ClientJointPoint` | -- | 目标位置 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

```cpp
auto state = robot.GetRobotRealtimeState();
Codroid::MotionWaitOptions wait;
wait.timeout_s = 60.0;
robot.MovLSync(
    Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
    150, 500, wait);
```

---

### MovCSync

```cpp
bool MovCSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
              double speed, double acc, const MotionWaitOptions& wait = {},
              double blend = -1, double relativeBlend = -1,
              const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式圆弧运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点（圆弧上） |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

```cpp
robot.MovCSync(
    Codroid::CartesianPoint::MmDeg(mid),
    Codroid::CartesianPoint::MmDeg(end),
    100, 300);
```

---

### MovCircleSync

```cpp
bool MovCircleSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   int circle_num, double speed, double acc, const MotionWaitOptions& wait = {},
                   double blend = -1, double relativeBlend = -1,
                   const std::vector<double>& coor = {}, const std::vector<double>& tool = {});
```

阻塞式整圆运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `middle` | `const ClientCartesianPoint&` | -- | 中间点 |
| `target` | `const ClientCartesianPoint&` | -- | 终点 |
| `circle_num` | `int` | -- | 整圆圈数 |
| `speed` | `double` | -- | 速度（mm/s） |
| `acc` | `double` | -- | 加速度 |
| `wait` | `const MotionWaitOptions&` | `{}` | 等待选项 |
| `blend` | `double` | -1 | 平滑半径。与 `relativeBlend` 互斥。-1 表示不下发 |
| `relativeBlend` | `double` | -1 | 相对平滑比（0--100）。与 `blend` 互斥。-1 表示不下发 |
| `coor` | `const std::vector<double>&` | `{}` | 用户坐标系 |
| `tool` | `const std::vector<double>&` | `{}` | 工具坐标系 |

**返回值：** `bool` -- 成功到达目标返回 `true`

**异常：** `std::runtime_error` -- 运动超时或异常状态

---

## MoveTo（规划运动）

### MoveTo

```cpp
CommandResult MoveTo(const MoveToParams& params, int id = 1);
```

MoveTo 预设/规划运动。运行期间需要心跳。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const MoveToParams&` | -- | MoveTo 参数（类型 + 可选目标） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**MoveToParams 结构：**

```cpp
struct MoveToParams {
    MoveToType type = MoveToType::Home;  // 运动类型
    MoveToTarget target;                 // 目标点（仅 Joint/Line 需要）
};
```

**MoveToType 枚举：**

| 名称 | 值 | 说明 |
|------|-----|------|
| `Stop` | -1 | 停止 MoveTo |
| `Home` | 0 | 回 Home |
| `Safe` | 1 | 回安全位 |
| `Candle` | 2 | 回 Candle 位 |
| `Packing` | 3 | 回 Packing 位 |
| `Joint` | 4 | 关节规划到目标 |
| `Line` | 5 | 直线规划到目标 |
| `ResumePoint` | 6 | 恢复程序执行 |

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

MoveTo 心跳。使用 `MoveToType::Joint` 或 `Line` 时，每 >=500ms 调用一次，否则运动会停止。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopMoveTo

```cpp
CommandResult StopMoveTo(int id = 1);
```

停止 MoveTo 运动。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## Jog（点动）

### Jog

```cpp
CommandResult Jog(const JogParams& params, int id = 1);
```

点动。需要约每 500ms 发送心跳维持。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const JogParams&` | -- | 点动参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**JogParams 结构：**

```cpp
struct JogParams {
    JogMode mode = JogMode::Line;       // Joint=1, Line=2
    double speed = 0.0;                 // -1~1 的比例
    int index = 1;                      // 轴号（Joint 模式）或方向（Line 模式）
    CoorType coorType = CoorType::User; // 坐标系类型
    int coorId = 1;                     // 坐标系 ID
};
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `mode` | `JogMode` | `Line` | 点动模式：`Joint`(1) 或 `Line`(2) |
| `speed` | `double` | 0.0 | 速度比例（-1~1） |
| `index` | `int` | 1 | 轴号（Joint 模式 1--6）或方向（Line 模式） |
| `coorType` | `CoorType` | `User` | 坐标系类型：`User` 或 `Tool`；Jog 协议下发数字值，`User=0`、`Tool=1` |
| `coorId` | `int` | 1 | 坐标系 ID |

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

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### JogHeartbeat

```cpp
CommandResult JogHeartbeat(int id = 1);
```

点动心跳。点动期间每 >=500ms 调用一次。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## CRI 实时数据

### StartCriDataPush

```cpp
CommandResult StartCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

请求 CRI 状态 UDP 推送。固定参数：100ms 周期、高精度、mask 0xFFFF、308 字节 UDP 包。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `udpIp` | `const std::string&` | -- | 本机 IP 地址，用于接收 UDP 数据 |
| `udpPort` | `int` | -- | 本机 UDP 端口 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.StartCriDataPush("192.168.1.150", 18888);
robot.WaitForCriData(5.0); // 等待首帧
```

---

### StopCriDataPush

```cpp
CommandResult StopCriDataPush(int id = 1);
CommandResult StopCriDataPush(const std::string& udpIp, int udpPort, int id = 1);
```

停止 CRI 数据推送。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `udpIp` | `const std::string&` | -- | 本机 IP（带参版本） |
| `udpPort` | `int` | -- | 本机 UDP 端口（带参版本） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.StopCriDataPush("192.168.1.150", 18888);
```

---

### StartCriControl

```cpp
CommandResult StartCriControl(int filterType, int durationMs, int startBuffer, int id = 1);
```

启动实时控制会话。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `filterType` | `int` | -- | 滤波类型（0=关闭，1=均值，2=二阶低通，3=椭圆） |
| `durationMs` | `int` | -- | 控制周期（ms），须与 `CriRealtimeDispatcher::SendTrajectory` 的 period 一致 |
| `startBuffer` | `int` | -- | 起始缓冲帧数（1~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.StartCriControl(1, 4, 5);  // 均值滤波, 4ms 周期, 5 帧缓冲
```

---

### StopCriControl

```cpp
CommandResult StopCriControl(int id = 1);
```

停止实时控制会话。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### GetCriUdpListenPort

```cpp
int GetCriUdpListenPort() const;
```

获取 CRI UDP 监听端口。未启动推送时可能为 0。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `int` -- 当前 CRI UDP 监听端口

**异常：** 无

---

### GetRobotRealtimeState

```cpp
ClientRealtimeState GetRobotRealtimeState() const;
```

获取线程安全的 CRI 数据快照。须先 `StartCriDataPush`，首帧到达前 `data_valid` 可能为 false。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `ClientRealtimeState` -- 实时状态快照

**异常：** 无

**ClientRealtimeState 关键字段：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `timestamp_ms` | `int64_t` | 控制器端时间戳（ms） |
| `data_valid` | `bool` | 数据是否有效 |
| `joint_position` | `std::vector<double>` | 关节位置（度），六轴 |
| `joint_velocity` | `std::vector<double>` | 关节角速度（deg/s） |
| `tcp_pose` | `std::vector<double>` | [x,y,z,rx,ry,rz]，mm + 度 |
| `tcp_velocity` | `std::vector<double>` | 线 mm/s，角 deg/s |
| `tcp_linear_velocity_mm_s` | `double` | TCP 线速度标量 |
| `joint_output_torque` | `std::vector<double>` | 关节输出力矩 |
| `joint_external_force` | `std::vector<double>` | 关节外部力 |
| `in_motion` | `bool` | 是否正在运动 |
| `has_alarm` | `bool` | 是否有报警 |
| `collision_stopped` | `bool` | 是否因碰撞停止 |
| `emergency_stop_pressed` | `bool` | 急停是否按下 |
| `remote_mode` | `bool` | 是否远程模式 |
| `auto_mode` | `bool` | 是否自动模式 |
| `manual_mode` | `bool` | 是否手动模式 |
| `realtime_control_mode` | `bool` | 是否实时控制模式 |
| `project_running` | `bool` | 工程是否运行中 |

```cpp
auto state = robot.GetRobotRealtimeState();
if (state.data_valid) {
    std::cout << "J1 = " << state.joint_position[0] << std::endl;
    std::cout << "TCP X = " << state.tcp_pose[0] << std::endl;
}
```

---

### SetCriDataReceived

```cpp
void SetCriDataReceived(std::function<void(const ClientRealtimeState&)> cb);
```

设置 CRI 数据回调。每次收到 CRI 帧时触发。避免在回调中长时间阻塞。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `cb` | `std::function<void(const ClientRealtimeState&)>` | -- | 回调函数 |

**返回值：** 无

**异常：** 无

```cpp
robot.SetCriDataReceived([](const Codroid::ClientRealtimeState& data) {
    std::cout << "Joints: ";
    for (auto j : data.joint_position) std::cout << j << " ";
    std::cout << std::endl;
});
```

---

### WaitForCriData

```cpp
void WaitForCriData(double timeout_s = 5.0);
```

阻塞等待第一个 CRI 数据帧到达。调用 `*Sync` 阻塞运动方法前必须确保 CRI 数据已开始推送。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `timeout_s` | `double` | 5.0 | 最大等待秒数 |

**返回值：** 无

**异常：** `std::runtime_error` -- 超时未收到数据

```cpp
robot.StartCriDataPush("192.168.1.150", 18888);
robot.WaitForCriData(5.0); // 等待首帧
// 现在可以安全调用 *Sync 方法
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

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `mainScript` | `const std::string&` | -- | 主脚本内容 |
| `subThreads` | `const std::unordered_map<std::string, std::string>&` | `{}` | 子线程脚本 |
| `subPrograms` | `const std::unordered_map<std::string, std::string>&` | `{}` | 子程序脚本 |
| `interrupts` | `const std::unordered_map<std::string, std::string>&` | `{}` | 中断处理脚本 |
| `vars` | `const nlohmann::json&` | `{}` | 注入变量 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.RunScript("movej(j1, v50) sub1() end");
```

---

### EnterRemoteScriptMode

```cpp
CommandResult EnterRemoteScriptMode(int id = 1);
```

进入远程脚本模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### Run

```cpp
CommandResult Run(const std::string& projectId, int id = 1);
```

按工程 ID 运行工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `projectId` | `const std::string&` | -- | 工程 ID |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### RunByIndex

```cpp
CommandResult RunByIndex(int index, int id = 1);
```

通过索引运行工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `index` | `int` | -- | 工程索引 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### RunStep

```cpp
CommandResult RunStep(const std::string& projectId, int id = 1);
```

单步运行工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `projectId` | `const std::string&` | -- | 工程 ID |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### PauseProject

```cpp
CommandResult PauseProject(int id = 1);
```

暂停工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### ResumeProject

```cpp
CommandResult ResumeProject(int id = 1);
```

恢复工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### StopProject

```cpp
CommandResult StopProject(int id = 1);
```

停止工程。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

## 全局变量

### GetGlobalVars

```cpp
nlohmann::json GetGlobalVars(int id = 1);
```

获取所有全局变量（原始 JSON 响应）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `nlohmann::json` -- 控制器返回的原始 JSON

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
nlohmann::json vars = robot.GetGlobalVars();
std::cout << vars.dump(2) << std::endl;
```

---

### SaveGlobalVars

```cpp
CommandResult SaveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1);
```

保存全局变量。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `vars` | `const std::map<std::string, Variable>&` | -- | 变量名到值的映射 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**Variable 结构：**

```cpp
struct Variable {
    std::string val;  // JSON 字符串
    std::string nm;   // 备注
    template<typename T>
    Variable(const T& value, const std::string& note = "");
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `val` | `std::string` | JSON 格式的值 |
| `nm` | `std::string` | 备注信息 |

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

删除全局变量。删除不存在的变量不会报错。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `names` | `const std::vector<std::string>&` | -- | 要删除的变量名列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.RemoveGlobalVars({"counter", "name"});
```

---

## 运动学

### ForwardKinematics

```cpp
std::vector<double> ForwardKinematics(const FKParams& params, int id = 1);
```

正运动学：关节角 -> TCP 位姿。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const FKParams&` | -- | 正解参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<double>` -- TCP 位姿 [x,y,z,rx,ry,rz]（mm + 度）

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**FKParams 结构：**

```cpp
struct FKParams {
    std::vector<double> jp;    // 关节角（度）
    std::vector<double> coor;  // 可选用户坐标系
    std::vector<double> tool;  // 可选工具坐标系
    std::vector<double> ep;    // 可选附加轴
    explicit FKParams(const std::vector<double>& jointPos);
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `jp` | `std::vector<double>` | 关节角（度），六轴 |
| `coor` | `std::vector<double>` | 用户坐标系 [x,y,z,a,b,c]（可选） |
| `tool` | `std::vector<double>` | 工具坐标系（可选） |
| `ep` | `std::vector<double>` | 附加轴（可选） |

```cpp
Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
auto pose = robot.ForwardKinematics(fk);
// pose = [x, y, z, rx, ry, rz] in mm + deg
```

---

### InverseKinematics

```cpp
std::vector<double> InverseKinematics(const IKParams& params, int id = 1);
```

逆运动学：TCP 位姿 -> 关节角。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const IKParams&` | -- | 逆解参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<double>` -- 关节角（度），六轴

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**IKParams 结构：**

```cpp
struct IKParams {
    std::vector<double> cp;   // TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;   // 参考关节角（度），用作 IK 起始猜测
    std::vector<double> ep;   // 可选附加轴
    explicit IKParams(const std::vector<double>& cartesianPos);
};
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `cp` | `std::vector<double>` | TCP 位姿 [x,y,z,rx,ry,rz]（mm + 度） |
| `rj` | `std::vector<double>` | 参考关节角（度），用作 IK 起始猜测 |
| `ep` | `std::vector<double>` | 附加轴（可选） |

```cpp
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
ik.rj = {0, 0, 90, 0, 90, 0}; // 参考关节
auto joints = robot.InverseKinematics(ik);
```

---

### CalculateRelativePose

```cpp
std::vector<double> CalculateRelativePose(const RelativePoseParams& params, int id = 1);
```

笛卡尔相对位姿计算。在用户或工具坐标系中对当前 TCP 施加偏移。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `params` | `const RelativePoseParams&` | -- | 相对位姿参数 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `std::vector<double>` -- 计算后的位姿 [x,y,z,rx,ry,rz]

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**RelativePoseParams 结构：**

```cpp
struct RelativePoseParams {
    std::vector<double> pos;      // 当前位姿 [x,y,z,rx,ry,rz]
    std::vector<double> offset;   // 偏移量 [dx,dy,dz,drx,dry,drz]
    CoorType coorType = CoorType::Tool;  // 坐标系类型
    std::vector<double> posCoor;  // 可选位置坐标系
    std::vector<double> coor;     // 可选坐标系
};
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `pos` | `std::vector<double>` | -- | 当前 TCP 位姿 |
| `offset` | `std::vector<double>` | -- | [dx,dy,dz,drx,dry,drz] 偏移量 |
| `coorType` | `CoorType` | `Tool` | 坐标系类型：`User` 或 `Tool` |
| `posCoor` | `std::vector<double>` | `{}` | 位置坐标系（可选） |
| `coor` | `std::vector<double>` | `{}` | 坐标系定义（可选） |

```cpp
auto state = robot.GetRobotRealtimeState();
Codroid::RelativePoseParams params(
    state.tcp_pose,
    {0, 0, -100, 0, 0, 0},
    Codroid::CoorType::Tool
);
auto newPose = robot.CalculateRelativePose(params);
```

---

## 主题订阅

### SubscribePublishTopic

```cpp
ClientPublishSubscription SubscribePublishTopic(
    std::string topicTy,
    std::function<void(const ClientPublishNotification&)> handler,
    int tc_milliseconds = 100);
```

订阅推送主题。返回订阅句柄，析构时自动停止订阅。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `topicTy` | `std::string` | -- | 主题字符串（如 `"publish/RobotStatus"`） |
| `handler` | `std::function<void(const ClientPublishNotification&)>` | -- | 回调函数 |
| `tc_milliseconds` | `int` | 100 | 推送周期（ms） |

**返回值：** `ClientPublishSubscription` -- 订阅句柄，析构或调用 `Dispose()` 时停止

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientPublishNotification 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `ty` | `std::string` | 主题类型 |
| `db_json` | `std::string` | `db` 子树的 JSON 字符串 |
| `raw_json` | `std::string` | 整帧 JSON |

```cpp
auto sub = robot.SubscribePublishTopic(
    "publish/RobotStatus",
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "状态更新: " << n.db_json << std::endl;
    }
);
// 订阅在 sub 析构前有效
```

---

## 机器人设置参数

### GetRobotParameters

```cpp
ClientRobotParameters GetRobotParameters(int id = 1);
```

获取设置界面参数（协议 19.7）。返回工具坐标系、载荷坐标系、用户坐标系及默认 ID。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `id` | `int` | 1 | 请求 ID |

**返回值：** `ClientRobotParameters` -- 机器人完整参数集

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientRobotParameters 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `valid` | `bool` | 数据是否有效（失败时为 false） |
| `default_tool_id` | `int` | 当前激活的工具坐标系编号 |
| `default_payload_id` | `int` | 当前激活的载荷编号 |
| `default_coordinate_id` | `int` | 当前激活的用户坐标系编号 |
| `max_payload` | `double` | 最大载荷质量（kg） |
| `tool` | `std::vector<ClientRobotFrame>` | 工具坐标系列表 |
| `payload` | `std::vector<ClientRobotPayload>` | 载荷坐标系列表 |
| `coordinate` | `std::vector<ClientRobotFrame>` | 用户坐标系列表 |

```cpp
auto params = robot.GetRobotParameters();
if (params.valid) {
    std::cout << "默认工具: " << params.default_tool_id << std::endl;
    std::cout << "最大载荷: " << params.max_payload << " kg" << std::endl;
}
```

---

### SetDefaultPayloadId

```cpp
CommandResult SetDefaultPayloadId(int payloadId, int id = 1);
```

设置默认载荷编号（1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `payloadId` | `int` | -- | 载荷编号（1~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetDefaultToolId

```cpp
CommandResult SetDefaultToolId(int toolId, int id = 1);
```

设置默认工具坐标系编号（1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `toolId` | `int` | -- | 工具编号（1~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetDefaultUserCoordinateId

```cpp
CommandResult SetDefaultUserCoordinateId(int coordinateId, int id = 1);
```

设置默认用户坐标系编号（1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `coordinateId` | `int` | -- | 用户坐标系编号（1~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SaveToolFrames

```cpp
CommandResult SaveToolFrames(const std::vector<ClientRobotFrame>& frames, int id = 1);
```

直接下发完整工具坐标系表（必须包含 id 0~15，id=0 必须全零）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frames` | `const std::vector<ClientRobotFrame>&` | -- | 工具坐标系列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetToolFrame

```cpp
CommandResult SetToolFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);
```

修改单个工具坐标系（先读后改，frame_id 为 1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frame_id` | `int` | -- | 工具坐标系编号（1~15） |
| `frame` | `const ClientRobotFrame&` | -- | 坐标系定义 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientRobotFrame 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | `int` | 坐标系编号 |
| `x` | `double` | X 偏移（mm） |
| `y` | `double` | Y 偏移（mm） |
| `z` | `double` | Z 偏移（mm） |
| `a` | `double` | 绕 X 旋转（度） |
| `b` | `double` | 绕 Y 旋转（度） |
| `c` | `double` | 绕 Z 旋转（度） |

```cpp
Codroid::CodroidClient::ClientRobotFrame frame;
frame.id = 1; frame.x = 0; frame.y = 0; frame.z = 100;
frame.a = 0; frame.b = 0; frame.c = 0;
robot.SetToolFrame(1, frame);
```

---

### SavePayloadFrames

```cpp
CommandResult SavePayloadFrames(const std::vector<ClientRobotPayload>& frames, int id = 1);
```

直接下发完整载荷坐标系表。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frames` | `const std::vector<ClientRobotPayload>&` | -- | 载荷坐标系列表 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetPayloadFrame

```cpp
CommandResult SetPayloadFrame(int frame_id, const ClientRobotPayload& frame, int id = 1);
```

修改单个载荷坐标系（先读后改，frame_id 为 1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frame_id` | `int` | -- | 载荷编号（1~15） |
| `frame` | `const ClientRobotPayload&` | -- | 载荷定义 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

**ClientRobotPayload 结构：**

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | `int` | 载荷编号 |
| `m` | `double` | 质量（kg） |
| `mx` | `double` | 质心 X 偏移（mm） |
| `my` | `double` | 质心 Y 偏移（mm） |
| `mz` | `double` | 质心 Z 偏移（mm） |

```cpp
Codroid::CodroidClient::ClientRobotPayload payload;
payload.id = 1; payload.m = 2.5;
payload.mx = 0; payload.my = 0; payload.mz = 50;
robot.SetPayloadFrame(1, payload);
```

---

### SetUserCoordinateFrame

```cpp
CommandResult SetUserCoordinateFrame(int frame_id, const ClientRobotFrame& frame, int id = 1);
```

修改单个用户坐标系（先读后改，frame_id 为 1~15）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `frame_id` | `int` | -- | 用户坐标系编号（1~15） |
| `frame` | `const ClientRobotFrame&` | -- | 坐标系定义 |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
Codroid::CodroidClient::ClientRobotFrame frame;
frame.id = 1; frame.x = 100; frame.y = 200; frame.z = 0;
frame.a = 0; frame.b = 0; frame.c = 45;
robot.SetUserCoordinateFrame(1, frame);
```

---

### SetPayload

```cpp
CommandResult SetPayload(int payloadId, int id = 1);
```

运行时切换当前载荷。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `payloadId` | `int` | -- | 载荷槽位编号（0~15） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

---

### SetManualMoveRate

```cpp
CommandResult SetManualMoveRate(int pct, int id = 1);
```

设置手动运动倍率（0~100）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `pct` | `int` | -- | 运动倍率百分比（0~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetManualMoveRate(50);  // 50% 速度
```

---

### SetAutoMoveRate

```cpp
CommandResult SetAutoMoveRate(int pct, int id = 1);
```

设置自动运动倍率（0~100）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `pct` | `int` | -- | 运动倍率百分比（0~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetAutoMoveRate(100);  // 全速
```

---

### SetCollisionSensitivity

```cpp
CommandResult SetCollisionSensitivity(int sensitivity, int id = 1);
```

设置碰撞检测灵敏度（0~100）。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `sensitivity` | `int` | -- | 碰撞灵敏度（0~100） |
| `id` | `int` | 1 | 请求 ID |

**返回值：** `CommandResult` -- 控制器响应

**异常：** 当 `SetThrowOnCommandError(true)` 时，失败抛 `CodroidCommandException`

```cpp
robot.SetCollisionSensitivity(50);
```

---

## 错误处理设置

### SetThrowOnCommandError

```cpp
void SetThrowOnCommandError(bool enable);
```

设置为 `true` 时，指令失败抛 `CodroidCommandException`；默认为 `false`，通过 `CommandResult::error_msg` 返回。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enable` | `bool` | -- | 是否启用异常模式 |

**返回值：** 无

**异常：** 无

```cpp
robot.SetThrowOnCommandError(true);
try {
    robot.SetDo(999, 1); // 无效端口，将抛异常
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "控制器错误: " << ex.controller_error() << std::endl;
}
```

---

### ThrowOnCommandError

```cpp
bool ThrowOnCommandError() const noexcept;
```

获取当前错误处理模式。

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| -- | -- | -- | 无参 |

**返回值：** `bool` -- `true` 表示异常模式，`false` 表示返回值模式

**异常：** 无

---

## 完整示例

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    try {
        // 1. 连接、切换远程、上电
        if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
            std::cerr << "连接失败" << std::endl;
            return 1;
        }

        // 2. 等待 CRI 数据（阻塞运动必需）
        robot.WaitForCriData(5.0);

        // 3. IO 操作
        int di0 = robot.GetDi(0);
        std::cout << "DI 0 = " << di0 << std::endl;
        robot.SetDo(10, di0);

        // 4. 寄存器
        double reg = robot.GetRegisterValue(49100);
        std::cout << "Register 49100 = " << reg << std::endl;
        robot.SetRegisterValue(49100, reg + 1);

        // 5. 非阻塞运动
        robot.MovJ(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100);

        // 6. 阻塞运动
        auto state = robot.GetRobotRealtimeState();
        robot.MovLSync(
            Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
            150, 500);

        // 7. 多段路径
        std::vector<Codroid::ClientMoveInstruction> path = {
            Codroid::ClientMoveInstruction::MovJ(
                Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}), 40, 100),
            Codroid::ClientMoveInstruction::MovL(
                Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}), 150, 500)
        };
        robot.Move(path);

        // 8. 全局变量
        std::map<std::string, Codroid::Variable> vars = {
            {"counter", Codroid::Variable(42, "计数器")}
        };
        robot.SaveGlobalVars(vars);

        // 9. 运动学
        Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
        auto pose = robot.ForwardKinematics(fk);

    } catch (const Codroid::CodroidCommandException& ex) {
        std::cerr << "指令失败: " << ex.controller_error() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "错误: " << ex.what() << std::endl;
    }

    robot.Disconnect();
    return 0;
}
```

---

# 运动控制 API

本章介绍运动相关的类型和 API：点位表示、路径指令、阻塞运动。

---

## 点位类型

### JointPoint

关节空间目标点（六轴角，单位：度）。

```cpp
struct JointPoint {
    std::vector<double> jp;  // 六轴关节角（度）

    static JointPoint Degrees(std::vector<double> joints_deg);
};
```

**工厂方法：**
```cpp
// 六轴关节角（度）
auto joints = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
```

**使用场景：**
- `MovJ(JointPoint)` — 关节运动到关节目标
- `MovL(JointPoint)` — 直线运动到关节目标
- `MoveInstruction::MovJ(JointPoint)` — 路径段
- `MoveToTarget::Joint(JointPoint)` — MoveTo 规划

---

### CartesianPoint

笛卡尔末端目标点（TCP 位姿：mm + 度）。

```cpp
struct CartesianPoint {
    std::vector<double> cp;  // [x, y, z, rx, ry, rz]，前三 mm，后三度
    std::vector<double> rj;  // 逆解参考关节角（度）

    static CartesianPoint MmDeg(std::vector<double> pose_mm_deg);
    static CartesianPoint MmDegWithRef(std::vector<double> pose_mm_deg, std::vector<double> ref_joints_deg);
};
```

**工厂方法：**

#### MmDeg

仅 TCP 位姿，不指定参考关节。

```cpp
auto pose = Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0});
```

#### MmDegWithRef

TCP 位姿 + 逆解参考关节。**建议**用 CRI `joint_position` 当前值作 `rj`。

```cpp
auto state = robot.GetRobotRealtimeState();
auto pose = Codroid::CartesianPoint::MmDegWithRef(
    {400, 0, 300, 180, 0, 0},
    state.joint_position
);
```

**为什么需要 `rj`？**
- 多组关节解时控制器据此选解，避免跳解
- 建议始终使用 `MmDegWithRef`

**使用场景：**
- `MovL(CartesianPoint)` — 直线运动到笛卡尔目标
- `MovJ(CartesianPoint)` — 关节运动到笛卡尔目标（控制器做逆解）
- `MoveInstruction::MovL(CartesianPoint)` — 路径段
- `MoveToTarget::Cartesian(CartesianPoint)` — MoveTo 规划

---

### MovePoint

路径段中的单个目标点（协议层）。

```cpp
struct MovePoint {
    std::vector<double> jp;  // 关节角（度），与 cp 二选一
    std::vector<double> cp;  // TCP [x,y,z,rx,ry,rz]（mm+度）
    std::vector<double> rj;  // 仅 cp 有效时：逆解参考关节（度）
    std::vector<double> ep;  // 附加轴（可选）

    static MovePoint Joint(JointPoint joint);
    static MovePoint Cartesian(CartesianPoint cart);
};
```

**工厂方法：**
```cpp
// 从 JointPoint 生成
auto mp1 = Codroid::MovePoint::Joint(Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));

// 从 CartesianPoint 生成
auto mp2 = Codroid::MovePoint::Cartesian(Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}));
```

**注意：** 每条路径点只应填 **jp 或 cp 之一**（打包时 jp 优先）。

---

## 路径指令

### ClientMoveInstruction / MoveInstruction

路径中的一段运动指令。

```cpp
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

    // 静态工厂方法
    static ClientMoveInstruction MovJ(ClientJointPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovJ(ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovL(ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovL(ClientJointPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovC(ClientCartesianPoint middle, ClientCartesianPoint target, double speed, double acceleration, double blend = -1.0);
    static ClientMoveInstruction MovCircle(ClientCartesianPoint middle, ClientCartesianPoint target, int circle_num, double speed, double acceleration, double blend = -1.0);
};
```

### ClientMoveType / MoveType

```cpp
enum class ClientMoveType {
    MovJ,       // 关节运动
    MovL,       // 直线（笛卡尔）
    MovC,       // 圆弧（经由中间点）
    MovCircle   // 整圆/多圈
};
```

### 运动类型详解

#### MovJ — 关节运动

关节空间插补，目标可为关节角或笛卡尔位姿。

```cpp
// 目标为关节角
auto inst1 = Codroid::ClientMoveInstruction::MovJ(
    Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}),
    40, 100
);

// 目标为笛卡尔位姿（控制器做逆解）
auto inst2 = Codroid::ClientMoveInstruction::MovJ(
    Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}),
    40, 100
);
```

#### MovL — 直线运动

笛卡尔直线插补，目标可为笛卡尔位姿或关节角。

```cpp
// 目标为笛卡尔位姿
auto inst = Codroid::ClientMoveInstruction::MovL(
    Codroid::CartesianPoint::MmDegWithRef(
        {400, 0, 300, 180, 0, 0},
        state.joint_position
    ),
    150, 500
);
```

#### MovC — 圆弧运动

经由中间点的圆弧。

```cpp
auto inst = Codroid::ClientMoveInstruction::MovC(
    Codroid::CartesianPoint::MmDeg({450, 50, 300, 180, 0, 0}),  // 中间点
    Codroid::CartesianPoint::MmDeg({500, 0, 300, 180, 0, 0}),   // 目标点
    100, 300
);
```

#### MovCircle — 整圆运动

整圆/多圈运动。

```cpp
auto inst = Codroid::ClientMoveInstruction::MovCircle(
    Codroid::CartesianPoint::MmDeg({450, 50, 300, 180, 0, 0}),  // 中间点
    Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0}),   // 目标点
    2,    // 圈数
    100, 300
);
```

### blend（过渡半径）

- `blend < 0`：不下发，用控制器默认（精确停止）
- `blend = 0`：精确停止
- `blend > 0`：过渡半径（mm），路径在目标点附近平滑过渡

```cpp
// 精确停止
auto inst1 = Codroid::ClientMoveInstruction::MovJ(joints, 40, 100, 0);

// 过渡半径 50mm
auto inst2 = Codroid::ClientMoveInstruction::MovJ(joints, 40, 100, 50);
```

---

## 路径执行

### Move / MovePath

```cpp
CommandResult Move(const std::vector<ClientMoveInstruction>& path, int id = 1);
CommandResult MovePath(const std::vector<ClientMoveInstruction>& path, int id = 1);
```

多段路径按顺序执行。

**示例：**
```cpp
auto state = robot.GetRobotRealtimeState();

std::vector<Codroid::ClientMoveInstruction> path = {
    // 段 1：关节运动到起始位
    Codroid::ClientMoveInstruction::MovJ(
        Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}),
        40, 100, 0
    ),
    // 段 2：直线运动到目标位
    Codroid::ClientMoveInstruction::MovL(
        Codroid::CartesianPoint::MmDegWithRef({400, 0, 300, 180, 0, 0}, state.joint_position),
        150, 500, 0
    ),
    // 段 3：圆弧运动
    Codroid::ClientMoveInstruction::MovC(
        Codroid::CartesianPoint::MmDeg({450, 50, 300, 180, 0, 0}),
        Codroid::CartesianPoint::MmDeg({500, 0, 300, 180, 0, 0}),
        100, 300
    )
};

robot.Move(path);
```

---

## 阻塞运动（Sync）

阻塞式运动，等待 CRI 确认到达目标后才返回。

### 前置条件

调用 `*Sync` 方法前，必须确保 CRI 数据已开始推送：

```cpp
// 1. 连接
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");

// 2. 启动 CRI 推送
robot.StartCriDataPush("192.168.1.150", 9030);

// 3. 等待首帧
robot.WaitForCriData(5.0);

// 4. 现在可以使用 *Sync 方法
robot.MovLSync(target, 150, 500);
```

### MoveSync

```cpp
bool MoveSync(const std::vector<ClientMoveInstruction>& path, const MotionWaitOptions& wait = {});
```

阻塞式路径执行。

### MovJSync

```cpp
bool MovJSync(const ClientJointPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovJSync(const ClientCartesianPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式关节运动。

### MovLSync

```cpp
bool MovLSync(const ClientCartesianPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
bool MovLSync(const ClientJointPoint& target, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式直线运动。

### MovCSync

```cpp
bool MovCSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
              double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式圆弧运动。

### MovCircleSync

```cpp
bool MovCircleSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                   int circle_num, double speed, double acc, const MotionWaitOptions& wait = {});
```

阻塞式整圆运动。

### MotionWaitOptions

```cpp
struct MotionWaitOptions {
    double timeout_s = 60.0;                        // 整体等待超时（秒）
    double poll_interval_s = 0.05;                  // CRI 轮询间隔（秒）
    double cri_stale_timeout_s = 0.5;               // CRI 数据过期判定（秒）
    int settled_samples = 3;                        // InMotion=false 连续稳定采样数
};
```

**自定义等待选项：**
```cpp
Codroid::MotionWaitOptions wait;
wait.timeout_s = 30.0;           // 30 秒超时

robot.MovLSync(target, 150, 500, wait);
```

### 返回值

- `true` — 到达目标
- `false` — 超时未到达

---

## 运动控制

### PauseRobotMotion / ResumeRobotMotion / StopRobotMove

```cpp
CommandResult PauseRobotMotion(int id = 1);
CommandResult ResumeRobotMotion(int id = 1);
CommandResult StopRobotMove(int id = 1);
```

暂停、恢复、停止当前运动。

---

## MoveTo（规划运动）

### MoveToParams

```cpp
struct MoveToParams {
    MoveToType type = MoveToType::Home;
    MoveToTarget target;

    MoveToParams() = default;
    explicit MoveToParams(MoveToType t);
    MoveToParams(MoveToType t, const MoveToTarget& tgt);
};
```

### MoveToType

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

### MoveToTarget

```cpp
struct MoveToTarget {
    std::vector<double> cp;
    std::vector<double> jp;

    static MoveToTarget Joint(JointPoint joint);
    static MoveToTarget Cartesian(CartesianPoint cart);
};
```

### 使用示例

```cpp
// 回 Home
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Home));

// 关节规划到目标
auto target = Codroid::MoveToTarget::Joint(
    Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));
robot.MoveTo(Codroid::MoveToParams(Codroid::MoveToType::Joint, target));

// 心跳（每 ≥500ms 调用）
while (moving) {
    robot.MoveToHeartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

// 停止
robot.StopMoveTo();
```

---

## Jog（点动）

### JogParams

```cpp
struct JogParams {
    JogMode mode = JogMode::Line;
    double speed = 0.0;     // -1~1 的比例
    int index = 1;          // 轴号或方向
    CoorType coorType = CoorType::User; // Jog 协议下发 User=0, Tool=1
    int coorId = 1;

    JogParams() = default;
    JogParams(JogMode m, double s, int i, CoorType ct = CoorType::User, int cid = 1);
};
```

### JogMode

```cpp
enum class JogMode {
    Joint = 1,  // 关节点动
    Line = 2    // 直线点动
};
```

### 使用示例

```cpp
// 关节 1 正向点动，速度 50%
robot.Jog(Codroid::JogParams(Codroid::JogMode::Joint, 0.5, 1));

// 笛卡尔 X 正向点动，速度 30%
robot.Jog(Codroid::JogParams(Codroid::JogMode::Line, 0.3, 1));

// 用户坐标系 2 下的点动
robot.Jog(Codroid::JogParams(Codroid::JogMode::Line, 0.3, 1, Codroid::CoorType::User, 2));

// 心跳
while (jogging) {
    robot.JogHeartbeat();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

// 停止
robot.StopJog();
```

---

## 最佳实践

### 1. 始终使用 MmDegWithRef

```cpp
// 好：指定参考关节
auto state = robot.GetRobotRealtimeState();
auto target = Codroid::CartesianPoint::MmDegWithRef(
    {400, 0, 300, 180, 0, 0},
    state.joint_position
);

// 不推荐：不指定参考关节
auto target = Codroid::CartesianPoint::MmDeg({400, 0, 300, 180, 0, 0});
```

### 2. 使用阻塞运动确保到达

```cpp
// 非阻塞：立即返回，不等待到达
robot.MovL(target, 150, 500);

// 阻塞：等待到达目标
robot.MovLSync(target, 150, 500);
```

### 3. 合理设置过渡半径

```cpp
// 精确停止（适合最终定位）
auto inst1 = Codroid::ClientMoveInstruction::MovL(target1, 150, 500, 0);

// 过渡（适合连续路径，提高效率）
auto inst2 = Codroid::ClientMoveInstruction::MovL(target2, 150, 500, 50);
```

### 4. 路径段间避免重复首点

```cpp
// 好：路径段连续
std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(jp1, 40, 100),
    Codroid::ClientMoveInstruction::MovL(cp2, 150, 500),  // 从 jp1 终点到 cp2
    Codroid::ClientMoveInstruction::MovL(cp3, 150, 500)   // 从 cp2 终点到 cp3
};
```

---

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

---

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

---

# IO 与寄存器 API

本章介绍数字量/模拟量 IO 操作和寄存器读写。

---

## IO 操作

### 数字量输入 (DI)

#### GetDi

```cpp
int GetDi(int port, int id = 1);
```

读取数字量输入。

**参数：**
- `port` — 端口号

**返回：** DI 值（0 或 1）

**示例：**
```cpp
int di0 = robot.GetDi(0);
int di1 = robot.GetDi(1);
std::cout << "DI 0 = " << di0 << ", DI 1 = " << di1 << std::endl;
```

---

### 数字量输出 (DO)

#### GetDo

```cpp
int GetDo(int port, int id = 1);
```

读取数字量输出。

**参数：**
- `port` — 端口号

**返回：** DO 值（0 或 1）

---

#### SetDo

```cpp
CommandResult SetDo(int port, int value, int id = 1);
```

设置数字量输出。

**参数：**
- `port` — 端口号
- `value` — 值（0 或 1）

**示例：**
```cpp
// 设置 DO 10 为 1
auto result = robot.SetDo(10, 1);
if (!result.Ok()) {
    std::cerr << "设置失败: " << result.error_msg << std::endl;
}

// 读取 DI 0 并写入 DO 10
int di0 = robot.GetDi(0);
robot.SetDo(10, di0);
```

---

### 模拟量输入 (AI)

#### GetAi

```cpp
double GetAi(int port, int id = 1);
```

读取模拟量输入。

**参数：**
- `port` — 端口号

**返回：** AI 值

**示例：**
```cpp
double ai0 = robot.GetAi(0);
std::cout << "AI 0 = " << ai0 << std::endl;
```

---

### 模拟量输出 (AO)

#### GetAo

```cpp
double GetAo(int port, int id = 1);
```

读取模拟量输出。

**参数：**
- `port` — 端口号

**返回：** AO 值

---

#### SetAo

```cpp
CommandResult SetAo(int port, double value, int id = 1);
```

设置模拟量输出。

**参数：**
- `port` — 端口号
- `value` — 值

**示例：**
```cpp
// 设置 AO 0 为 3.3V
robot.SetAo(0, 3.3);
```

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

**示例：**
```cpp
double value = robot.GetRegisterValue(49100);
std::cout << "寄存器 49100 = " << value << std::endl;
```

---

### GetRegisterValues

```cpp
std::vector<ClientRegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1);
```

批量读取寄存器。

**参数：**
- `addresses` — 地址列表

**返回：** `ClientRegisterInfo` 列表

**ClientRegisterInfo：**
```cpp
struct ClientRegisterInfo {
    int address = 0;    // 地址
    double value = 0.0; // 值
};
```

**示例：**
```cpp
std::vector<int> addresses = {49100, 49101, 49102};
auto values = robot.GetRegisterValues(addresses);

for (const auto& reg : values) {
    std::cout << "寄存器 " << reg.address << " = " << reg.value << std::endl;
}
```

---

### SetRegisterValue

```cpp
CommandResult SetRegisterValue(int address, double value, int id = 1);
```

设置寄存器值。

**参数：**
- `address` — 寄存器地址
- `value` — 值

**示例：**
```cpp
// 读取并修改
double value = robot.GetRegisterValue(49100);
robot.SetRegisterValue(49100, value + 1);
```

---

## ClientIoInfo / IOInfo

IO 读回信息结构体。

```cpp
struct ClientIoInfo {
    std::string type;   // 类型字符串（DI/DO/AI/AO）
    int port = 0;       // 端口号
    double value = 0.0; // 值
};
```

---

## 完整示例

### 基础 IO 操作

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 读取 DI
    for (int i = 0; i < 8; i++) {
        int di = robot.GetDi(i);
        std::cout << "DI " << i << " = " << di << std::endl;
    }

    // 读取 AI
    for (int i = 0; i < 4; i++) {
        double ai = robot.GetAi(i);
        std::cout << "AI " << i << " = " << ai << std::endl;
    }

    // 设置 DO
    robot.SetDo(0, 1);
    robot.SetDo(1, 0);

    // 设置 AO
    robot.SetAo(0, 3.3);

    // 读取 DO 状态
    int do0 = robot.GetDo(0);
    std::cout << "DO 0 = " << do0 << std::endl;

    robot.Disconnect();
    return 0;
}
```

---

### 寄存器读写

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 读取单个寄存器
    double value = robot.GetRegisterValue(49100);
    std::cout << "寄存器 49100 = " << value << std::endl;

    // 修改寄存器
    robot.SetRegisterValue(49100, value + 1);

    // 批量读取
    std::vector<int> addresses = {49100, 49101, 49102, 49103};
    auto values = robot.GetRegisterValues(addresses);

    std::cout << "批量读取:" << std::endl;
    for (const auto& reg : values) {
        std::cout << "  " << reg.address << " = " << reg.value << std::endl;
    }

    // 批量修改
    for (const auto& reg : values) {
        robot.SetRegisterValue(reg.address, reg.value * 2);
    }

    robot.Disconnect();
    return 0;
}
```

---

### DI 触发运动

```cpp
#include "Codroid/client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 启动 CRI 推送（用于阻塞运动）
    robot.StartCriDataPush("192.168.1.150", 9030);
    robot.WaitForCriData(5.0);

    std::cout << "等待 DI 0 触发..." << std::endl;

    // 等待 DI 0 为 1
    while (robot.GetDi(0) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "DI 0 触发，开始运动" << std::endl;

    // 执行运动
    auto target = Codroid::CartesianPoint::MmDegWithRef(
        {400, 0, 300, 180, 0, 0},
        robot.GetRobotRealtimeState().joint_position
    );
    robot.MovLSync(target, 150, 500);

    std::cout << "运动完成" << std::endl;

    // 设置 DO 0 指示完成
    robot.SetDo(0, 1);

    robot.StopCriDataPush();
    robot.Disconnect();
    return 0;
}
```

---

## 最佳实践

### 1. 检查返回值

```cpp
// 好：检查返回值
auto result = robot.SetDo(10, 1);
if (!result.Ok()) {
    std::cerr << "设置失败: " << result.error_msg << std::endl;
    // 错误处理
}

// 不推荐：忽略返回值
robot.SetDo(10, 1);
```

### 2. 使用异常模式

```cpp
robot.SetThrowOnCommandError(true);

try {
    robot.SetDo(10, 1);
    robot.SetRegisterValue(49100, 100);
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "错误: " << ex.controller_error() << std::endl;
}
```

### 3. 批量读取提高效率

```cpp
// 好：批量读取
std::vector<int> addresses = {49100, 49101, 49102};
auto values = robot.GetRegisterValues(addresses);

// 不推荐：逐个读取
double v1 = robot.GetRegisterValue(49100);
double v2 = robot.GetRegisterValue(49101);
double v3 = robot.GetRegisterValue(49102);
```

### 4. IO 端口范围

- DI/DO 端口范围取决于控制器硬件配置
- AI/AO 端口范围取决于控制器硬件配置
- 使用无效端口会返回错误

---

## 常见问题

### Q1: GetDi 返回值始终为 0

1. 检查端口号是否正确
2. 检查硬件连接
3. 确认控制器已上电

### Q2: SetDo 不生效

1. 检查端口号是否正确
2. 检查返回值是否有错误
3. 确认控制器处于远程模式

### Q3: 寄存器地址范围

寄存器地址范围取决于控制器固件版本。常见地址：
- 49100~49199：通用寄存器
- 其他地址请参考控制器文档

### Q4: 模拟量精度

AI/AO 精度取决于硬件配置。SDK 返回 `double` 类型，实际精度由硬件决定。

---

# 辅助工具 API

本章介绍主题订阅、全局变量、运动学和控制台 UTF-8 等辅助功能。

---

## 主题订阅

### SubscribePublishTopic

```cpp
ClientPublishSubscription SubscribePublishTopic(
    std::string topicTy,
    std::function<void(const ClientPublishNotification&)> handler,
    int tc_milliseconds = 100);
```

订阅控制器推送的主题。

**参数：**
- `topicTy` — 主题字符串（如 `"publish/RobotStatus"`）
- `handler` — 回调函数
- `tc_milliseconds` — 推送周期，默认 100 ms

**返回：** 订阅句柄，析构时自动停止订阅

---

### 可用主题

| 主题 | 说明 | 数据格式 |
|------|------|----------|
| `publish/ProjectState` | 工程运行状态 | ProjectState |
| `publish/VarUpdate` | 变量更新 | JSON |
| `publish/RobotStatus` | 机器人状态 | RobotStatus |
| `publish/RobotPosture` | 机器人位姿 | RobotPosture |
| `publish/RobotCoordinate` | 坐标系 | JSON |
| `publish/Log` | 日志 | JSON |
| `publish/Error` | 错误 | JSON |

---

### PublishTopics 常量

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

**使用示例：**
```cpp
// 使用常量
auto sub = robot.SubscribePublishTopic(
    Codroid::PublishTopics::RobotStatus,
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "状态更新: " << n.db_json << std::endl;
    }
);

// 使用字符串
auto sub2 = robot.SubscribePublishTopic(
    "publish/ProjectState",
    [](const Codroid::ClientPublishNotification& n) {
        std::cout << "工程状态: " << n.db_json << std::endl;
    }
);
```

---

### ClientPublishNotification

```cpp
struct ClientPublishNotification {
    std::string ty;        // 主题类型
    std::string db_json;   // db 子树的 JSON 字符串
    std::string raw_json;  // 整帧 JSON
};
```

**使用示例：**
```cpp
auto sub = robot.SubscribePublishTopic(
    "publish/RobotStatus",
    [](const Codroid::ClientPublishNotification& n) {
        // 解析 JSON
        auto db = nlohmann::json::parse(n.db_json);

        // 读取字段
        if (db.contains("mode")) {
            int mode = db["mode"];
            std::cout << "模式: " << mode << std::endl;
        }

        if (db.contains("state")) {
            int state = db["state"];
            std::cout << "状态: " << state << std::endl;
        }
    }
);
```

---

### ClientPublishSubscription

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

**生命周期管理：**
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

    // 可选：提前停止
    sub.Dispose();
}
// sub 析构，订阅自动停止
```

---

## 全局变量

### GetGlobalVars

```cpp
nlohmann::json GetGlobalVars(int id = 1);
```

获取所有全局变量（原始 JSON 响应）。

**返回：** JSON 对象

**使用示例：**
```cpp
auto vars = robot.GetGlobalVars();
std::cout << "全局变量: " << vars.dump(2) << std::endl;

// 遍历变量
for (auto& [name, value] : vars.items()) {
    std::cout << name << " = " << value << std::endl;
}
```

---

### SaveGlobalVars

```cpp
CommandResult SaveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1);
```

保存全局变量。

**参数：**
- `vars` — 变量映射（名称 → Variable）

**Variable：**
```cpp
struct Variable {
    std::string val;  // JSON 字符串
    std::string nm;   // 备注

    template<typename T>
    Variable(const T& value, const std::string& note = "");
    Variable() = default;
};
```

**使用示例：**
```cpp
std::map<std::string, Codroid::Variable> vars = {
    {"counter", Codroid::Variable(42, "计数器")},
    {"name", Codroid::Variable(std::string("test"), "名称")},
    {"flag", Codroid::Variable(true, "标志")}
};

auto result = robot.SaveGlobalVars(vars);
if (!result.Ok()) {
    std::cerr << "保存失败: " << result.error_msg << std::endl;
}
```

---

### RemoveGlobalVars

```cpp
CommandResult RemoveGlobalVars(const std::vector<std::string>& names, int id = 1);
```

删除全局变量。

**参数：**
- `names` — 变量名列表

**使用示例：**
```cpp
std::vector<std::string> names = {"counter", "name"};
robot.RemoveGlobalVars(names);
```

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
    std::vector<double> jp;    // 关节角（度）
    std::vector<double> coor;  // 可选用户坐标系
    std::vector<double> tool;  // 可选工具坐标系
    std::vector<double> ep;    // 可选附加轴

    explicit FKParams(const std::vector<double>& jointPos);
    FKParams() = default;
};
```

**返回：** TCP 位姿 [x,y,z,rx,ry,rz]（mm+度）

**使用示例：**
```cpp
// 基本用法
Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
auto tcp = robot.ForwardKinematics(fk);

std::cout << "TCP 位姿: ";
for (auto v : tcp) std::cout << v << " ";
std::cout << std::endl;

// 使用当前关节位置
auto state = robot.GetRobotRealtimeState();
Codroid::FKParams fk2(state.joint_position);
auto tcp2 = robot.ForwardKinematics(fk2);
```

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

    explicit IKParams(const std::vector<double>& cartesianPos);
    IKParams() = default;
};
```

**返回：** 关节角（度）

**使用示例：**
```cpp
// 基本用法
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
auto joints = robot.InverseKinematics(ik);

std::cout << "关节角: ";
for (auto v : joints) std::cout << v << " ";
std::cout << std::endl;

// 指定参考关节（避免跳解）
Codroid::IKParams ik2({400, 0, 300, 180, 0, 0});
ik2.rj = {0, 0, 90, 0, 90, 0};
auto joints2 = robot.InverseKinematics(ik2);
```

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
    CoorType coorType = CoorType::Tool;
    std::vector<double> posCoor;  // 可选位置坐标系
    std::vector<double> coor;     // 可选坐标系

    RelativePoseParams(const std::vector<double>& p, const std::vector<double>& o, CoorType type);
    RelativePoseParams() = default;
};
```

**使用示例：**
```cpp
// 在工具系下偏移
std::vector<double> current = {400, 0, 300, 180, 0, 0};
std::vector<double> offset = {100, 0, 0, 0, 0, 0};  // X 方向偏移 100mm

Codroid::RelativePoseParams params(current, offset, Codroid::CoorType::Tool);
auto newPose = robot.CalculateRelativePose(params);

std::cout << "新位姿: ";
for (auto v : newPose) std::cout << v << " ";
std::cout << std::endl;
```

---

## 控制台 UTF-8

### InitConsoleUtf8

由 `#include "Codroid/client.hpp"` 间接提供（也可单独 include `console_utf8.hpp`）。

```cpp
void InitConsoleUtf8();
```

将当前进程的控制台输入/输出代码页设为 UTF-8（**仅 Windows 生效**；Linux 为空操作）。

**使用示例：**
```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::InitConsoleUtf8();
    std::cout << "中文输出测试" << std::endl;
    // ...
    return 0;
}
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

**参数：**
- `mainScript` — 主脚本代码
- `subThreads` — 子线程（名称 → 代码）
- `subPrograms` — 子程序（名称 → 代码）
- `interrupts` — 中断（名称 → 代码）
- `vars` — 变量

**使用示例：**
```cpp
std::string mainScript = R"(
    print("Hello from Lua")
    local counter = 0
    for i = 1, 10 do
        counter = counter + i
    end
    print("Sum: " .. counter)
)";

auto result = robot.RunScript(mainScript);
if (!result.Ok()) {
    std::cerr << "脚本执行失败: " << result.error_msg << std::endl;
}
```

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

**参数：**
- `projectId` — 工程 ID
- `index` — 工程索引

**使用示例：**
```cpp
// 通过 ID 运行
robot.Run("project_001");

// 通过索引运行
robot.RunByIndex(0);

// 单步运行
robot.RunStep("project_001");
```

---

### PauseProject / ResumeProject / StopProject

```cpp
CommandResult PauseProject(int id = 1);
CommandResult ResumeProject(int id = 1);
CommandResult StopProject(int id = 1);
```

暂停、恢复、停止工程。

**使用示例：**
```cpp
// 运行工程
robot.Run("project_001");

// 暂停
robot.PauseProject();

// 恢复
robot.ResumeProject();

// 停止
robot.StopProject();
```

---

## 完整示例：主题订阅

```cpp
#include "Codroid/client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 订阅机器人状态
    auto statusSub = robot.SubscribePublishTopic(
        Codroid::PublishTopics::RobotStatus,
        [](const Codroid::ClientPublishNotification& n) {
            auto db = nlohmann::json::parse(n.db_json);
            std::cout << "[状态] 模式=" << db["mode"]
                      << " 状态=" << db["state"]
                      << " 运动中=" << db["isMoving"]
                      << std::endl;
        },
        50  // 50ms 周期
    );

    // 订阅工程状态
    auto projectSub = robot.SubscribePublishTopic(
        Codroid::PublishTopics::ProjectState,
        [](const Codroid::ClientPublishNotification& n) {
            auto db = nlohmann::json::parse(n.db_json);
            std::cout << "[工程] ID=" << db["id"]
                      << " 状态=" << db["state"]
                      << std::endl;
        }
    );

    // 订阅错误
    auto errorSub = robot.SubscribePublishTopic(
        Codroid::PublishTopics::Error,
        [](const Codroid::ClientPublishNotification& n) {
            std::cerr << "[错误] " << n.db_json << std::endl;
        }
    );

    // 等待一段时间接收推送
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 订阅在作用域结束时自动停止
    robot.Disconnect();
    return 0;
}
```

---

## 完整示例：运动学计算

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    if (!robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 正解：关节角 → TCP
    Codroid::FKParams fk({0, 0, 90, 0, 90, 0});
    auto tcp = robot.ForwardKinematics(fk);

    std::cout << "正解结果:" << std::endl;
    std::cout << "  X=" << tcp[0] << " mm" << std::endl;
    std::cout << "  Y=" << tcp[1] << " mm" << std::endl;
    std::cout << "  Z=" << tcp[2] << " mm" << std::endl;
    std::cout << "  Rx=" << tcp[3] << " deg" << std::endl;
    std::cout << "  Ry=" << tcp[4] << " deg" << std::endl;
    std::cout << "  Rz=" << tcp[5] << " deg" << std::endl;

    // 逆解：TCP → 关节角
    Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
    ik.rj = {0, 0, 90, 0, 90, 0};  // 参考关节
    auto joints = robot.InverseKinematics(ik);

    std::cout << "逆解结果:" << std::endl;
    for (int i = 0; i < 6; i++) {
        std::cout << "  J" << (i+1) << "=" << joints[i] << " deg" << std::endl;
    }

    // 相对位姿计算
    std::vector<double> current = {400, 0, 300, 180, 0, 0};
    std::vector<double> offset = {50, 0, 0, 0, 0, 0};  // X 方向偏移 50mm

    Codroid::RelativePoseParams relParams(current, offset, Codroid::CoorType::Tool);
    auto newPose = robot.CalculateRelativePose(relParams);

    std::cout << "相对位姿计算:" << std::endl;
    std::cout << "  原始: " << current[0] << ", " << current[1] << ", " << current[2] << std::endl;
    std::cout << "  新位: " << newPose[0] << ", " << newPose[1] << ", " << newPose[2] << std::endl;

    robot.Disconnect();
    return 0;
}
```

---

## 最佳实践

### 1. 及时停止订阅

```cpp
{
    auto sub = robot.SubscribePublishTopic(...);
    // 使用订阅
    // ...
    sub.Dispose();  // 提前停止
}
// 或让析构函数自动停止
```

### 2. 使用 PublishTopics 常量

```cpp
// 好：使用常量
robot.SubscribePublishTopic(Codroid::PublishTopics::RobotStatus, ...);

// 不推荐：硬编码字符串
robot.SubscribePublishTopic("publish/RobotStatus", ...);
```

### 3. 运动学参考关节

```cpp
// 好：指定参考关节
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
ik.rj = currentState.joint_position;
auto joints = robot.InverseKinematics(ik);

// 不推荐：不指定参考关节
Codroid::IKParams ik({400, 0, 300, 180, 0, 0});
auto joints = robot.InverseKinematics(ik);
```

### 4. Windows 控制台 UTF-8

```cpp
// 在 main 函数开头调用
#ifdef _WIN32
Codroid::InitConsoleUtf8();
#endif
```

---

## 常见问题

### Q1: 主题订阅没有收到数据

1. 确认已连接控制器
2. 检查主题名称是否正确
3. 确认控制器支持该主题

### Q2: 全局变量保存失败

1. 检查变量名是否合法
2. 检查值格式是否正确（JSON 字符串）
3. 确认控制器处于远程模式

### Q3: 运动学计算结果异常

1. 检查关节角范围
2. 检查 TCP 位姿是否在工作空间内
3. 指定参考关节避免多解

### Q4: Windows 控制台中文乱码

1. 确保调用 `Codroid::InitConsoleUtf8()`
2. 确保源文件使用 UTF-8 编码
3. 确保控制台字体支持中文

---

# C++ 力控接口说明

当前 C++ SDK 与 Python 力控接口对齐。公开入口仍为 `#include "Codroid/client.hpp"`。

## 初始化与校准

- `ZeroForceCalibration(int calibrationTimeMs = 1000)`：零力校准 / 带载去皮。
- `InitForceControl(...)`：初始化力控。SDK 内部固定 `algo=1`（导纳），当前不允许调用方传入算法参数。`compliance` / `constantForce` / `forceLimit` 为 **`nlohmann::json`** 对象。
- `StartForceControl()` / `StopForceControl(int smoothTimeMs = 500)`：启动 / 停止力控。

`FTSensorDriftCalibration` 已废弃并移除。

## 在线参数与安全

- `TuneForceParams(...)`：在线更新刚度、阻尼、质量、期望力等。
- `StartContactDetection(...)`：接触检测。
- `SetOverforceProtection(...)`：过力保护。
- `SetForceDataHealth(...)`：力数据健康监控。

## 状态读取

`GetForceState()` 返回 `ClientForceControlState`，字段包括：

- `enabled`、`pending`、`valid`、`is_contact`、`is_overforce`：`bool`
- `algo`、`health`：`int`
- `wrench_tcp`、`wrench_base`、`desired_wrench`、`track_error`：`std::vector<double>`
- `axis_mode`：`std::vector<int>`

也可使用单字段 getter，例如 `GetForceStateEnabled()`、`GetForceStateWrenchTcp()`。

## 测试示例

见 `examples/15_force_control.cpp`：

```bash
./build_linux/15_force_control state
./build_linux/15_force_control calibration
./build_linux/15_force_control constant
./build_linux/15_force_control contact --allow-motion
```

示例中的控制器 IP 需按现场修改。

---

