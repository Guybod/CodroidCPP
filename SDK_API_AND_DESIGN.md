# Codroid C# SDK — API 与收发 / 设计说明

本文描述 **`CodroidSDK`** 中对外 API、与控制器之间的 **发送 / 接收** 路径，以及 **C# 分层设计**（为何 IO「批量请求 + 分拆解析」等）。  
协议字段细节以控制器文档为准；此处与源码 **`CodroidClient`**、`FutureTcpClient` 行为对齐。

---

## 1. 通信模型总览

| 通道 | 用途 | 说明 |
|------|------|------|
| **TCP**（默认 **9001**） | JSON 指令与响应、主题订阅 | UTF-8；请求带整数 **`id`**；部分下行报文 **无 `id`**，仅靠 **`ty`** 区分推送 |
| **UDP** | CRI 实时二进制 | 由 TCP **`CRI/StartDataPush`** 约定推送到本机 `ip:port`；SDK 固定 **308 字节** 包（六轴、无附加轴），解析见 `CriRealtimePacketParser` |

**固件要求**：本 SDK 对外封装的 **全部 API** 均要求控制器固件 **≥ 2.3.3.43**（C++ 常量 `MinControllerFirmware`）。

---

## 2. 发送（TCP）

### 2.1 请求帧格式（与控制器配对）

绝大多数指令序列化为一条 JSON，形如：

```json
{ "id": <int>, "ty": "<指令路径>", "db": <任意 JSON：对象 / 数组 / 字符串 / …> }
```

- **`id`**：由 `CodroidClient` 内部 **`NextId()`** 单调递增，用于匹配响应。
- **`ty`**：协议指令名字符串（见下文 API 表）。
- **`db`**：业务体；部分指令使用 **空字符串 `""`**（与协议示例一致），通过 `SendCommandEmptyDb` 发送。

实现位置：`FutureTcpClient.SendCommand`（`AsyncTcpClient.cs`）：`JsonSerializer.Serialize` 后写入 `NetworkStream`。

### 2.2 发送串行化

`FutureTcpClient` 使用 **`SemaphoreSlim _tcpWriteGate`**：**同一 TCP 连接上写流串行**，避免 `SendCommand` 与 **订阅帧**（见 §3.3）交错损坏 JSON。

### 2.3 主题订阅帧（特殊：无 `id`）

首次对某主题注册回调时，额外发送：

```json
{ "ty": "<publish/…>", "tc": <毫秒> }
```

**不含 `id`、`db`**；**不等待**控制器对该帧的「响应」。推送到达时在接收线程按 **`ty`** 分发给回调。默认 **`tc`**：`PublishSubscribeDefaults.TcMilliseconds`（100）。

---

## 3. 接收（TCP）

### 3.1 接收循环

`FutureTcpClient` 在连接后启动 **`ReceiveWorker`**：从流读字节累加到缓冲区，按 **`{` / `}`** 括号配对切出 **完整 JSON 文本**（简化版「流上 JSON 切片」，假定负载内花括号平衡）。

### 3.2 两条分发路径

对每条完整 JSON，**`ProcessSingleMessage`**：

1. **若存在数值类型 `id`**，且该 `id` 在 `_promises` 中有等待任务 → **`SetResult(rawJson)`**，唤醒对应的 `SendCommand`。
2. **否则** 若有 **`ty`** → 构造 **`PublishNotification`（Ty, Db, RawJson）**，在线程池触发该 `ty` 下注册的所有 **`Action<PublishNotification>`**。

因此：**带 `id` 的是请求-响应配对**；**无整数 `id` 的多为 `publish/...` 推送**。

### 3.3 响应交付与错误

`SendCommand` 将原始字符串反序列化为 **`CommonResponse`**（`Define.cs`：`id`, `ty`, `db`, `err`）。

- 若 **`err` 非空** → 抛出 **`CodroidCommandException`**（附带 `RequestId`、`CommandType`、`ControllerError`、`Response`）。
- **10 秒内**未收到匹配 `id` → **`TimeoutException`**。

---

## 4. `CodroidClient` 的职责（门面层）

`Codroid.cs` 中的 **`CodroidClient`**：

