# Codroid C++ SDK 版本说明

## v3.0.0（2026-07-20）

### Breaking Change

- **唯对外入口**：客户代码只需 `#include "Codroid/client.hpp"`（聚合 DTO、轨迹、CRI 下发、`nlohmann::json`）。`CodroidController` / 内部 `CodroidDefine` 在 `src/internal/`，**不进发布包**。
- **JSON 用 nlohmann**：力控 / 脚本 vars / 全局变量 / RS485 等公开 API 使用 **`nlohmann::json`**；`Variable` 模板构造会自动 `dump()`。发布包附带 `include/nlohmann/`，用户**不必**再单独 `#include <nlohmann/json.hpp>`。
- **Asio / KDL 不对客户暴露**：Asio 仅实现侧；本地 `kinematicsInit/Fk/Ik` 已删除，`libCodroid` 不链 `libkdl`。正逆解用 TCP `ForwardKinematics` / `InverseKinematics`。
- **示例**：统一在 `examples/`（仅 `CodroidClient` / `client.hpp`）；已删除 `examples_client/`。

### 公开头白名单

`client.hpp`（入口）、`types.hpp`、`CodroidExport.h`、`console_utf8.hpp`、`cri_realtime_dispatcher.hpp`、`trajectory_*.hpp`，以及随包 `nlohmann/`。

---

## v2.1.11（2026-07-13）

### 新增

- **力控接口更新**：新增 `ZeroForceCalibration`、`InitForceControl`、`StartForceControl`、`StopForceControl`、`TuneForceParams`、`StartContactDetection`、`SetOverforceProtection`、`SetForceDataHealth`、`GetForceState` 及单字段状态 getter
- **力控测试示例**：新增 `examples_client/08_force_control.cpp`

### Breaking Change

- **移除 `FTSensorDriftCalibration`**：新协议使用 `ZeroForceCalibration(calibrationTimeMs)`
- **`InitForceControl` 固定导纳**：当前固定下发 `algo=1`，不开放算法参数

---

## v2.1.10（2026-06-03）

### Breaking Change

- **`MotionWaitOptions` 容差字段彻底移除**：`joint_tolerance_deg`、`cartesian_position_tolerance_mm`、`cartesian_orientation_tolerance_deg` 已删除，旧代码引用会编译报错

---

## v2.1.8（2026-06-03）

### Breaking Change

- **`*Sync` 阻塞运动完成判定逻辑简化**：仅依据 CRI `InMotion` 标志（曾运动 + 连续 `SettledSamples` 次停稳），**不再**比对关节角或 TCP 与目标点误差
  - 碰撞、急停、报警仍会抛异常（这些场景会触发 `has_alarm` / `emergency_stop`）
  - 外部 `StopRobotMove` 打断视为正常结束
  - 移除「运动已停止，但未到达目标点」异常
- **`MotionWaitOptions` 容差字段废弃**：`joint_tolerance_deg`、`cartesian_position_tolerance_mm`、`cartesian_orientation_tolerance_deg` 标记 `[[deprecated]]`，不再生效

---

## v2.1.7（2026-06-01）

### 新增

- **`CposToCpos` / `CposToCposPose`**：坐标系转换（协议 `Robot/cpostocpos`），将 TCP 位姿从坐标系1+工具1 转换到坐标系2+工具2，与 C# 对齐
- **暴露已有 Controller 方法到 Client**：
  - `SetStartLine` / `ClearStartLine`（工程启动行）
  - `GetProjectVar`（工程变量）
  - `Rs485Init` / `Rs485Flush` / `Rs485Read` / `Rs485Write`（RS485 通信）
  - `SetExtendArrayType` / `RemoveExtendArray`（寄存器扩展）
- **`GetGlobalVarsCatalog`**：全局变量结构化获取（与 `GetGlobalVars` 同协议，客户端解析）
- **`SaveUserCoordinateFrames`**：批量下发用户坐标系表（19.6）

### 修复

- **修复 `packInstruction` middlePoint `rj` 死代码 Bug**：提取 `packMovePoint()` 共用函数，targetPoint 和 middlePoint 统一走同一打包逻辑
- **修复 middlePoint 忽略 `jp`**：现在 `jp` 优先级与 targetPoint 一致
- **修复 `packInstruction` blend/relativeBlend 互斥逻辑**：同时传入时只发 `blend`
- **修复空 targetPoint 静默发送**：`jp`/`cp`/`ep` 全空时抛出 `std::invalid_argument`
- **新增 `ep`（外部轴）字段序列化**
- **新增 CRI 数据时效性检查**：同步运动轮询循环中新增基于 `steady_clock` 的数据年龄判定

---

## v2.1.5（2026-05-30）

### Bug Fixes

