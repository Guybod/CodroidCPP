#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
    // 按现场网络修改。local_ip 必须是本机能接收控制器 UDP 推送的网卡地址。
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    for (int i = 0; i < 50; ++i) {
        const auto state = robot.GetRobotRealtimeState();
        if (state.data_valid && state.timestamp_ms > 0) {
            // SDK 对外已转换为业务单位：关节 deg，TCP 前三位 mm、后三位 deg。
            std::cout << "timestamp=" << state.timestamp_ms;
            if (state.joint_position.size() >= 6) {
                std::cout << " joint[0]=" << state.joint_position[0] << " deg";
            }
            if (state.tcp_pose.size() >= 6) {
                std::cout << " tcp.x=" << state.tcp_pose[0] << " mm";
            }
            std::cout << "\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    robot.Disconnect();
    std::cout << "Done.\n";
    return 0;
}