- 持有 **`FutureTcpClient`**，对外暴露 **连接、工程、全局变量、模式、IO、寄存器、运动学、运动、CRI、主题订阅** 等方法。
- 负责 **`id` 生成**、部分 **`db` 拼装**、以及 **CRI UDP** 生命周期（`StartCriDataPush` / `StopCriDataPush`、后台接收、`Data` / `CriData`、事件 **`CriDataReceived`**）。
- **不把** JSON 读写细节泄漏给调用方；复杂响应解析拆到 **静态 Parser / Validation** 类（见 §6）。

---

## 5. `CodroidClient` API ↔ 协议 `ty` 一览

下列表中 **`db`** 为简要形状；精确字段以控制器文档与源码匿名对象为准。

### 5.1 连接与会话

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `Connect()` | （仅 `FutureTcpClient.Connect`，无 JSON） | — |
| `Disconnect()` | — | 停止 CRI UDP、`FutureTcpClient.Disconnect` |

### 5.2 工程（project）

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `EnterRemoteScriptMode()` | `project/enterRemoteScriptMode` | `{}` |
| `RunScript(main, subThreads?, subPrograms?, interrupts?, vars?)` | `project/runScript` | `{ scripts: { main, subThreads?, subPrograms?, interrupts? }, vars? }`；空映射不发字段 |
| `Run(projectID)` | `project/run` | `{ id: projectID }` |
| `RunByIndex(index)` | `project/runByIndex` | 索引 |
| `RunStep(projectID)` | `project/runStep` | `{ id: projectID }` |
| `PauseProject()` | `project/pause` | `{}` |
| `ResumeProject()` | `project/resume` | `{}` |
| `StopProject()` | `project/stop` | `{}` |

### 5.3 全局变量（globalVar）

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `GetGlobalVars()` | `globalVar/getVars` | `{}` |
| `GetGlobalVarsCatalog()` | 同上 | 返回后对 `db` 调 **`GlobalVarCatalogParser.Parse`** |
| `SaveGlobalVar` / `SaveGlobalVars` | `globalVar/saveVars` | 对象：键为变量名，值为 `{ val: 字符串, nm?: 备注 }`（`val` 由 **`GlobalVarValueFormatter.ToWireString`** 生成） |
| `RemoveGlobalVars(names)` | `globalVar/removeVars` | 字符串数组 |

命名校验：**`GlobalVarNaming.Validate`**；原始 JSON 字面量值使用 **`GlobalVarRawJson`** 避免双重转义。

### 5.4 机器人模式 / 电源 / 拖拽 / 系统

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `SwitchOn()` | `Robot/switchOn` | `{}` |
| `SwitchOff()` | `Robot/switchOff` | `""` |
| `ToManual()` | `Robot/toManual` | `""` |
| `ToAuto()` | `Robot/toAuto` | `""` |
| `ToRemote()` | `Robot/toRemote` | `""` |
| `EnterManualModeViaAuto()` | 组合调用（源码顺序） | — |
| `EnterRemoteModeViaAuto()` | 组合调用 | — |
| `ToSimulation()` | `Robot/toSimulation` | `""` |
| `ToActual()` | `Robot/toActual` | `""` |
| `StartDrag()` | `Robot/startDrag` | `""` |
| `StopDrag()` | `Robot/stopDrag` | `""` |
| `ClearSystemError()` | `System/clearError` | `""` |

### 5.5 IO（IOManager）

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `GetIoValues(pins)` | `IOManager/GetIOValue` | **`IoGetResponseParser.BuildGetQuery`**：对象数组 `[{ type, port }, …]` |
| `GetDi` / `GetDo` / `GetAi` / `GetAo` | 同上（内部单点列表） | 返回的 **`CommonResponse`** 再经 **`ParseDigital` / `ParseAnalog`** 取单一端口 |
| `SetDo(port, value)` | `IOManager/SetIOValue` | `{ type: "DO", port, value }`（仅 0/1） |
| `SetAo(port, value)` | `IOManager/SetIOValue` | `{ type: "AO", port, value }` |

常量 **`IoPortKind`**：`DI` / `DO` / `AI` / `AO`。

### 5.6 寄存器（RegisterManager）

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `GetRegisterValues(addresses)` | `RegisterManager/GetRegisterValue` | **地址 int 数组** |
| `GetRegisterValue(address)` | 同上（单元素数组） | — |
| `SetRegisterValue(address, int)` | `RegisterManager/SetRegisterValue` | `{ address, value }` |
| `SetRegisterValue(address, double)` | 同上 | `{ address, value }` |
| `SetExtendArrayType(index, type)` | `RegisterManager/setExtendArrayType` | `{ index, type }` |
| `RemoveExtendArray(index)` | `RegisterManager/removeExtendArray` | `{ index }` |

