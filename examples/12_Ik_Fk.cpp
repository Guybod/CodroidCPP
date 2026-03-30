#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidControlInterface.h"


void ik_fk(Codroid::CodroidControlInterface& robot) {
    // 基础正解
    Codroid::FKParams fk( {0, 0, 90, 0, 90, 0} );
    std::vector<double> pos = robot.forwardKinematics(fk);
    std::cout << "Forward Kinematics Result: [x, y, z, a, b, c] = ";
    for (double val : pos) {
        std::cout << std::fixed << std::setprecision(2) << val << " ";
    }
    std::cout << std::endl;

    // 带工具坐标系的正解
    Codroid::FKParams fkComplex;
    fkComplex.jp = {10, 20, 30, 40, 50, 60};
    fkComplex.tool = {0, 0, 100, 0, 0, 0}; // 考虑了 100mm 长的工具
    std::vector<double> posComplex = robot.forwardKinematics(fkComplex);

    // 逆解
    Codroid::IKParams ik( pos );
    std::vector<double> joints = robot.inverseKinematics(ik);
    std::cout << "Inverse Kinematics Result: [j1, j2, j3, j4, j5, j6] = ";
    for (double val : joints) {
        std::cout << std::fixed << std::setprecision(2) << val << " ";
    }
    std::cout << std::endl;

}

int main() {
    Codroid::CodroidControlInterface robot;
    std::string robot_ip = "192.168.1.136"; // 替换为实际的机器人 IP 地址

    if (!robot.connect(robot_ip)) {
        std::cerr << "Failed to connect to robot." << std::endl;
        return -1;
    }
    std::cout << "Connected to robot successfully!" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Starting ik_fk test..." << std::endl;
    ik_fk(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    robot.disconnect();
    return 0;
}