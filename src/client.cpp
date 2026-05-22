#include "Codroid/client.hpp"

#include "Codroid/CodroidController.h"

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

CommandResult CodroidClient::MovJ(const ClientJointPoint& target, double speed, double acceleration, int id) {
    return to_client_result(impl_->controller.movJ(target, speed, acceleration, id));
}

CommandResult CodroidClient::MovJ(const ClientCartesianPoint& target, double speed, double acceleration, int id) {
    return to_client_result(impl_->controller.movJ(target, speed, acceleration, id));
}

CommandResult CodroidClient::MovJ(const ClientMovePoint& target, double speed, double acceleration, int id) {
    return to_client_result(impl_->controller.movJ(target, speed, acceleration, id));
}

CommandResult CodroidClient::MovL(const ClientCartesianPoint& target, double speed, double acceleration,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(target, speed, acceleration, coor, tool, id));
}

CommandResult CodroidClient::MovL(const ClientJointPoint& target, double speed, double acceleration,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(target, speed, acceleration, coor, tool, id));
}

CommandResult CodroidClient::MovL(const ClientMovePoint& target, double speed, double acceleration,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(target, speed, acceleration, coor, tool, id));
}

CommandResult CodroidClient::MovC(const ClientCartesianPoint& middle,
                                  const ClientCartesianPoint& target,
                                  double speed,
                                  double acceleration,
                                  int id) {
    ClientCartesianPoint mid = middle;
    ClientCartesianPoint tgt = target;
    return Move({ClientMoveInstruction::MovC(std::move(mid), std::move(tgt), speed, acceleration)}, id);
}

CommandResult CodroidClient::MovCircle(const ClientCartesianPoint& middle,
                                       const ClientCartesianPoint& target,
                                       int circle_num,
                                       double speed,
                                       double acceleration,
                                       int id) {
    ClientCartesianPoint mid = middle;
    ClientCartesianPoint tgt = target;
    return Move({ClientMoveInstruction::MovCircle(std::move(mid), std::move(tgt), circle_num, speed, acceleration)},
                id);
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

} // namespace Codroid
