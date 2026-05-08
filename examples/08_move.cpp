#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "Codroid/CodroidController.h"

std::string now() {
    auto t = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(t);
    std::tm bt = *std::localtime(&timer);

    std::ostringstream oss;
    oss << "[" << std::put_time(&bt, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count()
        << "] ";
    return oss.str();
}

void checkResponse(const std::string& action, const Codroid::Response& res) {
    if (!res.error_msg.empty()) {
        std::cerr << now() << action << " Failed: " << res.error_msg << std::endl;
    } else {
        std::cout << now() << action << " Success" << std::endl;
    }
}

void startMove(Codroid::CodroidController& robot) {
    try {
        robot.switchOn();
        robot.stopMove();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<double> homeJoints = {0.0, 0.0, 90.0, 0.0, 90.0, 0.0};
        std::cout << "\n" << now() << "1. Sending movJ non-blocking array mode" << std::endl;
        auto res1 = robot.movJ(homeJoints, 50, 100);
        checkResponse("movJ Array", res1);

        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::vector<double> handelJoints = {927.504,214.495,598.998,179.999,0,-90};
        std::cout << "\n" << now() << "2. Sending movL non-blocking array mode" << std::endl;
        auto res2 = robot.movL(handelJoints, 500, 1000);
        checkResponse("movL Array", res2);

        std::this_thread::sleep_for(std::chrono::seconds(2));
        robot.switchOff();
    } catch (const std::exception& e) {
        std::cerr << now() << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::string robot_ip = "192.168.8.136";
    std::string local_pc_ip = "192.168.8.150";
    int robot_port = 9001;

    Codroid::CodroidController robot;

    std::atomic<bool> cri_poll{true};
    std::thread cri_thread([&]() {
        while (cri_poll.load()) {
            auto s = robot.getRobotRealtimeState();
            if (s.data_valid && !s.joint_position.empty()) {
                std::cout << "\rtimestamp=" << s.timestamp_ms 
                          << " joint1(deg)=" << std::fixed
                          << std::setprecision(4) << s.joint_position[0] 
                          << " joint2(deg)=" << std::fixed
                          << std::setprecision(4) << s.joint_position[1]
                          << " joint3(deg)=" << std::fixed
                          << std::setprecision(4) << s.joint_position[2]
                          << " joint4(deg)=" << std::fixed
                          << std::setprecision(4) << s.joint_position[3]
                          << " joint5(deg)=" << std::fixed
                          << std::setprecision(4) << s.joint_position[4]
                          << " joint6(deg)=" << std::fixed
                          << std::setprecision(4) << s.joint_position[5]
                          << " in_motion=" << (s.in_motion ? "Y" : "N") << "    " << std::flush;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << std::endl;
    });

    std::cout << now() << "Connecting to robot (CRI UDP target IP=" << local_pc_ip << ")..." << std::endl;
    if (!robot.connect(robot_ip, robot_port, local_pc_ip)) {
        cri_poll = false;
        cri_thread.join();
        std::cerr << now() << "Failed to connect" << std::endl;
        return -1;
    }

    std::cout << now() << "Connected to robot successfully" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    startMove(robot);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    cri_poll = false;
    cri_thread.join();

    robot.disconnect();
    std::cout << now() << "Disconnected. Test finished" << std::endl;

    return 0;
}
