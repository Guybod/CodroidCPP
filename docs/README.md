# CodroidCPP SDK 文档 / CodroidCPP SDK Documentation

**版本 / Version:** 2.1.5 | **命名空间 / Namespace:** `Codroid`

---

## 手册 / Manuals

| 语言 / Language | 文件 / File | 说明 / Description |
|----------------|-------------|-------------------|
| 中文 | [CodroidCPP-SDK-Manual-v2.1.5-zh.md](CodroidCPP-SDK-Manual-v2.1.5-zh.md) | 完整中文手册 |
| English | [CodroidCPP-SDK-Manual-v2.1.5-en.md](CodroidCPP-SDK-Manual-v2.1.5-en.md) | Complete English manual |

---

## 分章节文档 / Chapter Documents

| # | 文档 / Document | 说明 / Description |
|---|----------------|-------------------|
| 1 | [快速上手](01-quick-start.md) | 构建、连接并运行第一个程序 |
| 2 | [核心概念](02-concepts.md) | 生命周期、TCP 模型、单位约定、异常处理 |
| 3 | [CodroidClient API](03-api-reference-codroidclient.md) | CodroidClient 完整 API 参考 |
| 4 | [运动控制](04-api-reference-motion.md) | JointPoint、CartesianPoint、MoveInstruction、阻塞运动 |
| 5 | [数据类型与枚举](05-api-reference-types.md) | CommandResult、ClientRealtimeState、RobotFrame、异常类 |
| 6 | [CRI 实时控制](06-api-reference-cri.md) | CriRealtimeDispatcher、TrajectoryGenerator、轨迹下发 |
| 7 | [IO 与寄存器](07-api-reference-io-register.md) | DI/DO/AI/AO 操作、寄存器读写 |
| 8 | [辅助工具](08-api-reference-utilities.md) | 主题订阅、全局变量、运动学、控制台 UTF-8 |

---

## 环境要求 / Environment Requirements

| 平台 / Platform | 编译器 / Compiler | 构建工具 / Build Tool |
|-----------------|------------------|----------------------|
| Linux | GCC 9+ / Clang 10+ | CMake 3.14+ |
| Windows | MSVC 2019/2022/2026 | CMake 3.14+ |
| Windows | MinGW-w64 (MSYS2) | CMake 3.14+ |

### 依赖项 / Dependencies

- **Asio** (独立版，非 Boost) — 网络通信 / Networking
- **nlohmann/json** — JSON 序列化 / JSON serialization
- **GoogleTest** (可选) — 单元测试 / Unit testing

---

## 快速示例 / Quick Example

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    // 连接控制器并上电 / Connect and power on
    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败 / Connection failed" << std::endl;
        return 1;
    }

    // 读取数字输入 / Read digital input
    int di0 = robot.GetDi(0);
    std::cout << "DI 0 = " << di0 << std::endl;

    // 设置数字输出 / Set digital output
    robot.SetDo(10, di0);

    // 关节运动 / Joint motion
    auto joints = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
    robot.MovJ(joints, 40, 100);

    // 断开连接 / Disconnect
    robot.Disconnect();
    return 0;
}
```

---

## 构建 / Build

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

脚本支持 / Script supports:
- `1`：Visual Studio 2019
- `2`：Visual Studio 2022
- `3`：Visual Studio 2026

产物在 `build_msvc/Debug` 与 `build_msvc/Release`。

### Windows (MinGW)

```bat
build_mingw.bat
```

产物在 `build_mingw/`。

---

## 示例程序 / Examples

推荐按以下顺序学习 / Recommended order:

| 示例 / Example | 说明 / Description |
|---------------|-------------------|
| `examples/01_basic_usage.cpp` | 基础连接与调用 / Basic connection and calls |
| `examples/08_move.cpp` | 运动指令 / Motion commands |
| `examples/09_io_test.cpp` | IO 操作 / IO operations |
| `examples/14_cri_trajectory.cpp` | CRI 全流程与轨迹下发 / CRI full workflow and trajectory |

---

## 固件要求 / Firmware Requirement

本 SDK 所有对外接口均要求控制器固件 **≥ 2.3.3.43**。

All SDK interfaces require controller firmware **≥ 2.3.3.43**.

代码常量见 `Codroid::MinControllerFirmware`（`CodroidDefine.h`）。

---

## 技术支持 / Technical Support

- 协议细节 / Protocol details: `AGENTS.md`、`PROTOCOL_LINE_BY_LINE.md`
- API 设计 / API design: `SDK_API_AND_DESIGN.md`
- 轨迹算法 / Trajectory algorithm: `TRAJECTORY_ALGORITHM.md`
