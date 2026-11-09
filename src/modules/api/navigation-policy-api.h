// 2026-11-08
/* purpose
* hot-reloadable navigation rebuild/fallback policy
* host owns Recast/Detour meshes, collision data, and query execution
* this file DOES NOT own world geometry, actors, or collision state
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_navigation_policy {

constexpr std::uint32_t ABI_VERSION = 1;

// Inputs describe why the host thinks a rebuild may be needed. Outputs decide
// whether the fixed tick may rebuild, whether streamed geometry is ignored, and
// how much work is allowed. The host can always fall back to the custom planner
// when no published navmesh is available.
struct NavigationPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t hasMesh;
    std::uint32_t staticTriangles;
    std::uint32_t streamedTriangles;
    std::uint32_t collisionChanged;
    std::uint32_t profileChanged;
    std::uint32_t inFixedTick;
    std::uint32_t dirtyTiles;
    float elapsedMs;
    std::uint32_t outAllowRebuild;
    std::uint32_t outStaticOnly;
    std::uint32_t outTilesPerUpdate;
    float outMaxBuildMs;
    std::uint32_t outGeneration;
};

} // namespace mimita_navigation_policy
