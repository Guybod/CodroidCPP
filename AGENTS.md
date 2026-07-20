# AGENTS.md — Codroid 多语言 SDK 对齐契约

本文档供 **C#（本仓库）**、**Python**、**C++** 三套机械臂控制器 SDK 共用：约定分层、模块边界与协议常量，避免实现分叉。  
**规范以本仓库 `CodroidSDK/` 源码为准**；其它语言实现行为不一致时，优先对照此处与 C# 行为，再考虑是否修订协议或修正 Bug。

---

## 1. 产品与技术边界

- **控制器**：Codroid 系列机器人控制器。
- **指令通道**：**TCP**，JSON 文本；默认端口 **`9001`**（与 C# `CodroidClient` 构造逻辑一致）。
- **CRI 实时数据**：**UDP** 二进制；由 TCP 指令 `CRI/StartDataPush` 请求推送至本机指定端口；当前参考实现假定 **固定 308 字节** 载荷（六轴、无附加轴、`mask=0xFFFF`、高精度），见下方常量。
- **主题推送（协议 15.x）**：同一 TCP 连接上，除带整数 `id` 的请求/响应外，可按 `ty` 分发 **无 `id`** 的 JSON 推送；订阅 API 与 C# `SubscribePublishTopic` 对齐。

---

## 2. 协议与数据约定（三语言必须一致）

### 工程惯例：**默认使用毫米与度**

集成商与上层应用（示教、工艺、与 TCP JSON 运动学接口等）**通常按毫米、度** 理解与传递数据；本仓库 **`CodroidClient` / `CriRealTimeData` 对外字段已统一为该惯例**。

**CRI UDP 线上仍为米与弧度**：多语言 SDK 必须在解析路径上完成与 `CriRealtimePacketParser` **相同的系数换算**，再暴露给业务代码。**切勿**把 UDP 里的原始 `double` 直接当作毫米或度使用，否则数值会差一个数量级或约 57.3 倍。

### 2.1 TCP JSON 通用响应

下行响应字段名与控制器一致：

| 字段 | 含义 |
|------|------|
| `id` | 与请求对应的序号（类型实现可为 int/long/string，但要能与发送的 id 匹配） |
| `ty` | 类型或路由标识 |
| `db` | 业务载荷（对象/数组，按接口解析） |
| `err` | 非空表示控制器报错，本次调用视为失败 |

### 2.2 请求序号

- 每条需要配对的命令使用单调递增的 **`id`**（C# 为 `Interlocked.Increment`）。
- 推送帧 **无整数 `id`**，靠 **`ty`** 区分主题。

### 2.3 CRI UDP 包（与 `CriRealtimePacketParser` 对齐）

固定策略：6 轴、无附加轴、`mask=0xFFFF`、高精度、`duration=100ms` → 载荷长度 **308 字节**；仅处理此长度的包，其它丢弃或记录。**所有多字节标量字段均小端字节序（little-endian）**。

#### 2.3.1 308 字节布局（精确偏移，C# `CriRealtimePacketParser` 实现一致）

| 偏移 | 字段 | 类型 | 长度 | 线上单位 | 说明 |
|------|------|------|------|----------|------|
| 0..7 | `timestamp` | Int64 | 8 | ms | 控制器时间戳 |
| 8..9 | `status1` | UInt16 | 2 | — | 16 位状态字（位定义见 §2.3.2） |
| 10..11 | `status2` | UInt16 | 2 | — | 高 8 位为 CRI 错误码，位 0 为实时控制模式 |
| 12..59 | `jointPosition[6]` | Float64 ×6 | 48 | **rad** | 关节位置 |
| 60..107 | `jointVelocity[6]` | Float64 ×6 | 48 | **rad/s** | 关节角速度 |
| 108..155 | `tcpPose[6]` | Float64 ×6 | 48 | **m**（前三）+ **rad**（后三） | TCP 位姿 [x,y,z,rx,ry,rz] |
| 156..203 | `tcpVelocity[6]` | Float64 ×6 | 48 | **m/s**（前三）+ **rad/s**（后三） | TCP 六维速度 |
| 204..211 | `tcpLinearVelocity` | Float64 | 8 | **m/s** | TCP 线速度标量 |
| 212..259 | `jointOutputTorque[6]` | Float64 ×6 | 48 | 原始 | 关节输出力矩（C# 未做单位换算，按原始浮点透传） |
| 260..307 | `jointExternalForce[6]` | Float64 ×6 | 48 | 原始 | 关节外力（同上，按原始浮点透传） |

