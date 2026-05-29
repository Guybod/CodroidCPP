/**
 * @file 07_sync_motion.cpp
 * @brief 客户示例：阻塞式运动 API（*Sync）。
 *
 * 演示 `MovJSync` / `MovLSync` / `MovCSync` / `MovCircleSync` / `MoveSync`。
 * 发送运动指令后自动轮询 CRI 数据，直到机器人稳定到达目标，无需手动 sleep。
 *
 * 前置条件：
 *   - 已使能；CRI 推送已启动（ConnectRemoteAndSwitchOn 传 local_ip 即自动启动）
 *
 * 构建：client_07_sync_motion
 * 运行前修改 robot_ip / local_ip。
 */

#include "Codroid/client.hpp"
#include "Codroid/console_utf8.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (result.Ok()) {
        std::cout << action << " OK\n";
    } else {
        std::cerr << action << " failed: " << result.error_msg << "\n";
    }
}

} // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    // 等待首帧 CRI 到达（*Sync 方法依赖 CRI 判断到达）
    std::cout << "Waiting for CRI data...\n";
    try {
        robot.WaitForCriData(5.0);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        robot.Disconnect();
        return 1;
    }
    std::cout << "CRI data ready.\n\n";

    // 测试点（与 AGENTS.md §5.1 S20 常量一致）
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
        // -----------------------------------------------------------------
        // 1. MovJSync — 阻塞式关节运动
        // -----------------------------------------------------------------
        std::cout << "[1] MovJSync(JointPoint)\n";
        robot.MovJSync(joint_p1, 40.0, 100.0);
        std::cout << "  ✓ Reached target\n\n";

        // -----------------------------------------------------------------
        // 2. MovLSync — 阻塞式直线运动
        // -----------------------------------------------------------------
        std::cout << "[2] MovLSync(CartesianPoint)\n";
        robot.MovLSync(cart_p1, 150.0, 500.0);
        std::cout << "  ✓ Reached target\n\n";

        // -----------------------------------------------------------------
        // 3. MovJSync + CartesianPoint — 关节插补到 TCP
        // -----------------------------------------------------------------
        std::cout << "[3] MovJSync(CartesianPoint)\n";
        robot.MovJSync(cart_p2, 40.0, 100.0);
        std::cout << "  ✓ Reached target\n\n";

        // -----------------------------------------------------------------
        // 4. MovCSync — 阻塞式圆弧运动
        // -----------------------------------------------------------------
        std::cout << "[4] MovCSync\n";
        robot.MovCSync(circle_middle, circle_end, 120.0, 400.0);
        std::cout << "  ✓ Reached target\n\n";

        // -----------------------------------------------------------------
        // 5. MoveSync — 阻塞式多段路径
        // -----------------------------------------------------------------
        std::cout << "[5] MoveSync (multi-segment path)\n";
        std::vector<Codroid::ClientMoveInstruction> path = {
            Codroid::ClientMoveInstruction::MovJ(joint_p1, 40.0, 100.0),
            Codroid::ClientMoveInstruction::MovL(cart_p1, 150.0, 500.0),
            Codroid::ClientMoveInstruction::MovL(joint_home, 150.0, 500.0),
        };
        robot.MoveSync(path);
        std::cout << "  ✓ All segments reached\n\n";

        // -----------------------------------------------------------------
        // 6. 自定义 MotionWaitOptions
        // -----------------------------------------------------------------
        std::cout << "[6] MovJSync with custom MotionWaitOptions\n";
        Codroid::MotionWaitOptions opts;
        opts.timeout_s = 30.0;
        opts.joint_tolerance_deg = 0.5;  // 放宽关节容差
        opts.settled_samples = 2;         // 连续 2 次稳定即可
        robot.MovJSync(joint_p1, 40.0, 100.0, opts);
        std::cout << "  ✓ Reached target (custom tolerance)\n\n";

        // 回 Home
        robot.MovJSync(joint_home, 40.0, 100.0);

        std::cout << "All sync motion demos completed.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        robot.Disconnect();
        return 1;
    }

    robot.Disconnect();
    return 0;
}
