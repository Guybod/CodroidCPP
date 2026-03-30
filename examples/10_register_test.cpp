#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidControlInterface.h"


void testRegisterAdvanced(Codroid::CodroidControlInterface& robot) {
    // 1. 设置寄存器
    robot.setRegisterValue(9032, 1);
    robot.setRegisterValue(49100, 12345);
    robot.setRegisterValue(49300, 123.45);

    // 2. 批量读取
    auto regs = robot.getRegisterValues({9032, 49100, 49300});
    for (const auto& r : regs) {
        std::cout << "Addr: " << r.address << " Val: " << r.value << std::endl;
    }

    // 3. 配置扩展数组
    robot.setExtendArrayType(1, Codroid::ExtendArrayType::Float32);

    // 4. 移除扩展数组
    robot.removeExtendArray(99);
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
    std::cout << "Starting register test..." << std::endl;
    testRegisterAdvanced(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    robot.disconnect();
    return 0;
}