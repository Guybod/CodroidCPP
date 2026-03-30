# Codroid SDK

Codroid SDK 是一个用于机械臂远程控制的跨平台 C++ 开发工具包。它通过 TCP 通信协议实现与机器人的交互，涵盖了运动控制（movJ/movL/movC）、IO 管理、寄存器操作、数据流订阅及实时控制等核心功能。

## 🌟 特点

- **开箱即用**：底层依赖库（Asio 和 nlohmann/json）已内置于 `third_party` 文件夹中，克隆仓库后无需额外安装系统依赖即可直接编译。
- **双通道架构**：支持独立的指令通道与状态订阅通道，确保高频运动指令下发与实时状态反馈互不干扰。
- **单位标准化**：SDK 内部已自动完成单位换算。
  - **长度单位**：毫米 (mm)
  - **角度单位**：角度 (deg)（机器人返回的弧度数据已在内部自动转换为角度，并保留 3 位小数）。
  - **数值精度**：所有运动学返回结果均经过 3 位小数舍入处理（如 0.001mm / 0.001°），有效过滤浮点数运算产生的微小数值噪声（如 -2.88e-07 将自动转为 0.000）。
- **内置运动学引擎**：集成了高性能正逆解算法库，支持 6 轴机械臂的实时坐标转换。

## 📁 项目结构

*   `include/Codroid/` - SDK 对外公开接口头文件。
*   `kinematics/` - 运动学正逆解库
*   `src/` - SDK 核心逻辑实现源代码。
*   `third_party/` - 内置依赖库（Asio Standalone, nlohmann/json）。
*   `examples/` - 功能测试示例（点动、路径运动、IO 测试、寄存器测试等）。
*   `build_linux.sh` - Linux 一键构建脚本。
*   `build_msvc.bat` - Windows MSVC 一键构建脚本。

## 🛠️ 构建指南 -> Linux (Ubuntu)
#### 第一阶段：准备基础编译环境

```bash
# 更新软件源
sudo apt update

# 安装基础构建工具（GCC, G++, Make）和 CMake
sudo apt install build-essential cmake git -y
```

#### 第二阶段：编译 Codroid SDK 和官方示例
执行脚本会同时编译 SDK 动态库、运动学模块以及所有示例程序：

```bash
cd /path/to/CodroidSDK
chmod +x build_linux.sh
./build_linux.sh
```
构建产物：

* 动态库：build_linux/libCodroid.so
* 示例程序：位于 build_linux/ 目录下的各个可执行文件（如 move_test）。

#### 第三阶段：运行测试示例
由于 libCodroid.so 依赖于 kinematics/ 目录下的第三方动态库（libFk_Ik_so.so 等），运行程序时必须同时指定这两个路径：

运行示例：
```bash
cd build_linux
export LD_LIBRARY_PATH=.:../kinematics:$LD_LIBRARY_PATH
./13_kinematics  # 运行运动学测试
```

## 📁 关键依赖说明
在 Linux 环境下，SDK 的完整运行依赖以下文件序列，发布应用时请务必保持它们的相对位置：
```text
your_app_bin           # 您的可执行程序
├── libCodroid.so      # SDK 主库
├── libFk_Ik_so.so     # 运动学核心库 (必须与主库同目录或在系统路径)
└── libkdl.so          # 运动学辅助库
```

#### 第四阶段：如何在你的独立项目中引入该 SDK
如果您的项目需要集成此 SDK，请参考以下 CMakeLists.txt 配置。特别注意 RPATH 的设置，它可以让您的程序在运行时自动找到库文件，无需手动 export。

推荐的目录结构
```test
MyRobotApp/
├── CMakeLists.txt              # 你的主项目 CMake 配置文件
├── build/                      # 编译输出目录 (自动生成)
├── include/                    # 存放你自己项目的头文件 (.h)
│   └── my_robot_logic.h
├── src/                        # 存放你自己项目的源代码 (.cpp)
│   └── main.cpp
└── third_party/                # 存放外部 SDK 或第三方库
    └── CodroidSDK/             # 直接将 CodroidSDK 仓库放入此处
        └── cpp/                # SDK 的 C++ 核心目录
            ├── include/        # SDK 公开头文件
            ├── src/            # SDK 源码
            ├── kinematics/     # 运动学库 (.so / .h)
            ├── third_party/    # SDK 依赖的 asio/json
            ├── build_linux/    # SDK 编译后的产物 (libCodroid.so)
            └── CMakeLists.txt  # SDK 的 CMake 脚本
```
CMakeLists.txt 模板：
```Cmake
cmake_minimum_required(VERSION 3.10)
project(MyRobotApp LANGUAGES CXX)

# --- 1. 基础编译设置 ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 2. 定义 Codroid SDK 路径变量 (方便后续引用) ---
# 指向 SDK 的 cpp 根目录
set(SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CodroidSDK/cpp)

# --- 3. 添加包含路径 (解决 "没有那个文件或目录" 的关键) ---
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include        # 你自己的头文件
    ${SDK_ROOT}/include                        # SDK 公开接口
    ${SDK_ROOT}                                # 必须包含 SDK 根目录，以支持 #include "kinematics/robotModel.h"
    ${SDK_ROOT}/third_party                    # 必须包含，以找到 asio.hpp
)

# --- 4. 添加宏定义 ---
add_definitions(-DASIO_STANDALONE)

# --- 5. 编译你的主程序 ---
add_executable(${PROJECT_NAME} src/main.cpp)

# --- 6. 链接设置 (针对 Linux 环境) ---
if(UNIX)
    # 告诉链接器去哪里找编译好的 libCodroid.so 以及运动学 .so
    target_link_directories(${PROJECT_NAME} PRIVATE 
        ${SDK_ROOT}/build_linux 
        ${SDK_ROOT}/kinematics
    )

    # 链接库：注意顺序，Codroid 依赖底层的运动学库
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        Codroid 
        Fk_Ik_so 
        kdl 
        pthread
    )

    # --- 【核心神技】设置 RPATH ---
    # 这步非常重要！设置后，你运行程序时，系统会自动去 SDK 目录下找 .so 
    # 无需再手动 export LD_LIBRARY_PATH
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN:${SDK_ROOT}/build_linux:${SDK_ROOT}/kinematics"
    )
endif()

# --- 7. 链接设置 (针对 Windows MSVC 环境) ---
if(MSVC)
    target_link_directories(${PROJECT_NAME} PRIVATE ${SDK_ROOT}/build_msvc/Release)
    target_link_libraries(${PROJECT_NAME} PRIVATE Codroid ws2_32 mswsock)
endif()
```