- **修复阻塞运动欧拉角到达判定**：`180°` 和 `-180°` 是同一姿态，但之前直接算差值 `|180-(-180)|=360°`，导致判定永远不通过。现在归一化到 `[-180, 180]` 后再比较
- **修复 `packInstruction` blend/relativeBlend 互斥逻辑**：从 `else if` 改为两个独立 `if`，允许 relativeBlend 独立生效
- **修复 `setPayload` 校验范围**：从 1-15 改为 0-15，与 C# 对齐

### 改进

- **MovJ/MovL/MovC/MovCircle 便捷方法补全参数**：所有便捷方法新增 `blend`、`relativeBlend`、`coor`、`tool` 可选参数，与 C#/Python 对齐
- **所有 *Sync 方法补全参数**：`MovJSync`、`MovLSync`、`MovCSync`、`MovCircleSync` 新增 `blend`、`relativeBlend`、`coor`、`tool` 可选参数

---

## v2.1.2（2026-05-29）

### 新增

- **阻塞式运动 API**（对齐 C# `*Sync` 方法）：
  - `MoveSync`、`MovJSync`（JointPoint / CartesianPoint）、`MovLSync`（CartesianPoint / JointPoint）、`MovCSync`、`MovCircleSync`
  - `MotionWaitOptions` 结构体：可配置超时、轮询间隔、CRI 过期判定、稳定采样数、关节/笛卡尔容差
- **`StopMoveTo()`**：发送 `type=-1` 停止 MoveTo 运动
- **`WaitForCriData(timeout_s)`**：阻塞等待首帧 CRI 数据到达
- `MoveToType::Stop = -1` 枚举值
- 从 `CodroidController` 暴露到 `CodroidClient` 的方法：
  - 模式控制：`EnterManualModeViaAuto`、`EnterRemoteModeViaAuto`、`ToSimulation`、`ToActual`、`StartDrag`、`StopDrag`
  - 工程/脚本：`RunScript`（含 `subThreads`/`subPrograms`/`interrupts`/`vars`）、`EnterRemoteScriptMode`、`Run`、`RunByIndex`、`RunStep`、`PauseProject`、`ResumeProject`、`StopProject`
  - 全局变量：`GetGlobalVars`、`SaveGlobalVars`、`RemoveGlobalVars`
  - Jog：`Jog`、`StopJog`、`JogHeartbeat`
  - MoveTo：`MoveTo`、`MoveToHeartbeat`
  - 运动学：`ForwardKinematics`、`InverseKinematics`、`CalculateRelativePose`
- 新增示例：`examples_client/07_sync_motion.cpp`（阻塞式运动演示）

---

## v2.1.1（2026-05-21）

在 v2.1.0 基础上的累积更新（运动 API、路径构建、MinGW 构建）。客户可读公告见 **`UPDATE_ANNOUNCEMENT.md`**。

### 破坏性变更：运动点位

- `MovJ` / `MovL` / `MovC` / `MovCircle` 不再接受裸 `std::vector<double>`。
- 使用 **`JointPoint::Degrees`**（关节）与 **`CartesianPoint::MmDeg` / `MmDegWithRef`**（TCP）。
- 路径：`ClientMoveInstruction::MovJ` / `MovL` / `MovC` / `MovCircle` + **`Move()`**（`MovePath` 别名）。

### 其它

- `CodroidDefine.h` 补充各类中文注释；`ClientMovePoint` 与 `MovePoint` 统一。
- `build_mingw.bat`：错误检测、单工具链目录、可选 `MINGW_BIN`。
- `examples_client/04_move.cpp`、`README.md` 更新。

### 迁移

见 `UPDATE_ANNOUNCEMENT.md` §六。

---

## v2.1.0（2026-05-21）

面向 **Codroid 控制器固件 ≥ 2.3.3.43** 的 Linux / Windows 客户库更新。公开头文件目录为 `include/Codroid/`，客户主入口为 `Codroid/client.hpp`。

---

### 新增功能

#### 机器人设置参数（协议 19.2 ~ 19.7）

| API | 协议 | 说明 |
|-----|------|------|
| `GetRobotParameters` | `Robot/GetRobotParameter` | 读取设置界面完整参数（默认编号、Tool/Payload/Coordinate 表等） |
| `SetDefaultPayloadId` | `Robot/SaveRobotParameter` | 仅设置默认负载编号 |
| `SetDefaultToolId` | `Robot/SaveRobotParameter` | 仅设置默认工具坐标系编号 |
| `SetDefaultUserCoordinateId` | `Robot/SaveRobotParameter` | 仅设置默认用户坐标系编号 |
| `SetToolFrame` | `Robot/SaveRobotParameter` | 修改单个工具坐标系（内部先 Get 再 patch，其它槽位不变） |
| `SetPayloadFrame` | `Robot/SaveRobotParameter` | 修改单个负载坐标系（同上） |
| `SetUserCoordinateFrame` | `Robot/SaveRobotParameter` | 修改单个用户坐标系（同上） |
| `SaveToolFrames` | `Robot/SaveRobotParameter` | 直接下发 `Tool` 数组 |
| `SavePayloadFrames` | `Robot/SaveRobotParameter` | 直接下发 `Payload` 数组 |

