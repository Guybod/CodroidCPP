#include "Codroid/cri_realtime_dispatcher.hpp"

#include "Codroid/CodroidDefine.h"

#include <asio.hpp>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Codroid {
namespace {

constexpr double kDegToRad = M_PI / 180.0;
constexpr double kMmToM = 0.001;

/** 总是按小端写入 8 字节 double，不依赖主机序。 */
inline void write_le_f64(std::uint8_t* dst, double value) {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    std::uint64_t u = 0;
    std::memcpy(&u, &value, sizeof(u));
    for (int i = 0; i < 8; ++i) {
        dst[i] = static_cast<std::uint8_t>((u >> (8 * i)) & 0xFFU);
    }
}

void apply_convert_to_si_(std::array<double, 6>& p, TrajectorySpace space, bool convert) {
    if (!convert) {
        return;
    }
    if (space == TrajectorySpace::Joint) {
        for (double& v : p) {
            v *= kDegToRad;
        }
    } else {
        p[0] *= kMmToM;
        p[1] *= kMmToM;
        p[2] *= kMmToM;
        p[3] *= kDegToRad;
        p[4] *= kDegToRad;
        p[5] *= kDegToRad;
    }
}

void build_packet_(std::array<std::uint8_t, 64>& out, const std::array<double, 6>& position6, TrajectorySpace space,
                   bool convert_to_si) {
    out.fill(0);
    // 0..7 timestamp = 0
    std::array<double, 6> p = position6;
    apply_convert_to_si_(p, space, convert_to_si);
    for (int i = 0; i < 6; ++i) {
        write_le_f64(&out[8 + 8 * i], p[i]);
    }
    out[56] = (space == TrajectorySpace::Joint) ? 0U : 1U;
    // 57..63 保持 0
}

} // namespace

struct CriRealtimeDispatcher::IoContextSocket {
    asio::io_context io;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint dest;

    IoContextSocket(const std::string& ip, std::uint16_t port) : socket(io) {
        asio::ip::address addr = asio::ip::make_address(ip);
        dest = asio::ip::udp::endpoint(addr, port);
        asio::error_code ec;
        socket.open(dest.protocol(), ec);
        if (ec) {
            throw CodroidException("CriRealtimeDispatcher: cannot open UDP socket: " + ec.message());
        }
    }
};

CriRealtimeDispatcher::CriRealtimeDispatcher(std::string controller_ip, int controller_udp_port, bool convert_to_si)
    : controller_ip_(std::move(controller_ip))
    , controller_port_(static_cast<std::uint16_t>(controller_udp_port))
    , convert_to_si_(convert_to_si) {
    if (controller_udp_port <= 0 || controller_udp_port > 65535) {
        throw CodroidException("CriRealtimeDispatcher: controller_udp_port out of range");
    }
    impl_ = std::make_unique<IoContextSocket>(controller_ip_, controller_port_);
}

CriRealtimeDispatcher::~CriRealtimeDispatcher() { Close(); }

CriRealtimeDispatcher::CriRealtimeDispatcher(CriRealtimeDispatcher&& other) noexcept
    : controller_ip_(std::move(other.controller_ip_))
    , controller_port_(other.controller_port_)
    , convert_to_si_(other.convert_to_si_)
    , impl_(std::move(other.impl_)) {}

CriRealtimeDispatcher& CriRealtimeDispatcher::operator=(CriRealtimeDispatcher&& other) noexcept {
    if (this != &other) {
        controller_ip_ = std::move(other.controller_ip_);
        controller_port_ = other.controller_port_;
        convert_to_si_ = other.convert_to_si_;
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void CriRealtimeDispatcher::Close() noexcept { impl_.reset(); }

void CriRealtimeDispatcher::ensure_open_() const {
    if (!impl_) {
        throw CodroidException("CriRealtimeDispatcher: object is closed");
    }
}

void CriRealtimeDispatcher::validate_period_ms_(int period_ms) {
    if (period_ms <= 0 || period_ms > 1000) {
        throw CodroidException("CriRealtimeDispatcher: period_ms must be in (0, 1000]");
    }
}

void CriRealtimeDispatcher::send_packet_(const std::array<double, 6>& position6, TrajectorySpace space) {
    std::array<std::uint8_t, 64> buf{};
    build_packet_(buf, position6, space, convert_to_si_);
    asio::error_code ec;
    impl_->socket.send_to(asio::buffer(buf), impl_->dest, 0, ec);
    if (ec) {
        throw CodroidException("CriRealtimeDispatcher: UDP send failed: " + ec.message());
    }
}

void CriRealtimeDispatcher::SendCommand(const std::array<double, 6>& position6, TrajectorySpace space) {
    ensure_open_();
    send_packet_(position6, space);
}

void CriRealtimeDispatcher::SendTrajectory(const std::vector<TrajectoryPoint>& trajectory, TrajectorySpace space,
                                           int period_ms, const std::atomic<bool>* cancel) {
    ensure_open_();
    validate_period_ms_(period_ms);
    if (trajectory.empty()) {
        return;
    }
    const auto interval = std::chrono::milliseconds(period_ms);
    for (size_t i = 0; i < trajectory.size(); ++i) {
        if (cancel && cancel->load(std::memory_order_acquire)) {
            return;
        }
        if (i > 0) {
            std::this_thread::sleep_for(interval);
            if (cancel && cancel->load(std::memory_order_acquire)) {
                return;
            }
        }
        send_packet_(trajectory[i].position, space);
    }
}

} // namespace Codroid
