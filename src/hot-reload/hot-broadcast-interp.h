// 09 23 2026
/* purpose
* Define the generic, hot-replaceable server broadcast-interpolation policy hook
* and the ONE implementation shared by the cold EXE fallback and the hot
* provider. The EXE owns the sample buffer, the bracketing search, and the lerp
* mechanism; a hot module owns whether smoothing is enabled and how far the
* broadcast may move per tick.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own the sample buffer or the interpolation mechanism.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameBroadcastInterpV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t configSmoothing;   // NetworkingConfig serverSmoothing
    float configMaxSpeed;            // serverBroadcastMaxSpeed (0 = unlimited)
    float dt;
    // out
    std::uint32_t useSmoothing;      // 1 = run the interpolation mechanism
    float maxDelta;                  // per-tick movement cap (0 = unlimited)
    std::uint32_t result;
};

using GameBroadcastInterpFn = void (MIMITA_GAME_CALL *)(
    void* host, GameBroadcastInterpV1* request);

static constexpr std::uint64_t GAME_CAP_BROADCAST_INTERP =
    gameHash("net.broadcast-interp");
static constexpr std::uint64_t GAME_SIG_BROADCAST_INTERP =
    gameHash("sig.net.broadcast-interp.v1");

namespace HotBroadcastInterpImpl {

inline void evaluate(GameBroadcastInterpV1& r)
{
    r.useSmoothing = r.configSmoothing ? 1u : 0u;
    r.maxDelta = r.configMaxSpeed > 0.0f ? r.configMaxSpeed * r.dt : 0.0f;
    r.result = 1u;
}

} // namespace HotBroadcastInterpImpl

} // namespace MimitaNet
