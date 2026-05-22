/**
 * @file 04_move.cpp
 * @brief 客户示例：点到点运动（MovJ/MovL/MovC）与多段路径（Move / Robot/move）。
 *
 * 点位类型（编译期区分，勿混用裸 vector）：
 *   - JointPoint::Degrees({j1..j6})     关节目标，单位 deg
 *   - CartesianPoint::MmDeg({x,y,z,rx,ry,rz})  TCP 目标，mm + deg
 *
 * 运动方式 × 点位类型（四种合法组合，单点 API 与路径工厂均支持）：
 *   | API / 工厂              | 参数类型        | 协议字段      |
 *   |-------------------------|-----------------|---------------|
 *   | MovJ / MovJ(jp)         | JointPoint      | movJ + jp     |
 *   | MovJ / MovJ(cp)         | CartesianPoint  | movJ + cp     |
 *   | MovL / MovL(cp)         | CartesianPoint  | movL + cp     |
 *   | MovL / MovL(jp)         | JointPoint      | movL + jp     |
 *
 * MovC / MovCircle 仅使用 CartesianPoint（中间点 + 终点）。
 * Move(path) 一次下发多段 ClientMoveInstruction，控制器按顺序执行。
 *
 * 构建：client_04_move（Linux 见 build_linux.sh）
 * 运行前修改 robot_ip / local_ip；非阻塞指令，示例用 sleep 便于观察。
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
        std::cout << action << " OK\n";
    } else {
        std::cerr << action << " failed: " << result.error_msg << "\n";
    }
}

/** Robot/move、MovJ 等返回后机械臂仍在动；示例固定等待 2s 再发下一条。 */
void wait_motion_hint() {
    std::this_thread::sleep_for(std::chrono::seconds(7));
}

} // namespace

int main() {
    // -------------------------------------------------------------------------
    // 连接（TCP 9001 + 切远程 + CRI 推送 + 上电）
    // -------------------------------------------------------------------------
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    print_result("SetAutoMoveRate", robot.SetAutoMoveRate(40, robot.NextRequestId()));
    print_result("StopRobotMove", robot.StopRobotMove(robot.NextRequestId()));
    wait_motion_hint();

    // -------------------------------------------------------------------------
    // 测试点（与 AGENTS.md §5.1 S20 movJ/movL 常量一致，可按现场示教值修改）
    // -------------------------------------------------------------------------
    const auto joint_p1 = Codroid::JointPoint::Degrees({0.0, 0.0, 90.0, 0.0, 90.0, 0.0});
    const auto joint_p2 = Codroid::JointPoint::Degrees({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    const auto joint_p3 = Codroid::JointPoint::Degrees({-0.001,7.826,114.101,31.926,90,-0.002});

    const auto line_p1 =
        Codroid::CartesianPoint::MmDeg({927.511, 214.489, 486.524, 179.999, 0.0, -89.999});
    const auto line_p2 =
        Codroid::CartesianPoint::MmDeg({927.516, 214.489, 900, 180.0, 0.0, -89.999});

    const auto circle_end = line_p1;
    const auto circle_middle =
        Codroid::CartesianPoint::MmDeg({927.515,27.23,738.722,-180,0,-89.999});
    // -------------------------------------------------------------------------
    // 一、单条指令（每条单独一次 Robot/move，等价于只含一段的 path）
    // -------------------------------------------------------------------------

    // movJ + jp：关节运动到关节角
    print_result("MovJ(joint_p1)", robot.MovJ(joint_p1, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovJ(joint_p2)", robot.MovJ(joint_p2, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovJ(joint_p1) again", robot.MovJ(joint_p1, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    // movL + cp：直线运动到 TCP 位姿
    print_result("MovL(line_p1)", robot.MovL(line_p1, 150.0, 500.0, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    // movJ + cp：关节运动到笛卡尔点（控制器逆解）
    // 若需稳定选解，可改为：
    //   auto cp = Codroid::CartesianPoint::MmDegWithRef({...tcp...}, robot.GetRobotRealtimeState().joint_position);
    print_result("MovJ(line_p1)", robot.MovJ(line_p2, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    // movL + jp：直线运动到关节目标
    print_result("MovL(joint_p2)", robot.MovL(joint_p3, 150.0, 500.0, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovL(line_p2)", robot.MovL(line_p2, 150.0, 500.0, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    // 圆弧：中间点 line_p2，终点 line_p3（均为 CartesianPoint）
    print_result("MovC(line_p2 -> line_p3)", robot.MovC(circle_middle, circle_end, 120.0, 400.0, robot.NextRequestId()));
    wait_motion_hint();

    // -------------------------------------------------------------------------
    // 二、多段路径 Move(path) — 一次 TCP 指令下发整段序列
    // ClientMoveInstruction::MovJ/MovL/MovC 根据「工厂名 + JointPoint/CartesianPoint」
    // 自动写入 targetPoint 的 jp 或 cp，无需手动拼 MovePoint。
    // -------------------------------------------------------------------------
    const std::vector<Codroid::ClientMoveInstruction> path = {
        Codroid::ClientMoveInstruction::MovJ(joint_p1, 40.0, 100.0),   // movJ + jp
        Codroid::ClientMoveInstruction::MovJ(line_p1, 40.0, 100.0),    // movJ + cp
        Codroid::ClientMoveInstruction::MovL(line_p2, 150.0, 500.0),   // movL + cp
        Codroid::ClientMoveInstruction::MovL(joint_p1, 150.0, 500.0),  // movL + jp
        Codroid::ClientMoveInstruction::MovC(circle_middle, circle_end, 120.0, 400.0),
    };

    print_result("Move(path)", robot.Move(path, robot.NextRequestId()));
    wait_motion_hint();
    wait_motion_hint();
    wait_motion_hint();
    print_result("StopRobotMove", robot.StopRobotMove(robot.NextRequestId()));
    robot.Disconnect();

    std::cout << "Done.\n";
    return 0;
}
