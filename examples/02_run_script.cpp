/**
 * @file 02_run_script.cpp
 * @brief 远程脚本：RunScript（主脚本 + 子线程/子程序/中断 + 注入变量）。
 *
 * 仅需：#include "Codroid/client.hpp"
 * 运行前修改 robot_ip；脚本内容按现场工程调整。
 */
#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

/** 打印指令结果：成功打 OK，失败打控制器错误信息 */
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
    // 仅 TCP 连接；本示例自行切远程并上电
    if (!robot.Connect(robot_ip)) {
        std::cerr << "连接失败\n";
        return 1;
    }

    // 远程脚本需在远程模式且已使能
    print_result("切远程(经自动)", robot.EnterRemoteModeViaAuto(robot.NextRequestId()));
    print_result("上电", robot.SwitchOn(robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 主脚本、子线程、子程序、中断脚本（Lua 字符串）
    const std::string main_script = "print(v1)\nrunScript(\"t1\")\nprint(2)\n";
    std::unordered_map<std::string, std::string> threads{
        {"t1", "print(3)\nwait(2000)callSubprogram(\"p1\")"}};
    std::unordered_map<std::string, std::string> programs{{"p1", "print(v2)"}};
    std::unordered_map<std::string, std::string> interrupts{{"in1", "print(5)"}};
    // 注入变量：用 nlohmann::json 对象（由 client.hpp 提供）
    nlohmann::json vars = {{"v1", 1}, {"v2", "hello"}};

    print_result("RunScript",
                 robot.RunScript(main_script, threads, programs, interrupts, vars, robot.NextRequestId()));

    // 示例用固定延时等待脚本跑完；产线建议订阅工程/脚本状态
    std::cout << "等待脚本执行（固定延时）...\n";
    std::this_thread::sleep_for(std::chrono::seconds(8));

    print_result("下电", robot.SwitchOff(robot.NextRequestId()));
    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
