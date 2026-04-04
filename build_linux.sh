#!/bin/bash

# 开启严格模式：遇到错误立即停止
set -e

echo "=================================================="
echo "      Codroid SDK Linux 构建脚本 (Ubuntu .so)      "
echo "=================================================="

# 1. 环境准备：物理复制第三方库 (确保 third_party 目录是最新的)
echo "[1/2] 正在同步系统依赖库到项目内部 (third_party)..."

# 清理并重建目录，确保没有旧文件残留
rm -rf third_party
mkdir -p third_party

# 复制 asio.hpp 和 asio 文件夹
if [ -f "/usr/include/asio.hpp" ]; then
    echo "  -> 复制 asio.hpp"
    cp /usr/include/asio.hpp third_party/
    cp -r /usr/include/asio third_party/
else
    echo "  错误: 找不到 /usr/include/asio.hpp，请运行: sudo apt install libasio-dev"
    exit 1
fi

# 复制 nlohmann (json) 文件夹
if [ -d "/usr/include/nlohmann" ]; then
    echo "  -> 复制 nlohmann (json)"
    cp -r /usr/include/nlohmann third_party/
else
    echo "  错误: 找不到 /usr/include/nlohmann，请运行: sudo apt install nlohmann-json3-dev"
    exit 1
fi

# 2. 编译 Linux 版本
echo "[2/2] 开始编译 Linux 动态库 (.so) 及示例程序..."

# 清理并创建 Linux 构建目录
rm -rf build_linux
mkdir -p build_linux && cd build_linux

# 运行 CMake 和 Make
cmake ..
make -j$(nproc)

cd ..

echo "=================================================="
echo "                Linux 构建成功!                   "
echo "=================================================="
echo "📁 产物路径 : build_linux/"
echo "   - 动态库 : build_linux/libCodroid.so"
echo "   - 测试程序: (位于 build_linux/ 目录下)"
echo ""
echo "▶ 快速运行 move_test:"
echo "   cd build_linux"
echo "   export LD_LIBRARY_PATH=.:\$LD_LIBRARY_PATH"
echo "   ./move_test"
echo "=================================================="