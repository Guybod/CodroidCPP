#include "codroid/client.hpp"
#include "codroid/cri_realtime_dispatcher.hpp"
#include "codroid/trajectory_generator.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

bool wait_first_cri(Codroid::CodroidClient& robot, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = robot.GetRobotRealtimeState();
        if (state.data_valid && state.timestamp_ms > 0 && state.joint_position.size() >= 6) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_realtime_mode(Codroid::CodroidClient& robot, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (robot.GetRobotRealtimeState().realtime_control_mode) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

void print_result(const char* action, const Codroid::CommandResult& result) {
    if (!result.Ok()) {
        std::cerr << action << " failed: " << result.error_msg << "\n";
    }
}

} // namespace

int main() {
    // 按现场网络修改。local_ip 用于接收控制器 CRI UDP 状态推送。
    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";
    // 必须与 StartCriControl 的 durationMs 以及 SendTrajectory 的 period_ms 保持一致。
    const int duration_ms = 4;

    Codroid::CodroidClient robot;
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, 9001, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed\n";
        return 1;
    }

    if (!wait_first_cri(robot, std::chrono::seconds(5))) {
        std::cerr << "Timeout waiting for CRI frame\n";
        robot.Disconnect();
        return 1;
    }

    // 以当前 CRI 关节角作为轨迹起点，避免从假定零位突然跳变。
    auto start = robot.GetRobotRealtimeState();
    std::array<double, 6> start_joint{};
    for (int i = 0; i < 6; ++i) {
        start_joint[static_cast<size_t>(i)] = start.joint_position[static_cast<size_t>(i)];
    }

    const auto start_result = robot.StartCriControl(1, duration_ms, 5, robot.NextRequestId());
    print_result("StartCriControl", start_result);
    if (!start_result.Ok()) {
        robot.Disconnect();
        return 1;
    }

    if (!wait_realtime_mode(robot, std::chrono::seconds(3))) {
        std::cerr << "Timeout waiting for realtime_control_mode\n";
        robot.StopCriControl(robot.NextRequestId());
        robot.Disconnect();
        return 1;
    }

    Codroid::TrajectoryRequest req;
    req.space = Codroid::TrajectorySpace::Joint;
    req.profile = Codroid::TrajectoryProfile::Cubic;
    // 250Hz = 1000 / 4ms；频率和实时控制周期保持一致。
    req.frequency_hz = 1000.0 / duration_ms;
    req.speed = 30.0;
    req.acceleration = 120.0;

    const std::array<double, 6> target{{0.0, 0.0, 90.0, 0.0, 90.0, 0.0}};
    auto trajectory = Codroid::TrajectoryGenerator::Generate(start_joint, target, req);

    std::cout << "Send joint trajectory: points=" << trajectory.size() << "\n";
    // convert_to_si=true：上层传 deg/mm，发送前自动转换为控制器线上 rad/m。
    Codroid::CriRealtimeDispatcher dispatcher(robot_ip, 9030, true);
    dispatcher.SendTrajectory(trajectory, Codroid::TrajectorySpace::Joint, duration_ms);
    dispatcher.Close();

    print_result("StopCriControl", robot.StopCriControl(robot.NextRequestId()));
    const int udp_port = robot.GetCriUdpListenPort();
    if (udp_port > 0) {
        print_result("StopCriDataPush", robot.StopCriDataPush(local_ip, udp_port, robot.NextRequestId()));
    }
    robot.Disconnect();

    std::cout << "Done.\n";
    return 0;
}
