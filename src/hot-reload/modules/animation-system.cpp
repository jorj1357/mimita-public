// 09 14 2026
/* purpose
* animation.main: the local-player animation step as a hot generic runtime
* system. It registers in the post-movement domain, which the kernel times after
* every movement path, and resolves generic capabilities by id (animation.update
* bridge today; skeleton.apply for a fully hot pose computation next).
* Owning the call here is what makes animation reachable while hot movement
* skips the built-in physics step.
* Does NOT own the animator implementation yet; that is the next hot step.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <cstdint>

namespace {

// The local player's animation is now owned by the generic hot path
// (hot.animation-policy -> pose-generation -> skeleton.apply -> render). This
// system no longer calls the typed `animation.update` bridge for the local
// player: the typed procedural pose no longer owns THE_PLAYER's animation.
// The bridge capability is retained for replay/legacy compatibility only.
void MIMITA_GAME_CALL animationMainTick(void* /*host*/, std::uint64_t /*tick*/,
                                        float /*dt*/)
{
    // Intentionally empty: the generic hot pose path owns local animation now.
}

const MimitaHotPackage::SystemRegistrar s_animationMain{
    {gameHash("animation.main"), GAME_DOMAIN_POST_MOVEMENT, 0, 0,
     animationMainTick, "animation.main"}};

} // namespace

#endif
