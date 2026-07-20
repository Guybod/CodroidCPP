/**
 * @file 01_connect.cpp
 * @brief 最小连接示例：远程上电 + 读取 CRI 首帧状态。
 *
 * 业务代码只需：#include "Codroid/client.hpp"
 *
 * 流程：ConnectRemoteAndSwitchOn → 读 GetRobotRealtimeState → Disconnect
 * 运行前请按现场修改 robot_ip / local_ip。
 */
#include "Codroid/client.hpp"

#include <iostream>
#include <string>

int main() {
    // Windows 控制台 UTF-8；Linux 为空操作
    Codroid::InitConsoleUtf8();

    // robot_ip：控制器 IP；local_ip：本机接收 CRI UDP 推送的网卡地址
    const std::string robot_ip = "192.168.1.136";
    const std::string local_ip = "192.168.1.150";

    Codroid::CodroidClient robot;

    std::cout << "正在连接并上电...\n";
    // 组合流程：TCP 连接 → 自动/远程 → 启动 CRI 推送 → 上电使能
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn 失败\n";
        return 1;
    }

    // 刚连上时 CRI 可能尚未到首帧；业务中可 WaitForCriData 或轮询 data_valid
    const auto state = robot.GetRobotRealtimeState();
    if (state.data_valid) {
        std::cout << "CRI 已就绪，时间戳=" << state.timestamp_ms << " ms\n";
    } else {
        std::cout << "已连接，但尚未收到 CRI 数据帧\n";
    }

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
