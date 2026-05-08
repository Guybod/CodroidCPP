/**
 * @file 14_cri_trajectory.cpp
 * @brief CRI 实时控制完整流程 + `TRAJECTORY_ALGORITHM.md` §9 离线数值自检（同一可执行文件）。
 *
 * **流程**（与 `SDK_API_AND_DESIGN.md` §7.3 / `AGENTS.md` §6 一致）：
 *  1. （仅 CPU）跑 §9 轨迹发生器回归，失败则直接退出；
 *  2. TCP：`ConnectRemoteAndSwitchOn`（自动→远程、CRI 推送、上电）；
 *  3. 等待首帧 CRI（时间戳与关节有效）；
 *  4. `StartCriControl`，轮询 `realtime_control_mode`；
 *  5. 再读一帧状态，按 **`AGENTS.md` §5.2** 规划关节 / 笛卡尔多段轨迹并 UDP 下发。
 *
 * 三段轨迹与 §5.2 `CodroidCRITest` 常量一致；`SendTrajectory` 每种 `TrajectorySpace` 分次发送。
 *
 * 前置：`durationMs` = `period_ms`（如 4 ↔ 250 Hz）。
 */

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "codroid/client.hpp"
#include "codroid/cri_realtime_dispatcher.hpp"
#include "codroid/trajectory_generator.hpp"

