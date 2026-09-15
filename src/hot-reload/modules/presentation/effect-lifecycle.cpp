// 09 15 2026
/* purpose
* Hot effect lifecycle. A render.frame system owns the lifetime of any entity
* carrying the generic `EffectLifetime` component: it ages, integrates Transform
* by Velocity, grows/fades the entity's PresentationState, and destroys the
* entity on expiry. Any runtime package can create a brand-new effect from
* generic data + logical resources; no EXE enum, switch, or ABI field.
* Cold renderer only draws the entity (mesh/effect primitives).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-effect.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"

#include <algorithm>
#include <cstdint>

namespace {

void MIMITA_GAME_CALL effectLifecycleTick(void* host, std::uint64_t /*tick*/,
                                          float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->dynamicWriteComponent || !ctx->readComponent ||
        !ctx->writeComponent || !ctx->entityDestroy)
        return;

    std::uint64_t entities[128];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_EFFECT_LIFETIME_COMPONENT, entities, 128);
    for (std::uint32_t i = 0; i < count; ++i) {
        HotEffectLifetimeV1 life{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_EFFECT_LIFETIME_COMPONENT, &life,
                                       sizeof(life)))
            continue;
        life.age += dt;
        if (life.age >= life.lifetime) {
            ctx->entityDestroy(ctx->host, entities[i]);
            continue;
        }
        ctx->dynamicWriteComponent(ctx->host, entities[i],
                                   HOT_EFFECT_LIFETIME_COMPONENT, &life,
                                   sizeof(life));

        // Integrate motion from the generic Velocity component.
        GameTransformComponentV1 tf{};
        if (ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_TRANSFORM,
                               &tf, sizeof(tf))) {
            GameVelocityComponentV1 vel{};
            if (ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_VELOCITY,
                                   &vel, sizeof(vel))) {
                tf.position[0] += vel.linear[0] * dt;
                tf.position[1] += vel.linear[1] * dt;
                tf.position[2] += vel.linear[2] * dt;
            }
            ctx->writeComponent(ctx->host, entities[i], GAME_COMPONENT_TRANSFORM,
                                &tf, sizeof(tf));
        }

        // Grow + fade the presentation.
        HotPresentationStateV1 present{};
        if (ctx->dynamicReadComponent(ctx->host, entities[i],
                                      HOT_PRESENTATION_COMPONENT, &present,
                                      sizeof(present))) {
            const float t = life.age / life.lifetime;
            present.scale = life.scale0 * (1.0f + life.growth * life.age);
            const float fadeStart =
                life.fadeStart >= 0.0f ? life.fadeStart : 0.0f;
            const float fadeSpan = std::max(0.0001f, 1.0f - fadeStart);
            const float alpha = t <= fadeStart ? 1.0f
                                               : 1.0f - (t - fadeStart) / fadeSpan;
            present.color[3] = std::clamp(alpha, 0.0f, 1.0f);
            ctx->dynamicWriteComponent(ctx->host, entities[i],
                                       HOT_PRESENTATION_COMPONENT, &present,
                                       sizeof(present));
        }
    }
}

const MimitaHotPackage::SchemaRegistrar s_effectLifetimeSchema{
    {HOT_EFFECT_LIFETIME_COMPONENT, gameHash("EffectLifetime.v1"),
     sizeof(HotEffectLifetimeV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "EffectLifetime", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_effectLifecycleSystem{
    {gameHash("hot.effect-lifecycle"), GAME_DOMAIN_RENDER, 3, 0,
     effectLifecycleTick, "hot.effect-lifecycle"}};

} // namespace

#endif
