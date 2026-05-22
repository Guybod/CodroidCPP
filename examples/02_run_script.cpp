#include <chrono>
#include <iostream>
#include <thread>

#include "../include/Codroid/CodroidController.h"
#include "Codroid/console_utf8.hpp"

int main() {
    Codroid::InitConsoleUtf8();
    Codroid::CodroidController robot;

    std::string robot_ip = "192.168.1.136";
    int robot_port = 9001;

    std::cout << "Connecting to robot at " << robot_ip << ":" << robot_port << "..." << std::endl;

    if (!robot.connect(robot_ip, robot_port)) {
        std::cerr << "Critical Error: Could not connect to the robot!" << std::endl;
        return -1;
    }

    std::cout << "Connected successfully!" << std::endl << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Sending SwitchOn command..." << std::endl;
    auto resOn = robot.switchOn(101);
    Codroid::CodroidController::printResponse(resOn);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Codroid::RunScriptParams scriptData("print(v1)\nrunScript(\"t1\")\nprint(2)\n");

    scriptData.subThreads["t1"] = "print(3)\nwait(2000)callSubprogram(\"p1\")";
    scriptData.subPrograms["p1"] = "print(v2)";
    scriptData.interrupts["in1"] = "print(5)";

    scriptData.vars["v1"] = 1;
    scriptData.vars["v2"] = "hello";

    auto res = robot.runScript(scriptData, 1);

    Codroid::CodroidController::printResponse(res);

    std::cout << "Waiting for script finish (fixed delay)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(8));

    std::cout << "Sending SwitchOff command..." << std::endl;
    auto resOff = robot.switchOff(102);
    Codroid::CodroidController::printResponse(resOff);

    robot.disconnect();
    std::cout << "Connection closed." << std::endl;

    return 0;
}
