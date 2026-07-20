/**
 * @file 13_cri_state.cpp
 * @brief 读取 CRI 实时状态快照（关节度 / TCP mm+度）。
 *
 * ConnectRemoteAndSwitchOn 传入 local_ip 后会启动 CRI UDP 推送。
 * 仅需：#include "Codroid/client.hpp"
 */
#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
    Codroid::InitConsoleUtf8();

    // local_ip 必须是本机能接收控制器 UDP 的网卡地址
    const std::string robot_ip = "192.168.1.136";
    const std::string local_ip = "192.168.1.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn 失败\n";
        return 1;
    }

    // 轮询等待首帧有效数据（也可 robot.WaitForCriData(5.0)）
    for (int i = 0; i < 50; ++i) {
        const auto state = robot.GetRobotRealtimeState();
        if (state.data_valid && state.timestamp_ms > 0) {
            // SDK 已换算为业务单位：关节 deg；TCP 前三 mm、后三 deg
            std::cout << "时间戳=" << state.timestamp_ms;
            if (state.joint_position.size() >= 6) {
                std::cout << " 关节[0]=" << state.joint_position[0] << " 度";
            }
            if (state.tcp_pose.size() >= 6) {
                std::cout << " TCP.x=" << state.tcp_pose[0] << " mm";
            }
            std::cout << "\n";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
