// 09 16 2026
/* purpose
* Hot effect aging. A client-only fixed 60 Hz system owns the lifetime of the
* EXISTING pooled surface decals (bullet holes, cracks, blood splats) and blood
* spray particles: it ages them, fades alpha over the tick, and kills them on
* expiry. All values live here, so effect aging is editable live with no EXE
* rebuild. It claims aging so the cold pool stops aging (one owner).
* Part/effect-particle aging stays on the existing hot gameUpdateEffects hook.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Fixed 60 Hz tick counter shared with the hot part-aging hook in
// src/effects/effect-part.cpp (gameUpdateEffects). The hook steps parts by the
// number of ticks elapsed instead of the render-frame dt, so part aging is
// tick-accurate and editable live with no cold build.
std::uint32_t g_hotEffectTick = 0;

namespace {

using EffectPoolFn = bool (MIMITA_GAME_CALL *)(void*, GameEffectPoolV1*);

constexpr float kTickDt = 1.0f / 60.0f;

EffectPoolFn resolvePool(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->resolveCapability)
        return nullptr;
    return reinterpret_cast<EffectPoolFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_POOL));
}

std::uint32_t poolCount(EffectPoolFn fn, GameplayContextV1* ctx,
                        std::uint32_t kind)
{
    GameEffectPoolV1 q{};
    q.op = GAME_EFFECT_POOL_COUNT;
    q.kind = kind;
    if (!fn(ctx->host, &q))
        return 0;
    return q.count;
}

void killEntry(EffectPoolFn fn, GameplayContextV1* ctx, std::uint32_t kind,
               std::uint32_t index)
{
    GameEffectPoolV1 k{};
    k.op = GAME_EFFECT_POOL_KILL;
    k.kind = kind;
    k.index = index;
    fn(ctx->host, &k);
}

// Surface decals: age, fade alpha over the fade window, darken is left to the
// renderer; killed at lifetime. Add any custom fade curve here.
void ageDecals(EffectPoolFn fn, GameplayContextV1* ctx)
{
    std::uint32_t count = poolCount(fn, ctx, GAME_EFFECT_POOL_DECALS);
    std::uint32_t i = 0;
    while (i < count) {
        GameEffectPoolV1 g{};
        g.op = GAME_EFFECT_POOL_GET;
        g.kind = GAME_EFFECT_POOL_DECALS;
        g.index = i;
        if (!fn(ctx->host, &g)) {
            ++i;
            continue;
        }
        const float age = g.age + kTickDt;
        if (age >= g.lifetime) {
            killEntry(fn, ctx, GAME_EFFECT_POOL_DECALS, i);
            --count;   // swap-remove moved the last entry into slot i
            continue;
        }
        g.age = age;
        const float hold = std::max(0.0f, g.lifetime - g.fadeTime);
        if (g.fadeTime > 0.0f && age > hold) {
            const float t = std::clamp((age - hold) / g.fadeTime, 0.0f, 1.0f);
            g.alpha = std::max(0.0f, 1.0f - t);
        }
        g.op = GAME_EFFECT_POOL_SET;
        fn(ctx->host, &g);
        ++i;
    }
}

// Blood spray: gravity + air drag + fade tail, killed at lifetime.
void ageBlood(EffectPoolFn fn, GameplayContextV1* ctx)
{
    const float gravity = -2.5f;
    const float airDrag = 0.97f;
    std::uint32_t count = poolCount(fn, ctx, GAME_EFFECT_POOL_BLOOD);
    std::uint32_t i = 0;
    while (i < count) {
        GameEffectPoolV1 g{};
        g.op = GAME_EFFECT_POOL_GET;
        g.kind = GAME_EFFECT_POOL_BLOOD;
        g.index = i;
        if (!fn(ctx->host, &g)) {
            ++i;
            continue;
        }
        if (g.age >= g.lifetime) {
            killEntry(fn, ctx, GAME_EFFECT_POOL_BLOOD, i);
            --count;
            continue;
        }
        for (int k = 0; k < 3; ++k)
            g.position[k] += g.velocity[k] * kTickDt;
        g.velocity[2] += gravity * kTickDt;
        const float drag = std::pow(airDrag, kTickDt * 60.0f);
        for (int k = 0; k < 3; ++k)
            g.velocity[k] *= drag;
        g.age += kTickDt;
        const float fadeStart = g.lifetime * 0.4f;
        if (g.age > fadeStart && g.lifetime > fadeStart) {
            g.alpha = std::clamp(
                1.0f - (g.age - fadeStart) / (g.lifetime - fadeStart), 0.0f, 1.0f);
        }
        g.op = GAME_EFFECT_POOL_SET;
        fn(ctx->host, &g);
        ++i;
    }
}

void MIMITA_GAME_CALL effectAgeTick(void* host, std::uint64_t /*tick*/,
                                    float /*dt*/)
{
    // Advance the shared fixed-tick counter first (the part-aging hook reads it).
    ++g_hotEffectTick;
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    EffectPoolFn fn = resolvePool(ctx);
    if (!fn)
        return;
    // Claim aging so the cold pool stops aging these (one owner). Idempotent.
    {
        GameEffectPoolV1 c{};
        c.op = GAME_EFFECT_POOL_CLAIM;
        c.count = 1;
        fn(ctx->host, &c);
    }
    ageDecals(fn, ctx);
    ageBlood(fn, ctx);
}

const MimitaHotPackage::SystemRegistrar s_effectAgeSystem{
    {gameHash("hot.effect-age"), GAME_DOMAIN_CLIENT_TICK, 5, 0, effectAgeTick,
     "hot.effect-age"}};

} // namespace

#endif
