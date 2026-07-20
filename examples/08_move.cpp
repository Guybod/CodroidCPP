/**
 * @file 08_move.cpp
 * @brief 点到点运动（MovJ/MovL/MovC）与多段路径 Move。
 *
 * 点位类型（勿混用裸 vector）：
 *   - JointPoint::Degrees({j1..j6})              关节目标，单位度
 *   - CartesianPoint::MmDeg({x,y,z,rx,ry,rz})   TCP，mm + 度
 *
 * 四种合法组合：MovJ+jp / MovJ+cp / MovL+cp / MovL+jp；MovC 仅笛卡尔。
 * 仅需：#include "Codroid/client.hpp"
 * 运行前修改 robot_ip / local_ip；指令为非阻塞，示例用 sleep 便于观察。
 */
#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (result.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << result.error_msg << "\n";
    }
}

/** Robot/move 返回后机构仍在运动；示例固定等待再发下一条 */
void wait_motion_hint() {
    std::this_thread::sleep_for(std::chrono::seconds(7));
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

    print_result("设置自动倍率", robot.SetAutoMoveRate(40, robot.NextRequestId()));
    print_result("停止当前运动", robot.StopRobotMove(robot.NextRequestId()));
    wait_motion_hint();

    // 测试点（与 AGENTS.md §5.1 S20 常量一致，可按现场示教修改）
    const auto joint_p1 = Codroid::JointPoint::Degrees({0.0, 0.0, 90.0, 0.0, 90.0, 0.0});
    const auto joint_p2 = Codroid::JointPoint::Degrees({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    const auto joint_p3 = Codroid::JointPoint::Degrees({-0.001, 7.826, 114.101, 31.926, 90, -0.002});

    const auto line_p1 =
        Codroid::CartesianPoint::MmDeg({927.511, 214.489, 486.524, 179.999, 0.0, -89.999});
    const auto line_p2 =
        Codroid::CartesianPoint::MmDeg({927.516, 214.489, 900, 180.0, 0.0, -89.999});

    const auto circle_end = line_p1;
    const auto circle_middle =
        Codroid::CartesianPoint::MmDeg({927.515, 27.23, 738.722, -180, 0, -89.999});

    // ---------- 一、单条指令（每次一次 Robot/move）----------
    std::cout << "\n=== 单条 MovJ / MovL / MovC ===\n";

    print_result("MovJ(关节→关节)", robot.MovJ(joint_p1, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovJ(回零姿态)", robot.MovJ(joint_p2, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovJ(再去 P1)", robot.MovJ(joint_p1, 40.0, 100.0, -1, -1, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovL(直线→TCP)", robot.MovL(line_p1, 550.0, 500.0, -1, -1, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    // MovJ + 笛卡尔：控制器内部逆解；需要稳定选解时用 MmDegWithRef + 当前关节作 rj
    print_result("MovJ(关节插补→TCP)", robot.MovJ(line_p2, 40.0, 100.0, -1, -1, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovL(直线→关节目标)", robot.MovL(joint_p3, 550.0, 500.0, -1, -1, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovL(line_p2)", robot.MovL(line_p2, 550.0, 500.0, -1, -1, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovC(圆弧)",
                 robot.MovC(circle_middle, circle_end, 120.0, 400.0, -1, -1, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    // ---------- 二、多段路径：一次 TCP 下发整段序列 ----------
    std::cout << "\n=== 多段 Move(path) ===\n";
    const std::vector<Codroid::ClientMoveInstruction> path = {
        Codroid::ClientMoveInstruction::MovJ(joint_p1, 40.0, 100.0),   // movJ + jp
        Codroid::ClientMoveInstruction::MovJ(line_p1, 40.0, 100.0),    // movJ + cp
        Codroid::ClientMoveInstruction::MovL(line_p2, 550.0, 500.0),   // movL + cp
        Codroid::ClientMoveInstruction::MovL(joint_p1, 550.0, 500.0),  // movL + jp
        Codroid::ClientMoveInstruction::MovC(circle_middle, circle_end, 120.0, 400.0),
    };

    print_result("Move(多段路径)", robot.Move(path, robot.NextRequestId()));
    wait_motion_hint();
    wait_motion_hint();
    wait_motion_hint();

    print_result("停止运动", robot.StopRobotMove(robot.NextRequestId()));
    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
