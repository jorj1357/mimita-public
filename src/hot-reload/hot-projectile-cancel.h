// 09 23 2026
/* purpose
* Define the generic, hot-replaceable projectile cancellation policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the projectile container, despawn, entity lifetime, and packet transport;
* a hot module owns when a live projectile must be cancelled (for example its
* owner died).
* POD only: no STL or engine objects cross the boundary.
* Does NOT own projectile storage, despawn, or transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

static constexpr std::uint64_t GAME_SIG_PROJECTILE_CANCEL =
    gameHash("sig.net.projectile-cancel.v1");

namespace HotProjectileCancelImpl {

// Preserve the current cold behavior: cancel a projectile whose required owner
// is confirmed dead. A hot provider may override.
inline void evaluate(GameProjectileCancelV1& r)
{
    r.outCancel = r.ownerDead ? 1u : 0u;
    r.outEmitDespawn = r.outCancel;
    r.handled = 1u;
    r.result = 1u;
    if (r.outCancel && r.reason[0] == '\0')
        r.reason[0] = '\0';
}

} // namespace HotProjectileCancelImpl

} // namespace MimitaNet
