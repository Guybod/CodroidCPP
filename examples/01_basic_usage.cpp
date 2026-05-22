#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidController.h"
#include "Codroid/console_utf8.hpp"

int main() {
    Codroid::InitConsoleUtf8();
    // 1. 实例化控制类
    Codroid::CodroidController robot;

    // 2. 配置机械臂 IP 和 端口 (请根据实际情况修改)
    std::string robot_ip = "192.168.1.136"; 
    int robot_port = 9001;  // 默认端口通常是 9001，连接有默认参数，可以不传

    std::cout << "Connecting to robot at " << robot_ip << ":" << robot_port << "..." << std::endl;

    // 3. 尝试连接（默认不开启 CRI UDP；若需推送可传第三参：本机 IP，如 connect(robot_ip, robot_port, "192.168.1.10")）
    if (!robot.connect(robot_ip, robot_port)) {
        std::cerr << "Critical Error: Could not connect to the robot!" << std::endl;
        return -1;
    }
    std::cout << "Connected successfully!" << std::endl << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // 调用上电接口
    std::cout << "Sending SwitchOn command..." << std::endl;
    auto resOn = robot.switchOn(101);
    Codroid::CodroidController::printResponse(resOn);

    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 调用下电接口
    std::cout << "Sending SwitchOff command..." << std::endl;
    auto resOff = robot.switchOff(102);
    Codroid::CodroidController::printResponse(resOff);

    // 断开连接 (析构函数也会自动处理，但手动断开是好习惯)
    robot.disconnect();
    std::cout << "Connection closed." << std::endl;

    return 0;
}