批量读返回：`RegisterResponseParser.ParseAligned` 校验 **`db` 数组长度与顺序**与请求地址一致。

### 5.7 运动学（Robot）

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `AposToCpos` | `Robot/apostocpos` | `{ jp, coor, tool, ep }` |
| `AposToCposPose` | 同上 | 成功后 **`RobotKinematics.ParseDbAsVector6(resp.db)`** |
| `CposToApos` | `Robot/cpostoapos` | 笛卡尔 + 参考关节等（见源码） |
| `CposToAposJoints` | 同上 | `ParseDbAsVector6` |
| `CalculateRelativePose` | `Robot/calculateRelativePose` | 相对位姿参数 |
| `CalculateRelativePoseResult` | 同上 | `ParseDbAsVector6` |

六维校验：**`RobotKinematics.RequireVector6`**。

### 5.8 运动控制（Robot）

| 对外 API | 协议 `ty` | `db` 说明 |
|----------|-----------|-----------|
| `StartJog(parameters)` | `Robot/jog` | `RobotJogParameters` 序列化 |
| `StopJog()` | `Robot/stopJog` | `""` |
| `JogHeartbeat()` | `Robot/jogHeartbeat` | `""` |
| `MoveTo(kind, target?)` | `Robot/moveTo` | 见 `RobotMotion.cs` 构建 |
| `MoveToHeartbeat()` | `Robot/moveToHeartbeat` | `""` |
| `SetManualMoveRate` / `SetAutoMoveRate` | `Robot/setManualMoveRate` / `setAutoMoveRate` | 百分比等 |
| `SetCollisionSensitivity` | `Robot/setCollisionSensitivity` | 固件 ≥ 2.3.3.43；校验 0~100 后下发 |
| `SetPayload` | `Robot/setPayload` | 运行时切换当前负载；固件 ≥ 2.3.3.43 |
| `GetRobotParameters` | `Robot/GetRobotParameter` | 读取设置界面参数；固件 ≥ 2.3.3.43 |
| `SetDefaultPayloadId` | `Robot/SaveRobotParameter` | `db={defaultPayloadId}`；固件 ≥ 2.3.3.43 |
| `SetDefaultToolId` | `Robot/SaveRobotParameter` | `db={defaultToolId}` |
| `SetDefaultUserCoordinateId` | `Robot/SaveRobotParameter` | `db={defaultCoordinateId}` |
| `SaveToolFrames` / `SetToolFrame` | `Robot/SaveRobotParameter` | `db={Tool:[...]}`；单槽先 Get 再 patch |
| `SavePayloadFrames` / `SetPayloadFrame` | `Robot/SaveRobotParameter` | `db={Payload:[...]}` |
| `SetUserCoordinateFrame` | `Robot/SaveRobotParameter` | `db={Coordinate:[...]}` |
| `Move(instructions)` | `Robot/move` | **`MotionCommandJson.SerializeMoveInstructions`** → `JsonElement` |
| `PauseRobotMotion()` | `Robot/pause` | `""` |
| `ResumeRobotMotion()` | `Robot/resume` | `""` |
| `StopRobotMove()` | `Robot/stopMove` | `""` |

运动类型字符串：**`MoveKinds`**（`movJ` / `movL` / …）；指令体类型定义在 **`RobotMotion.cs`**。

### 5.9 CRI 数据推送 + 实时控制（TCP + UDP）

| 对外 API | 协议 `ty` | 说明 |
|----------|-----------|------|
| `StartCriDataPush(udpIp, udpPort)` | `CRI/StartDataPush` | 先起本地 UDP，再发 `db`：`ip, port, duration, highPercision, mask`（SDK 内常量；注意 `highPercision` 拼写按协议原文） |
| `StopCriDataPush(udpIp?, udpPort?)` | `CRI/StopDataPush` | 可选带 `ip/port`；**始终**关闭本地 UDP |
| `StartCriControl(filterType=1, durationMs=4, startBuffer=5)` | `CRI/StartControl` | `db: { filterType, duration, startBuffer }`；客户端预校验 `filterType ∈ [0,3]`、`durationMs ∈ [1,16] ∧ 1000 % durationMs == 0`、`startBuffer ∈ [1,100]` |
| `StopCriControl()` | `CRI/StopControl` | `db = ""`（与协议原文一致） |