## 🛠️ 构建指南 -> Windows (Visual Studio / MSVC)
在 Windows 平台上，本 SDK 使用 MSVC 编译器，建议使用 Visual Studio 2019 或 2022。
#### 第一阶段：准备基础编译环境

1. **安装 CMake：** 前往[CMake 官网](https://cmake.org/download/)下载并安装（建议版本 ≥ 3.10）。安装时请勾选“Add CMake to the system PATH”。

2. **安装 Visual Studio：** 下载[Visual Studio](https://visualstudio.microsoft.com/zh-hans/downloads/)在安装程序中务必勾选：
   - **“使用 C++ 的桌面开发” (Desktop development with C++)**
   - **确保包含 MSVC v142/v143 构建工具和 Windows 10/11 SDK。**

#### 第二阶段：编译 Codroid SDK 和官方示例
在项目根目录下，直接双击运行或在 CMD 中执行：

```bash
cd CodroidSDK/cpp
build_msvc.bat
```
该脚本会自动调用 CMake 进行配置，并使用 MSVC 同时生成 Debug 和 Release 版本的库文件。

构建产物说明：
编译完成后，产物位于 build_msvc 目录下：
* build_msvc/Debug
* build_msvc/Release。

|文件名称|所在路径|用途|
|:---|:---|:---|
|Codroid.lib|build_msvc/Release/|**开发时链接：** 在 VS 链接器中设置。
|Codroid.dll|build_msvc/Release/|**运行时依赖：** 必须放在 .exe 同级目录。
|xxx.exe|build_msvc/Release/|**示例程序：** 可直接运行的测试 demo。

## 💻 如何在您的项目中使用

1. **属性配置：**
   - **包含目录：** 添加 CodroidSDK/include 和 CodroidSDK/third_party。
   - **库目录：** 添加 CodroidSDK/cpp/build_msvc/Release。
   - **附加依赖项：** 手动添加 Codroid.lib。
2. **字符集设置：** VS 默认使用 GBK，建议在项目属性中开启 /utf-8 编译选项，以防止中文注释或字符导致编译报错。
3. **DLL 路径（核心）：**
    - Windows 默认不会自动查找 .dll。运行程序时，请务必将生成的 Codroid.dll 复制到您的可执行文件（.exe）同级目录下。

## ⚠️ Windows 环境特别说明 (重要)
1. 运动学模块限制：
  - 由于目前 `kinematics/` 目录仅提供 Linux 版本的 `.so` 动态库，**Windows 版 SDK 暂时不支持调用运动学正逆解函数。**
  - 在 Windows 下编译时，SDK 会通过条件编译跳过运动学实现，基础的 TCP 通信、IO 控制和运动指令（movJ/L/C）仍可正常工作。
2. 架构匹配：
  - 编译后的库默认为 **x64** 架构，请确保您的主项目也将目标平台设置为 **x64**（不要使用 x86/Win32）。
## ⚠️ 注意事项
1. 状态同步延迟：受限于机器人端状态推送固定为 1 秒/次，Wait 系列阻塞函数在判断机器人停止时可能会存在最高 1 秒的自然同步延迟。
2. 空数组崩溃预防：SDK 内部实现了防御性编程，发送指令时会自动剔除空的 coor 或 tool 数组字段，以规避机器人后端处理空数组时的崩溃 Bug。
3. 多线程安全：sendCommand 内部已通过 std::mutex 加锁。支持在多线程环境下并发调用非阻塞运动指令，但建议在同一时刻只由一个线程控制机器人运动。
4. 单位验证：请注意，机器人推送的 joint 数据在 SDK 内部被视为角度（Degrees）。若您的机器人固件返回的是弧度，SDK 将按照 1rad ≈ 57.295deg 进行自动转换。