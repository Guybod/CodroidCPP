#include "Codroid/CodroidSubscriber.h"
#include <iostream>
#include <iomanip>

int main() {
    Codroid::CodroidSubscriber sub;

    // 订阅位姿回调
    sub.setPostureCallback([](const Codroid::RobotPosture& p) {
        std::cout << "\r[Posture] Joint: [" 
                  << p.joint[0] << ", " << p.joint[1] << ", " << p.joint[2] << "] " << std::flush;
    });

    // 订阅状态回调
    sub.setStatusCallback([](const Codroid::RobotStatus& s) {
        std::cout << "\n[Status] Mode: " << s.mode 
                  << " | IsMoving: " << (s.isMoving ? "YES" : "NO") 
                  << " | StateName: " << s.stateName << std::endl;
    });

    // 订阅日志回调
    sub.setLogCallback([](const std::string& msg, int level) {
        std::cout << "\n[Robot Log] " << msg << std::endl;
    });

    std::cout << "Connecting to Subscriber Channel..." << std::endl;
    if (sub.connect("192.168.1.136", 9001)) { // 替换为真实 IP
        sub.subscribe("RobotStatus", 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        sub.subscribe("RobotPosture", 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        sub.subscribe("Log");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::cout << "Subscribed. Press Enter to quit..." << std::endl;
        std::cin.get();
    } else {
        std::cerr << "Failed to connect!" << std::endl;
    }

    return 0;
}