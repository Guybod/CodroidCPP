/**
 * @file 10_sync_motion.cpp
 * @brief 阻塞式运动：MovJSync / MovLSync / MovCSync / MoveSync。
 *
 * 发送后自动轮询 CRI，直到 InMotion 稳定停止，无需手动 sleep。
 * 前置：已使能；CRI 推送已开（ConnectRemoteAndSwitchOn 传 local_ip 即可）。
 * 仅需：#include "Codroid/client.hpp"
 */
#include "Codroid/client.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (result.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << result.error_msg << "\n";
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

    // *Sync 依赖 CRI 判断到位，先等首帧
    std::cout << "等待 CRI 数据...\n";
    try {
        robot.WaitForCriData(5.0);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        robot.Disconnect();
        return 1;
    }
    std::cout << "CRI 就绪。\n\n";

    // 测试点（与 AGENTS.md §5.1 S20 一致）
    const auto joint_home = Codroid::JointPoint::Degrees({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    const auto joint_p1 = Codroid::JointPoint::Degrees({0.0, 0.0, 90.0, 0.0, 90.0, 0.0});
    const auto cart_p1 =
        Codroid::CartesianPoint::MmDeg({927.511, 214.489, 486.524, 179.999, 0.0, -89.999});
    const auto cart_p2 =
        Codroid::CartesianPoint::MmDeg({927.516, 214.489, 900.0, 180.0, 0.0, -89.999});
    const auto circle_middle =
        Codroid::CartesianPoint::MmDeg({927.515, 27.23, 738.722, -180.0, 0.0, -89.999});
    const auto circle_end = cart_p1;

    try {
        std::cout << "[1] MovJSync（关节）\n";
        robot.MovJSync(joint_p1, 40.0, 100.0);
        std::cout << "  ✓ 到达\n\n";

        std::cout << "[2] MovLSync（直线）\n";
        robot.MovLSync(cart_p1, 150.0, 500.0);
        std::cout << "  ✓ 到达\n\n";

        std::cout << "[3] MovJSync（关节插补到 TCP）\n";
        robot.MovJSync(cart_p2, 40.0, 100.0);
        std::cout << "  ✓ 到达\n\n";

        std::cout << "[4] MovCSync（圆弧）\n";
        robot.MovCSync(circle_middle, circle_end, 120.0, 400.0);
        std::cout << "  ✓ 到达\n\n";

        std::cout << "[5] MoveSync（多段路径）\n";
        std::vector<Codroid::ClientMoveInstruction> path = {
            Codroid::ClientMoveInstruction::MovJ(joint_p1, 40.0, 100.0),
            Codroid::ClientMoveInstruction::MovL(cart_p1, 150.0, 500.0),
            Codroid::ClientMoveInstruction::MovL(joint_home, 150.0, 500.0),
        };
        robot.MoveSync(path);
        std::cout << "  ✓ 全部段到达\n\n";

        // 自定义等待参数：超时、稳定采样次数等
        std::cout << "[6] 自定义 MotionWaitOptions\n";
        Codroid::MotionWaitOptions opts;
        opts.timeout_s = 30.0;
        opts.settled_samples = 2;  // 连续 2 次 InMotion=false 即视为到位
        robot.MovJSync(joint_p1, 40.0, 100.0, opts);
        std::cout << "  ✓ 到达（自定义选项）\n\n";

        robot.MovJSync(joint_home, 40.0, 100.0);
        std::cout << "全部 Sync 演示完成。\n";

    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        robot.Disconnect();
        return 1;
    }

    robot.Disconnect();
    return 0;
}
