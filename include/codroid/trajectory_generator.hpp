/**
 * @file trajectory_generator.hpp
 * @brief 离线轨迹生成（与 TRAJECTORY_ALGORITHM.md、SDK_API_AND_DESIGN.md §7.1 对齐）。
 */

#ifndef CODROID_TRAJECTORY_GENERATOR_HPP
#define CODROID_TRAJECTORY_GENERATOR_HPP

#include "Codroid/CodroidExport.h"
#include "codroid/trajectory_types.hpp"

#include <array>
#include <vector>

namespace Codroid {

class CODROID_API TrajectoryGenerator {
public:
    static std::vector<TrajectoryPoint> Generate(const std::array<double, 6>& start,
                                                 const std::array<double, 6>& target,
                                                 const TrajectoryRequest& request);

    /** @brief 多段路点拼接；跳过后续段首点，避免重复采样（TRAJECTORY_ALGORITHM.md §6）。 */
    static std::vector<TrajectoryPoint> GenerateMultiSegment(const std::vector<std::array<double, 6>>& waypoints,
                                                             const TrajectoryRequest& request);
};

} // namespace Codroid

#endif
