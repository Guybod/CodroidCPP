/**
 * @file trajectory_types.hpp
 * @brief 轨迹生成与 CRI 下发共用类型（与 SDK_API_AND_DESIGN.md §7、TRAJECTORY_ALGORITHM.md 一致）。
 */

#ifndef CODROID_TRAJECTORY_TYPES_HPP
#define CODROID_TRAJECTORY_TYPES_HPP

#include <array>
#include <optional>

namespace Codroid {

/** @brief 关节空间（deg）或笛卡尔 [x,y,z,rx,ry,rz]（mm + deg，固定欧拉 XYZ 外旋）。 */
enum class TrajectorySpace {
    Joint,
    Cartesian
};

/** @brief 时间标度曲线种类（Cubic / Trapezoidal）。 */
enum class TrajectoryProfile {
    Cubic,
    Trapezoidal
};

/**
 * @brief 轨迹生成请求（与 C# TrajectoryRequest 字段对应）。
 * @note `speed` 与 `duration_seconds` 必须二选一；`acceleration` 在 §1.1 中要求恒大于 0（梯形规划使用；三次曲线亦校验）。
 */
struct TrajectoryRequest {
    TrajectorySpace space = TrajectorySpace::Joint;
    double frequency_hz = 250.0;
    std::optional<double> speed;
    std::optional<double> duration_seconds;
    TrajectoryProfile profile = TrajectoryProfile::Cubic;
    double acceleration = 1.0;
};

/**
 * @brief 采样点：`time_seconds` 为段内时间（多段拼接时 `TrajectoryGenerator::GenerateMultiSegment` 会叠加基座时间）。
 */
struct TrajectoryPoint {
    double time_seconds = 0.0;
    std::array<double, 6> position{};
};

} // namespace Codroid

#endif