附加轴（`ExternalAxisPosition`）当前固定策略下 **不存在**（六轴无附加轴），SDK 解析后填空数组。若启用附加轴需另行约定 mask 与新长度。

#### 2.3.2 `status1` 位定义（UInt16，bit 0 为最低位）

| Bit | C# 字段 | 含义 |
|-----|---------|------|
| 0 | `ProjectRunning` | 工程正在运行 |
| 1 | `ProjectStopped` | 工程已停止 |
| 2 | `ProjectPaused` | 工程已暂停 |
| 3 | `Enabling` | 正在使能过程中 |
| 4 | `NotEnabled` | 未处于使能就绪 |
| 5 | `ManualMode` | 手动模式 |
| 6 | `Dragging` | 拖拽（示教）有效 |
| 7 | `InMotion` | 机构运动中 |
| 8 | `CollisionStopped` | 碰撞检测导致停止 |
| 9 | `InSafetyPosition` | 处于安全位置 |
| 10 | `HasAlarm` | 存在报警 |
| 11 | `SimulationMode` | 仿真模式 |
| 12 | `EmergencyStopPressed` | 急停已按下 |
| 13 | `RescueMode` | 救援模式 |
| 14 | `AutoMode` | 自动模式 |
| 15 | `RemoteMode` | 远程模式 |

#### 2.3.3 `status2` 解析（UInt16）

| 区段 | C# 字段 | 含义 |
|------|---------|------|
| Bit 0 | `RealTimeControlMode` | 实时控制模式有效（`StartCriControl` 后等待此位置 1） |
| 高 8 位 `(raw >> 8) & 0xFF` | `CriErrorCode` | CRI 错误码字节 |

#### 2.3.4 解析后的参考 SDK 模型（C# `CriRealTimeData`，三语言数值对齐目标）

| 属性 | 对外单位（Parse 之后） | 说明 |
|------|------------------------|------|
| `TimestampMs` | ms | 直接透传 |
| `Status1Raw` / `Status2Raw` | UInt16 | 原始位字段，便于调试 |
| `JointPosition` / `JointVelocity` | **度** / **度/s** | × `180/π` |
| `TcpPose` | **mm** + **度** | 前三 × 1000，后三 × `180/π` |
| `TcpVelocity` | **mm/s** + **度/s** | 同上 |
| `TcpLinearVelocity` | **mm/s** | × 1000 |
| `JointOutputTorque` / `JointExternalForce` | 原始浮点 | 不换算，按控制器原值透传 |
| `ExternalAxisPosition` | 空数组 | 当前固定策略下恒空 |

C# 解析后还会对所有浮点字段做 `Math.Round(x, 3, AwayFromZero)`（`DefaultDecimalPlaces = 3`），用于减少打印 / 比对噪声；其它语言可视需要决定是否做相同舍入。

其它语言实现：**读 UDP 后按上表偏移解码 → 做 SI→毫米/度 转换 → 暴露公共类型**，使与 C# **`CriRealTimeData` 数值完全一致**。若某语言额外提供「纯 SI」API，须在文档中单独标明，避免与默认的毫米/度层混用。

### 2.4 CRI 启动推送（与 C# 默认一致）

参考 `CodroidClient`：周期 **100 ms**、mask **0xFFFF**、高精度 **true**（与控制器约定一致即可）。

### 2.5 主题字符串（字面量必须完全相同）

与 `PublishTopics` 一致，订阅/分发时禁止自行改写大小写或路径：

- `publish/ProjectState`
- `publish/VarUpdate`
- `publish/RobotStatus`
- `publish/RobotPosture`
- `publish/RobotCoordinate`
- `publish/Log`
- `publish/Error`

默认订阅帧中的 **`tc`（毫秒）** 默认 **100**（`PublishSubscribeDefaults.TcMilliseconds`）。

---

## 3. 推荐目录与模块映射（三语言同构）

下列 **逻辑文件名 / 模块名** 建议在 Python、`codroid-cpp` 等仓库中保持 **一一对应**，便于 Issue、PR 与文档交叉引用。

