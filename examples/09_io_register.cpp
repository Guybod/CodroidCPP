/**
 * @file 09_io_register.cpp
 * @brief IO（DO 读写）与寄存器读写示例。
 *
 * 仅需：#include "Codroid/client.hpp"
 * 端口号 / 寄存器地址请按控制器配置修改。
 */
#include "Codroid/client.hpp"

#include <iostream>
#include <string>

namespace {

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (result.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << result.error_msg << "\n";
    }
}

}  // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.1.136";
    const std::string local_ip = "192.168.1.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn 失败\n";
        return 1;
    }

    // ----- 数字输出：写 1 → 读回 → 写 0 -----
    const int do_port = 0;  // 从控制器 IO 配置确认端口号
    print_result("SetDo=1", robot.SetDo(do_port, 1, robot.NextRequestId()));
    std::cout << "DO" << do_port << " 读回=" << robot.GetDo(do_port, robot.NextRequestId()) << "\n";
    print_result("SetDo=0", robot.SetDo(do_port, 0, robot.NextRequestId()));

    // ----- 寄存器：写入再读回（地址需与工程映射一致）-----
    const int register_address = 49100;
    print_result("写寄存器", robot.SetRegisterValue(register_address, 520.0, robot.NextRequestId()));
    std::cout << "寄存器 " << register_address << " 读回="
              << robot.GetRegisterValue(register_address, robot.NextRequestId()) << "\n";

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