namespace {

/** `AGENTS.md` §5.2 `joint` — 终点关节角（度） */
constexpr std::array<double, 6> kAgentJointP1{{0.0, 0.0, 90.0, 0.0, 90.0, 0.0}};
constexpr std::array<double, 6> kAgentJointP2{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};

constexpr std::array<double, 6> kAgentPathW1{{1139.996, 214.490, 899.010, -91.506, -0.001, -89.999}};
constexpr std::array<double, 6> kAgentPathW2{{1139.994, -222.730, 899.022, -91.506, -0.002, -136.466}};
constexpr std::array<double, 6> kAgentPathW3{{915.480, -73.000, 599.316, 166.910, -5.170, -90.726}};
constexpr std::array<double, 6> kAgentPathW4Home{{927.505, 214.495, 898.994, 180.000, 0.000, -90.000}};

bool approx(double a, double b, double eps = 1e-5) { return std::abs(a - b) <= eps; }

/** `TRAJECTORY_ALGORITHM.md` §9 — 不连控制器，仅验证 `TrajectoryGenerator`。 */
bool run_trajectory_algorithm_s9() {
    {
        std::array<double, 6> start = {0, 0, 0, 0, 0, 0};
        std::array<double, 6> target = {0, 0, 90, 0, 90, 0};
        Codroid::TrajectoryRequest req;
        req.space = Codroid::TrajectorySpace::Joint;
        req.profile = Codroid::TrajectoryProfile::Cubic;
        req.frequency_hz = 250;
        req.speed = 30.0;
        req.acceleration = 1.0;
        auto pts = Codroid::TrajectoryGenerator::Generate(start, target, req);
        if (pts.size() != 751u || !approx(pts[0].time_seconds, 0.0) || !approx(pts[750].time_seconds, 3.0))
            return false;
        if (!approx(pts[375].position[2], 45.0) || !approx(pts[375].position[4], 45.0))
            return false;
        for (int i = 0; i < 6; ++i) {
            if (!approx(pts[750].position[i], target[i], 1e-4))
                return false;
        }
    }
    {
        std::array<double, 6> start = {1000, 0, 500, 180, 0, -90};
        std::array<double, 6> target = {1000, 0, 300, 180, 0, -90};
        Codroid::TrajectoryRequest req;
        req.space = Codroid::TrajectorySpace::Cartesian;
        req.profile = Codroid::TrajectoryProfile::Trapezoidal;
        req.frequency_hz = 250;
        req.speed = 80.0;
        req.acceleration = 400.0;
        auto pts = Codroid::TrajectoryGenerator::Generate(start, target, req);
        if (!approx(pts.back().time_seconds, 2.7))
            return false;
        const double dt = 1.0 / 250.0;
        int k = static_cast<int>(std::round(1.0 / dt));
        if (k < 1 || static_cast<size_t>(k) >= pts.size())
            return false;
        double vz = (pts[static_cast<size_t>(k)].position[2] - pts[static_cast<size_t>(k - 1)].position[2]) / dt;
        if (!approx(vz, -80.0, 1.0))
            return false;
        for (int i = 0; i < 6; ++i) {
            if (!approx(pts.back().position[i], target[i], 1e-3))
                return false;
        }
    }
    {
        std::array<double, 6> start = {1139.994, -222.730, 899.022, -91.506, -0.002, -136.466};
        std::array<double, 6> target = {915.480, -73.000, 599.316, 166.910, -5.170, -90.726};
        Codroid::TrajectoryRequest req;
        req.space = Codroid::TrajectorySpace::Cartesian;
        req.profile = Codroid::TrajectoryProfile::Trapezoidal;
        req.frequency_hz = 250;
        req.speed = 80.0;
        req.acceleration = 400.0;
        auto pts = Codroid::TrajectoryGenerator::Generate(start, target, req);
        for (int i = 0; i < 6; ++i) {
            if (!approx(pts.front().position[i], start[i], 1e-4) || !approx(pts.back().position[i], target[i], 1e-4))
                return false;
        }
    }
    {
        std::array<double, 6> start = {500, 0, 500, 0, 0, 0};
        std::array<double, 6> target = {500, 0, 500, 30, 0, 0};
        Codroid::TrajectoryRequest req;
        req.space = Codroid::TrajectorySpace::Cartesian;
        req.profile = Codroid::TrajectoryProfile::Cubic;
        req.frequency_hz = 250;
        req.duration_seconds = 2.0;
        req.acceleration = 1.0;
        auto pts = Codroid::TrajectoryGenerator::Generate(start, target, req);
        if (pts.size() != 501u || !approx(pts[250].position[3], 15.0, 0.05) || !approx(pts.back().position[3], 30.0, 1e-3))
            return false;
    }
    return true;
}

bool wait_for_first_cri(Codroid::CodroidClient& robot, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto st = robot.GetRobotRealtimeState();
        if (st.data_valid && st.timestamp_ms > 0 && st.joint_position.size() >= 6)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_realtime_control(Codroid::CodroidClient& robot, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (robot.GetRobotRealtimeState().realtime_control_mode)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

void print_err(const char* step, const Codroid::CommandResult& r) {
    if (!r.error_msg.empty())
        std::cerr << "[失败] " << step << ": " << r.error_msg << '\n';
}

std::optional<std::array<double, 6>> tcp_pose_array_mm_deg(const Codroid::ClientRealtimeState& st) {
    if (!st.data_valid || st.tcp_pose.size() < 6)
        return std::nullopt;
    std::array<double, 6> p{};
    for (int i = 0; i < 6; ++i)
        p[static_cast<size_t>(i)] = st.tcp_pose[static_cast<size_t>(i)];
    return p;
}

std::vector<std::array<double, 6>> rectangle_cart_waypoints_mm_deg(const std::array<double, 6>& tcp0) {
    std::vector<std::array<double, 6>> w;
    w.reserve(5);
    w.push_back(tcp0);
    auto ax = [&](std::array<double, 6> a, double dx, double dy, double dz) {
        a[0] += dx;
        a[1] += dy;
        a[2] += dz;
        return a;
    };
    w.push_back(ax(tcp0, 0.0, 0.0, -200.0));
    w.push_back(ax(w.back(), 0.0, -200.0, 0.0));
    w.push_back(ax(w.back(), 0.0, 0.0, 200.0));
    w.push_back(ax(w.back(), 0.0, 200.0, 0.0));
    return w;
}

void sleep_cycles(int duration_ms, int cycles) {
    std::this_thread::sleep_for(std::chrono::milliseconds(std::max(1, duration_ms * cycles)));
}

void log_send_begin(const char* tag, size_t points, int period_ms) {
    const double est_sec = (points > 0) ? (static_cast<double>(points - 1) * static_cast<double>(period_ms) / 1000.0) : 0.0;
    std::cout << "[" << tag << "] send begin: points=" << points << ", period_ms=" << period_ms << ", est=" << est_sec
              << " s\n"
              << std::flush;
}

void log_send_end(const char* tag, std::chrono::steady_clock::time_point t0) {
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "[" << tag << "] send done, elapsed_ms=" << dt << '\n' << std::flush;
}

} // namespace

int main() {
    std::cout << "TRAJECTORY_ALGORITHM.md §9 (offline)...\n";
    if (!run_trajectory_algorithm_s9()) {
        std::cerr << "§9 trajectory self-check failed.\n";
        return 1;
    }
    std::cout << "§9 OK.\n";

    const std::string robot_ip = "192.168.8.136";
    const std::string local_ip = "192.168.8.150";
    const int tcp_port = 9001;
    const int duration_ms = 4;

    Codroid::CodroidClient robot;

    std::cout << "ConnectRemoteAndSwitchOn (TCP + Auto/Remote + CRI push + SwitchOn)...\n";
    if (!robot.ConnectRemoteAndSwitchOn(robot_ip, tcp_port, local_ip)) {
        std::cerr << "ConnectRemoteAndSwitchOn failed.\n";
        return 1;
    }

    std::cout << "Wait first CRI frame...\n";
    if (!wait_for_first_cri(robot, std::chrono::seconds(5))) {
        std::cerr << "Timeout: no CRI. Check network / local_ip.\n";
        robot.Disconnect();
        return 1;
    }

    const int id_ctl = robot.NextRequestId();
    auto r_start = robot.StartCriControl(1, duration_ms, 5, id_ctl);
    if (!r_start.error_msg.empty()) {
        print_err("StartCriControl", r_start);
        robot.Disconnect();
        return 1;
    }

    std::cout << "Wait realtime_control_mode...\n";
    if (!wait_realtime_control(robot, std::chrono::seconds(3))) {
        std::cerr << "Timeout: realtime_control_mode.\n";
        robot.StopCriControl(robot.NextRequestId());
        robot.Disconnect();
        return 1;
    }

    const auto st_plan = robot.GetRobotRealtimeState();
    std::array<double, 6> start_joint_deg{};
    for (int i = 0; i < 6; ++i)
        start_joint_deg[static_cast<size_t>(i)] = st_plan.joint_position[static_cast<size_t>(i)];

    const double hz = 1000.0 / static_cast<double>(duration_ms);

    Codroid::TrajectoryRequest tr_joint;
    tr_joint.space = Codroid::TrajectorySpace::Joint;
    tr_joint.profile = Codroid::TrajectoryProfile::Cubic;
    tr_joint.frequency_hz = hz;
    tr_joint.speed = 30.0;
    tr_joint.acceleration = 120.0;

    const std::vector<std::array<double, 6>> joint_wp{start_joint_deg, kAgentJointP1, kAgentJointP2, kAgentJointP1};
    std::vector<Codroid::TrajectoryPoint> path_joint;
    try {
        path_joint = Codroid::TrajectoryGenerator::GenerateMultiSegment(joint_wp, tr_joint);
    } catch (const std::exception& ex) {
        std::cerr << "TrajectoryGenerator joint: " << ex.what() << '\n';
        robot.StopCriControl(robot.NextRequestId());
        robot.Disconnect();
        return 1;
    }
    if (path_joint.empty()) {
        std::cerr << "Joint path empty.\n";
        robot.StopCriControl(robot.NextRequestId());
        robot.Disconnect();
        return 1;
    }
    std::cout << "[joint] pts=" << path_joint.size() << " T=" << path_joint.back().time_seconds << " s\n";

    Codroid::TrajectoryRequest tr_cart;
    tr_cart.space = Codroid::TrajectorySpace::Cartesian;
    tr_cart.profile = Codroid::TrajectoryProfile::Trapezoidal;
    tr_cart.frequency_hz = hz;
    tr_cart.speed = 80.0;
    tr_cart.acceleration = 400.0;

    try {
        Codroid::CriRealtimeDispatcher dispatcher(robot_ip, 9030, true);
        std::cout << "UDP 9030 period_ms=" << duration_ms << " [joint]\n";
        log_send_begin("joint", path_joint.size(), duration_ms);
        const auto t_joint = std::chrono::steady_clock::now();
        dispatcher.SendTrajectory(path_joint, Codroid::TrajectorySpace::Joint, duration_ms);
        log_send_end("joint", t_joint);

        sleep_cycles(duration_ms, 2);
        const auto tcp_cart0 = tcp_pose_array_mm_deg(robot.GetRobotRealtimeState());
        if (!tcp_cart0) {
            std::cerr << "No TcpPose after joint.\n";
            dispatcher.Close();
            robot.StopCriControl(robot.NextRequestId());
            robot.Disconnect();
            return 1;
        }

        auto path_cart = Codroid::TrajectoryGenerator::GenerateMultiSegment(rectangle_cart_waypoints_mm_deg(*tcp_cart0),
                                                                           tr_cart);
        std::cout << "[cart] pts=" << path_cart.size() << " T=" << path_cart.back().time_seconds << " s\n";
        log_send_begin("cart", path_cart.size(), duration_ms);
        const auto t_cart = std::chrono::steady_clock::now();
        dispatcher.SendTrajectory(path_cart, Codroid::TrajectorySpace::Cartesian, duration_ms);
        log_send_end("cart", t_cart);

        sleep_cycles(duration_ms, 2);
        const auto tcp_path0 = tcp_pose_array_mm_deg(robot.GetRobotRealtimeState());
        if (!tcp_path0) {
            std::cerr << "No TcpPose before path.\n";
            dispatcher.Close();
            robot.StopCriControl(robot.NextRequestId());
            robot.Disconnect();
            return 1;
        }

        const std::vector<std::array<double, 6>> path_wp{
            *tcp_path0, kAgentPathW1, kAgentPathW2, kAgentPathW3, kAgentPathW4Home};
        auto path_motion = Codroid::TrajectoryGenerator::GenerateMultiSegment(path_wp, tr_cart);
        std::cout << "[path] pts=" << path_motion.size() << " T=" << path_motion.back().time_seconds << " s\n";
        log_send_begin("path", path_motion.size(), duration_ms);
        const auto t_path = std::chrono::steady_clock::now();
        dispatcher.SendTrajectory(path_motion, Codroid::TrajectorySpace::Cartesian, duration_ms);
        log_send_end("path", t_path);

        dispatcher.Close();
    } catch (const std::exception& ex) {
        std::cerr << "SendTrajectory: " << ex.what() << '\n';
    }

    std::cout << "[cleanup] StopCriControl begin\n" << std::flush;
    const int id_stop = robot.NextRequestId();
    auto r_stop = robot.StopCriControl(id_stop);
    print_err("StopCriControl", r_stop);
    std::cout << "[cleanup] StopCriControl end\n" << std::flush;

    const int udp_listen = robot.GetCriUdpListenPort();
    if (udp_listen > 0) {
        std::cout << "[cleanup] StopCriDataPush begin, udp_listen=" << udp_listen << '\n' << std::flush;
        const int id_push = robot.NextRequestId();
        auto r_push = robot.StopCriDataPush(local_ip, udp_listen, id_push);
        print_err("StopCriDataPush", r_push);
        std::cout << "[cleanup] StopCriDataPush end\n" << std::flush;
    }

    std::cout << "[cleanup] Disconnect begin\n" << std::flush;
    robot.Disconnect();
    std::cout << "[cleanup] Disconnect end\n" << std::flush;
    std::cout << "Done.\n" << std::flush;
    return 0;
}
