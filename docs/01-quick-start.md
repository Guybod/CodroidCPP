# 快速上手

## 构建 SDK

### Linux

```bash
# 克隆仓库
git clone <repository-url>
cd CodroidCPP

# 构建
chmod +x build_linux.sh
./build_linux.sh
```

产物在 `build_linux/`：
- `libCodroid.so` — 动态库
- `examples/` — 示例程序

### Windows (MSVC)

```bat
REM 克隆仓库后
cd CodroidCPP

REM 构建
build_msvc.bat
```

选择 Visual Studio 版本：
- `1`：Visual Studio 2019
- `2`：Visual Studio 2022
- `3`：Visual Studio 2026

产物在 `build_msvc/Debug` 和 `build_msvc/Release`。

### Windows (MinGW)

```bat
cd CodroidCPP
build_mingw.bat
```

产物在 `build_mingw/`。

---

## 最小示例

连接控制器，读取数字输入，写入数字输出，然后断开。

```cpp
#include "Codroid/client.hpp"
#include <iostream>

int main() {
    Codroid::CodroidClient robot;

    // 连接、切换远程、上电
    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 读取 DI 端口 0
    int di0 = robot.GetDi(0);
    std::cout << "DI 0 = " << di0 << std::endl;

    // 将 DI 值写入 DO 端口 10
    robot.SetDo(10, di0);

    // 断开连接
    robot.Disconnect();
    return 0;
}
```

---

## 完整工作流示例

```cpp
#include "Codroid/client.hpp"
#include "Codroid/cri_realtime_dispatcher.hpp"
#include "Codroid/trajectory_generator.hpp"
#include <iostream>

int main() {
    // Windows 控制台 UTF-8 支持
    #ifdef _WIN32
    Codroid::ConsoleUtf8::InitConsoleUtf8();
    #endif

    Codroid::CodroidClient robot;

    // 1. 连接
    if (!robot.ConnectRemoteAndSwitchOn("192.168.8.136", 9001, "192.168.8.150")) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 2. IO 操作
    int di0 = robot.GetDi(0);
    robot.SetDo(10, di0);

    // 3. 寄存器
    double regValue = robot.GetRegisterValue(49100);
    robot.SetRegisterValue(49100, regValue + 1);

    // 4. 运动
    auto joints = Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0});
    robot.MovJ(joints, 40, 100);

    // 5. 阻塞运动（等待到达目标）
    auto target = Codroid::CartesianPoint::MmDegWithRef(
        {400, 0, 300, 180, 0, 0},
        robot.GetRobotRealtimeState().joint_position
    );
    robot.MovLSync(target, 150, 500);

    // 6. 断开
    robot.Disconnect();
    return 0;
}
```

---

## 运行示例程序

```bash
# Linux
cd build_linux
./examples/01_basic_usage 192.168.8.136

# Windows (MSVC)
cd build_msvc\Release
01_basic_usage.exe 192.168.8.136
```

---

## 错误处理

TCP 指令失败时的行为取决于 `SetThrowOnCommandError` 设置：

### 默认模式（不抛异常）

```cpp
Codroid::CodroidClient robot;
robot.Connect("192.168.8.136");

auto result = robot.SetDo(999, 1); // 无效端口
if (!result.Ok()) {
    std::cerr << "控制器错误: " << result.error_msg << std::endl;
}
```

### 抛异常模式

```cpp
robot.SetThrowOnCommandError(true);

try {
    robot.SetDo(999, 1); // 无效端口
} catch (const Codroid::CodroidCommandException& ex) {
    std::cerr << "控制器错误: " << ex.controller_error() << std::endl;
    std::cerr << "请求 ID: " << ex.request_id() << std::endl;
    std::cerr << "原始响应: " << ex.raw_response_json() << std::endl;
}
```

### 异常类型

| 异常 | 触发条件 |
|------|----------|
| `CodroidCommandException` | 控制器返回 `err` 字段 |
| `CodroidException` | 通用运行时错误 |
| `std::runtime_error` | 标准库异常 |

---

## 在项目中使用

### CMake

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject)

find_package(Codroid REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app Codroid::Codroid)
```

### 手动链接

```bash
# Linux
g++ -std=c++17 main.cpp -I/path/to/CodroidCPP/include \
    -L/path/to/CodroidCPP/build_linux -lCodroid \
    -I/path/to/third_party/asio/include \
    -I/path/to/third_party/json/include

# Windows (MSVC)
cl /std:c++17 main.cpp /I<path>\include \
    /I<path>\third_party\asio\include \
    /I<path>\third_party\json\include \
    /link /LIBPATH:<path>\build_msvc\Release Codroid.lib
```

---

## 下一步

- [核心概念](02-concepts.md) — 了解生命周期、TCP 模型、单位约定
- [CodroidClient API](03-api-reference-codroidclient.md) — 完整 API 参考
- [运动控制](04-api-reference-motion.md) — 运动指令详解
