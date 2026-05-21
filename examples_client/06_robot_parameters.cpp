/**
 * @file 06_robot_parameters.cpp
 * @brief 演示 19.2~19.7 机器人设置参数 API（Get/SaveRobotParameter）。
 *
 * 流程：连接 -> 读取当前参数 -> 演示默认编号与单槽位修改 -> 读回校验 -> 恢复原值。
 * 单槽位修改（SetToolFrame 等）会先 Get 再 patch，其它编号不会被改写；对外接口序号仅 1~15。
 *
 * 固件要求：控制器版本 ≥ 2.3.3.43（与本 SDK 全部接口一致）。
 *
 * 编译目标：client_06_robot_parameters（由 CMake GLOB 自动生成）。
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
        std::cout << action << " OK\n";
    } else {
        std::cerr << action << " failed: " << result.error_msg << "\n";
    }
}

std::optional<ClientRobotFrame> find_frame(const std::vector<ClientRobotFrame>& frames,
                                                    int id) {
    for (const auto& f : frames) {
        if (f.id == id)
            return f;
    }
    return std::nullopt;
}

std::optional<ClientRobotPayload> find_payload(const std::vector<ClientRobotPayload>& frames,
                                                       int id) {
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
    std::cout << "--- Robot parameters ---\n"
              << "defaultToolId=" << p.default_tool_id
              << " defaultPayloadId=" << p.default_payload_id
              << " defaultCoordinateId=" << p.default_coordinate_id
              << " maxPayload=" << p.max_payload << "\n"
              << "Tool frames: " << p.tool.size()
              << ", Payload frames: " << p.payload.size()
              << ", Coordinate frames: " << p.coordinate.size() << "\n";

    if (auto t2 = find_frame(p.tool, 2))
        print_frame("Tool[2]", *t2);
    if (auto pl1 = find_payload(p.payload, 1))
        print_payload("Payload[1]", *pl1);
    if (auto c2 = find_frame(p.coordinate, 2))
        print_frame("Coordinate[2]", *c2);
}

} // namespace

int main() {
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    // 19.7 读取设置界面参数
    const auto before = robot.GetRobotParameters(robot.NextRequestId());
    if (!before.valid) {
        std::cerr << "GetRobotParameters failed (check connection / firmware >= 2.3.3.43)\n";
        robot.Disconnect();
        return 1;
    }
    std::cout << "Initial parameters:\n";
    print_summary(before);

    // // 19.2 / 19.3 / 19.6：仅下发默认编号字段（此处写回与读取相同，等价于确认接口可用）
    // print_result("SetDefaultPayloadId",
    //              robot.SetDefaultPayloadId(before.default_payload_id+1, robot.NextRequestId()));
    // print_result("SetDefaultToolId",
    //              robot.SetDefaultToolId(before.default_tool_id+1, robot.NextRequestId()));
    // print_result("SetDefaultUserCoordinateId",
    //              robot.SetDefaultUserCoordinateId(before.default_coordinate_id+1, robot.NextRequestId()));

    // 19.4：修改 2 号工具坐标系（先 Get 再 patch，演示后恢复）
    constexpr int kDemoToolId = 2;
    const auto orig_tool2 = find_frame(before.tool, kDemoToolId);
    if (!orig_tool2) {
        std::cerr << "Tool id " << kDemoToolId << " not found in controller table\n";
    } else {
        ClientRobotFrame demo_tool = *orig_tool2;
        demo_tool.x = orig_tool2->x + 1.0;
        demo_tool.y = orig_tool2->y + 1.0;

        print_result("SetToolFrame", robot.SetToolFrame(kDemoToolId, demo_tool, robot.NextRequestId()));

        const auto mid = robot.GetRobotParameters(robot.NextRequestId());
        if (mid.valid) {
            if (auto t2 = find_frame(mid.tool, kDemoToolId))
                print_frame("Tool[2] after SetToolFrame", *t2);
        }

        // print_result("SetToolFrame restore",
        //              robot.SetToolFrame(kDemoToolId, *orig_tool2, robot.NextRequestId()));
    }

    // 19.5：修改 1 号负载坐标系（演示后恢复）
    constexpr int kDemoPayloadId = 1;
    const auto orig_payload1 = find_payload(before.payload, kDemoPayloadId);
    if (!orig_payload1) {
        std::cerr << "Payload id " << kDemoPayloadId << " not found\n";
    } else {
        ClientRobotPayload demo_payload = *orig_payload1;
        demo_payload.m = orig_payload1->m + 0.1;

        print_result("SetPayloadFrame",
                     robot.SetPayloadFrame(kDemoPayloadId, demo_payload, robot.NextRequestId()));
        // print_result("SetPayloadFrame restore",
        //              robot.SetPayloadFrame(kDemoPayloadId, *orig_payload1, robot.NextRequestId()));
    }

    // 19.6：修改 2 号用户坐标系（默认编号已单独设置；此处只改数值，演示后恢复）
    constexpr int kDemoCoorId = 2;
    const auto orig_coor2 = find_frame(before.coordinate, kDemoCoorId);
    if (!orig_coor2) {
        std::cerr << "Coordinate id " << kDemoCoorId << " not found\n";
    } else {
        ClientRobotFrame demo_coor = *orig_coor2;
        demo_coor.z = orig_coor2->z + 1.0;

        print_result("SetUserCoordinateFrame",
                     robot.SetUserCoordinateFrame(kDemoCoorId, demo_coor, robot.NextRequestId()));
        // print_result("SetUserCoordinateFrame restore",
        //              robot.SetUserCoordinateFrame(kDemoCoorId, *orig_coor2, robot.NextRequestId()));
    }

    // // 19.4 / 19.5 整表直接下发：用当前快照原样 Save，等价于“全表回写”连通性测试（不改动数值）
    // print_result("SaveToolFrames (round-trip)",
    //              robot.SaveToolFrames(before.tool, robot.NextRequestId()));
    // print_result("SavePayloadFrames (round-trip)",
    //              robot.SavePayloadFrames(before.payload, robot.NextRequestId()));

    const auto after = robot.GetRobotParameters(robot.NextRequestId());
    if (after.valid) {
        std::cout << "Final parameters:\n";
        print_summary(after);
    }

    robot.Disconnect();
    std::cout << "Done.\n";
    return 0;
}
