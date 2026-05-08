#include "codroid/client.hpp"

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

MovePoint to_internal_point(const ClientMovePoint& point) {
    MovePoint out;
    out.jp = point.jp;
    out.cp = point.cp;
    out.rj = point.rj;
    out.ep = point.ep;
    return out;
}

MoveInstruction to_internal_instruction(const ClientMoveInstruction& inst) {
    MoveInstruction out;
    out.type = to_internal_move_type(inst.type);
    out.speed = inst.speed;
    out.acc = inst.acceleration;
    out.blend = inst.blend;
    out.relativeBlend = inst.relative_blend;
    out.circleNum = inst.circle_num;
    out.targetPoint = to_internal_point(inst.target);
    out.middlePoint = to_internal_point(inst.middle);
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

ClientMovePoint ClientMovePoint::Joint(std::vector<double> joints_deg) {
    ClientMovePoint out;
    out.jp = std::move(joints_deg);
    return out;
}

ClientMovePoint ClientMovePoint::Cartesian(std::vector<double> pose_mm_deg) {
    ClientMovePoint out;
    out.cp = std::move(pose_mm_deg);
    return out;
}

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

CommandResult CodroidClient::MovJ(const std::vector<double>& joints_deg, double speed, double acceleration, int id) {
    return to_client_result(impl_->controller.movJ(joints_deg, speed, acceleration, id));
}

CommandResult CodroidClient::MovL(const std::vector<double>& pose_mm_deg, double speed, double acceleration,
                                  const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    return to_client_result(impl_->controller.movL(pose_mm_deg, speed, acceleration, coor, tool, id));
}

CommandResult CodroidClient::MovC(const std::vector<double>& middle_pose_mm_deg,
                                  const std::vector<double>& target_pose_mm_deg,
                                  double speed,
                                  double acceleration,
                                  int id) {
    ClientMoveInstruction inst;
    inst.type = ClientMoveType::MovC;
    inst.speed = speed;
    inst.acceleration = acceleration;
    inst.middle = ClientMovePoint::Cartesian(middle_pose_mm_deg);
    inst.target = ClientMovePoint::Cartesian(target_pose_mm_deg);
    return MovePath({inst}, id);
}

CommandResult CodroidClient::MovCircle(const std::vector<double>& middle_pose_mm_deg,
                                       const std::vector<double>& target_pose_mm_deg,
                                       int circle_num,
                                       double speed,
                                       double acceleration,
                                       int id) {
    ClientMoveInstruction inst;
    inst.type = ClientMoveType::MovCircle;
    inst.speed = speed;
    inst.acceleration = acceleration;
    inst.circle_num = circle_num;
    inst.middle = ClientMovePoint::Cartesian(middle_pose_mm_deg);
    inst.target = ClientMovePoint::Cartesian(target_pose_mm_deg);
    return MovePath({inst}, id);
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
