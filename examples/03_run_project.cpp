/**
 * @file 03_run_project.cpp
 * @brief 工程控制：按索引运行、暂停、恢复、停止。
 *
 * 仅需：#include "Codroid/client.hpp"
 * 运行前修改 robot_ip，并将 RunByIndex 参数改为现场存在的工程索引。
 */
#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

void print_result(const char* action, const Codroid::CommandResult& r) {
    if (r.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << r.error_msg << "\n";
    }
}

}  // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.1.136";

    Codroid::CodroidClient robot;
    if (!robot.Connect(robot_ip)) {
        std::cerr << "连接失败\n";
        return 1;
    }

    print_result("切远程(经自动)", robot.EnterRemoteModeViaAuto(robot.NextRequestId()));
    print_result("上电", robot.SwitchOn(robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 按控制器工程列表的索引运行（0 仅为演示，请改成实际索引）
    print_result("按索引运行工程(0)", robot.RunByIndex(0, robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::seconds(5));

    print_result("暂停工程", robot.PauseProject(robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    print_result("恢复工程", robot.ResumeProject(robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    print_result("停止工程", robot.StopProject(robot.NextRequestId()));

    print_result("下电", robot.SwitchOff(robot.NextRequestId()));
    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
