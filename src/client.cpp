#include "Codroid/client.hpp"

#include "Codroid/CodroidController.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <utility>

namespace Codroid {

class CodroidClient::Impl {
public:
    CodroidController controller;
};

class ClientPublishSubscription::Impl {
public:
    explicit Impl(PublishTopicSubscription s) : subscription(std::move(s)) {}
    PublishTopicSubscription subscription;
};

namespace {

using ClientRobotFrame = CodroidClient::ClientRobotFrame;
using ClientRobotPayload = CodroidClient::ClientRobotPayload;
using ClientRobotParameters = CodroidClient::ClientRobotParameters;

MoveType to_internal_move_type(ClientMoveType type) {
    switch (type) {
    case ClientMoveType::MovJ:
        return MoveType::movJ;
    case ClientMoveType::MovL:
        return MoveType::movL;
    case ClientMoveType::MovC:
        return MoveType::movC;
    case ClientMoveType::MovCircle:
        return MoveType::movCircle;
    }
    return MoveType::movJ;
}

MoveInstruction to_internal_instruction(const ClientMoveInstruction& inst) {
    MoveInstruction out;
    out.type = to_internal_move_type(inst.type);
    out.speed = inst.speed;
    out.acc = inst.acceleration;
    out.blend = inst.blend;
    out.relativeBlend = inst.relative_blend;
    out.circleNum = inst.circle_num;
    out.targetPoint = inst.target;
    out.middlePoint = inst.middle;
    out.coor = inst.coor;
    out.tool = inst.tool;
    return out;
}

CommandResult to_client_result(const Response& r) {
    CommandResult out;
    out.id = r.id;
    out.ty = r.ty;
    out.error_msg = r.error_msg;
    out.raw_json = r.raw_json;
    return out;
}

ClientRobotFrame to_client_frame(const RobotFrameEntry& e) {
    ClientRobotFrame out;
    out.id = e.id;
    out.x = e.x;
    out.y = e.y;
    out.z = e.z;
    out.a = e.a;
    out.b = e.b;
    out.c = e.c;
    return out;
}

RobotFrameEntry to_internal_frame(const ClientRobotFrame& e) {
    RobotFrameEntry out;
    out.id = e.id;
    out.x = e.x;
    out.y = e.y;
    out.z = e.z;
    out.a = e.a;
    out.b = e.b;
    out.c = e.c;
    return out;
}

ClientRobotPayload to_client_payload(const RobotPayloadEntry& e) {
    ClientRobotPayload out;
    out.id = e.id;
    out.m = e.m;
    out.mx = e.mx;
    out.my = e.my;
    out.mz = e.mz;
    return out;
}

RobotPayloadEntry to_internal_payload(const ClientRobotPayload& e) {
    RobotPayloadEntry out;
    out.id = e.id;
    out.m = e.m;
    out.mx = e.mx;
    out.my = e.my;
    out.mz = e.mz;
    return out;
}

ClientRobotFrame parse_client_frame(const nlohmann::json& j) {
    ClientRobotFrame e;
    auto num = [](const nlohmann::json& v, double fb) {
        if (v.is_number())
            return v.get<double>();
        if (v.is_number_integer())
            return static_cast<double>(v.get<int64_t>());
        return fb;
    };
    if (j.contains("id"))
        e.id = j["id"].is_number_integer() ? static_cast<int>(j["id"].get<int64_t>()) : static_cast<int>(j["id"].get<double>());
    if (j.contains("x"))
        e.x = num(j["x"], 0.0);
    if (j.contains("y"))
        e.y = num(j["y"], 0.0);
    if (j.contains("z"))
        e.z = num(j["z"], 0.0);
    if (j.contains("a"))
        e.a = num(j["a"], 0.0);
    if (j.contains("b"))
        e.b = num(j["b"], 0.0);
    if (j.contains("c"))
        e.c = num(j["c"], 0.0);
    return e;
}

ClientRobotPayload parse_client_payload(const nlohmann::json& j) {
    ClientRobotPayload e;
    auto num = [](const nlohmann::json& v, double fb) {
        if (v.is_number())
            return v.get<double>();
        if (v.is_number_integer())
            return static_cast<double>(v.get<int64_t>());
        return fb;
    };
    if (j.contains("id"))
        e.id = j["id"].is_number_integer() ? static_cast<int>(j["id"].get<int64_t>()) : static_cast<int>(j["id"].get<double>());
    if (j.contains("m"))
        e.m = num(j["m"], 0.0);
    if (j.contains("mx"))
        e.mx = num(j["mx"], 0.0);
    if (j.contains("my"))
        e.my = num(j["my"], 0.0);
    if (j.contains("mz"))
        e.mz = num(j["mz"], 0.0);
    return e;
}

ClientRobotParameters to_client_parameters(const Response& r) {
    ClientRobotParameters out;
    if (!r.error_msg.empty())
        return out;
    const auto& db = r.db;
    auto db_int = [&db](const char* key, int fb) {
        if (!db.contains(key))
            return fb;
        const auto& v = db.at(key);
        return v.is_number_integer() ? static_cast<int>(v.get<int64_t>()) : static_cast<int>(v.get<double>());
    };
    auto db_num = [&db](const char* key, double fb) {
        if (!db.contains(key))
            return fb;
        const auto& v = db.at(key);
        return v.is_number() ? v.get<double>() : static_cast<double>(v.get<int64_t>());
    };
    out.valid = true;
    out.default_tool_id = db_int("defaultToolId", 0);
    out.default_payload_id = db_int("defaultPayloadId", 0);
    out.default_coordinate_id = db_int("defaultCoordinateId", 0);
    out.max_payload = db_num("maxPayload", 0.0);
    if (db.contains("Tool") && db["Tool"].is_array()) {
        for (const auto& item : db["Tool"]) {
            if (item.is_object())
                out.tool.push_back(parse_client_frame(item));
        }
    }
    if (db.contains("Payload") && db["Payload"].is_array()) {
        for (const auto& item : db["Payload"]) {
            if (item.is_object())
                out.payload.push_back(parse_client_payload(item));
        }
    }
    if (db.contains("Coordinate") && db["Coordinate"].is_array()) {
        for (const auto& item : db["Coordinate"]) {
            if (item.is_object())
                out.coordinate.push_back(parse_client_frame(item));
        }
    }
    return out;
}

ClientRealtimeState to_client_state(const RobotRealtimeState& s) {
    ClientRealtimeState out;
    out.timestamp_ms = s.timestamp_ms;
    out.data_valid = s.data_valid;
    out.status1_raw = s.status1_raw;
    out.status2_raw = s.status2_raw;
    out.joint_position = s.joint_position;
    out.joint_velocity = s.joint_velocity;
    out.tcp_pose = s.tcp_pose;
    out.tcp_velocity = s.tcp_velocity;
    out.tcp_linear_velocity_mm_s = s.tcp_linear_velocity_mm_s;
    out.joint_output_torque = s.joint_output_torque;
    out.joint_external_force = s.joint_external_force;
    out.external_axis_position = s.external_axis_position;
    out.project_running = s.project_running;
    out.project_stopped = s.project_stopped;
    out.project_paused = s.project_paused;
    out.enabling = s.enabling;
    out.not_enabled = s.not_enabled;
    out.manual_mode = s.manual_mode;
    out.dragging = s.dragging;
    out.in_motion = s.in_motion;
    out.collision_stopped = s.collision_stopped;
    out.in_safety_position = s.in_safety_position;
    out.has_alarm = s.has_alarm;
    out.simulation_mode = s.simulation_mode;
    out.emergency_stop_pressed = s.emergency_stop_pressed;
    out.rescue_mode = s.rescue_mode;
    out.auto_mode = s.auto_mode;
    out.remote_mode = s.remote_mode;
    out.realtime_control_mode = s.realtime_control_mode;
    out.cri_error_code = s.cri_error_code;
    return out;
}

// --- Sync motion helpers (v2.1.8: only check InMotion flag, no position comparison) ---

void wait_until_settled(CodroidController& ctrl,
                        const std::string& op_name, const MotionWaitOptions& opts) {
    if (opts.settled_samples <= 0)
        throw std::invalid_argument(op_name + ": MotionWaitOptions.settled_samples must be > 0");
    if (opts.poll_interval_s <= 0)
        throw std::invalid_argument(op_name + ": MotionWaitOptions.poll_interval_s must be > 0");

    auto start = std::chrono::steady_clock::now();
    int settled = 0;
    bool had_motion = false;

    while (std::chrono::steady_clock::now() - start < std::chrono::duration<double>(opts.timeout_s)) {
        auto state = ctrl.getRobotRealtimeState();
        if (!state.data_valid) {
            std::this_thread::sleep_for(std::chrono::duration<double>(opts.poll_interval_s));
            continue;
        }

        if (state.in_motion) had_motion = true;

        if (state.collision_stopped || state.emergency_stop_pressed || state.has_alarm) {
            throw std::runtime_error(op_name + " failed: abnormal state detected (CollisionStopped="
                                     + std::to_string(state.collision_stopped)
                                     + ", EmergencyStopPressed=" + std::to_string(state.emergency_stop_pressed)
                                     + ", HasAlarm=" + std::to_string(state.has_alarm) + ")");
        }

        bool still = !state.in_motion;
        if (had_motion && still) {
            if (++settled >= opts.settled_samples) return;
        } else {
            settled = 0;
        }

        std::this_thread::sleep_for(std::chrono::duration<double>(opts.poll_interval_s));
    }

    auto tail = ctrl.getRobotRealtimeState();
    std::string jp_str;
    for (size_t i = 0; i < tail.joint_position.size(); ++i) {
        if (i > 0) jp_str += ", ";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3f", tail.joint_position[i]);
        jp_str += buf;
    }
    throw std::runtime_error(op_name + " wait timed out (" + std::to_string(opts.timeout_s)
                             + "s). Last state: InMotion=" + std::to_string(tail.in_motion)
                             + ", jp=[" + jp_str + "]");
}

} // namespace

ClientPublishSubscription::ClientPublishSubscription() = default;

ClientPublishSubscription::ClientPublishSubscription(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

ClientPublishSubscription::~ClientPublishSubscription() = default;

ClientPublishSubscription::ClientPublishSubscription(ClientPublishSubscription&&) noexcept = default;

ClientPublishSubscription& ClientPublishSubscription::operator=(ClientPublishSubscription&&) noexcept = default;

void ClientPublishSubscription::Dispose() {
    if (impl_) {
        impl_->subscription.Dispose();
        impl_.reset();
    }
}

bool ClientPublishSubscription::IsValid() const noexcept { return static_cast<bool>(impl_); }

CodroidClient::CodroidClient() : impl_(std::make_unique<Impl>()) {}

CodroidClient::~CodroidClient() = default;

CodroidClient::CodroidClient(CodroidClient&&) noexcept = default;

CodroidClient& CodroidClient::operator=(CodroidClient&&) noexcept = default;

int CodroidClient::NextRequestId() { return impl_->controller.NextRequestId(); }

bool CodroidClient::Connect(const std::string& ip, int port) { return impl_->controller.connectTcp(ip, port); }

bool CodroidClient::ConnectRemoteAndSwitchOn(const std::string& ip, int port, std::string local_ip) {
    if (!impl_->controller.connect(ip, port, std::move(local_ip)))
        return false;
    Response r = impl_->controller.switchOn(1);
    return r.error_msg.empty();
}

void CodroidClient::Disconnect() { impl_->controller.disconnect(); }

CommandResult CodroidClient::SwitchOn(int id) { return to_client_result(impl_->controller.switchOn(id)); }

CommandResult CodroidClient::SwitchOff(int id) { return to_client_result(impl_->controller.switchOff(id)); }

CommandResult CodroidClient::ToManual(int id) { return to_client_result(impl_->controller.toManualDirect(id)); }

CommandResult CodroidClient::ToAuto(int id) { return to_client_result(impl_->controller.toAuto(id)); }

CommandResult CodroidClient::ToRemote(int id) { return to_client_result(impl_->controller.toRemoteDirect(id)); }

CommandResult CodroidClient::ClearSystemError(int id) { return to_client_result(impl_->controller.clearError(id)); }

int CodroidClient::GetDi(int port, int id) { return impl_->controller.getDI(port, id); }

int CodroidClient::GetDo(int port, int id) { return impl_->controller.getDO(port, id); }

double CodroidClient::GetAi(int port, int id) { return impl_->controller.getAI(port, id); }

double CodroidClient::GetAo(int port, int id) { return impl_->controller.getAO(port, id); }

CommandResult CodroidClient::SetDo(int port, int value, int id) {
    return to_client_result(impl_->controller.setDO(port, value, id));
}

CommandResult CodroidClient::SetAo(int port, double value, int id) {
    return to_client_result(impl_->controller.setAO(port, value, id));
}

double CodroidClient::GetRegisterValue(int address, int id) {
    return impl_->controller.getRegisterValue(address, id);
}

std::vector<ClientRegisterInfo> CodroidClient::GetRegisterValues(const std::vector<int>& addresses, int id) {
    std::vector<ClientRegisterInfo> out;
    auto values = impl_->controller.getRegisterValues(addresses, id);
    out.reserve(values.size());
    for (const auto& v : values) {
        out.push_back(ClientRegisterInfo{v.address, v.value});
    }
    return out;
}

CommandResult CodroidClient::SetRegisterValue(int address, double value, int id) {
    return to_client_result(impl_->controller.setRegisterValue(address, value, id));
}

CommandResult CodroidClient::SetManualMoveRate(int pct, int id) {
    return to_client_result(impl_->controller.setManualSpeedRate(pct, id));
}

CommandResult CodroidClient::SetAutoMoveRate(int pct, int id) {
    return to_client_result(impl_->controller.setAutoSpeedRate(pct, id));
}

CommandResult CodroidClient::SetCollisionSensitivity(int sensitivity, int id) {
    return to_client_result(impl_->controller.setCollisionSensitivity(sensitivity, id));
}

CommandResult CodroidClient::SetPayload(int payloadId, int id) {
    return to_client_result(impl_->controller.setPayload(payloadId, id));
}

ClientRobotParameters CodroidClient::GetRobotParameters(int id) {
    return to_client_parameters(impl_->controller.getRobotParameter(id));
}

CommandResult CodroidClient::SetDefaultPayloadId(int payloadId, int id) {
    return to_client_result(impl_->controller.setDefaultPayloadId(payloadId, id));
}

CommandResult CodroidClient::SetDefaultToolId(int toolId, int id) {
    return to_client_result(impl_->controller.setDefaultToolId(toolId, id));
}

CommandResult CodroidClient::SetDefaultUserCoordinateId(int coordinateId, int id) {
    return to_client_result(impl_->controller.setDefaultCoordinateId(coordinateId, id));
}

CommandResult CodroidClient::SaveToolFrames(const std::vector<ClientRobotFrame>& frames, int id) {
    std::vector<RobotFrameEntry> internal;
    internal.reserve(frames.size());
    for (const auto& f : frames)
        internal.push_back(to_internal_frame(f));
    return to_client_result(impl_->controller.saveToolFrames(internal, id));
}

CommandResult CodroidClient::SetToolFrame(int frame_id, const ClientRobotFrame& frame, int id) {
    return to_client_result(impl_->controller.setToolFrame(frame_id, to_internal_frame(frame), id));
}

CommandResult CodroidClient::SavePayloadFrames(const std::vector<ClientRobotPayload>& frames, int id) {
    std::vector<RobotPayloadEntry> internal;
    internal.reserve(frames.size());
    for (const auto& f : frames)
        internal.push_back(to_internal_payload(f));
    return to_client_result(impl_->controller.savePayloadFrames(internal, id));
}

CommandResult CodroidClient::SetPayloadFrame(int frame_id, const ClientRobotPayload& frame, int id) {
    return to_client_result(impl_->controller.setPayloadFrame(frame_id, to_internal_payload(frame), id));
}

CommandResult CodroidClient::SetUserCoordinateFrame(int frame_id, const ClientRobotFrame& frame, int id) {
    return to_client_result(impl_->controller.setUserCoordinateFrame(frame_id, to_internal_frame(frame), id));
}

CommandResult CodroidClient::MovJ(const ClientJointPoint& target, double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movJ(target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovJ(const ClientCartesianPoint& target, double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movJ(target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovJ(const ClientMovePoint& target, double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movJ(target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovL(const ClientCartesianPoint& target, double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovL(const ClientJointPoint& target, double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovL(const ClientMovePoint& target, double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovC(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                                  double speed, double acceleration,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movC(middle, target, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::MovCircle(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                                       int circle_num, double speed, double acceleration,
                                       double blend, double relativeBlend,
                                       const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movCircle(middle, target, circle_num, speed, acceleration, blend, relativeBlend, coor, tool, id));
}

CommandResult CodroidClient::Move(const std::vector<ClientMoveInstruction>& path, int id) {
    return MovePath(path, id);
}

CommandResult CodroidClient::MovePath(const std::vector<ClientMoveInstruction>& path, int id) {
    std::vector<MoveInstruction> internal_path;
    internal_path.reserve(path.size());
    for (const auto& inst : path) {
        internal_path.push_back(to_internal_instruction(inst));
    }
    return to_client_result(impl_->controller.move(internal_path, id));
}

CommandResult CodroidClient::PauseRobotMotion(int id) { return to_client_result(impl_->controller.pauseMove(id)); }

CommandResult CodroidClient::ResumeRobotMotion(int id) { return to_client_result(impl_->controller.resumeMove(id)); }

CommandResult CodroidClient::StopRobotMove(int id) { return to_client_result(impl_->controller.stopMove(id)); }

CommandResult CodroidClient::StartCriDataPush(const std::string& udpIp, int udpPort, int id) {
    return to_client_result(impl_->controller.startDataPush(udpIp, udpPort, 100, id, true));
}

CommandResult CodroidClient::StopCriDataPush(int id) {
    return to_client_result(impl_->controller.stopDataPush(id));
}

CommandResult CodroidClient::StopCriDataPush(const std::string& udpIp, int udpPort, int id) {
    return to_client_result(impl_->controller.stopDataPush(udpIp, udpPort, id));
}

CommandResult CodroidClient::StartCriControl(int filterType, int durationMs, int startBuffer, int id) {
    return to_client_result(impl_->controller.startControl(durationMs, startBuffer, filterType, id));
}

CommandResult CodroidClient::StopCriControl(int id) {
    return to_client_result(impl_->controller.stopControl(id));
}

int CodroidClient::GetCriUdpListenPort() const { return impl_->controller.getCriUdpListenPort(); }

ClientRealtimeState CodroidClient::GetRobotRealtimeState() const {
    return to_client_state(impl_->controller.getRobotRealtimeState());
}

void CodroidClient::SetCriDataReceived(std::function<void(const ClientRealtimeState&)> cb) {
    if (!cb) {
        impl_->controller.setCriDataReceivedHandler({});
        return;
    }
    impl_->controller.setCriDataReceivedHandler([cb = std::move(cb)](const RobotRealtimeState& state) {
        cb(to_client_state(state));
    });
}

ClientPublishSubscription CodroidClient::SubscribePublishTopic(
    std::string topicTy, std::function<void(const ClientPublishNotification&)> handler, int tc_milliseconds) {
    if (!handler) {
        return ClientPublishSubscription();
    }
    auto sub = impl_->controller.subscribePublishTopic(
        std::move(topicTy),
        [handler = std::move(handler)](const PublishNotification& msg) {
            ClientPublishNotification out;
            out.ty = msg.ty;
            out.db_json = msg.db.dump();
            out.raw_json = msg.raw_json;
            handler(out);
        },
        tc_milliseconds);
    return ClientPublishSubscription(std::make_unique<ClientPublishSubscription::Impl>(std::move(sub)));
}

void CodroidClient::SetThrowOnCommandError(bool enable) {
    impl_->controller.setThrowOnCommandError(enable);
}

bool CodroidClient::ThrowOnCommandError() const noexcept {
    return impl_->controller.getThrowOnCommandError();
}

// --- Sync motion ---

bool CodroidClient::MoveSync(const std::vector<ClientMoveInstruction>& path, const MotionWaitOptions& wait) {
    auto r = Move(path);
    if (!r.Ok()) throw std::runtime_error("MoveSync: Move failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MoveSync", wait);
    return true;
}

bool CodroidClient::MovJSync(const ClientJointPoint& target, double speed, double acc,
                             const MotionWaitOptions& wait,
                             double blend, double relativeBlend,
                             const std::vector<double>& coor, const std::vector<double>& tool) {
    auto r = MovJ(target, speed, acc, blend, relativeBlend, coor, tool);
    if (!r.Ok()) throw std::runtime_error("MovJSync: MovJ failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MovJSync(JointPoint)", wait);
    return true;
}

bool CodroidClient::MovJSync(const ClientCartesianPoint& target, double speed, double acc,
                             const MotionWaitOptions& wait,
                             double blend, double relativeBlend,
                             const std::vector<double>& coor, const std::vector<double>& tool) {
    auto r = MovJ(target, speed, acc, blend, relativeBlend, coor, tool);
    if (!r.Ok()) throw std::runtime_error("MovJSync: MovJ failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MovJSync(CartesianPoint)", wait);
    return true;
}

bool CodroidClient::MovLSync(const ClientCartesianPoint& target, double speed, double acc,
                             const MotionWaitOptions& wait,
                             double blend, double relativeBlend,
                             const std::vector<double>& coor, const std::vector<double>& tool) {
    auto r = MovL(target, speed, acc, blend, relativeBlend, coor, tool);
    if (!r.Ok()) throw std::runtime_error("MovLSync: MovL failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MovLSync(CartesianPoint)", wait);
    return true;
}

bool CodroidClient::MovLSync(const ClientJointPoint& target, double speed, double acc,
                             const MotionWaitOptions& wait,
                             double blend, double relativeBlend,
                             const std::vector<double>& coor, const std::vector<double>& tool) {
    auto r = MovL(target, speed, acc, blend, relativeBlend, coor, tool);
    if (!r.Ok()) throw std::runtime_error("MovLSync: MovL failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MovLSync(JointPoint)", wait);
    return true;
}

bool CodroidClient::MovCSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                             double speed, double acc, const MotionWaitOptions& wait,
                             double blend, double relativeBlend,
                             const std::vector<double>& coor, const std::vector<double>& tool) {
    auto r = MovC(middle, target, speed, acc, blend, relativeBlend, coor, tool);
    if (!r.Ok()) throw std::runtime_error("MovCSync: MovC failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MovCSync", wait);
    return true;
}

bool CodroidClient::MovCircleSync(const ClientCartesianPoint& middle, const ClientCartesianPoint& target,
                                  int circle_num, double speed, double acc, const MotionWaitOptions& wait,
                                  double blend, double relativeBlend,
                                  const std::vector<double>& coor, const std::vector<double>& tool) {
    auto r = MovCircle(middle, target, circle_num, speed, acc, blend, relativeBlend, coor, tool);
    if (!r.Ok()) throw std::runtime_error("MovCircleSync: MovCircle failed: " + r.error_msg);
    wait_until_settled(impl_->controller, "MovCircleSync", wait);
    return true;
}

// --- MoveTo ---

CommandResult CodroidClient::MoveTo(const MoveToParams& params, int id) {
    return to_client_result(impl_->controller.moveTo(params, id));
}

CommandResult CodroidClient::MoveToHeartbeat(int id) {
    return to_client_result(impl_->controller.moveToHeartbeat(id));
}

CommandResult CodroidClient::StopMoveTo(int id) {
    return to_client_result(impl_->controller.moveTo(MoveToParams(MoveToType::Stop), id));
}

// --- Jog ---

CommandResult CodroidClient::Jog(const JogParams& params, int id) {
    return to_client_result(impl_->controller.jog(params, id));
}

CommandResult CodroidClient::StopJog(int id) {
    return to_client_result(impl_->controller.stopJog(id));
}

CommandResult CodroidClient::JogHeartbeat(int id) {
    return to_client_result(impl_->controller.jogHeartbeat(id));
}

// --- WaitForCriData ---

void CodroidClient::WaitForCriData(double timeout_s) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::duration<double>(timeout_s)) {
        auto state = impl_->controller.getRobotRealtimeState();
        if (state.data_valid) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    throw std::runtime_error("WaitForCriData timed out (" + std::to_string(timeout_s)
                             + "s). Ensure StartCriDataPush is called first.");
}

// --- Mode control ---

CommandResult CodroidClient::EnterManualModeViaAuto(int id) {
    return to_client_result(impl_->controller.toManual(id));
}

CommandResult CodroidClient::EnterRemoteModeViaAuto(int id) {
    return to_client_result(impl_->controller.toRemote(id));
}

CommandResult CodroidClient::ToSimulation(int id) {
    return to_client_result(impl_->controller.toSimulation(id));
}

CommandResult CodroidClient::ToActual(int id) {
    return to_client_result(impl_->controller.toActual(id));
}

CommandResult CodroidClient::StartDrag(int id) {
    return to_client_result(impl_->controller.startDrag(id));
}

CommandResult CodroidClient::StopDrag(int id) {
    return to_client_result(impl_->controller.stopDrag(id));
}

// --- Script & Project ---

CommandResult CodroidClient::RunScript(const std::string& mainScript,
                                       const std::unordered_map<std::string, std::string>& subThreads,
                                       const std::unordered_map<std::string, std::string>& subPrograms,
                                       const std::unordered_map<std::string, std::string>& interrupts,
                                       const nlohmann::json& vars,
                                       int id) {
    RunScriptParams params(mainScript);
    params.subThreads = subThreads;
    params.subPrograms = subPrograms;
    params.interrupts = interrupts;
    params.vars = vars;
    return to_client_result(impl_->controller.runScript(params, id));
}

CommandResult CodroidClient::EnterRemoteScriptMode(int id) {
    return to_client_result(impl_->controller.enterRemoteScriptMode(id));
}

CommandResult CodroidClient::Run(const std::string& projectId, int id) {
    return to_client_result(impl_->controller.runProject(projectId, id));
}

CommandResult CodroidClient::RunByIndex(int index, int id) {
    return to_client_result(impl_->controller.runProjectByIndex(index, id));
}

CommandResult CodroidClient::RunStep(const std::string& projectId, int id) {
    return to_client_result(impl_->controller.runStep(projectId, id));
}

CommandResult CodroidClient::PauseProject(int id) {
    return to_client_result(impl_->controller.pauseProject(id));
}

CommandResult CodroidClient::ResumeProject(int id) {
    return to_client_result(impl_->controller.resumeProject(id));
}

CommandResult CodroidClient::StopProject(int id) {
    return to_client_result(impl_->controller.stopProject(id));
}

// --- Global Variables ---

nlohmann::json CodroidClient::GetGlobalVars(int id) {
    auto r = impl_->controller.getGlobalVars(id);
    if (!r.error_msg.empty()) {
        if (impl_->controller.getThrowOnCommandError())
            throw CodroidCommandException(r.id, r.ty, r.error_msg, r.raw_json);
        return nullptr;
    }
    return r.db;
}

CommandResult CodroidClient::SaveGlobalVars(const std::map<std::string, Variable>& vars, int id) {
    return to_client_result(impl_->controller.saveGlobalVars(vars, id));
}

CommandResult CodroidClient::RemoveGlobalVars(const std::vector<std::string>& names, int id) {
    return to_client_result(impl_->controller.removeGlobalVars(names, id));
}

// --- Kinematics ---

std::vector<double> CodroidClient::ForwardKinematics(const FKParams& params, int id) {
    return impl_->controller.forwardKinematics(params, id);
}

std::vector<double> CodroidClient::InverseKinematics(const IKParams& params, int id) {
    return impl_->controller.inverseKinematics(params, id);
}

std::vector<double> CodroidClient::CalculateRelativePose(const RelativePoseParams& params, int id) {
    return impl_->controller.calculateRelativePose(params, id);
}

// --- 2.13/2.14 启动行 ---

CommandResult CodroidClient::SetStartLine(int line, int id) {
    return to_client_result(impl_->controller.setStartLine(line, id));
}

CommandResult CodroidClient::ClearStartLine(int id) {
    return to_client_result(impl_->controller.clearStartLine(id));
}

// --- 3. 全局变量目录 ---

nlohmann::json CodroidClient::GetGlobalVarsCatalog(int id) {
    return GetGlobalVars(id);
}

// --- 4.1 工程变量 ---

nlohmann::json CodroidClient::GetProjectVar(int id) {
    Response r = impl_->controller.getProjectVar(id);
    if (impl_->controller.getThrowOnCommandError() && !r.error_msg.empty()) {
        throw CodroidCommandException(r.id, "globalVar/GetProjectVarUpdate", r.error_msg, r.raw_json);
    }
    return r.db;
}

// --- 5. RS485 ---

CommandResult CodroidClient::Rs485Init(int baudrate, RS485StopBits stopBit, RS485Parity parity, int id) {
    return to_client_result(impl_->controller.RS485init(baudrate, stopBit, 8, parity, id));
}

CommandResult CodroidClient::Rs485Flush(int id) {
    return to_client_result(impl_->controller.RS485flush(id));
}

nlohmann::json CodroidClient::Rs485Read(int length, int timeout, int id) {
    Response r = impl_->controller.RS485read(length, timeout, id);
    if (impl_->controller.getThrowOnCommandError() && !r.error_msg.empty()) {
        throw CodroidCommandException(r.id, "EC2RS485/read", r.error_msg, r.raw_json);
    }
    return r.db;
}

CommandResult CodroidClient::Rs485Write(const std::vector<uint8_t>& data, int id) {
    return to_client_result(impl_->controller.RS485write(data, id));
}

// --- 10.4 坐标系转换 ---

std::vector<double> CodroidClient::CposToCpos(const std::vector<double>& cp,
                                               const std::vector<double>& coor1, const std::vector<double>& tool1,
                                               const std::vector<double>& coor2, const std::vector<double>& tool2,
                                               int id) {
    return impl_->controller.cposToCpos(cp, coor1, tool1, coor2, tool2, id);
}

CartesianPoint CodroidClient::CposToCposPose(const CartesianPoint& cp,
                                              const std::vector<double>& coor1, const std::vector<double>& tool1,
                                              const std::vector<double>& coor2, const std::vector<double>& tool2,
                                              int id) {
    auto result = impl_->controller.cposToCpos(cp.cp, coor1, tool1, coor2, tool2, id);
    CartesianPoint out;
    out.cp = std::move(result);
    return out;
}

// --- 14. 寄存器扩展 ---

CommandResult CodroidClient::SetExtendArrayType(int index, ExtendArrayType type, int id) {
    return to_client_result(impl_->controller.setExtendArrayType(index, type, id));
}

CommandResult CodroidClient::RemoveExtendArray(int index, int id) {
    return to_client_result(impl_->controller.removeExtendArray(index, id));
}

// --- 19. 用户坐标系批量写入 ---

CommandResult CodroidClient::SaveUserCoordinateFrames(const std::vector<ClientRobotFrame>& frames, int id) {
    std::vector<RobotFrameEntry> internal;
    internal.reserve(frames.size());
    for (const auto& f : frames) {
        RobotFrameEntry e;
        e.id = f.id;
        e.x = f.x;
        e.y = f.y;
        e.z = f.z;
        e.a = f.a;
        e.b = f.b;
        e.c = f.c;
        internal.push_back(std::move(e));
    }
    return to_client_result(impl_->controller.saveUserCoordinateFrames(internal, id));
}

} // namespace Codroid
