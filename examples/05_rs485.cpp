/**
 * @file 05_rs485.cpp
 * @brief RS485：初始化、清空缓冲、写、读。
 *
 * 仅需：#include "Codroid/client.hpp"
 * 波特率 / 报文内容请按现场外设修改。
 */
#include "Codroid/client.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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

    // 初始化串口参数（数据位由 SDK 固定为 8）
    print_result("RS485 初始化",
                 robot.Rs485Init(115200, Codroid::RS485StopBits::One, Codroid::RS485Parity::None,
                                 robot.NextRequestId()));
    print_result("清空接收缓冲", robot.Rs485Flush(robot.NextRequestId()));

    // 示例 Modbus 风格帧（演示用，请换成现场协议）
    const std::vector<std::uint8_t> payload{0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
    print_result("RS485 写入", robot.Rs485Write(payload, robot.NextRequestId()));

    // 读回：返回控制器 db 的 nlohmann::json
    nlohmann::json read_db = robot.Rs485Read(7, 1000, robot.NextRequestId());
    std::cout << "RS485 读回 db: " << read_db.dump() << "\n";

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
