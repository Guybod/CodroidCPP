#include "Codroid/trajectory_generator.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Codroid {
namespace {

constexpr double kEps = 1e-9;
constexpr double kLerpThreshold = 0.9995;
constexpr double kGimbalThreshold = 0.999999;

struct Quat {
    double w, x, y, z;
};

inline double deg_to_rad(double d) { return d * (M_PI / 180.0); }
inline double rad_to_deg(double r) { return r * (180.0 / M_PI); }

inline double clamp(double v, double lo, double hi) { return std::min(hi, std::max(lo, v)); }

inline double quat_len(const Quat& q) {
    return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

inline Quat quat_normalize(const Quat& q) {
    const double n = quat_len(q);
    if (n < 1e-15) {
        return {1, 0, 0, 0};
    }
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

inline double quat_dot(const Quat& a, const Quat& b) {
    return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

/** TRAJECTORY_ALGORITHM.md §5.1，输入度。 */
Quat to_quaternion(double rxDeg, double ryDeg, double rzDeg) {
    const double a = deg_to_rad(rxDeg) * 0.5;
    const double b = deg_to_rad(ryDeg) * 0.5;
    const double c = deg_to_rad(rzDeg) * 0.5;
    const double cx = std::cos(a);
    const double sx = std::sin(a);
    const double cy = std::cos(b);
    const double sy = std::sin(b);
    const double cz = std::cos(c);
    const double sz = std::sin(c);
    Quat q;
    q.w = cz * cy * cx + sz * sy * sx;
    q.x = cz * cy * sx - sz * sy * cx;
    q.y = cz * sy * cx + sz * cy * sx;
    q.z = sz * cy * cx - cz * sy * sx;
    return q;
}

/** TRAJECTORY_ALGORITHM.md §5.2，输出度。 */
void from_quaternion(const Quat& q0, double& rxDeg, double& ryDeg, double& rzDeg) {
    const Quat q = quat_normalize(q0);
    const double sb = clamp(2.0 * (q.w * q.y - q.x * q.z), -1.0, 1.0);
    double ry = std::asin(sb);

    double rx;
    double rz;
    if (std::abs(sb) < kGimbalThreshold) {
        rx = std::atan2(2.0 * (q.y * q.z + q.w * q.x), 1.0 - 2.0 * (q.x * q.x + q.y * q.y));
        rz = std::atan2(2.0 * (q.x * q.y + q.w * q.z), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
    } else {
        rx = std::atan2(-2.0 * (q.y * q.z - q.w * q.x), 1.0 - 2.0 * (q.x * q.x + q.z * q.z));
        rz = 0.0;
    }
    rxDeg = rad_to_deg(rx);
    ryDeg = rad_to_deg(ry);
    rzDeg = rad_to_deg(rz);
}

/** TRAJECTORY_ALGORITHM.md §5.3 */
Quat slerp(Quat q0, Quat q1, double t) {
    q0 = quat_normalize(q0);
    q1 = quat_normalize(q1);
    double dotv = quat_dot(q0, q1);
    if (dotv < 0.0) {
        q1.w = -q1.w;
        q1.x = -q1.x;
        q1.y = -q1.y;
        q1.z = -q1.z;
        dotv = -dotv;
    }
    if (dotv > kLerpThreshold) {
        Quat r{q0.w + static_cast<double>(t) * (q1.w - q0.w), q0.x + static_cast<double>(t) * (q1.x - q0.x),
               q0.y + static_cast<double>(t) * (q1.y - q0.y), q0.z + static_cast<double>(t) * (q1.z - q0.z)};
        return quat_normalize(r);
    }
    const double theta0 = std::acos(clamp(dotv, -1.0, 1.0));
    const double theta = theta0 * static_cast<double>(t);
    const double s0 = std::cos(theta) - dotv * std::sin(theta) / std::sin(theta0);
    const double s1 = std::sin(theta) / std::sin(theta0);
    Quat out{s0 * q0.w + s1 * q1.w, s0 * q0.x + s1 * q1.x, s0 * q0.y + s1 * q1.y, s0 * q0.z + s1 * q1.z};
    return quat_normalize(out);
}

struct MotionProfile {
    double T = 0;
    std::function<double(double)> scale_at;
};

void validate_inputs(const TrajectoryRequest& req) {
    if (!(req.frequency_hz > 0.0)) {
        throw std::invalid_argument("TrajectoryGenerator: FrequencyHz must be > 0");
    }
    const bool has_speed = req.speed.has_value();
    const bool has_dur = req.duration_seconds.has_value();
    if (has_speed == has_dur) {
        throw std::invalid_argument("TrajectoryGenerator: exactly one of speed or duration_seconds must be set");
    }
    if (has_speed && *req.speed <= 0.0) {
        throw std::invalid_argument("TrajectoryGenerator: speed must be > 0");
    }
    if (has_dur && *req.duration_seconds <= 0.0) {
        throw std::invalid_argument("TrajectoryGenerator: duration_seconds must be > 0");
    }
    if (!(req.acceleration > 0.0)) {
        throw std::invalid_argument("TrajectoryGenerator: acceleration must be > 0");
    }
}

MotionProfile make_cubic(double T) {
    MotionProfile p;
    p.T = T;
    p.scale_at = [T](double t) {
        if (t <= 0.0) {
            return 0.0;
        }
        if (t >= T) {
            return 1.0;
        }
        const double tau = t / T;
        return 3.0 * tau * tau - 2.0 * tau * tau * tau;
    };
    return p;
}

MotionProfile make_trapezoid(double T, double D, double V, double A, double Ta) {
    MotionProfile p;
    p.T = T;
    p.scale_at = [=](double t) {
        if (t <= 0.0) {
            return 0.0;
        }
        if (t >= T) {
            return 1.0;
        }
        double arc = 0.0;
        if (t < Ta) {
            arc = 0.5 * A * t * t;
        } else if (t > T - Ta) {
            const double tt = T - t;
            arc = D - 0.5 * A * tt * tt;
        } else {
            arc = 0.5 * A * Ta * Ta + V * (t - Ta);
        }
        double s = arc / D;
        return clamp(s, 0.0, 1.0);
    };
    return p;
}

MotionProfile trapezoid_from_speed(double D, double v, double a) {
    const double ta = v / a;
    const double da = 0.5 * v * ta;
    if (2.0 * da >= D) {
        const double vp = std::sqrt(a * D);
        const double ta2 = vp / a;
        const double T = 2.0 * ta2;
        return make_trapezoid(T, D, vp, a, ta2);
    }
    const double tc = (D - 2.0 * da) / v;
    const double T = 2.0 * ta + tc;
    return make_trapezoid(T, D, v, a, ta);
}

MotionProfile trapezoid_from_duration(double D, double T, double a) {
    const double disc = a * a * T * T - 4.0 * a * D;
    if (disc < 0.0) {
        const double vp = 2.0 * D / T;
        const double a_eff = 4.0 * D / (T * T);
        return make_trapezoid(T, D, vp, a_eff, T / 2.0);
    }
    const double v = (a * T - std::sqrt(disc)) / 2.0;
    const double ta = v / a;
    return make_trapezoid(T, D, v, a, ta);
}

MotionProfile compute_profile(double D, const TrajectoryRequest& req) {
    validate_inputs(req);

    if (req.profile == TrajectoryProfile::Cubic) {
        double T = 0;
        if (req.duration_seconds.has_value()) {
            T = *req.duration_seconds;
        } else {
            T = D / *req.speed;
        }
        return make_cubic(T);
    }

    // Trapezoidal
    if (req.speed.has_value()) {
        const double v = *req.speed;
        return trapezoid_from_speed(D, v, req.acceleration);
    }
    const double T = *req.duration_seconds;
    return trapezoid_from_duration(D, T, req.acceleration);
}

std::vector<TrajectoryPoint> generate_joint(const std::array<double, 6>& q0, const std::array<double, 6>& qf,
                                            const TrajectoryRequest& req) {
    double max_delta = 0.0;
    for (int i = 0; i < 6; ++i) {
        max_delta = std::max(max_delta, std::abs(qf[i] - q0[i]));
    }
    if (max_delta < kEps) {
        TrajectoryPoint one;
        one.time_seconds = 0.0;
        one.position = q0;
        return {one};
    }

    validate_inputs(req);
    MotionProfile prof = [&] {
        if (req.profile == TrajectoryProfile::Cubic) {
            double T = 0;
            if (req.duration_seconds.has_value()) {
                T = *req.duration_seconds;
            } else {
                T = max_delta / *req.speed;
            }
            return make_cubic(T);
        }
        if (req.speed.has_value()) {
            return trapezoid_from_speed(max_delta, *req.speed, req.acceleration);
        }
        return trapezoid_from_duration(max_delta, *req.duration_seconds, req.acceleration);
    }();

    const double dt = 1.0 / req.frequency_hz;
    const int n = std::max(2, static_cast<int>(std::ceil(prof.T / dt)) + 1);
    std::vector<TrajectoryPoint> out;
    out.reserve(static_cast<size_t>(n));
    for (int k = 0; k < n; ++k) {
        const double t = std::min(static_cast<double>(k) * dt, prof.T);
        const double s = prof.scale_at(t);
        TrajectoryPoint pt;
        pt.time_seconds = t;
        for (int i = 0; i < 6; ++i) {
            pt.position[i] = q0[i] + s * (qf[i] - q0[i]);
        }
        out.push_back(pt);
    }
    return out;
}

std::vector<TrajectoryPoint> generate_cartesian(const std::array<double, 6>& p0, const std::array<double, 6>& pf,
                                                const TrajectoryRequest& req) {
    const double dx = pf[0] - p0[0];
    const double dy = pf[1] - p0[1];
    const double dz = pf[2] - p0[2];
    const double D = std::sqrt(dx * dx + dy * dy + dz * dz);

    const Quat q0 = quat_normalize(to_quaternion(p0[3], p0[4], p0[5]));
    const Quat q1 = quat_normalize(to_quaternion(pf[3], pf[4], pf[5]));

    if (D < kEps) {
        const double ad = std::abs(quat_dot(q0, q1));
        if ((1.0 - std::abs(ad)) < 1e-9) {
            TrajectoryPoint one;
            one.time_seconds = 0.0;
            one.position = p0;
            return {one};
        }
        if (req.speed.has_value()) {
            throw std::invalid_argument("TrajectoryGenerator: pure orientation motion requires duration_seconds, not speed");
        }
    }

    MotionProfile prof;
    if (D >= kEps) {
        prof = compute_profile(D, req);
    } else {
        validate_inputs(req);
        if (req.profile == TrajectoryProfile::Cubic) {
            double T = *req.duration_seconds;
            prof = make_cubic(T);
        } else if (req.speed.has_value()) {
            throw std::invalid_argument("TrajectoryGenerator: pure orientation motion requires duration_seconds, not speed");
        } else {
            prof = trapezoid_from_duration(1.0, *req.duration_seconds, req.acceleration);
        }
    }

    const double dt = 1.0 / req.frequency_hz;
    const int n = std::max(2, static_cast<int>(std::ceil(prof.T / dt)) + 1);
    std::vector<TrajectoryPoint> out;
    out.reserve(static_cast<size_t>(n));

    for (int k = 0; k < n; ++k) {
        const double t = std::min(static_cast<double>(k) * dt, prof.T);
        const double s = prof.scale_at(t);
        TrajectoryPoint pt;
        pt.time_seconds = t;
        pt.position[0] = p0[0] + s * dx;
        pt.position[1] = p0[1] + s * dy;
        pt.position[2] = p0[2] + s * dz;
        const Quat qs = slerp(q0, q1, s);
        from_quaternion(qs, pt.position[3], pt.position[4], pt.position[5]);
        out.push_back(pt);
    }
    return out;
}

} // namespace

std::vector<TrajectoryPoint> TrajectoryGenerator::Generate(const std::array<double, 6>& start,
                                                           const std::array<double, 6>& target,
                                                           const TrajectoryRequest& request) {
    if (request.space == TrajectorySpace::Joint) {
        return generate_joint(start, target, request);
    }
    return generate_cartesian(start, target, request);
}

std::vector<TrajectoryPoint> TrajectoryGenerator::GenerateMultiSegment(const std::vector<std::array<double, 6>>& waypoints,
                                                                       const TrajectoryRequest& request) {
    if (waypoints.size() < 2) {
        return {};
    }
    std::vector<TrajectoryPoint> result;
    double t_base = 0.0;
    for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
        auto seg = Generate(waypoints[i], waypoints[i + 1], request);
        for (size_t k = 0; k < seg.size(); ++k) {
            if (i > 0 && k == 0) {
                continue;
            }
            TrajectoryPoint p = seg[k];
            p.time_seconds += t_base;
            result.push_back(p);
        }
        if (!seg.empty()) {
            t_base += seg.back().time_seconds;
        }
    }
    return result;
}

} // namespace Codroid