UDP 收到 **308 字节** → **`CriRealtimePacketParser.Parse`** → 更新 **`_criData`**（锁保护）→ 触发 **`CriDataReceived`**。只读快照：**`CriData`**（克隆）。

实时控制 UDP 下发由独立类 **`CriRealtimeDispatcher`** 提供（不在 `CodroidClient` 上挂载，保持门面层与实时管线解耦），详见 **§7**。

### 5.10 主题推送（15.x）

| 对外 API | 协议行为 | 说明 |
|----------|-----------|------|
| `SubscribePublishTopic(topicTy, handler, tcMilliseconds?)` | 首次对该 `topicTy` 发 **`{ ty, tc }`** | 回调接收 **`PublishNotification`**；返回 **`PublishTopicSubscription`**，`Dispose` 仅移除回调，**不发退订** |

常用 **`ty`** 常量：**`PublishTopics`**（如 `publish/RobotStatus`）。

---

## 6. C# 设计拆分（为何这样组织）

### 6.1 传输层：`FutureTcpClient`

- **单一职责**：TCP 连接、**按消息分割 JSON**、`id` 与 Promise 配对、**订阅回调表**、写锁。
- **不知情**：不关心 `IOManager` / `RegisterManager` 等业务含义。

### 6.2 门面：`CodroidClient`

- **编排**：组合「发指令 + 可选解析」；如 `GetGlobalVarsCatalog` = `GetGlobalVars` + `GlobalVarCatalogParser.Parse`。
- **状态**：连接内的 **`id`**、CRI **`UdpClient`** 与 **`CriRealTimeData`** 缓存。

### 6.3 IO：**批量请求 + 响应分拆解析**

- **原因**：协议 **`GetIOValue`** 天然支持一次查询多个 `(type, port)`，减少往返。
- **拆分方式**：
  - **编码**：**`IoGetResponseParser.BuildGetQuery`** 把 `List<(string Type, int Port)>` 编成 `db` 数组。
  - **解码**：响应 **`db`** 为数组，逐项匹配 **`type` + `port`**：
    - **`ParseDigital`**：`DI`/`DO` 的 `value` 规范为 **0/1**（兼容 bool、数字、字符串）。
    - **`ParseAnalog`**：`AI`/`AO` 读 **`double`**。
  - **便捷 API**：`GetDi` 等 = 「单元素批量请求 + 对应 Parse」，代码重复少、行为一致。

相关文件：**`CodroidIo.cs`**；**`CodroidClient`** 中 IO 方法只是薄封装。

### 6.4 寄存器：**对齐解析 + 类型安全的值读取**

- **`RegisterReadValue`**：保留原始 **`JsonElement Value`**，提供 **`GetInt32` / `TryGetInt32` / `GetDouble`**，兼容控制器返回数字或字符串等形式。
- **`RegisterResponseParser.ParseAligned`**：强制 **响应数组长度与请求地址列表一致**，且逐项 **`address`** 与请求下标一致，避免静默错位。
- **校验**：**`RegisterValidation`**（非空列表、扩展数组 index/type）。

相关文件：**`CodroidRegister.cs`**。

### 6.5 全局变量：**命名 / 取值编码 / 目录解析分离**

- **`GlobalVarNaming`**：Lua 风格命名 + 保留字表。
- **`GlobalVarValueFormatter`**：把 C# 对象转为写入协议的 **`val` 字符串**；**`GlobalVarRawJson`** 绕过二次 JSON 转义。
- **`GlobalVarCatalogParser`**：把 **`getVars`** 的 `db` 对象转成 **`Dictionary<string, GlobalVarCatalogEntry>`**。

相关文件：**`GlobalVariables.cs`**。

### 6.6 运动学：**共享六维解析**

- **`RobotKinematics.RequireVector6`**、**`ParseDbAsVector6`**：正逆解、相对位姿等多条接口共用，避免各处手写 `db` 数组解析。

相关文件：**`RobotKinematics.cs`**。

### 6.7 运动指令：**强类型模型 → JSON**

