#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidControlInterface.h"


void getIO(Codroid::CodroidControlInterface& robot) {
    // 1. 批量查询示例
    std::vector<Codroid::IOInfo> query = {
        {"DI", 0}, 
        {"DO", 10}, 
        {"AO", 2}
    };
    auto results = robot.getIOValues(query);

    for (const auto& res : results) {
        std::cout << "IO Type: " << res.type << " Port: " << res.port << " Value: " << res.value << std::endl;
    }

    // 2. 快捷查询示例
    int di0 = robot.getDI(0);
    std::cout << "DI-0 is: " << di0 << std::endl;
}

void setIO(Codroid::CodroidControlInterface& robot) {
    // 1. 设置数字输出 10 号端口为 高电平
    auto res1 = robot.setDO(10, 1);
    if (res1.error_msg.empty()) {
        std::cout << "DO 10 set to 1 success" << std::endl;
    }

    // 2. 设置模拟输出 2 号端口为 4.44V (或对应比例值)
    auto res2 = robot.setAO(2, 4.44);
    if (res2.error_msg.empty()) {
        std::cout << "AO 2 set to 4.44 success" << std::endl;
    }

    // 3. 使用通用接口设置
    robot.setIOValue("DO", 5, 1.0);
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
    std::cout << "Starting IO test..." << std::endl;
    getIO(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    setIO(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    getIO(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    robot.disconnect();
    return 0;
}