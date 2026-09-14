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

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

using AnimationUpdateFn = void (MIMITA_GAME_CALL *)(void*, float, std::uint32_t);

void MIMITA_GAME_CALL animationMainTick(void* host, std::uint64_t /*tick*/, float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability || !ctx->readComponent)
        return;

    std::uint32_t flags = 0;
    if (GameSharedStateV1* shared = sharedState(ctx)) {
        if (shared->localPlayerEntity != 0 &&
            (shared->modeFlags & GAME_MODE_FLAG_CREATION) == 0) {
            GameMovementIntentComponentV1 mi{};
            if (ctx->readComponent(ctx->host, shared->localPlayerEntity,
                                   GAME_COMPONENT_MOVEMENT_INTENT, &mi, sizeof(mi)) &&
                mi.pressed) {
                flags |= 1u;
            }
        }
    }

    auto fn = reinterpret_cast<AnimationUpdateFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_ANIMATION_UPDATE));
    if (fn)
        fn(ctx->host, dt, flags);
}

const MimitaHotPackage::SystemRegistrar s_animationMain{
    {gameHash("animation.main"), GAME_DOMAIN_POST_MOVEMENT, 0, 0,
     animationMainTick, "animation.main"}};

} // namespace

#endif
