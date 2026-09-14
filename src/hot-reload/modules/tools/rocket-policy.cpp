// 09 14 2026
/* purpose
* Hot rocket-launcher projectile policy. Registers the rocket projectile's
* impact behavior through the generic combat router, replacing the cold
* per-type explode flags. Editing this file changes rocket impact policy live.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdio>

namespace {

// NETWORK_WEAPON_ROCKET_LAUNCHER value (see network/packets.h). Kept as a plain
// constant so the hot package does not depend on network internals.
constexpr std::uint64_t kRocketNetworkId = 5;

void MIMITA_GAME_CALL rocketImpactPolicy(const ProjectileImpactPolicyV1* impact,
                                         GameplayContextV1* /*context*/)
{
    if (!impact)
        return;
    // Rockets always detonate on any impact, including end of life.
    const_cast<ProjectileImpactPolicyV1*>(impact)->outExplode = 1;
    std::printf("[ROCKET.POLICY] impact type=%llu hitKind=%u victim=%u\n",
                (unsigned long long)impact->projectileTypeId,
                (unsigned)impact->hitKind, (unsigned)impact->victimId);
}

} // namespace

const MimitaHotPackage::ProjectileBehaviorRegistrar s_rocketPolicy{
    kRocketNetworkId, rocketImpactPolicy};

#endif
