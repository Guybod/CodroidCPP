/**
 * @file client.hpp
 * @brief CodroidClient — PascalCase public API aligned with AGENTS.md §4.1 (C# names).
 *
 * Internal transport/protocol implementation remains in CodroidController; this type
 * hides the legacy camelCase / snake_case method names from typical customer includes.
 */

#ifndef CODROID_SDK_CLIENT_HPP
#define CODROID_SDK_CLIENT_HPP

#include "Codroid/CodroidController.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Codroid {

class CODROID_API CodroidClient : private CodroidController {
public:
    using CodroidController::CodroidController;

    // --- Local kinematics (.so), not controller JSON ---
    using CodroidController::kinematicsInit;
    using CodroidController::kinematicsInit_mm_deg;
    using CodroidController::kinematicsFk;
    using CodroidController::kinematicsIk;

    static void PrintResponse(const Response& resp) { printResponse(resp); }

    /** @brief 见 CodroidController::NextRequestId()；多线程并发发指令时请用此分配唯一 id。 */
    int NextRequestId() { return CodroidController::NextRequestId(); }

    // --- Connection (C# naming) ---
    bool Connect(const std::string& ip, int port = 9001) { return connectTcp(ip, port); }
    /** TCP + Auto + Remote + optional CRI (local_ip non-empty) + SwitchOn. */
    bool ConnectRemoteAndSwitchOn(const std::string& ip, int port = 9001, std::string local_ip = {});
    void Disconnect() { disconnect(); }

    // --- Project ---
    Response EnterRemoteScriptMode(int id = 1) { return enterRemoteScriptMode(id); }
    Response RunScript(const RunScriptParams& params, int id = 1) { return runScript(params, id); }
    Response Run(const std::string& projectId, int id = 1) { return runProject(projectId, id); }
    Response RunByIndex(int index, int id = 1) { return runProjectByIndex(index, id); }
    Response RunStep(const std::string& projectId, int id = 1) { return runStep(projectId, id); }
    Response PauseProject(int id = 1) { return pauseProject(id); }
    Response ResumeProject(int id = 1) { return resumeProject(id); }
    Response StopProject(int id = 1) { return stopProject(id); }