- **`MoveInstruction`**、**`MoveTargetPoint`**、**`MoveKinds`** 等在 **`RobotMotion.cs`** 定义。
- **`MotionCommandJson.SerializeMoveInstructions`**：统一序列化为控制器期望的 **`Robot/move`** 负载结构，避免 `CodroidClient` 内手写大块匿名对象。

### 6.8 CRI：**解析与客户端状态分离**

- **`CriRealtimePacketParser`**：纯函数，`byte[]` → **`CriRealTimeData`**（含 **rad/m → 度/mm** 换算）。
- **`CodroidClient`**：套接字、线程、锁、事件；**`Data`** 为热数据引用，**`CriData`** 为克隆快照。

### 6.9 异常

- **`CodroidCommandException`**：区分「控制器 **`err`**」与包装后的其它失败；可选附带完整 **`CommonResponse`** 便于调试。

相关文件：**`CodroidCommandException.cs`**。

---

## 7. 实时控制：轨迹生成 + UDP 下发

C# 实时控制由 **两个独立类** 组成，**离线生成轨迹 → UDP 周期下发**。任一段都可单独调用，便于在控制器外做仿真或单元测试。`CodroidClient` 不直接嵌入这两类，保持门面层与实时管线解耦。

> **算法层细节**（Cubic / Trapezoidal 形状、退化分支、Euler↔Quaternion、SLERP、多段拼接、回归测试向量）抽到独立文件 **`TRAJECTORY_ALGORITHM.md`**，跨语言共用。本节仅描述 C# 类形态与边界。

### 7.1 `TrajectoryGenerator`（`CodroidSDK/Trajectory.cs`）

```csharp
public static IEnumerable<TrajectoryPoint> Generate(
    IReadOnlyList<double> start,
    IReadOnlyList<double> target,
    TrajectoryRequest request);
```

入参字段（`TrajectoryRequest`）：

| 字段 | 类型 | 含义 |
|------|------|------|
| `Space` | `TrajectorySpace.Joint` / `Cartesian` | 关节空间 6 维（deg）或 `[x,y,z,rx,ry,rz]`（mm + deg，**固定欧拉 XYZ 外旋**） |
| `FrequencyHz` | `double` | 采样频率，建议与 `StartCriControl.durationMs` 倒数对齐 |
| `Speed` / `DurationSeconds` | `double?` | 二选一；关节 `Speed` 单位 deg/s（作用于位移最大轴），笛卡尔 `Speed` 单位 mm/s（线位移） |
| `Profile` | `Cubic` / `Trapezoidal` | 三次平滑（不严格匀速） vs 梯形（含匀速段） |
| `Acceleration` | `double` | 仅 `Trapezoidal` 使用；关节 deg/s²、笛卡尔 mm/s² |

设计要点：

- **关节空间**：取位移最大的关节确定段时长，其它轴按比例缩放，保证「同时启动、同时到达」，不出现某轴提前到位。
- **笛卡尔空间**：位置走直线，姿态用 SLERP（四元数 `q = qz·qy·qx`）。位置 / 姿态使用 **同一时间标度**，故梯形匀速段下 TCP 线速度恒定，角速度按比例同步。
- **SLERP 走最短半球**：`dot < 0 → q1 = -q1`，避免欧拉数值跨过 ±360° 时绕远路；`dot > 0.9995` 退化为 nlerp 防止 `sin θ → 0` 除零。
- **纯姿态运动**（线位移 `D < 1e-9`）：必须用 `DurationSeconds`，用 `Speed` 会抛 `ArgumentException`，提示无法从线速度推导时长。
- **梯形规划自适应**：`Speed` 模式下若 `2·da ≥ D` 退化为三角形（峰值 `√(a·D)`，无匀速段）；`DurationSeconds` 模式下若给定加速度不足以在 `T` 内走完 `D`，自动用等效加速度走三角形。
- **欧拉↔四元数**：`EulerXyz` 工具内嵌于本文件（`internal`），双精度，避免 `System.Numerics.Quaternion` 单精度累计误差；万向锁附近 `|sin β| > 0.999999` 固定 `rz=0` 求 `rx`，防数值爆炸。

### 7.2 `CriRealtimeDispatcher`（`CodroidSDK/CriRealtimeDispatcher.cs`）

