/**
 * @file cri_realtime_dispatcher.hpp
 * @brief CRI 实时控制 UDP 下发（64 字节包），与 SDK_API_AND_DESIGN.md §7.2 / C# CriRealtimeDispatcher 对齐。
 *
 * @note 独立于 CodroidClient：先通过 TCP 完成 Connect、StartCriDataPush、StartCriControl，再构造本类向控制器 UDP 口发送轨迹。
 */

#ifndef CODROID_CRI_REALTIME_DISPATCHER_HPP
#define CODROID_CRI_REALTIME_DISPATCHER_HPP

#include "Codroid/CodroidExport.h"
#include "codroid/trajectory_types.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Codroid {

/**
 * @brief 向控制器 CRI 实时控制 UDP 端口发送 64 字节指令帧。
 *
 * 包布局（小端）：timestamp Int64@0 保留 0；position[6] Float64@8；type UInt8@56（0 关节 / 1 末端）；nc UInt8×7@57 保留 0。
 */
class CODROID_API CriRealtimeDispatcher {
public:
    /**
     * @param controller_ip 控制器 IP（可与 TCP 相同）。
     * @param controller_udp_port 默认 9030（与 C# 一致）。
     * @param convert_to_si 为 true 时：上层传入关节 **度**、笛卡尔 **mm + 度**，发送前转换为 **rad / m+rad**（与 CriRealtimePacketParser 反向成对）。
     */
    explicit CriRealtimeDispatcher(std::string controller_ip, int controller_udp_port = 9030,
                                   bool convert_to_si = true);
    ~CriRealtimeDispatcher();

    CriRealtimeDispatcher(const CriRealtimeDispatcher&) = delete;
    CriRealtimeDispatcher& operator=(const CriRealtimeDispatcher&) = delete;

    CriRealtimeDispatcher(CriRealtimeDispatcher&& other) noexcept;
    CriRealtimeDispatcher& operator=(CriRealtimeDispatcher&& other) noexcept;

    /** @brief 发送单帧（阻塞直到操作系统写完或出错）。 */
    void SendCommand(const std::array<double, 6>& position6, TrajectorySpace space);

    /**
     * @brief 按周期下发整条轨迹：首帧立即发送，之后每隔 period_ms 发送下一帧（与 C# PeriodicTimer 语义一致）。
     * @param cancel 非空且 *cancel==true 时提前结束（不发剩余点）。
     * @param period_ms 须满足 (0, 1000]，且建议与 StartCriControl 的 durationMs 一致。
     */
    void SendTrajectory(const std::vector<TrajectoryPoint>& trajectory, TrajectorySpace space, int period_ms,
                        const std::atomic<bool>* cancel = nullptr);

    /** @brief 关闭 UDP；之后调用 Send* 将抛 CodroidException。 */
    void Close() noexcept;

    bool IsOpen() const noexcept { return static_cast<bool>(impl_); }

private:
    void ensure_open_() const;
    void send_packet_(const std::array<double, 6>& position6, TrajectorySpace space);
    static void validate_period_ms_(int period_ms);

    std::string controller_ip_;
    uint16_t controller_port_;
    bool convert_to_si_;

    struct IoContextSocket;
    std::unique_ptr<IoContextSocket> impl_;
};

} // namespace Codroid

#endif
