// 09 23 2026
/* purpose
* Define the generic, hot-replaceable client predicted-projectile correction
* policy and the ONE implementation shared by the cold EXE fallback and the hot
* provider. The EXE owns the physics simulation, render state, and server-state
* buffers; a hot module owns when a server disagreement is large enough to
* correct and how gently.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own projectile state or transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameProjectileCorrectionV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t serverHasSentUpdate;
    float positionError;
    float errorThreshold;      // e.g. 4.0
    float positionBlend;       // e.g. 0.3
    float velocityBlend;       // e.g. 0.25
    float rotationBlend;       // e.g. 0.20
    // out
    std::uint32_t correct;     // 1 = blend toward server state
    float outPositionBlend;
    float outVelocityBlend;
    float outRotationBlend;
    std::uint32_t result;
};

using GameProjectileCorrectionFn = void (MIMITA_GAME_CALL *)(
    void* host, GameProjectileCorrectionV1* request);

static constexpr std::uint64_t GAME_CAP_PROJECTILE_CORRECTION =
    gameHash("net.projectile-correction");
static constexpr std::uint64_t GAME_SIG_PROJECTILE_CORRECTION =
    gameHash("sig.net.projectile-correction.v1");

namespace HotProjectileCorrectionImpl {

inline void evaluate(GameProjectileCorrectionV1& r)
{
    r.correct = (r.serverHasSentUpdate && r.positionError > r.errorThreshold) ? 1u : 0u;
    r.outPositionBlend = r.positionBlend;
    r.outVelocityBlend = r.velocityBlend;
    r.outRotationBlend = r.rotationBlend;
    r.result = 1u;
}

} // namespace HotProjectileCorrectionImpl

} // namespace MimitaNet
