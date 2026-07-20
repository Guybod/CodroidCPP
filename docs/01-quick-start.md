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
