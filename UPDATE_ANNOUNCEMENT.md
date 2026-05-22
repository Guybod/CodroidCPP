# Codroid C++ SDK 更新公告

**发布日期**：2026-05-21  
**适用版本**：`main` 分支（`v2.1.0` 之后累积更新，建议以最新 `main` 或即将发布的 **v2.1.1** 标签为准）  
**固件要求**：控制器 **≥ 2.3.3.43**（与 v2.1.0 相同）

---

## 一、摘要

本次更新在 **v2.1.0**（机器人设置参数、Linux/Windows 交付包）基础上，重点改进了 **运动控制 API 的类型安全** 与 **多段路径构建方式**，并修复 **Windows MinGW** 构建脚本问题。若你的工程仍在用裸 `std::vector<double>` 传关节/TCP 点位，请按下文迁移。

**预编译包与源码**：[GitHub Releases — v2.1.0](https://github.com/Guybod/CodroidCPP/releases/tag/v2.1.0)（后续 v2.1.1 标签发布后将更新附件）

---

## 二、重要变更：运动点位类型（破坏性）

### 2.1 不再用裸 `vector` 表示点位

| 以前（已废弃） | 现在（必须） |
|----------------|--------------|
| `MovJ({0,0,90,0,90,0}, speed, acc)` | `MovJ(JointPoint::Degrees({...}), speed, acc)` |
| `MovL({x,y,z,rx,ry,rz}, speed, acc)` | `MovL(CartesianPoint::MmDeg({...}), speed, acc)` |

**原因**：`vector<double>` 无法区分「六轴关节角」与「TCP 位姿」，易传错导致控制器逆解异常或运动异常。

### 2.2 两种业务点位类型

| 类型 | 工厂 | 含义 | 单位 |
|------|------|------|------|
| `JointPoint` | `Degrees({j1..j6})` | 关节目标 | 度 |
| `CartesianPoint` | `MmDeg({x,y,z,rx,ry,rz})` | TCP 目标 | mm + 度 |

### 2.3 四种「运动方式 × 点位」组合（单点与路径均支持）

| 业务含义 | 调用示例 |
|----------|----------|
| 关节运动 → 关节 | `MovJ(JointPoint::Degrees(...), ...)` |
| 关节运动 → TCP（控制器逆解） | `MovJ(CartesianPoint::..., ...)` |
| 直线运动 → TCP | `MovL(CartesianPoint::MmDeg(...), ...)` |
| 直线运动 → 关节 | `MovL(JointPoint::Degrees(...), ...)` |

### 2.4 `MmDegWithRef` — 笛卡尔目标 + 参考关节

当目标为 **TCP** 且使用 **movJ**（或路径中的 movJ+cp）时，逆解可能有多组关节解。请使用：

```cpp
auto st = robot.GetRobotRealtimeState();
auto target = Codroid::CartesianPoint::MmDegWithRef(
    {927.5, 214.5, 899.0, 180.0, 0.0, -90.0},
    st.joint_position);  // 当前关节作 rj，避免跳解
robot.MovJ(target, 40, 100, robot.NextRequestId());
```

仅 `MmDeg(tcp)` 时，SDK 下发默认参考关节 `[20,20,20,20,20,20]`，现场可能不稳定。

---

## 三、多段路径 `Move` / `MovePath`

一次 TCP 指令下发整条路径（`Robot/move`），每段用 **`ClientMoveInstruction` 静态工厂** 构建，入参同样为 `JointPoint` / `CartesianPoint`：

```cpp
const std::vector<Codroid::ClientMoveInstruction> path = {
    Codroid::ClientMoveInstruction::MovJ(joint_p1, 40, 100),   // movJ + jp
    Codroid::ClientMoveInstruction::MovJ(line_p1, 40, 100),    // movJ + cp
    Codroid::ClientMoveInstruction::MovL(line_p2, 150, 500),   // movL + cp
    Codroid::ClientMoveInstruction::MovL(joint_p2, 150, 500),  // movL + jp
    Codroid::ClientMoveInstruction::MovC(mid, end, 120, 400),
};
robot.Move(path, robot.NextRequestId());  // 与 C# `Move` 对齐；`MovePath` 为别名
```

**类型说明**（详见 `include/Codroid/CodroidDefine.h` 注释）：

- `MovePoint`：协议 JSON 中的一个路点（一般由工厂从 `JointPoint`/`CartesianPoint` 转换，应用层少直接使用）
- `ClientMoveInstruction`：路径中的一段（类型 + 速度 + 目标/中间点）

---

## 四、v2.1.0 已有能力（本次未改协议）

- 机器人设置参数：`GetRobotParameters`、`SetToolFrame`、`SetPayloadFrame` 等（协议 19.2~19.7）
- 客户入口：`#include "Codroid/client.hpp"`
- CRI 实时状态（mm/度）、`examples_client/01`~`06`
- GitHub Releases 提供 Linux / Windows 预编译 zip/tar.gz

---

## 五、构建与工具链

- **MinGW**：`build_mingw.bat` 已修复「失败仍显示成功」、混用 `ucrt64`/`mingw64` PATH 导致 CMake 找不到编译器等问题；请使用**同一套** `mingw64\bin`，或设置 `MINGW_BIN`。
- **Linux**：`build_linux.sh`；示例 RPATH 已含 `kinematics/`。
- **打包**：`package_linux.sh` / `package_msvc.bat` / `package_mingw.bat` 生成压缩包。

---

## 六、迁移清单

1. 全局替换运动调用：`MovJ`/`MovL`/`MovC`/`Move` 路径段改为 `JointPoint` / `CartesianPoint`。
2. 凡 **movJ 到 TCP** 或 **movL 到 TCP 且在意姿态解**，优先 `MmDegWithRef` + CRI 当前关节。
3. 头文件路径：`#include "Codroid/client.hpp"`（目录 `include/Codroid/`）。
4. 对照示例：`examples_client/04_move.cpp`（含注释与四组合路径）。
5. 文档：`README.md` §6 运动控制、`CodroidDefine.h` 类型注释。

---

## 七、获取方式

```bash
git clone https://github.com/Guybod/CodroidCPP.git
cd CodroidCPP
git pull   # 确保包含 5f664d7 及之后提交
./build_linux.sh
```

Windows：拉取最新代码后执行 `build_msvc.bat` 或 `build_mingw.bat`。

---

## 八、反馈

问题与需求请通过仓库 Issues 反馈，注明控制器型号、固件版本与复现用的 `MovJ`/`Move` 代码片段。