配套类型：`ClientRobotFrame`、`ClientRobotPayload`、`ClientRobotParameters`。

#### 碰撞灵敏度（协议 19.1，已有封装）

- `SetCollisionSensitivity(sensitivity)`：`Robot/setCollisionSensitivity`，灵敏度 **0~100**。

#### 运行时负载切换（已有封装）

- `SetPayload(payloadId)`：`Robot/setPayload`，与「默认负载编号」不同，用于运行中切换当前负载。

---

### 行为约定

- **固件要求**：本 SDK 对外封装的全部 TCP/UDP 接口均要求控制器固件 **≥ 2.3.3.43**（常量 `MinControllerFirmware`）。
- **序号范围**：负载、工具、用户坐标系的**对外可设序号**统一为 **1~15**；传入 0 或大于 15 时 SDK 本地校验失败。
- **0 号槽位**：控制器 `Get` 返回的 0 号条目恒为全 0（底层安全逻辑）；SDK **不提供**对 0 号的设置接口；`Set*Frame` 回写整表时会保留控制器中的 0 号数据。
- **仅改默认编号**：`SetDefaultPayloadId` / `SetDefaultToolId` / `SetDefaultUserCoordinateId` **无需**先调用 `GetRobotParameters`，只下发对应单个字段。
- **改某一槽位数值**：`SetToolFrame` / `SetPayloadFrame` / `SetUserCoordinateFrame` **无需**用户手动 Get，SDK 内部自动完成读—改—写。

---

### 构建与交付

- **头文件路径**：统一为 `#include "Codroid/client.hpp"`（目录 `include/Codroid/`）。
- **Linux 示例/库运行时**：示例程序 RUNPATH 已包含 `kinematics/`，便于加载 `libkdl.so`、`libFk_Ik_so.so`；亦可设置 `LD_LIBRARY_PATH=.:../kinematics`。
- **打包脚本**：`package_linux.sh`、`package_msvc.bat`、`package_mingw.bat` 在生成目录后自动产出压缩包。
- **预编译下载**：见 GitHub [Releases v2.1.0](https://github.com/Guybod/CodroidCPP/releases/tag/v2.1.0) 附件（不入库源码）。

---

### 示例

- 新增 `examples_client/06_robot_parameters.cpp`（构建目标 `client_06_robot_parameters`）：演示读取参数、默认编号、单槽位修改与整表 round-trip。

---

### 升级指引

1. 将业务工程中的 `#include "codroid/..."` 改为 `#include "Codroid/..."`（若仍使用旧小写路径）。
2. 确认控制器固件 **≥ 2.3.3.43**。
3. 机器人设置相关调用使用 **1~15** 序号；不要对 0 号调用设置类 API。
4. Linux 运行客户程序时保证 `libCodroid.so` 与同目录或 `LD_LIBRARY_PATH` 下的 `libkdl.so`、`libFk_Ik_so.so` 可加载。

---

### 快速示例

```cpp
#include "Codroid/client.hpp"

Codroid::CodroidClient robot;
robot.ConnectRemoteAndSwitchOn("192.168.1.136", 9001, "192.168.1.150");

// 读取
auto params = robot.GetRobotParameters();
if (!params.valid) { /* 处理错误 */ }

// 只改默认工具号（无需先 Get）
robot.SetDefaultToolId(2);

// 只改 2 号工具坐标（内部自动 Get → patch → Save）
Codroid::CodroidClient::ClientRobotFrame f;
f.x = 15; f.y = 20;
robot.SetToolFrame(2, f);

robot.Disconnect();
```

---

## v2.0.0

- 客户侧 `CodroidClient` 门面、CRI 实时数据（mm/deg）、轨迹生成与 UDP 下发、主题订阅等（详见历史提交与 `SDK_GUIDE.md`）。

---

## 提交参考

| 版本 | 主要提交 |
|------|----------|
| v2.1.1 | `5f664d7` 运动类型与路径工厂；`6c89d45` JointPoint/CartesianPoint；`7954fa1` MinGW |
| v2.1.0 | `8edb664` 机器人参数 API；`6c9a786` 打包脚本与 `include/Codroid` |
| v2.0.0 | `72b54b8` 统一版本与架构 |