| 职责 | C#（本仓库） | Python（建议） | C++（建议） |
|------|----------------|----------------|-------------|
| TCP 收发、按 `id`/`ty` 分派、推送回调注册 | `AsyncTcpClient.cs`（`FutureTcpClient`） | `tcp_client.py` / `transport.py` | `tcp_client.hpp/.cpp` |
| 对外主入口：连接、会话、组合指令 | `Codroid.cs` → `CodroidClient` | `client.py` → `CodroidClient` | `client.hpp/.cpp` |
| 寄存器 | `CodroidRegister.cs` | `register.py` | `register.hpp/.cpp` |
| IO | `CodroidIo.cs` | `io.py` | `io.hpp/.cpp` |
| 运动 | `RobotMotion.cs` | `motion.py` | `motion.hpp/.cpp` |
| 正逆解 | `RobotKinematics.cs` | `kinematics.py` | `kinematics.hpp/.cpp` |
| 全局变量 | `GlobalVariables.cs` | `global_variables.py` | `global_variables.hpp/.cpp` |
| 主题订阅类型与常量 | `CodroidPublish.cs` | `publish.py` | `publish.hpp` |
| CRI UDP 解析 | `CriRealtimePacketParser.cs` | `cri_parser.py` | `cri_parser.hpp/.cpp` |
| 离线轨迹生成（关节 / 笛卡尔；Cubic / Trapezoidal） | `Trajectory.cs`（`TrajectoryGenerator` 等） | `trajectory.py` | `trajectory.hpp/.cpp` |
| CRI 实时控制 UDP 周期下发 | `CriRealtimeDispatcher.cs` | `cri_dispatcher.py` | `cri_dispatcher.hpp/.cpp` |
| 共享 DTO / 常量 | `Define.cs`（`CommonResponse`、`CriRealTimeData` 等） | `types.py` / `models.py` | `types.hpp` |
| 控制器错误异常 | `CodroidCommandException.cs` | `exceptions.py` | `exceptions.hpp` |

**原则**：`CodroidClient`（或同名）只做 **编排** 与 **跨模块共用状态**（如连接、CRI 缓存）；具体协议条目放在对应模块，避免一个巨型文件。

### 3.1 Python SDK 推荐仓库结构

新建 Python SDK 时建议直接采用下列结构；公开 API 名称仍按 **§4.1** 的 C# 函数名（PascalCase）对齐，文件名按 Python 习惯用 snake_case。

```text
codroid-python/
├── AGENTS.md
├── SDK_API_AND_DESIGN.md
├── PROTOCOL_LINE_BY_LINE.md
├── TRAJECTORY_ALGORITHM.md
├── README.md
├── pyproject.toml
├── src/
│   └── codroid/
│       ├── __init__.py              # 导出 CodroidClient、常用 DTO / 常量
│       ├── client.py                # CodroidClient：连接、会话、跨模块薄封装
│       ├── transport.py             # TCP JSON 收发、id 匹配、publish/ty 分发
│       ├── types.py                 # CommonResponse、CriRealTimeData 等 DTO
│       ├── exceptions.py            # CodroidCommandException 等统一异常
│       ├── global_variables.py      # globalVar/*，命名校验、val 编码
│       ├── io.py                    # IOManager/*，批量读 + 单点便捷 API
│       ├── register.py              # RegisterManager/*，读写与对齐解析
│       ├── kinematics.py            # Robot/apostocpos、cpostoapos、relative pose
│       ├── motion.py                # jog、moveTo、move、倍率、负载、碰撞灵敏度
│       ├── publish.py               # PublishTopics、PublishNotification、订阅句柄
│       ├── cri_parser.py            # 308 字节 CRI UDP 解析，m/rad -> mm/deg
│       ├── trajectory.py            # TrajectoryGenerator、Cubic/Trapezoidal、Euler/SLERP
│       └── cri_dispatcher.py        # 64 字节 CommandData UDP 周期下发
├── examples/
│   ├── codroid_test.py              # 对齐 C# CodroidTest：global/kin/io/register/cri/motion
│   └── codroid_cri_test.py          # 对齐 C# CodroidCRITest：joint/cart/path
└── tests/
    ├── test_cri_parser.py           # 308 字节解析与单位换算
    ├── test_trajectory.py           # TRAJECTORY_ALGORITHM.md §9 回归向量
    └── test_payload_shapes.py       # ty/db 形状快照测试
```

Python 默认可先实现「同步外观 + 后台线程收 TCP/UDP」以贴近 C# 使用方式；若额外提供 asyncio 版本，放在单独命名空间或内部实现层，不改变默认公开函数名。

### 3.2 C++ SDK 推荐仓库结构

C++ 建议采用 `include/` + `src/` 分离，头文件名称与 §3 模块名对齐。公开类 / 函数名使用 C# 同名 PascalCase，类型可按 C++ 习惯放入 `codroid` 命名空间。