    // --- Global variables ---
    Response GetGlobalVars(int id = 1) { return getGlobalVars(id); }
    std::map<std::string, Variable> GetGlobalVarsCatalog(int id = 1);
    Response SaveGlobalVar(const std::string& name, const Variable& value, int id = 1);
    Response SaveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1) {
        return saveGlobalVars(vars, id);
    }
    Response RemoveGlobalVars(const std::vector<std::string>& names, int id = 1) {
        return removeGlobalVars(names, id);
    }

    // --- Mode / system (C# names) ---
    Response SwitchOn(int id = 1) { return switchOn(id); }
    Response SwitchOff(int id = 1) { return switchOff(id); }
    Response ToManual(int id = 1) { return toManualDirect(id); }
    Response EnterManualModeViaAuto(int id = 1) { return toManual(id); }
    Response ToAuto(int id = 1) { return toAuto(id); }
    Response ToRemote(int id = 1) { return toRemoteDirect(id); }
    Response EnterRemoteModeViaAuto(int id = 1) { return toRemote(id); }
    Response ToSimulation(int id = 1) { return toSimulation(id); }
    Response ToActual(int id = 1) { return toActual(id); }
    Response StartDrag(int id = 1) { return startDrag(id); }
    Response StopDrag(int id = 1) { return stopDrag(id); }
    Response ClearSystemError(int id = 1) { return clearError(id); }

    // --- IO ---
    std::vector<IOInfo> GetIoValues(const std::vector<IOInfo>& pins, int id = 1) {
        return getIOValues(pins, id);
    }
    int GetDi(int port, int id = 1) { return getDI(port, id); }
    int GetDo(int port, int id = 1) { return getDO(port, id); }
    double GetAi(int port, int id = 1) { return getAI(port, id); }
    double GetAo(int port, int id = 1) { return getAO(port, id); }
    Response SetDo(int port, int value, int id = 1) { return setDO(port, value, id); }
    Response SetAo(int port, double value, int id = 1) { return setAO(port, value, id); }

    // --- Registers ---
    double GetRegisterValue(int address, int id = 1) { return getRegisterValue(address, id); }
    std::vector<RegisterInfo> GetRegisterValues(const std::vector<int>& addresses, int id = 1) {
        return getRegisterValues(addresses, id);
    }
    Response SetRegisterValue(int address, double value, int id = 1) {
        return setRegisterValue(address, value, id);
    }
    Response SetExtendArrayType(int index, ExtendArrayType type, int id = 1) {
        return setExtendArrayType(index, type, id);
    }
    Response RemoveExtendArray(int index, int id = 1) { return removeExtendArray(index, id); }

    // --- Protocol kinematics ---
    std::vector<double> AposToCpos(const FKParams& params, int id = 1) {
        return forwardKinematics(params, id);
    }
    std::vector<double> AposToCposPose(const FKParams& params, int id = 1) {
        return forwardKinematics(params, id);
    }
    std::vector<double> CposToApos(const IKParams& params, int id = 1) {
        return inverseKinematics(params, id);
    }
    std::vector<double> CposToAposJoints(const IKParams& params, int id = 1) {
        return inverseKinematics(params, id);
    }
    std::vector<double> CalculateRelativePose(const RelativePoseParams& params, int id = 1) {
        return calculateRelativePose(params, id);
    }
    std::vector<double> CalculateRelativePoseResult(const RelativePoseParams& params, int id = 1) {
        return calculateRelativePose(params, id);
    }

    // --- Motion ---
    Response StartJog(const JogParams& params, int id = 1) { return jog(params, id); }
    Response StopJog(int id = 1) { return stopJog(id); }
    Response JogHeartbeat(int id = 1) { return jogHeartbeat(id); }
    Response MoveTo(const MoveToParams& params, int id = 1) { return moveTo(params, id); }
    Response MoveToHeartbeat(int id = 1) { return moveToHeartbeat(id); }
    Response SetManualMoveRate(int pct, int id = 1) { return setManualSpeedRate(pct, id); }
    Response SetAutoMoveRate(int pct, int id = 1) { return setAutoSpeedRate(pct, id); }
    Response SetCollisionSensitivity(int sensitivity, int id = 1) {
        return setCollisionSensitivity(sensitivity, id);
    }
    Response SetPayload(int payloadId, int id = 1) { return setPayload(payloadId, id); }
    Response Move(const std::vector<MoveInstruction>& path, int id = 1) { return move(path, id); }
    Response PauseRobotMotion(int id = 1) { return pauseMove(id); }
    Response ResumeRobotMotion(int id = 1) { return resumeMove(id); }
    Response StopRobotMove(int id = 1) { return stopMove(id); }

    // --- CRI ---
    Response StartCriDataPush(const std::string& udpIp, int udpPort, int id = 1) {
        return startDataPush(udpIp, udpPort, 100, id, true);
    }
    Response StopCriDataPush(int id = 1) { return stopDataPush(id); }
    Response StopCriDataPush(const std::string& udpIp, int udpPort, int id = 1) {
        return stopDataPush(udpIp, udpPort, id);
    }
    int GetCriUdpListenPort() const { return getCriUdpListenPort(); }
    RobotRealtimeState GetRobotRealtimeState() const { return getRobotRealtimeState(); }

    /** 等价 C# **`CriDataReceived`**；空函数则清空监听。 */
    void SetCriDataReceived(std::function<void(const RobotRealtimeState&)> cb) {
        CodroidController::setCriDataReceivedHandler(std::move(cb));
    }
    /** 严格模式：`err` non-empty、`sendCommand` 抛 **`CodroidCommandException`**。 */
    void SetThrowOnCommandError(bool enable) { CodroidController::setThrowOnCommandError(enable); }
    bool ThrowOnCommandError() const noexcept { return CodroidController::getThrowOnCommandError(); }

    Response StartCriControl(int filterType, int durationMs, int startBuffer, int id = 1) {
        return startControl(durationMs, startBuffer, filterType, id);
    }
    Response StopCriControl(int id = 1) { return stopControl(id); }

    PublishTopicSubscription SubscribePublishTopic(std::string topicTy,
                                                   PublishTopicHandler handler,
                                                   int tc_milliseconds = 100) {
        return subscribePublishTopic(std::move(topicTy), std::move(handler), tc_milliseconds);
    }

    // --- Project lines / misc (legacy protocol; not in §4.1 table) ---
    Response SetStartLine(int line, int id = 1) { return setStartLine(line, id); }
    Response ClearStartLine(int id = 1) { return clearStartLine(id); }
    Response GetProjectVar(int id = 1) { return getProjectVar(id); }

    // --- RS485 (optional; names unchanged from controller) ---
    using CodroidController::RS485init;
    using CodroidController::RS485flush;
    using CodroidController::RS485read;
    using CodroidController::RS485write;

    // --- mov* helpers (same as controller) ---
    using CodroidController::movJ;
    using CodroidController::movL;
    using CodroidController::movC;
    using CodroidController::movCircle;

    // Offline trajectory: `TrajectoryGenerator` in `codroid/trajectory_generator.hpp`. Real-time UDP: `CriRealtimeDispatcher`.
};

} // namespace Codroid

#endif
