/**
 * @file 07_move_to.cpp
 * @brief MoveTo（RunTo 规划运动）+ MoveToHeartbeat。
 *
 * type=Joint/Line 时须周期心跳（≥500ms），否则规划运动会停。
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

/**
 * 下发一次 MoveTo，并维持约 6 秒心跳。
 * @param tag 日志标签，便于区分场景
 */
void run_move_to(Codroid::CodroidClient& robot, const Codroid::MoveToParams& params, const char* tag) {
    auto res = robot.MoveTo(params, robot.NextRequestId());
    if (!res.Ok()) {
        std::cerr << tag << " MoveTo 失败: " << res.error_msg << "\n";
        return;
    }
    std::cout << tag << " 已启动，发送心跳...\n";
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        print_result("MoveTo 心跳", robot.MoveToHeartbeat(robot.NextRequestId()));
    }
}

}  // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.1.136";
    const std::string local_ip = "192.168.1.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn 失败\n";
        return 1;
    }

    // 用户目标点：关节角（度）与笛卡尔（mm+度）；请换成现场安全示教点
    const auto joint_target = Codroid::MoveToTarget::Joint(
        Codroid::JointPoint::Degrees({0, 0, 90, 0, 90, 0}));
    const auto cart_target = Codroid::MoveToTarget::Cartesian(Codroid::CartesianPoint::MmDeg(
        {278.823, 335.857, 1018.803, -101.953, 23.121, -28.329}));

    // 直线规划 → 关节目标 / 笛卡尔目标
    run_move_to(robot, Codroid::MoveToParams(Codroid::MoveToType::Line, joint_target), "直线+关节点");
    run_move_to(robot, Codroid::MoveToParams(Codroid::MoveToType::Line, cart_target), "直线+笛卡尔点");
    // 关节规划 → 关节目标 / 笛卡尔目标
    run_move_to(robot, Codroid::MoveToParams(Codroid::MoveToType::Joint, joint_target), "关节+关节点");
    run_move_to(robot, Codroid::MoveToParams(Codroid::MoveToType::Joint, cart_target), "关节+笛卡尔点");
    // 控制器内置位：Home / Candle（无需 target）
    run_move_to(robot, Codroid::MoveToParams(Codroid::MoveToType::Home), "回 Home");
    run_move_to(robot, Codroid::MoveToParams(Codroid::MoveToType::Candle), "回 Candle");

    print_result("停止 MoveTo", robot.StopMoveTo(robot.NextRequestId()));
    print_result("下电", robot.SwitchOff(robot.NextRequestId()));
    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
