#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace Codroid;

namespace {

void print_result(const std::string& label, const CommandResult& r) {
    std::cout << label << ": " << (r.Ok() ? "OK" : ("ERR " + r.error_msg)) << "\n";
}

std::string fmt6(const std::vector<double>& values) {
    std::string out = "[";
    for (int i = 0; i < 6; ++i) {
        if (i) out += ", ";
        double v = i < static_cast<int>(values.size()) ? values[static_cast<size_t>(i)] : 0.0;
        out += std::to_string(v);
    }
    out += "]";
    return out;
}

void print_state(CodroidClient& robot) {
    ClientForceControlState s = robot.GetForceState();
    std::cout << "enabled=" << s.enabled
              << " pending=" << s.pending
              << " algo=" << s.algo
              << " valid=" << s.valid
              << " contact=" << s.is_contact
              << " overforce=" << s.is_overforce
              << " health=" << s.health << "\n";
    std::cout << "wrenchTcp=" << fmt6(s.wrench_tcp) << "\n";
    std::cout << "desiredWrench=" << fmt6(s.desired_wrench) << "\n";
}

void poll_state(CodroidClient& robot, double seconds) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(seconds * 1000));
    while (std::chrono::steady_clock::now() < end) {
        print_state(robot);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string ip = argc > 1 ? argv[1] : "192.168.1.136";
    std::string mode = argc > 2 ? argv[2] : "state";
    bool allow_motion = argc > 3 && std::string(argv[3]) == "--allow-motion";

    CodroidClient robot;
    if (!robot.Connect(ip)) {
        std::cerr << "Connect failed\n";
        return 1;
    }

    try {
        if (mode == "state") {
            print_state(robot);
            robot.Disconnect();
            return 0;
        }

        print_result("ClearSystemError", robot.ClearSystemError());
        print_result("EnterRemoteModeViaAuto", robot.EnterRemoteModeViaAuto());
        print_result("SwitchOn", robot.SwitchOn());
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (mode == "calibration") {
            print_result("ZeroForceCalibration", robot.ZeroForceCalibration(1000));
        } else if (mode == "safety") {
            print_result("SetOverforceProtection",
                         robot.SetOverforceProtection(1, {150, 150, 20, 40, 40, 40}, 20));
            print_result("SetForceDataHealth",
                         robot.SetForceDataHealth(1, 200, 0.9));
        } else if (mode == "compliance") {
            nlohmann::json compliance = {
                {"stiffness", std::vector<double>{0, 0, 0, 0, 0, 0}},
                {"damping", std::vector<double>{250, 250, 50, 7.5, 7.5, 7.5}},
                {"mass", std::vector<double>{2.5, 2.5, 1.5, 0.15, 0.15, 0.15}},
            };
            print_result("InitForceControl",
                         robot.InitForceControl(
                             ForceFrame::Tcp,
                             {ForceAxisMode::Position, ForceAxisMode::Position, ForceAxisMode::Compliant,
                              ForceAxisMode::Position, ForceAxisMode::Position, ForceAxisMode::Position},
                             compliance));
            print_result("StartForceControl", robot.StartForceControl());
            poll_state(robot, 5.0);
            print_result("StopForceControl", robot.StopForceControl(500));
        } else if (mode == "constant") {
            nlohmann::json constant_force = {
                {"desiredForce", std::vector<double>{0, 0, 2, 0, 0, 0}},
                {"damping", std::vector<double>{250, 250, 250, 7.5, 7.5, 7.5}},
                {"mass", std::vector<double>{2.5, 2.5, 2.5, 0.15, 0.15, 0.15}},
                {"rampTimeMs", 500},
            };
            print_result("InitForceControl",
                         robot.InitForceControl(
                             ForceFrame::Tcp,
                             {ForceAxisMode::Position, ForceAxisMode::Position, ForceAxisMode::Force,
                              ForceAxisMode::Position, ForceAxisMode::Position, ForceAxisMode::Position},
                             nlohmann::json::object(), constant_force));
            print_result("StartForceControl", robot.StartForceControl());
            poll_state(robot, 3.0);
            print_result("TuneForceParams",
                         robot.TuneForceParams({}, {}, {}, {0, 0, 5, 0, 0, 0}, {}, {}, 500));
            poll_state(robot, 3.0);
            print_result("StopForceControl", robot.StopForceControl(500));
        } else if (mode == "contact") {
            if (!allow_motion) {
                throw std::runtime_error("contact mode requires --allow-motion");
            }
            nlohmann::json compliance = {
                {"stiffness", std::vector<double>{0, 0, 0, 0, 0, 0}},
                {"damping", std::vector<double>{250, 250, 50, 7.5, 7.5, 7.5}},
                {"mass", std::vector<double>{2.5, 2.5, 1.5, 0.15, 0.15, 0.15}},
            };
            nlohmann::json constant_force = {
                {"desiredForce", std::vector<double>{0, 0, 0, 0, 0, 0}},
                {"damping", std::vector<double>{250, 250, 250, 7.5, 7.5, 7.5}},
                {"mass", std::vector<double>{2.5, 2.5, 2.5, 0.15, 0.15, 0.15}},
            };
            print_result("InitForceControl",
                         robot.InitForceControl(
                             ForceFrame::Tcp,
                             {ForceAxisMode::Compliant, ForceAxisMode::Compliant, ForceAxisMode::Force,
                              ForceAxisMode::Compliant, ForceAxisMode::Compliant, ForceAxisMode::Compliant},
                             compliance, constant_force));
            print_result("StartForceControl", robot.StartForceControl());
            print_result("StartContactDetection",
                         robot.StartContactDetection({0, 0, -1, 0, 0, 0}, 0.002, 3, 0, 0.01, 5000));
            poll_state(robot, 5.0);
            print_result("StopForceControl", robot.StopForceControl(500));
        } else {
            throw std::runtime_error("unknown mode: " + mode);
        }

        robot.ToAuto();
        robot.ToManual();
        robot.SwitchOff();
        robot.Disconnect();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        try { robot.StopForceControl(300); } catch (...) {}
        try { robot.SwitchOff(); } catch (...) {}
        robot.Disconnect();
        return 2;
    }
}