```text
codroid-cpp/
├── AGENTS.md
├── SDK_API_AND_DESIGN.md
├── PROTOCOL_LINE_BY_LINE.md
├── TRAJECTORY_ALGORITHM.md
├── README.md
├── CMakeLists.txt
├── include/
│   └── codroid/
│       ├── client.hpp               # CodroidClient
│       ├── tcp_client.hpp           # TCP JSON 收发、id 匹配、publish/ty 分发
│       ├── types.hpp                # CommonResponse、CriRealTimeData 等 DTO
│       ├── exceptions.hpp           # CodroidCommandException 等统一异常
│       ├── global_variables.hpp
│       ├── io.hpp
│       ├── register.hpp
│       ├── kinematics.hpp
│       ├── motion.hpp
│       ├── publish.hpp
│       ├── cri_parser.hpp
│       ├── trajectory.hpp
│       └── cri_dispatcher.hpp
├── src/
│   ├── client.cpp
│   ├── tcp_client.cpp
│   ├── global_variables.cpp
│   ├── io.cpp
│   ├── register.cpp
│   ├── kinematics.cpp
│   ├── motion.cpp
│   ├── publish.cpp
│   ├── cri_parser.cpp
│   ├── trajectory.cpp
│   └── cri_dispatcher.cpp
├── examples/
│   ├── codroid_test.cpp
│   └── codroid_cri_test.cpp
└── tests/
    ├── test_cri_parser.cpp
    ├── test_trajectory.cpp
    └── test_payload_shapes.cpp
```

C++ 推荐依赖 **standalone Asio + nlohmann/json + vcpkg**，但 `tcp_client` 对外行为必须与 C# `FutureTcpClient` 对齐：请求按整数 `id` 匹配响应，无整数 `id` 的下行 JSON 按 `ty` 分发给订阅回调。

#### 3.2.1 C++ 技术选型（推荐固定）

| 项 | 推荐 |
|----|------|
| C++ 标准 | C++20（若必须兼容旧编译器，可退到 C++17，但线程取消能力会弱一些） |
| 网络库 | standalone `asio`（不依赖 Boost） |
| JSON | `nlohmann-json` |
| 依赖管理 | `vcpkg` manifest 模式 |
| 构建 | CMake + `CMakePresets.json` |
| 命名空间 | `codroid` |
| 公开函数名 | 与 §4.1 C# 函数名完全一致（PascalCase，不加 Async 后缀） |

`vcpkg.json` 建议：

```json
{
  "name": "codroid-cpp",
  "version-string": "0.1.0",
  "dependencies": [
    "asio",
    "nlohmann-json"
  ]
}
```

`CMakeLists.txt` 基线：

```cmake
cmake_minimum_required(VERSION 3.20)
project(codroid_cpp LANGUAGES CXX)

add_library(codroid
    src/client.cpp
    src/tcp_client.cpp
    src/global_variables.cpp
    src/io.cpp
    src/register.cpp
    src/kinematics.cpp
    src/motion.cpp
    src/publish.cpp
    src/cri_parser.cpp
    src/trajectory.cpp
    src/cri_dispatcher.cpp
)

target_compile_features(codroid PUBLIC cxx_std_20)
target_include_directories(codroid PUBLIC include)

find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(codroid
    PUBLIC
        asio::asio
        nlohmann_json::nlohmann_json
)
```

