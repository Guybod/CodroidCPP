# Codroid C++ SDK 版本说明

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
- **打包脚本**：`package_linux.sh`、`package_msvc.bat`、`package_mingw.bat` 在生成目录后自动产出压缩包；本 tag 预置产物见 `releases/v2.1.0/`：
  - `CodroidSDK-Linux-x64.tar.gz`
  - `CodroidSDK-Windows-MSVC-x64.zip`
  - `CodroidSDK-Windows-MinGW-x64.zip`

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
robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150");

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
| v2.1.0 | `8edb664` 机器人参数 API；`6c9a786` 打包脚本与 `include/Codroid` |
| v2.0.0 | `72b54b8` 统一版本与架构 |
