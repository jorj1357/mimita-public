// 09 23 2026
/* purpose
* Define the generic, hot-replaceable NPC ground-clamp policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. When the
// decimated headless collision world misses the floor, a server NPC below the
* floor is pinned back onto it and marked grounded; airborne NPCs are untouched.
* The EXE owns the floor-height query and the body state; a hot module owns the
* rest height and whether/how the clamp applies.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own the world query or NPC movement.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameNpcGroundClampV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t haveFloor;      // floorZ > -1e5
    float floorZ;
    float posZ;
    float velZ;
    float restHeight;             // capsule half-height (cold-supplied default)
    // out
    std::uint32_t clamp;          // 1 = apply the clamp
    float outPosZ;
    float outVelZ;
    std::uint32_t result;
};

using GameNpcGroundClampFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameNpcGroundClampV1* request);

static constexpr std::uint64_t GAME_CAP_NPC_GROUND_CLAMP =
    gameHash("net.npc-ground-clamp");
static constexpr std::uint64_t GAME_SIG_NPC_GROUND_CLAMP =
    gameHash("sig.net.npc-ground-clamp.v1");

namespace HotNpcGroundClampImpl {

inline void evaluate(GameNpcGroundClampV1& r)
{
    r.clamp = 0u;
    r.outPosZ = r.posZ;
    r.outVelZ = r.velZ;
    r.result = 1u;

    if (!r.haveFloor)
        return;
    const float restZ = r.floorZ + r.restHeight;
    if (r.posZ < restZ) {
        r.clamp = 1u;
        r.outPosZ = restZ;
        r.outVelZ = r.velZ < 0.0f ? 0.0f : r.velZ;
    }
}

} // namespace HotNpcGroundClampImpl

} // namespace MimitaNet