`CMakePresets.json` 建议提供至少 Linux debug preset；Windows 可后续补：

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "linux-debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/linux-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "linux-debug",
      "configurePreset": "linux-debug"
    }
  ]
}
```

#### 3.2.2 C++ 实现计划（按此顺序落地）

1. **基础类型与异常**：先实现 `types.hpp` / `exceptions.hpp`，定义 `CommonResponse`、`CriRealTimeData`、`CodroidCommandException`，并固定 mm/deg 对外单位。
2. **TCP 传输层**：`TcpClient` 使用 `asio::io_context` + `tcp::socket` + 后台接收线程；发送 JSON 前用互斥锁或 strand 串行化；响应用 `std::promise<nlohmann::json>` / `std::future` 按整数 `id` 匹配；无整数 `id` 的 JSON 按 `ty` 分发 publish 回调。
3. **CodroidClient 门面**：实现 `Connect`、`Disconnect`、`NextId`、`SendCommand` 包装，以及 project / mode / system 的薄封装；公开函数名照 §4.1。
4. **协议模块**：按顺序补 `global_variables`、`io`、`register`、`kinematics`、`motion`、`publish`；`db` 字段形状逐条对照 `PROTOCOL_LINE_BY_LINE.md`。
5. **CRI UDP 解析**：`cri_parser` 严格按 `AGENTS.md §2.3.1` 的 308 字节偏移、小端、m/rad→mm/deg 转换实现；`CriRealTimeData` 快照更新需线程安全。
6. **轨迹算法**：`trajectory` 完全照 `TRAJECTORY_ALGORITHM.md` 实现 Cubic / Trapezoidal、Euler XYZ 外旋、SLERP 和多段拼接；先用 §9 回归向量验数值。
7. **实时控制下发**：`cri_dispatcher` 实现 64 字节 CommandData，小端写入 double；默认把 mm/deg 转为 m/rad；下发周期必须等于 `StartCriControl(durationMs)`。
8. **示例与测试**：`examples/codroid_test.cpp` 对齐 C# `CodroidTest`；`examples/codroid_cri_test.cpp` 对齐 C# `CodroidCRITest` 的 `joint/cart/path`；`tests/test_payload_shapes.cpp` 固化关键 ty/db 形状。

#### 3.2.3 C++ 关键实现约束

- **不要**把 Asio 的异步回调接口直接暴露成用户主 API；默认外观与 C# 一致：函数返回结果或抛异常，内部可用 future / promise 等待。
- TCP 写入必须串行化，避免两条 JSON 帧在 socket 中交错。
- 接收线程切 JSON 时需与 C# `FutureTcpClient` 行为一致：按完整 JSON 对象分割，然后先查整数 `id`，否则按 `ty` 推送。
- `Disconnect` 必须停止 CRI UDP 接收、停止 TCP 接收线程、关闭 socket，并唤醒/失败所有未完成 promise。
- 所有协议错误统一抛 `CodroidCommandException`，其中保留 request id、ty、controller err 和原始响应 JSON，便于现场排查。
- 测试中不得把 CRI UDP 原始 m/rad 暴露给默认模型；默认 `CriRealTimeData` 必须与 C# 一样是 mm/deg。

---

## 4. 公共 API 形态约定

1. **连接**：`Connect` → 可选高层 `ConnectRemoteAndSwitchOn`（自动→远程、上电等顺序与 C# 一致）。
2. **命令**：封装为 **async/协程/可选 future**，超时与错误语义与 C# 对齐（如 10 s 超时、`err` 映射为统一异常类型）。
3. **CRI**：提供 **线程安全快照**（等价于 `CriData` 克隆）与 **可选事件/回调**（等价于 `CriDataReceived`）。
4. **订阅**：返回 **可释放句柄**（等价 `PublishTopicSubscription.Dispose`）；**不向控制器发退订**；TCP 断开后订阅失效，需重连后重新订阅。

### 4.1 跨语言公开函数命名

三语言公开 API **统一使用 C# 当前函数名**：即便底层是异步/协程实现，函数名也 **不加 `Async` 后缀**。Python / C++ 不要自行改成 `xxx_async`、`xxxAsync` 或同步/异步两套命名；如某语言额外提供底层异步风格，可作为内部实现或单独命名空间，默认用户入口仍按下表。

| 功能组 | 公开函数名（Python / C++ 需与 C# 对齐） |
|--------|----------------------------------------|
| 连接 / 工程 | `Connect`, `Disconnect`, `ConnectRemoteAndSwitchOn`, `EnterRemoteScriptMode`, `RunScript`, `Run`, `RunByIndex`, `RunStep`, `PauseProject`, `ResumeProject`, `StopProject` |
| 全局变量 | `GetGlobalVars`, `GetGlobalVarsCatalog`, `SaveGlobalVar`, `SaveGlobalVars`, `RemoveGlobalVars` |
| 模式 / 系统 | `SwitchOn`, `SwitchOff`, `ToManual`, `ToAuto`, `ToRemote`, `EnterManualModeViaAuto`, `EnterRemoteModeViaAuto`, `ToSimulation`, `ToActual`, `StartDrag`, `StopDrag`, `ClearSystemError` |
| IO | `GetIoValues`, `GetDi`, `GetDo`, `GetAi`, `GetAo`, `SetDo`, `SetAo` |
| 寄存器 | `GetRegisterValue`, `GetRegisterValues`, `SetRegisterValue`, `SetExtendArrayType`, `RemoveExtendArray` |
| 运动学 | `AposToCpos`, `AposToCposPose`, `CposToApos`, `CposToAposJoints`, `CalculateRelativePose`, `CalculateRelativePoseResult` |
| 运动控制 | `StartJog`, `StopJog`, `JogHeartbeat`, `MoveTo`, `MoveToHeartbeat`, `SetManualMoveRate`, `SetAutoMoveRate`, `SetCollisionSensitivity`, `SetPayload`, `Move`, `PauseRobotMotion`, `ResumeRobotMotion`, `StopRobotMove` |
| CRI / 主题 | `StartCriDataPush`, `StopCriDataPush`, `StartCriControl`, `StopCriControl`, `SubscribePublishTopic` |
| 轨迹 / 实时下发 | `TrajectoryGenerator.Generate`, `CriRealtimeDispatcher.SendCommand`, `CriRealtimeDispatcher.SendTrajectory` |

---

## 5. 参考示例与验收（CodroidTest）

本仓库 `CodroidTest/Program.cs` 定义多段演示：**全局变量、正逆解、IO、寄存器、RobotStatus 订阅、CRI、S20 运动+CRI** 等。

其它语言 SDK 建议提供 **同名或同顺序的示例入口**（CLI 子命令可与注释中的 `global` / `cri` / `kin` / `io` / `register` / `robotstatus` / `motion` 对齐），便于三方对照行为。

### 5.1 共用测试数据（寄存器 · S20 运动）— C# / Python 对齐用

以下数值与 **`CodroidTest/Program.cs`** 中 **`RunRegisterTest`**、**`RunS20MotionCriCombo`** 内常量一致；实现其它语言示例时请 **照抄本节**，避免三套 SDK 各写一套数。**不包含** IO、全局变量、正逆解等段的专用数据（仍以源码为准）。若改测试程序，须 **同步改本节**。

#### 寄存器联调

| 区段 | 地址列表 | 写入目标值 | 清零 |
|------|-----------|------------|------|
| A | `9032, 9033, 9034, 9035` | 全部写 **1** | 全部写 **0** |
| B（整型） | `49100, 49102, 49104` | 全部写 **520** | 全部写 **0** |
| C（浮点） | `49300, 49302, 49304` | 全部写 **520.52** | 全部写 **0.0** |

流程语义：每组均为「批量读 → 按上表写入 → 逐个读回 → 按清零列写回」。

#### 运动联调（机型 **S20-180-ECO_V2**，`motion` / `s20` / `movecri`）

**movJ**（`MoveKinds.MovJ`）：`Speed = 40`，`Acc = 100`，`Blend = 25`。

| 步骤 | 关节目标 `jp`（六轴，**度**） |
|------|------------------------------|
| 目标 1 | `[0, 0, 90, 0, 90, 0]` |
| 目标 2 | `[0, 0, 0, 0, 0, 0]` |
| 目标 3 | `[0, 0, 90, 0, 90, 0]` |

**movL**（`MoveKinds.MovL`）：`Speed = 150`，`Acc = 500`，`Blend = 25`。笛卡尔 `cp` 为 `[x, y, z, rx, ry, rz]`：**mm + 度**。

- **文档参考起点 cp**（仅打印说明，**不单独下发**）：`[927.504, 214.495, 898.998, 179.999, 0.0, -90.0]`
- **P1**：`[927.511, 214.489, 486.524, 179.999, 0.0, -89.999]`
- **P2**：`[927.516, -160.239, 486.534, 180.0, 0.0, -89.999]`
- **P3**：`[927.515, -160.238, 1111.244, -179.999, 0.0, -89.999]`
- **P4**：`[927.512, 351.971, 1111.249, -179.998, 0.0, -89.999]`

**逆解参考关节 `Rj`**：`movL` 指令中的 `rj` **不是**上表常量，而是下发前瞬间从 **CRI `JointPosition`**（或等价实时源）读取的六轴角（**度**）。Python/C# 应对齐「同一时刻快照」再发 `Move`。

**CRI 本机绑定（示例常量，现场请改）**：`192.168.8.150:18888`（与 `Program.cs` 中 `localUdpIp` / `localUdpPort` 一致）。

### 5.2 CRI 实时控制示例数据（`CodroidCRITest`）— 三段测试点

以下为 **`CodroidCRITest/Program.cs`** 中三段测试的测试点与轨迹规划参数；与 **§6** 的工作流契约配套使用。Python/C++ 实现等价示例时请 **照抄本节**。所有段都有 3 秒倒计时 + Ctrl+C 兜底，起点取自 **CRI 实时回传**（`CriData.JointPosition` / `CriData.TcpPose`）。

**全局参数（三段共用）**：

- 实时控制：`filterType=1`，`durationMs=4`，`startBuffer=5`
- 轨迹采样：`FrequencyHz = 1000 / durationMs = 250`
- UDP 下发周期：`periodMs = 4`
- 控制器 IP / 本机 IP（示例常量，现场覆盖）：`192.168.8.136` / `192.168.8.150:18888`

#### `joint` — 关节段

| 参数 | 值 |
|------|-----|
| `Space` | `Joint` |
| `Profile` | `Cubic` |
| `Speed` | **30 deg/s**（位移最大轴） |
| `Acceleration` | 120 deg/s²（仅 `Trapezoidal` 时生效，此段未用） |

| 步骤 | 六轴关节角（**度**） |
|------|---------------------|
| 起点 | **CRI 当前 `JointPosition`** |
| P1 | `[0, 0, 90, 0, 90, 0]` |
| P2 | `[0, 0, 0, 0, 0, 0]` |
| P3 | `[0, 0, 90, 0, 90, 0]` |

> 与 §5.1 的 `movJ` 三段目标点一致；区别是这里走 CRI 实时控制（连续插值下发），不是 `Robot/move` 指令。

#### `cart` — 笛卡尔平移段（YZ 平面矩形，姿态保持）

| 参数 | 值 |
|------|-----|
| `Space` | `Cartesian` |
| `Profile` | `Trapezoidal`（含匀速段） |
| `Speed` | **80 mm/s**（线速度，匀速段恒定） |
| `Acceleration` | **400 mm/s²** |

| 步骤 | 相对偏移 `(dx, dy, dz)` (mm) | 姿态 |
|------|------------------------------|------|
| 起点 | **CRI 当前 `TcpPose`**（含起始 `(rx,ry,rz)`） | — |
| P1 | `(0,    0, -200)` 向下 | 保持 |
| P2 | `(0, -200,    0)` y- | 保持 |
| P3 | `(0,    0, +200)` z+（恢复 z） | 保持 |
| P4 | `(0, +200,    0)` y+ → 回到原点 | 保持 |

> 整段 `(rx, ry, rz)` 维持起点不变；SLERP 在等姿态间退化为恒等映射。

#### `path` — 自定义全位姿路径（4 点 + 回 home）

| 参数 | 值 |
|------|-----|
| `Space` | `Cartesian` |
| `Profile` | `Trapezoidal` |
| `Speed` | **80 mm/s** |
| `Acceleration` | **400 mm/s²** |

| 步骤 | `[x, y, z, rx, ry, rz]`（mm + **度**，固定欧拉 XYZ 外旋） |
|------|------------------------------------------------------------|
| 起点 | **CRI 当前 `TcpPose`** |
| P1 | `[1139.996,  214.490, 899.010, -91.506, -0.001,  -89.999]` |
| P2 | `[1139.994, -222.730, 899.022, -91.506, -0.002, -136.466]` |
| P3 | `[ 915.480,  -73.000, 599.316, 166.910, -5.170,  -90.726]` |
| P4 = home | `[ 927.505,  214.495, 898.994, 180.000,  0.000,  -90.000]` |

> P4（home）= **§5.1 movL 文档参考起点 cp** 的实测值（机型 S20-180-ECO_V2）。理论上即「`cart` 段走完后回到原点」的位姿，但实际起点仍以 CRI 为准。
> P2→P3 的欧拉数值看起来 `rx` 跨过 ±180° 边界，SLERP 走 SO(3) 测地线（取共轭半球）实际旋转 ~104°，不会绕远路。

---

## 6. CRI 实时控制工作流契约

各语言 SDK 应提供与 C# 同形态的「**离线生成轨迹 → UDP 周期下发**」组件；调用顺序、参数、单位与下文一致。C# 实现细节见 **`SDK_API_AND_DESIGN.md` §7**，参考程序见 **`CodroidCRITest/Program.cs`**。

### 6.1 完整时序

```
TCP Connect (9001)
  → ConnectRemoteAndSwitchOn                     切自动→远程，上电使能
  → CRI/StartDataPush  duration=100ms,
                       mask=0xFFFF,
                       highPercision=true        本机 UDP 收 308 字节包
  ──── 等首帧 CRI（TimestampMs > 0）作为轨迹起点
  ──── 离线生成完整轨迹（关节 deg / 笛卡尔 mm + deg）
  → CRI/StartControl  filterType=1,
                      duration=4ms,
                      startBuffer=5
  ──── 轮询 CriData.RealTimeControlMode == true（建议超时 3 s）
  → CommandData × N（UDP 9030, 64 字节, 周期 = StartControl.duration）
  → CRI/StopControl                              务必在 finally 中
  → CRI/StopDataPush
