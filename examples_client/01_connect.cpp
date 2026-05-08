#include "codroid/client.hpp"

#include <iostream>
#include <string>

int main() {
    // 按现场网络修改：robot_ip 是控制器地址，local_ip 是本机接收 CRI UDP 的网卡地址。
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;

    std::cout << "ConnectRemoteAndSwitchOn...\n";
    // 组合流程：TCP 连接 -> 切自动/远程 -> 启动 CRI 数据推送 -> 上电。
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    // 刚连接后 CRI 可能还没到首帧；业务中通常需要轮询等待 data_valid。
    const auto state = robot.GetRobotRealtimeState();
    if (state.data_valid) {
        std::cout << "CRI timestamp: " << state.timestamp_ms << "\n";
    } else {
        std::cout << "Connected. CRI frame not received yet.\n";
    }

    robot.Disconnect();
    std::cout << "Done.\n";
    return 0;
}
