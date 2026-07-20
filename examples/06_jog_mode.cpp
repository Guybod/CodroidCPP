#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidController.h"
#include "Codroid/console_utf8.hpp"

namespace {

void printSend(int id, const std::string& ty, const nlohmann::json& db) {
    nlohmann::json req;
    req["id"] = id;
    req["ty"] = ty;
    req["db"] = db;
    std::cout << "[SEND] " << req.dump() << std::endl;
}

int jogCoorTypeToWire(Codroid::CoorType coorType) {
    return coorType == Codroid::CoorType::User ? 0 : 1;
}

}  // namespace

void startJogging(Codroid::CodroidController& robot) {
    const int id = 1;

    // 1. 启动 X 轴正向点动 (直线模式, 速度 0.5, 索引 1 代表 X)
    Codroid::JogParams p(Codroid::JogMode::Line, 0.5, 1);
    nlohmann::json jog_db;
    jog_db["mode"] = p.mode;
    jog_db["speed"] = p.speed;
    jog_db["index"] = p.index;
    jog_db["coorType"] = jogCoorTypeToWire(p.coorType);
    jog_db["coorId"] = p.coorId;
    printSend(id, "Robot/jog", jog_db);
    robot.jog(p, id);

    // 2. 开启一个循环或定时器发送心跳
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        printSend(id, "Robot/jogHeartbeat", nlohmann::json::object());
        robot.jogHeartbeat(id);
        std::cout << "点动运行中..." << std::endl;
    }

    // 3. 停止点动（通常速度设为 0 或发送停止指令）
    printSend(id, "Robot/stopJog", nlohmann::json::object());
    robot.stopJog(id);
}

int main() {
    Codroid::InitConsoleUtf8();
    Codroid::CodroidController robot;
    std::string robot_ip = "192.168.1.136"; // 替换为实际的机器人 IP 地址
    const int robot_port = 9001;

    if (!robot.connect(robot_ip, robot_port)) {
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
