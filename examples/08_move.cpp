#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include "Codroid/CodroidControlInterface.h"
#include "Codroid/CodroidSubscriber.h"

// 高精度时间戳函数 [HH:MM:SS.ms]
std::string now() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    
    std::ostringstream oss;
    oss << "[" << std::put_time(&bt, "%H:%M:%S") << "." 
        << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    return oss.str();
}

// 辅助函数：带时间戳的输出
void checkResponse(const std::string& action, const Codroid::Response& res) {
    if (!res.error_msg.empty()) {
        std::cerr << now() << action << " Failed: " << res.error_msg << std::endl;
    } else {
        std::cout << now() << action << " Success" << std::endl;
    }
}

void startMove(Codroid::CodroidControlInterface& robot) {
    try {
        robot.switchOn();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // 打印当前位姿
        std::vector<double> homeJoints = {90.0, 0.0, 90.0, 0.0, 90.0, 0.0};
        std::cout << "\n" << now() << "1. Sending movJ non-blocking array mode" << std::endl;
        auto res1 = robot.movJ(homeJoints, 50, 100); 
        checkResponse("movJ Array", res1);
        
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::vector<double> handelJoints = {-109.201,326.325,409.337,-179.999,0,0.001};
        std::cout << "\n" << now() << "1. Sending movJ non-blocking array mode" << std::endl;
        auto res2 = robot.movL(handelJoints, 500, 1000); 
        checkResponse("movJLArray", res2);

        std::this_thread::sleep_for(std::chrono::seconds(2));
        robot.switchOff();
    } 
    catch (const std::exception& e) {
        std::cerr << now() << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    Codroid::CodroidControlInterface robot;
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

    std::string robot_ip = "192.168.101.100"; 
    int robot_port = 9001;

    std::cout << now() << "Connecting to robot..." << std::endl;
    if (!robot.connect(robot_ip, robot_port)) {
        std::cerr << now() << "Failed to connect" << std::endl;
        return -1;
    }

    if (!sub.connect(robot_ip))
    {
        std::cerr << now() << "Failed to connect sub" << std::endl;
        return -1;
    }
    else{
        sub.subscribe("RobotStatus", 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        sub.subscribe("RobotPosture", 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    
    std::cout << now() << "Connected to robot successfully" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    
    startMove(robot);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    sub.disconnect();
    robot.disconnect();
    std::cout << now() << "Disconnected. Test finished" << std::endl;
    
    return 0;
}