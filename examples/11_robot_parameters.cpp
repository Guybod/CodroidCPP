/**
 * @file 11_robot_parameters.cpp
 * @brief 机器人设置参数（协议 19.2~19.7）：读表、改工具/负载/用户坐标系槽位。
 *
 * 单槽位修改会先 Get 再 patch，其它编号不被改写；对外序号仅 1~15。
 * 固件要求 ≥ 2.3.3.43。仅需：#include "Codroid/client.hpp"
 */
#include "Codroid/client.hpp"

#include <iostream>
#include <optional>
#include <string>

namespace {

using ClientRobotFrame = Codroid::CodroidClient::ClientRobotFrame;
using ClientRobotPayload = Codroid::CodroidClient::ClientRobotPayload;
using ClientRobotParameters = Codroid::CodroidClient::ClientRobotParameters;

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (result.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << result.error_msg << "\n";
    }
}

std::optional<ClientRobotFrame> find_frame(const std::vector<ClientRobotFrame>& frames, int id) {
    for (const auto& f : frames) {
        if (f.id == id)
            return f;
    }
    return std::nullopt;
}

std::optional<ClientRobotPayload> find_payload(const std::vector<ClientRobotPayload>& frames, int id) {
    for (const auto& f : frames) {
        if (f.id == id)
            return f;
    }
    return std::nullopt;
}

void print_frame(const char* label, const ClientRobotFrame& f) {
    std::cout << label << " id=" << f.id << " [x,y,z,a,b,c]=["
              << f.x << ", " << f.y << ", " << f.z << ", "
              << f.a << ", " << f.b << ", " << f.c << "]\n";
}

void print_payload(const char* label, const ClientRobotPayload& p) {
    std::cout << label << " id=" << p.id << " [m,mx,my,mz]=["
              << p.m << ", " << p.mx << ", " << p.my << ", " << p.mz << "]\n";
}

void print_summary(const ClientRobotParameters& p) {
    std::cout << "--- 机器人参数摘要 ---\n"
              << "默认工具=" << p.default_tool_id
              << " 默认负载=" << p.default_payload_id
              << " 默认用户坐标=" << p.default_coordinate_id
              << " 最大负载=" << p.max_payload << "\n"
              << "工具表=" << p.tool.size()
              << " 负载表=" << p.payload.size()
              << " 用户坐标表=" << p.coordinate.size() << "\n";

    if (auto t2 = find_frame(p.tool, 2))
        print_frame("工具[2]", *t2);
    if (auto pl1 = find_payload(p.payload, 1))
        print_payload("负载[1]", *pl1);
    if (auto c2 = find_frame(p.coordinate, 2))
        print_frame("用户坐标[2]", *c2);
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

    // 19.7：读取设置界面完整参数
    const auto before = robot.GetRobotParameters(robot.NextRequestId());
    if (!before.valid) {
        std::cerr << "GetRobotParameters 失败（检查连接 / 固件 ≥ 2.3.3.43）\n";
        robot.Disconnect();
        return 1;
    }
    std::cout << "初始参数：\n";
    print_summary(before);

    // 19.4：修改 2 号工具坐标系（演示后可按需恢复，下方 restore 已注释）
    constexpr int kDemoToolId = 2;
    const auto orig_tool2 = find_frame(before.tool, kDemoToolId);
    if (!orig_tool2) {
        std::cerr << "工具 id " << kDemoToolId << " 不在表中\n";
    } else {
        ClientRobotFrame demo_tool = *orig_tool2;
        demo_tool.x = orig_tool2->x + 1.0;
        demo_tool.y = orig_tool2->y + 1.0;

        print_result("SetToolFrame", robot.SetToolFrame(kDemoToolId, demo_tool, robot.NextRequestId()));

        const auto mid = robot.GetRobotParameters(robot.NextRequestId());
        if (mid.valid) {
            if (auto t2 = find_frame(mid.tool, kDemoToolId))
                print_frame("工具[2] 修改后", *t2);
        }
        // print_result("恢复工具[2]", robot.SetToolFrame(kDemoToolId, *orig_tool2, robot.NextRequestId()));
    }

    // 19.5：修改 1 号负载
    constexpr int kDemoPayloadId = 1;
    const auto orig_payload1 = find_payload(before.payload, kDemoPayloadId);
    if (!orig_payload1) {
        std::cerr << "负载 id " << kDemoPayloadId << " 不在表中\n";
    } else {
        ClientRobotPayload demo_payload = *orig_payload1;
        demo_payload.m = orig_payload1->m + 0.1;

        print_result("SetPayloadFrame",
                     robot.SetPayloadFrame(kDemoPayloadId, demo_payload, robot.NextRequestId()));
        // print_result("恢复负载[1]", robot.SetPayloadFrame(kDemoPayloadId, *orig_payload1, robot.NextRequestId()));
    }

    // 19.6：修改 2 号用户坐标系
    constexpr int kDemoCoorId = 2;
    const auto orig_coor2 = find_frame(before.coordinate, kDemoCoorId);
    if (!orig_coor2) {
        std::cerr << "用户坐标 id " << kDemoCoorId << " 不在表中\n";
    } else {
        ClientRobotFrame demo_coor = *orig_coor2;
        demo_coor.z = orig_coor2->z + 1.0;

        print_result("SetUserCoordinateFrame",
                     robot.SetUserCoordinateFrame(kDemoCoorId, demo_coor, robot.NextRequestId()));
        // print_result("恢复用户坐标[2]", robot.SetUserCoordinateFrame(kDemoCoorId, *orig_coor2, robot.NextRequestId()));
    }

    const auto after = robot.GetRobotParameters(robot.NextRequestId());
    if (after.valid) {
        std::cout << "最终参数：\n";
        print_summary(after);
    }

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