```csharp
public CriRealtimeDispatcher(string controllerIp, int controllerUdpPort = 9030, bool convertToSi = true);

public Task SendCommand(IReadOnlyList<double> position6, TrajectorySpace space, CancellationToken ct = default);
public Task SendTrajectory(IEnumerable<TrajectoryPoint> trajectory, TrajectorySpace space, int periodMs, CancellationToken ct = default);
```

数据包布局（64 字节，小端）：

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0..7 | `timestamp` | Int64 | 保留 0 |
| 8..55 | `position[6]` | Float64×6 | 关节（rad）或末端位姿（m + rad） |
| 56 | `type` | UInt8 | `0`=关节, `1`=末端 |
| 57..63 | `nc[7]` | UInt8×7 | 保留 |

设计要点：

- **`convertToSi`（默认 true）**：发送前 `deg→rad`、`mm→m`，与 `CriRealtimePacketParser` 的反向换算成对——上层始终用 deg/mm，SDK 边界做单位转换。若实测固件用 mm/deg，构造时传 `convertToSi: false`。
- **小端写入**：`BinaryPrimitives.WriteDoubleLittleEndian`，规避 `BitConverter` 在某些平台的字节序差异。
- **节奏**：`PeriodicTimer(TimeSpan.FromMilliseconds(periodMs))`；**首帧立即发**，之后每个 tick 发一帧；若某帧业务耗时超过周期，`PeriodicTimer` 会立即追发追平节奏，不会漂移累积。
- **`periodMs ∈ (0, 1000]`**：与 `StartCriControl.durationMs` 同区间。
- **取消语义**：`CancellationToken` 贯穿；`Dispose` 关闭 UDP 套接字，再次调用任意 Send 抛 `ObjectDisposedException`。

### 7.3 端到端编排（参考 `CodroidCRITest/Program.cs`）

```
ConnectRemoteAndSwitchOn          // TCP + 切远程 + 上电
  → StartCriDataPush(localIp, port)    // 开 UDP 推送，回读起点位姿
  ── 等首帧 CRI（CriData.TimestampMs > 0）
  ── 读 CriData.JointPosition / TcpPose 作为轨迹起点
  ── TrajectoryGenerator.Generate(...) 离线生成全轨迹
  → StartCriControl(1, 4, 5)      // filterType / durationMs / startBuffer
  ── 轮询 CriData.RealTimeControlMode == true（超时 3s）
  → new CriRealtimeDispatcher(robotIp).SendTrajectory(traj, space, periodMs=4)
  ── finally:
       StopCriControl()
       StopCriDataPush(localIp, port)
       robot.Disconnect()
```

**多段拼接**：相邻段 endpoint 共享，需在拼接处 **跳过后续段的首点** 避免重复采样。`CodroidCRITest/Program.cs::GenerateMultiSegment` 是参考实现。

**段间衔接**：`Cubic` 与 `Trapezoidal` 在端点处速度均为 0，故多段直接首尾相连即可，无需手工插入停顿；不同段的 `Profile` / `Speed` 可不同。

**测试输入数据**：`CodroidCRITest` 三段（`joint` / `cart` / `path`）使用的具体测试点、Speed/Acceleration/Profile 等 **示例常量统一放在 `AGENTS.md` §5.2**，避免在两份文档之间出现常量分叉；C# 与 Python/C++ 实现等价示例时直接对照 §5.2 即可。

跨语言契约见 **`AGENTS.md` §6**。

---

## 8. 与其它文档的关系

- **跨语言常量对齐**（寄存器地址、S20 运动数表、CRI 实时控制契约、CodroidCRITest 测试点）：**`AGENTS.md`** §5.1 / §5.2 / §6。
- **轨迹生成算法（跨语言一致基线）**：**`TRAJECTORY_ALGORITHM.md`**——含伪代码、数值退化分支与回归测试向量，Python / C++ 实现 §7.1 时按该文件落地。
- **协议条目逐条对照**：**`PROTOCOL_LINE_BY_LINE.md`**。
- **仓库使用说明**：**`README.md`**。

若控制器协议升级导致 **`ty` 或 `db` 形状变更**，应同时更新 **`CodroidClient`**、本文 **§5**，以及 **`AGENTS.md`** 中相关约定。若调整轨迹算法（例如改用 5 次多项式 / squad），先改 **`TRAJECTORY_ALGORITHM.md`** 与 C# 实现，再同步 Python / C++。
