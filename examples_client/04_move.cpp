#include "codroid/client.hpp"

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

void wait_motion_hint() {
    // 普通 Robot/move 指令是非阻塞返回；示例用短暂等待方便观察每段动作。
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

} // namespace

int main() {
    // 按现场网络修改。
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

    // MovJ 使用关节角，单位 deg。
    const std::vector<double> joint_p1{0.0, 0.0, 90.0, 0.0, 90.0, 0.0};
    const std::vector<double> joint_p2{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    print_result("MovJ P1", robot.MovJ(joint_p1, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovJ P2", robot.MovJ(joint_p2, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovJ P1 again", robot.MovJ(joint_p1, 40.0, 100.0, robot.NextRequestId()));
    wait_motion_hint();

    // MovL / MovC 使用 TCP 位姿 [x,y,z,rx,ry,rz]，前三位 mm，后三位 deg。
    const std::vector<double> line_p1{927.511, 214.489, 486.524, 179.999, 0.0, -89.999};
    const std::vector<double> line_p2{927.516, -160.239, 486.534, 180.0, 0.0, -89.999};
    const std::vector<double> line_p3{927.515, -160.238, 1111.244, -179.999, 0.0, -89.999};

    print_result("MovL P1", robot.MovL(line_p1, 150.0, 500.0, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    print_result("MovL P2", robot.MovL(line_p2, 150.0, 500.0, {}, {}, robot.NextRequestId()));
    wait_motion_hint();

    // MovC 的第一个点是圆弧中间点，第二个点是圆弧终点。
    print_result("MovC P2 -> P3", robot.MovC(line_p2, line_p3, 120.0, 400.0, robot.NextRequestId()));
    wait_motion_hint();

    // MovePath 一次下发多条运动指令，控制器按数组顺序执行。
    std::vector<Codroid::ClientMoveInstruction> path;
    Codroid::ClientMoveInstruction path_j;
    path_j.type = Codroid::ClientMoveType::MovJ;
    path_j.speed = 40.0;
    path_j.acceleration = 100.0;
    path_j.target = Codroid::ClientMovePoint::Joint(joint_p2);
    path.push_back(path_j);

    Codroid::ClientMoveInstruction path_l;
    path_l.type = Codroid::ClientMoveType::MovL;
    path_l.speed = 150.0;
    path_l.acceleration = 500.0;
    path_l.target = Codroid::ClientMovePoint::Cartesian(line_p1);
    path.push_back(path_l);

    Codroid::ClientMoveInstruction path_c;
    path_c.type = Codroid::ClientMoveType::MovC;
    path_c.speed = 120.0;
    path_c.acceleration = 400.0;
    path_c.middle = Codroid::ClientMovePoint::Cartesian(line_p2);
    path_c.target = Codroid::ClientMovePoint::Cartesian(line_p3);
    path.push_back(path_c);

    print_result("MovePath MovJ + MovL + MovC", robot.MovePath(path, robot.NextRequestId()));
    wait_motion_hint();

    print_result("StopRobotMove", robot.StopRobotMove(robot.NextRequestId()));
    robot.Disconnect();

    std::cout << "Done.\n";
    return 0;
}
