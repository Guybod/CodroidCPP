#include "Codroid/client.hpp"
#include "Codroid/console_utf8.hpp"

#include <iostream>
#include <string>

namespace {

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (result.Ok()) {
        std::cout << action << " OK\n";
    } else {
        std::cerr << action << " failed: " << result.error_msg << "\n";
    }
}

} // namespace

int main() {
    Codroid::InitConsoleUtf8();

    // 按现场网络修改。
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    const int do_port = 0;
    // DO/DI 端口号从控制器 IO 配置中确认；这里仅演示单点写入和读回。
    print_result("SetDo", robot.SetDo(do_port, 1, robot.NextRequestId()));
    std::cout << "DO" << do_port << "=" << robot.GetDo(do_port, robot.NextRequestId()) << "\n";
    print_result("SetDo reset", robot.SetDo(do_port, 0, robot.NextRequestId()));

    const int register_address = 49100;
    // 寄存器地址需要与工程或控制器变量映射一致。
    print_result("SetRegisterValue", robot.SetRegisterValue(register_address, 520.0, robot.NextRequestId()));
    std::cout << "Register " << register_address << "="
              << robot.GetRegisterValue(register_address, robot.NextRequestId()) << "\n";

    robot.Disconnect();
    std::cout << "Done.\n";
    return 0;
}
