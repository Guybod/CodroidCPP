#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include <atomic>

#include "../include/Codroid/CodroidControlInterface.h"
#include "../include/Codroid/CodroidSubscriber.h"

#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include <atomic>

#include "../include/Codroid/CodroidControlInterface.h"
#include "../include/Codroid/CodroidSubscriber.h"

int main() {
    // 1. 实例化控制类
    Codroid::CodroidControlInterface robot;
    Codroid::CodroidSubscriber sub;

    // 2. 配置机械臂 IP 和 端口 (请根据实际情况修改)
    std::string robot_ip = "192.168.1.136"; 
    int robot_port = 9001;  // 默认端口通常是 9001，连接有默认参数，可以不传

    // 3. 定义一个线程安全的原子变量，用于记录当前状态
    std::atomic<int> current_project_state{0};

    // 4. 设置工程状态回调
    sub.setProjectStatusCallback([&](const Codroid::ProjectState& ps) {
      switch (ps.state)
      {
      case 0:
        std::cout << "\n[Project Status] " << "空闲"<< std::endl;
        break;
      case 1:
        std::cout << "\n[Project Status] " << "正在加载"<< std::endl;
        break;
      case 2:
        std::cout << "\n[Project Status] " << "正在运行"<< std::endl;
        break;
      case 3:
        std::cout << "\n[Project Status] " << "暂停"<< std::endl;
        break;
      default:
        break;
      }  
        current_project_state = ps.state;// 线程安全赋值
    });

    std::cout << "Connecting to robot at " << robot_ip << ":" << robot_port << "..." << std::endl;

    // 3. 尝试连接
    if (!robot.connect(robot_ip)) {
        std::cerr << "Critical Error: Could not connect to the robot!" << std::endl;
        return -1;
    }
    if (sub.connect(robot_ip))
    {
      sub.subscribe("ProjectState", 100);
    }
    else{
      return -1;
    }
    
    std::cout << "Connected successfully!" << std::endl << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // 调用上电接口
    std::cout << "Sending SwitchOn command..." << std::endl;
    auto resOn = robot.switchOn(101);
    Codroid::CodroidControlInterface::printResponse(resOn);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto res = robot.runProjectByIndex(0);
    Codroid::CodroidControlInterface::printResponse(resOn);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto pauseres = robot.pauseProject();
    Codroid::CodroidControlInterface::printResponse(pauseres);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto resumeres = robot.resumeProject();
    Codroid::CodroidControlInterface::printResponse(resumeres);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    auto stopres = robot.stopProject();
    Codroid::CodroidControlInterface::printResponse(stopres);

    // 调用下电接口
    std::cout << "Sending SwitchOff command..." << std::endl;
    auto resOff = robot.switchOff(102);
    Codroid::CodroidControlInterface::printResponse(resOff);

    // 断开连接 (析构函数也会自动处理，但手动断开是好习惯)
    robot.disconnect();
    std::cout << "Connection closed." << std::endl;

    return 0;
}