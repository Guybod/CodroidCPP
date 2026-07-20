/**
 * @file 06_jog_mode.cpp
 * @brief 点动：Jog → 周期 JogHeartbeat → StopJog。
 *
 * 点动期间必须 ≥500ms 发送一次心跳，否则控制器会停点动。
 * 仅需：#include "Codroid/client.hpp"
 */
#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

void print_result(const char* action, const Codroid::CommandResult& r) {
    if (r.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << r.error_msg << "\n";
    }
}

}  // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.1.136";
    const std::string local_ip = "192.168.1.150";

    Codroid::CodroidClient robot;
    // 点动前需远程并使能
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn 失败\n";
        return 1;
    }

    // Line=直线点动；speed 为 -1~1 比例；index=1 表示 X 正向（以协议为准）
    Codroid::JogParams jog(Codroid::JogMode::Line, 0.5, 1);
    print_result("启动点动", robot.Jog(jog, robot.NextRequestId()));

    // 约 5 秒：每 500ms 发心跳
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        print_result("点动心跳", robot.JogHeartbeat(robot.NextRequestId()));
        std::cout << "点动运行中...\n";
    }

    print_result("停止点动", robot.StopJog(robot.NextRequestId()));
    print_result("下电", robot.SwitchOff(robot.NextRequestId()));
    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
