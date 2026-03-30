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
        std::cout << "\n[Project Status] " << ps.state << std::endl;
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

    // 1. 实例化参数对象，传入主程序代码
    Codroid::RunScriptParams scriptData("print(v1)\nrunScript(\"t1\")\nprint(2)\n");

    // 2. 添加子线程、子程序、中断
    scriptData.subThreads["t1"]  = "print(3)\nwait(2000)callSubprogram(\"p1\")";
    scriptData.subPrograms["p1"] = "print(v2)";
    scriptData.interrupts["in1"] = "print(5)";

    // 3. 添加变量 (由于 vars 定义为 json 对象，可以直接赋不同类型的值)
    scriptData.vars["v1"] = 1;         // 整数类型
    scriptData.vars["v2"] = "hello";   // 字符串类型

    // 4. 调用接口发送
    auto res = robot.runScript(scriptData, 1);

    Codroid::CodroidControlInterface::printResponse(res);
    
    // 等待状态变为 2 (Running)
    // 加上超时保护(比如最多等2秒)，防止网络丢包导致死循环
    int timeout_ms = 2000;
    while (current_project_state != 2 && timeout_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timeout_ms -= 10;
    }

    if (current_project_state == 2) {
        std::cout << "Script is running. Waiting for completion..." << std::endl;
        
        // 第二步：只要状态是 2，就一直等（说明正在运行）
        while (current_project_state == 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << "Script finished execution!" << std::endl;
    } else {
        std::cout << "Warning: Script did not start within the timeout period." << std::endl;
    }
    

    // 调用下电接口
    std::cout << "Sending SwitchOff command..." << std::endl;
    auto resOff = robot.switchOff(102);
    Codroid::CodroidControlInterface::printResponse(resOff);

    // 断开连接 (析构函数也会自动处理，但手动断开是好习惯)
    robot.disconnect();
    std::cout << "Connection closed." << std::endl;

    return 0;
}