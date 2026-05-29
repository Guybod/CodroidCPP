# CodroidCPP SDK 手册

**版本:** 2.1.2 | **命名空间:** `Codroid`

---

## 目录

| # | 文档 | 说明 |
|---|------|------|
| 1 | [快速上手](01-quick-start.md) | 安装、构建、连接并运行第一个程序 |
| 2 | [核心概念](02-concepts.md) | 生命周期、TCP 模型、单位约定、异常处理 |
| 3 | [CodroidClient API](03-api-reference-codroidclient.md) | CodroidClient 完整 API 参考 |
| 4 | [运动控制](04-api-reference-motion.md) | JointPoint、CartesianPoint、MoveInstruction、阻塞运动 |
| 5 | [数据类型与枚举](05-api-reference-types.md) | CommandResult、ClientRealtimeState、RobotFrame、异常类 |
| 6 | [CRI 实时控制](06-api-reference-cri.md) | CriRealtimeDispatcher、TrajectoryGenerator、轨迹下发 |
| 7 | [IO 与寄存器](07-api-reference-io-register.md) | DI/DO/AI/AO 操作、寄存器读写 |
| 8 | [辅助工具](08-api-reference-utilities.md) | 主题订阅、全局变量、运动学、控制台 UTF-8 |

---

## 环境要求

| 平台 | 编译器 | 构建工具 |
|------|--------|----------|
| Linux | GCC 9+ / Clang 10+ | CMake 3.14+ |
| Windows | MSVC 2019/2022/2026 | CMake 3.14+ |
| Windows | MinGW-w64 (MSYS2) | CMake 3.14+ |

### 依赖项

- **Asio** (独立版，非 Boost) — 网络通信
- **nlohmann/json** — JSON 序列化
- **GoogleTest** (可选) — 单元测试

### 构建

#### Linux

```bash
chmod +x build_linux.sh
./build_linux.sh
```

产物在 `build_linux/`（包含 `libCodroid.so` 与示例程序）。

#### Windows (MSVC)

```bat
build_msvc.bat
```

脚本支持：
- `1`：Visual Studio 2019
- `2`：Visual Studio 2022
- `3`：Visual Studio 2026

产物在 `build_msvc/Debug` 与 `build_msvc/Release`。

#### Windows (MinGW)

```bat
build_mingw.bat
```

产物在 `build_mingw/`。

---

## 快速示例

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    // 连接控制器并上电
    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 读取数字输入
    int di0 = robot.GetDi(0);
    std::cout << "DI 0 = " << di0 << std::endl;

    // 设置数字输出
    robot.SetDo(10, di0);

    // 关节运动
    auto joints = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
    robot.MovJ(joints, 40, 100);

    // 断开连接
    robot.Disconnect();
    return 0;
}
```

---

## API 命名约定

所有公共方法使用 **PascalCase** 命名，与 C# 和 Python SDK 保持一致。

```cpp
// 正确
robot.ConnectRemoteAndSwitchOn("192.168.8.136");
int di = robot.GetDi(0);
robot.MovJ(joints, 40, 100);
```

---

## 单位约定

| 层级 | 线性 | 角度 |
|------|------|------|
| SDK 公共 API | **mm** | **deg (度)** |
| TCP JSON 协议 | **mm** | **deg** |
| CRI UDP 二进制 (线上) | **m** | **rad (弧度)** |
| `ClientRealtimeState` (解析后) | **mm** | **deg** |

`CriRealtimeDispatcher` 在 `convert_to_si=true`（默认）时会自动将 mm/deg 转换为 m/rad。

---

## 固件要求

本 SDK 所有对外接口均要求控制器固件 **≥ 2.3.3.43**。

代码常量见 `Codroid::MinControllerFirmware`（`CodroidDefine.h`）。

---

## 示例程序

推荐按以下顺序学习：

| 示例 | 说明 |
|------|------|
| `examples/01_basic_usage.cpp` | 基础连接与调用 |
| `examples/08_move.cpp` | 运动指令 |
| `examples/09_io_test.cpp` | IO 操作 |
| `examples/14_cri_trajectory.cpp` | CRI 全流程与轨迹下发 |

---

## 常见问题

### Q1: 编译时找不到头文件

确保 CMake 能找到依赖库。检查 `CMakeLists.txt` 中的 `find_package` 或 `target_include_directories` 配置。

### Q2: 连接失败

1. 确认控制器 IP 地址正确
2. 确认控制器固件版本 ≥ 2.3.3.43
3. 检查网络连接和防火墙设置

### Q3: 运动指令不执行

1. 确认已调用 `ConnectRemoteAndSwitchOn` 或 `SwitchOn`
2. 确认控制器处于远程模式
3. 检查是否有报警或碰撞停止

### Q4: CRI 数据单位不一致

CRI UDP 原始数据使用 m/rad，SDK 内部已转换为 mm/deg。如果直接使用原始 UDP 数据，需要手动转换。

---

## 技术支持

- 协议细节：参见 `AGENTS.md`、`PROTOCOL_LINE_BY_LINE.md`
- API 设计：参见 `SDK_API_AND_DESIGN.md`
- 轨迹算法：参见 `TRAJECTORY_ALGORITHM.md`
