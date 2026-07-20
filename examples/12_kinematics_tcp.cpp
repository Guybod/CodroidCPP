/**
 * @file 12_kinematics_tcp.cpp
 * @brief TCP 正逆解：ForwardKinematics / InverseKinematics。
 *
 * 本地 KDL 已从 SDK 公开面移除；本示例走控制器 TCP 运动学接口。
 * 单位：关节度；TCP 为 mm + 度。仅需：#include "Codroid/client.hpp"
 */
#include "Codroid/client.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_vec(const char* label, const std::vector<double>& v) {
    std::cout << label << " = [";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) std::cout << ", ";
        std::cout << std::fixed << std::setprecision(3) << v[i];
    }
    std::cout << "]\n";
}

}  // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.1.136";

    Codroid::CodroidClient robot;
    if (!robot.Connect(robot_ip)) {
        std::cerr << "连接失败\n";
        return 1;
    }

    // 正解：六轴关节角（度）→ TCP [x,y,z,rx,ry,rz]（mm+度）
    Codroid::FKParams fk(std::vector<double>{0, 0, 90, 0, 90, 0});
    auto tcp = robot.ForwardKinematics(fk, robot.NextRequestId());
    print_vec("正解 TCP (mm+度)", tcp);

    // 带工具偏置的正解（tool：mm+度）
    Codroid::FKParams fk_tool;
    fk_tool.jp = {10, 20, 30, 40, 50, 60};
    fk_tool.tool = {0, 0, 100, 0, 0, 0};  // 工具 Z 方向 100mm
    auto tcp_tool = robot.ForwardKinematics(fk_tool, robot.NextRequestId());
    print_vec("正解(含工具)", tcp_tool);

    // 逆解：用上面正解得到的 TCP 求关节角（度）
    if (!tcp.empty()) {
        Codroid::IKParams ik(tcp);
        auto joints = robot.InverseKinematics(ik, robot.NextRequestId());
        print_vec("逆解关节 (度)", joints);
    }

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