TCP Disconnect
```

### 6.2 启动参数（与 C# 默认对齐）

| 参数 | 默认 | 范围 / 约束 |
|------|------|-------------|
| `filterType` | **1**（平均滤波） | 0~3：0=关闭，1=平均，2=二阶低通，3=椭圆 |
| `duration` | **4 ms** | 1~16，且必须能整除 1000（建议 1/2/4/5/8/10） |
| `startBuffer` | **5** | 1~100 |

下发周期 **必须等于** `StartControl.duration`，否则与控制器节奏失配。

### 6.3 CommandData 包（UDP 9030，64 字节，小端）

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0..7 | `timestamp` | Int64 | 保留 0 |
| 8..55 | `position[6]` | Float64×6 | 关节位置（rad）或末端位姿（m + rad） |
| 56 | `type` | UInt8 | `0`=关节, `1`=末端 |
| 57..63 | `nc[7]` | UInt8×7 | 保留 |

**单位**：与 §2.3 CRI UDP 数据流一致——**线上 SI（rad / m）**；上层调用约定 **mm + deg**，SDK 在下发前做 `deg→rad`、`mm→m` 换算（与 `CriRealtimePacketParser` 反向，符号对称）。各语言实现必须保持单位约定一致，避免与 C# 混用同字段名却不同单位。

### 6.4 兜底语义

`StopCriControl` / `StopCriDataPush` / TCP `Disconnect` **必须** 放在 `finally`（或等价兜底）。任何异常 / Ctrl+C / 超时都不能把控制器留在 `RealTimeControlMode=true` 的状态，否则需要现场人工介入。

### 6.5 轨迹生成约定（C# 基线，跨语言对齐）

- **关节**：取位移最大的关节确定段时长，其它轴按比例同步（同时启动 / 同时到达）。
- **笛卡尔位置**：直线插值；速度规划默认 `Cubic` 平滑，需要严格匀速段时使用 `Trapezoidal`。
- **笛卡尔姿态**：四元数 SLERP（约定 `q = qz · qy · qx`，对应 **固定欧拉 XYZ 外旋**）；`dot < 0` 时取共轭半球，确保走 SO(3) 上最短路径。
- **纯姿态**（线位移 ≈ 0）：必须以「时长」给定，不接受线速度。
- **多段**：相邻段端点共享（端点处速度为 0），拼接时跳过后续段首点避免重复采样。

> **完整算法（含伪代码、退化分支、万向锁处理、回归测试向量）见 `TRAJECTORY_ALGORITHM.md`**。Python / C++ 实现 `TrajectoryGenerator` 时直接照该文件落地，本节仅作高层契约。

---

## 7. 变更流程（避免三套漂移）

1. **协议或默认值变更**（端口、UDP 长度、`tc`、CRI 固定参数、实时控制参数范围等）：先改 **本仓库 C# 与本文档**，再同步 Python/C++。
2. **CodroidTest 常量变更**：同步更新上文 **§5.1** 中的寄存器地址与运动数表。
3. **新增指令**：在 **对应功能模块** 增加方法，并在 `CodroidClient` 暴露薄封装（若 C# 如此）；其它语言同路径增加。
4. **命名**：对外类名优先 **`CodroidClient`**、**`CriRealTimeData`**、**`CommonResponse`**、**`TrajectoryGenerator`**、**`CriRealtimeDispatcher`** 等与 C# 一致或明确别名表（在各自 README 列一行对照）。

---

## 8. 许可证

与仓库根目录 **`LICENSE`**（MIT）一致；衍生 SDK 建议使用相同许可证以便集成商合规统一。

---

## 9. 给 AI / 代理的短指引

在 Python 或 C++ 仓库中工作时：先读本文件；实现新接口前打开 **本仓库** 对应 `.cs` 文件核对 **JSON `ty` 字符串、字段名、超时与异常语义**；寄存器与 S20 运动示例的 **常量表以 §5.1 为准**；CRI 实时控制工作流以 **§6** 为准；整体示例行为以 **`CodroidTest`** / **`CodroidCRITest`** 为准。
