#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidControlInterface.h"


void startJogging(Codroid::CodroidControlInterface& robot) {
    // 1. 启动 X 轴正向点动 (直线模式, 速度 0.5, 索引 1 代表 X)
    Codroid::JogParams p(Codroid::JogMode::Line, 0.5, 1);
    robot.jog(p);

    // 2. 开启一个循环或定时器发送心跳
    for(int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.jogHeartbeat();
        std::cout << "点动运行中..." << std::endl;
    }
    
    // 3. 停止点动（通常速度设为 0 或发送停止指令）
    robot.stopJog();
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
    std::cout << "Starting jogging test..." << std::endl;
    startJogging(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    robot.disconnect();
    return 0;
}