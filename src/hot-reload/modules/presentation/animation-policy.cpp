// 09 15 2026
/* purpose
* Hot animation policy. A render.frame system selects which animation clip a
* generic entity plays from generic actor state (Velocity, Health) and advances
* its AnimationState. The cold renderer still owns skeleton/skinning/draw; this
* file owns the "which animation / how fast / how long" decision. No
* Player/Npc/Monster animation type. Editing this file and saving changes the
* running client's animation behavior.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstdint>

namespace {

void MIMITA_GAME_CALL animationPolicyTick(void* host, std::uint64_t /*tick*/,
                                          float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent)
        return;

    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        HotAnimationStateV1 anim{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_ANIMATION_STATE_COMPONENT, &anim,
                                       sizeof(anim)))
            continue;

        // Generic actor state: velocity (optional) and health (optional).
        float speed = 0.0f;
        GameVelocityComponentV1 vel{};
        if (ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_VELOCITY,
                               &vel, sizeof(vel))) {
            speed = std::sqrt(vel.linear[0] * vel.linear[0] +
                              vel.linear[1] * vel.linear[1] +
                              vel.linear[2] * vel.linear[2]);
        }
        bool dead = false;
        GameHealthComponentV1 hp{};
        if (ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_HEALTH, &hp,
                               sizeof(hp)))
            dead = hp.dead != 0;

        // Hot policy: which clip. Kept intentionally small for the first proof.
        constexpr float kMoveSpeedThreshold = 0.5f;
        std::uint64_t clip = HOT_ANIM_IDLE;
        std::uint32_t loop = 1;
        if (dead) {
            clip = HOT_ANIM_DEAD;
            loop = 0;
        } else if (speed > kMoveSpeedThreshold) {
            clip = HOT_ANIM_MOVE;
        }

        if (anim.clipId != clip) {
            anim.clipId = clip;
            anim.playbackTime = 0.0f;  // restart on clip change
        }
        anim.loop = loop;
        const float rate = anim.playbackRate > 0.0f ? anim.playbackRate : 1.0f;
        anim.playbackTime += dt * rate;

        ctx->dynamicWriteComponent(ctx->host, entities[i],
                                   HOT_ANIMATION_STATE_COMPONENT, &anim,
                                   sizeof(anim));
    }
}

const MimitaHotPackage::SchemaRegistrar s_animationStateSchema{
    {HOT_ANIMATION_STATE_COMPONENT, gameHash("AnimationState.v1"),
     sizeof(HotAnimationStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "AnimationState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_animationPolicySystem{
    {gameHash("hot.animation-policy"), GAME_DOMAIN_RENDER, 1, 0,
     animationPolicyTick, "hot.animation-policy"}};

} // namespace

#endif
