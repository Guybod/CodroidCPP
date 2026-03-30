#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidControlInterface.h"


void moveToExample(Codroid::CodroidControlInterface& robot) {
    // 场景：规划运动到指定点
    // 关节角度
    auto jointtarget = Codroid::MoveToTarget::Joint({0, 0, 90, 0, 90, 0});

    // 笛卡尔位置    
    auto cartesiantarget = Codroid::MoveToTarget::Cartesian({278.823,335.857,1018.803,-101.953,23.121,-28.329});

    // Linear motion to joint angle point / 直线运动到关节角度
    Codroid::MoveToParams params1(Codroid::MoveToType::Line, jointtarget);
    // Linear motion to the Cartesian coordinate point / 直线运动到笛卡尔坐标点
    Codroid::MoveToParams params2(Codroid::MoveToType::Line, cartesiantarget);
    // joint movement to joint angle point / 关节运动到关节角度点
    Codroid::MoveToParams params3(Codroid::MoveToType::Joint, jointtarget);
    // joint movement to Cartesian coordinate point / 关节运动到笛卡尔坐标点
    Codroid::MoveToParams params4(Codroid::MoveToType::Joint, cartesiantarget);

    // 1. send start command (params1) / 发送启动指令
    auto res1 = robot.moveTo(params1);
    if (!res1.error_msg.empty()) {
        std::cerr << "Failed to start MoveTo: " << res1.error_msg << std::endl;
        return;
    }

    // 2. keep maintaining heartbeat (simulating motion for 3 seconds) / 保持心跳 (模拟运动持续3秒)
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.moveToHeartbeat();
        std::cout << "MoveTo 心跳中..." << std::endl;
    }

    // 1. send start command (params2) / 发送启动指令
    auto res2 = robot.moveTo(params2);
    if (!res2.error_msg.empty()) {
        std::cerr << "Failed to start MoveTo: " << res2.error_msg << std::endl;
        return;
    }

    // 2. keep maintaining heartbeat (simulating motion for 3 seconds) / 保持心跳 (模拟运动持续3秒)
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.moveToHeartbeat();
        std::cout << "MoveTo 心跳中..." << std::endl;
    }
    
    // 1. send start command (params3) / 发送启动指令
    auto res3 = robot.moveTo(params3);
    if (!res3.error_msg.empty()) {
        std::cerr << "Failed to start MoveTo: " << res3.error_msg << std::endl;
        return;
    }

    // 2. keep maintaining heartbeat (simulating motion for 3 seconds) / 保持心跳 (模拟运动持续3秒)
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.moveToHeartbeat();
        std::cout << "MoveTo 心跳中..." << std::endl;
    }

    // 1. send start command (params4) / 发送启动指令
    auto res4 = robot.moveTo(params4);
    if (!res4.error_msg.empty()) {
        std::cerr << "Failed to start MoveTo: " << res4.error_msg << std::endl;
        return;
    }

    // 2. keep maintaining heartbeat (simulating motion for 3 seconds) / 保持心跳 (模拟运动持续3秒)
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.moveToHeartbeat();
        std::cout << "MoveTo 心跳中..." << std::endl;
    }

    // 1. send start command (Home)/ 发送启动指令
    auto res5 = robot.moveTo(Codroid::MoveToParams(Codroid::MoveToType::Home));
    if (!res5.error_msg.empty()) {
        std::cerr << "Failed to start MoveTo: " << res5.error_msg << std::endl;
        return;
    }

    // 2. keep maintaining heartbeat (simulating motion for 3 seconds) / 保持心跳 (模拟运动持续3秒)
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.moveToHeartbeat();
        std::cout << "MoveTo 心跳中..." << std::endl;
    }

    // 1. send start command (Candle)/ 发送启动指令
    auto res6 = robot.moveTo(Codroid::MoveToParams(Codroid::MoveToType::Candle));
    if (!res6.error_msg.empty()) {
        std::cerr << "Failed to start MoveTo: " << res6.error_msg << std::endl;
        return;
    }

    // 2. keep maintaining heartbeat (simulating motion for 3 seconds) / 保持心跳 (模拟运动持续3秒)
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        robot.moveToHeartbeat();
        std::cout << "MoveTo 心跳中..." << std::endl;
    }

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
    std::cout << "Starting moveTo test..." << std::endl;
    moveToExample(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    robot.disconnect();
    return 0;
}