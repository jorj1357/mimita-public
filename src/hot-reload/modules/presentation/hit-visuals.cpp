// 09 16 2026
/* purpose
* Hot world-hit feedback. Reproduces the original JSON-controlled look (flat
* textured PNG blood spray + blood/bullet-hole/crack decals, impact spheres,
* damage numbers, and a tick-based impact burst) but owns every value here, so
* editing this file changes the running client live. Uses only the existing cold
* primitives exposed through generic capabilities:
*  - effect.part   -> the pooled EffectPart (textured billboards, spheres,
*                     oriented damage streak, tick spheres, damage-number text)
*  - surface.effect -> textured surface decals (blood splat / bullet hole / crack)
* All values are copied from config/hitfx.json + config/impact_decals.json so the
* result matches the pre-migration behavior. JSON remains the fallback when this
* hot handler is absent.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-effect.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-tool-visual.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const char* const kBloadSprayTexture = "assets/textureshq/sblood1.png";
const char* const kBloodSplatTexture = "assets/textureshq/sblood1.png";
const char* const kBulletHoleTexture = "assets/textureshq/holes1.png";
const char* const kCrackTexture = "assets/textureshq/crackground1.png";

using EffectPartFn = void (MIMITA_GAME_CALL *)(void*, const GameEffectPartV1*);
using SurfaceEffectFn = void (MIMITA_GAME_CALL *)(void*, const GameSurfaceEffectV1*);

std::uint32_t hashU(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Deterministic per-hit pseudo-randomness (no shared RNG across generations).
std::uint32_t seedFrom(const float pos[3], std::int32_t damage)
{
    std::uint32_t s = 0x811c9dc5u;
    const std::uint32_t* p = reinterpret_cast<const std::uint32_t*>(pos);
    for (int i = 0; i < 3; ++i)
        s = hashU(s ^ p[i]);
    return hashU(s ^ static_cast<std::uint32_t>(damage));
}

float rnd01(std::uint32_t& s)
{
    s = hashU(s + 0x9e3779b9u);
    return static_cast<float>(s & 0x00ffffffu) / static_cast<float>(0x01000000);
}

void setStr(char* dst, std::size_t n, const char* src)
{
    if (n == 0)
        return;
    std::snprintf(dst, n, "%s", src ? src : "");
}

EffectPartFn resolvePart(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->resolveCapability)
        return nullptr;
    return reinterpret_cast<EffectPartFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_PART));
}

SurfaceEffectFn resolveSurface(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->resolveCapability)
        return nullptr;
    return reinterpret_cast<SurfaceEffectFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SURFACE_EFFECT));
}

void spawnPart(GameplayContextV1* ctx, const GameEffectPartV1& p)
{
    EffectPartFn fn = resolvePart(ctx);
    if (fn)
        fn(ctx->host, &p);
}

void spawnDecal(GameplayContextV1* ctx, const float pos[3], const float nrm[3],
                const float axis[3], const char* texture, float textureScale,
                std::uint32_t kind, const float color[3], float alpha, float radius,
                float height, float lifetime, float fadeTime)
{
    SurfaceEffectFn fn = resolveSurface(ctx);
    if (!fn)
        return;
    GameSurfaceEffectV1 d{};
    for (int k = 0; k < 3; ++k) {
        d.position[k] = pos[k];
        d.normal[k] = nrm[k];
        d.axis[k] = axis ? axis[k] : 0.0f;
    }
    d.color[0] = color[0];
    d.color[1] = color[1];
    d.color[2] = color[2];
    d.color[3] = alpha;
    d.radius = radius;
    d.height = height;
    d.lifetime = lifetime;
    d.fadeTime = fadeTime;
    setStr(d.texture, sizeof(d.texture), texture);
    d.textureScale = textureScale;
    d.decalKind = kind;
    fn(ctx->host, &d);
}

void spawnSphere(GameplayContextV1* ctx, const float pos[3], const float color[3],
                 float startRadius, float endRadius, float alpha, float lifetime)
{
    GameEffectPartV1 p{};
    for (int k = 0; k < 3; ++k) {
        p.position[k] = pos[k];
        p.color[k] = color[k];
    }
    p.scale = startRadius;
    p.endScale = endRadius > 0.0f ? endRadius : startRadius;
    p.alpha = alpha;
    p.maxLifetime = lifetime > 0.0f ? lifetime : (1.0f / 60.0f);
    p.sticky = 1u;
    p.billboardText = 0u;
    setStr(p.replayType, sizeof(p.replayType), "impact_entity");
    spawnPart(ctx, p);
}

// The long, thin red streak the original damage feedback draws through the hit.
void spawnDamageStreak(GameplayContextV1* ctx, const float pos[3],
                       const float dir[3], float length, float radius)
{
    float d[3] = {dir[0], dir[1], dir[2]};
    const float len = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
    if (len < 1e-4f) { d[0] = 0.0f; d[1] = 0.0f; d[2] = 1.0f; }
    else { d[0] /= len; d[1] /= len; d[2] /= len; }
    const float half = std::max(0.001f, length * 0.5f);
    GameEffectPartV1 p{};
    for (int k = 0; k < 3; ++k) {
        p.position[k] = pos[k] - d[k] * half;
        p.endPosition[k] = pos[k] + d[k] * half;
    }
    p.color[0] = 1.0f; p.color[1] = 0.01f; p.color[2] = 0.01f;
    p.scale = radius;
    p.endScale = radius;
    p.alpha = 0.5f;
    p.maxLifetime = 0.5f;
    p.sticky = 1u;
    p.billboardText = 0u;
    setStr(p.replayType, sizeof(p.replayType), "damage_impact_sphere");
    spawnPart(ctx, p);
}

void spawnDamageNumber(GameplayContextV1* ctx, const float pos[3], int damage)
{
    GameEffectPartV1 p{};
    for (int k = 0; k < 3; ++k)
        p.position[k] = pos[k];
    p.color[0] = 1.0f; p.color[1] = 1.0f; p.color[2] = 1.0f;
    p.scale = 1.0f;
    p.endScale = 1.0f;
    p.alpha = 1.0f;
    p.maxLifetime = 0.9f;
    p.billboardText = 1u;
    setStr(p.replayType, sizeof(p.replayType), "damage_number");
    setStr(p.label, sizeof(p.label), std::to_string(damage).c_str());
    spawnPart(ctx, p);
}

// Tangents in the surface plane, used to place decals and spread them.
void surfaceBasis(const float n[3], float outT[3], float outB[3])
{
    const float up[3] = {0.0f, 0.0f, 1.0f};
    float t[3] = {n[1]*up[2] - n[2]*up[1], n[2]*up[0] - n[0]*up[2],
                  n[0]*up[1] - n[1]*up[0]};
    float len = std::sqrt(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
    if (len < 0.1f) {
        const float alt[3] = {0.0f, 1.0f, 0.0f};
        t[0] = n[1]*alt[2] - n[2]*alt[1];
        t[1] = n[2]*alt[0] - n[0]*alt[2];
        t[2] = n[0]*alt[1] - n[1]*alt[0];
        len = std::sqrt(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
    }
    if (len < 1e-4f) { t[0] = 1.0f; t[1] = 0.0f; t[2] = 0.0f; len = 1.0f; }
    for (int k = 0; k < 3; ++k)
        outT[k] = t[k] / len;
    outB[0] = n[1]*outT[2] - n[2]*outT[1];
    outB[1] = n[2]*outT[0] - n[0]*outT[2];
    outB[2] = n[0]*outT[1] - n[1]*outT[0];
}

// Per-hit impact burst: a tick-staggered sphere at the hit point, driven by the
// hot tick system below so a burst's lifetime is defined in ticks.
static constexpr std::uint64_t HOT_HIT_BURST_COMPONENT = gameHash("HitBurstState");
struct HotHitBurstV1 {
    float position[3];
    float direction[3];
    float normal[3];
    std::uint32_t spawnTick;
    std::uint32_t totalTicks;
    std::int32_t damage;
    std::uint32_t hitEntity;
};

void createBurst(GameplayContextV1* ctx, const float pos[3], const float dir[3],
                 const float nrm[3], std::int32_t damage, std::uint32_t hitEntity,
                 std::uint32_t totalTicks)
{
    if (!ctx->entityCreate || !ctx->writeComponent || !ctx->dynamicWriteComponent)
        return;
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    for (int k = 0; k < 3; ++k)
        tf.position[k] = pos[k];
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    HotHitBurstV1 b{};
    for (int k = 0; k < 3; ++k) {
        b.position[k] = pos[k];
        b.direction[k] = dir[k];
        b.normal[k] = nrm[k];
    }
    b.spawnTick = static_cast<std::uint32_t>(ctx->tick);
    b.totalTicks = totalTicks;
    b.damage = damage;
    b.hitEntity = hitEntity;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_HIT_BURST_COMPONENT, &b,
                               sizeof(b));
}

void MIMITA_GAME_CALL hitBurstTick(void* host, std::uint64_t tick, float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->entityDestroy)
        return;
    std::uint64_t entities[128];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_HIT_BURST_COMPONENT, entities, 128);
    for (std::uint32_t i = 0; i < count; ++i) {
        HotHitBurstV1 b{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_HIT_BURST_COMPONENT, &b, sizeof(b)))
            continue;
        const std::uint32_t age = static_cast<std::uint32_t>(tick) - b.spawnTick;
        if (age >= b.totalTicks) {
            ctx->entityDestroy(ctx->host, entities[i]);
            continue;
        }
        // One short-lived sphere per tick, shrinking over the burst. This is the
        // "sphere lifetime in ticks" impact timeline.
        const float t = b.totalTicks > 0
            ? static_cast<float>(age) / static_cast<float>(b.totalTicks) : 0.0f;
        const float radius = (b.hitEntity ? 1.0f : 0.35f) * (1.0f - 0.6f * t);
        const float color[3] = {b.hitEntity ? 1.0f : 0.55f,
                                b.hitEntity ? 0.15f : 0.55f,
                                b.hitEntity ? 0.1f : 0.55f};
        spawnSphere(ctx, b.position, color, radius, radius,
                    b.hitEntity ? 0.9f : 0.5f, 1.0f / 60.0f);
    }
}

// ── Data-driven explosion timeline (client-only, fixed 60 Hz) ──────────
// Each layer is active over a tick range and is spawned once per tick with the
// interpolated radius/color/alpha. Add/remove as many layers as you want; each
// picks a primitive mesh (0 = sphere, or HOT_MESH_CUBE/BEAM/HEXAGON), per-axis
// scale, position offset (+ per-tick rise), rotation, and a tick lifetime.
struct ExplosionLayer {
    std::uint32_t startTick;   // inclusive age in ticks
    std::uint32_t endTick;     // inclusive age in ticks
    std::uint64_t meshId;      // 0 => sphere
    float offset[3];           // base position offset
    float offsetVel[3];        // extra offset per elapsed tick (rise)
    float rotation[3];         // euler radians
    float radiusStart;
    float radiusEnd;
    float scaleXYZ[3];
    float colorStart[3];
    float colorEnd[3];
    float alphaStart;
    float alphaEnd;
};

static const ExplosionLayer kExplosionLayers[] = {
    // ticks 1-2: big bright red sphere
    {1, 2, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 1.0f, 1.0f, {1, 1, 1},
     {1.0f, 0.12f, 0.08f}, {1.0f, 0.12f, 0.08f}, 1.0f, 1.0f},
    // ticks 3-10: smaller, darker red
    {3, 10, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0.8f, 0.8f, {1, 1, 1},
     {0.8f, 0.03f, 0.03f}, {0.45f, 0.0f, 0.0f}, 0.9f, 0.6f},
    // ticks 11-25: smallest, darkest red, fades out
    {11, 25, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0.6f, 0.6f, {1, 1, 1},
     {0.35f, 0.0f, 0.0f}, {0.12f, 0.0f, 0.0f}, 0.6f, 0.0f},
    // smoke: rises and grows while fading (gray sphere)
    {1, 25, 0, {0, 0, 0.2f}, {0, 0, 0.03f}, {0, 0, 0}, 0.6f, 1.3f, {1, 1, 1},
     {0.35f, 0.35f, 0.35f}, {0.12f, 0.12f, 0.12f}, 0.5f, 0.0f},
};
static constexpr std::uint32_t kExplosionTotalTicks = 25;

static constexpr std::uint64_t HOT_EXPLOSION_COMPONENT = gameHash("ExplosionState");
struct HotExplosionV1 {
    float position[3];
    float scale;
    std::uint32_t spawnTick;
    std::uint32_t totalTicks;
    std::uint32_t grenade;
    std::uint32_t reserved;
};

float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Spawn a real primitive mesh via the generic effect primitive (depth-tested in
// the world pass), with per-axis scale and rotation.
void spawnPrimitive(GameplayContextV1* ctx, std::uint64_t meshId,
                    const float pos[3], const float rotation[3],
                    const float scaleXYZ[3], float radius, const float color[3],
                    float alpha, float lifetimeSec)
{
    GameEffectPartV1 p{};
    for (int k = 0; k < 3; ++k) {
        p.position[k] = pos[k];
        p.rotation[k] = rotation[k];
        p.scaleXYZ[k] = scaleXYZ[k] > 0.0f ? scaleXYZ[k] : 1.0f;
        p.color[k] = color[k];
    }
    p.meshResourceId = meshId != 0 ? meshId : HOT_MESH_SPHERE;
    p.textureResourceId = 0;
    p.scale = radius;
    p.endScale = radius;
    p.alpha = alpha;
    // A part spawned by the client tick is aged by EffectPartSystem::update
    // before the render pass; a 1-tick lifetime dies before it draws. Use at
    // least two ticks so every layer is visible, and refresh each tick.
    p.maxLifetime = lifetimeSec > 0.0f ? lifetimeSec : (2.0f / 60.0f);
    p.billboardText = 0u;
    // Explosion replay type => long-range cull instead of the generic 40 m.
    setStr(p.replayType, sizeof(p.replayType), "effect_explosion_sphere");
    spawnPart(ctx, p);
}

void MIMITA_GAME_CALL explosionTimelineTick(void* host, std::uint64_t tick,
                                            float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->entityDestroy)
        return;
    std::uint64_t entities[64];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_EXPLOSION_COMPONENT, entities, 64);
    for (std::uint32_t i = 0; i < count; ++i) {
        HotExplosionV1 e{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_EXPLOSION_COMPONENT, &e, sizeof(e)))
            continue;
        // The cold `effect.request` path stamps tick 0; treat that as "starts
        // now" so those detonations are not destroyed immediately.
        if (e.spawnTick == 0) {
            e.spawnTick = static_cast<std::uint32_t>(tick);
            ctx->dynamicWriteComponent(ctx->host, entities[i],
                                       HOT_EXPLOSION_COMPONENT, &e, sizeof(e));
        }
        const std::uint32_t age = static_cast<std::uint32_t>(tick) - e.spawnTick;
        if (age > e.totalTicks) {
            ctx->entityDestroy(ctx->host, entities[i]);
            continue;
        }
        for (const ExplosionLayer& layer : kExplosionLayers) {
            if (age < layer.startTick || age > layer.endTick)
                continue;
            const float span = layer.endTick > layer.startTick
                ? static_cast<float>(layer.endTick - layer.startTick) : 1.0f;
            const float u = std::clamp(
                static_cast<float>(age - layer.startTick) / span, 0.0f, 1.0f);
            float pos[3];
            for (int k = 0; k < 3; ++k)
                pos[k] = e.position[k] +
                         (layer.offset[k] + layer.offsetVel[k] *
                          static_cast<float>(age)) * e.scale;
            float color[3];
            for (int k = 0; k < 3; ++k)
                color[k] = lerpf(layer.colorStart[k], layer.colorEnd[k], u);
            spawnPrimitive(ctx, layer.meshId, pos, layer.rotation, layer.scaleXYZ,
                           lerpf(layer.radiusStart, layer.radiusEnd, u) * e.scale,
                           color, lerpf(layer.alphaStart, layer.alphaEnd, u),
                           2.0f / 60.0f);
        }
    }
}

// Only a process with a local view composes effects (a dedicated server does
// not simulate them).
bool explosionHasLocalView(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return false;
    const auto* shared =
        reinterpret_cast<const GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC && shared->localPlayerEntity != 0;
}

} // namespace

void hotComposeHit(GameplayContextV1* ctx, const EffectRequestV1& req)
{
    if (!ctx)
        return;
    const bool blood = req.effectTypeId == gameHash("effect.hit.blood");
    const float pos[3] = {req.position[0], req.position[1], req.position[2]};
    float nrm[3] = {req.normal[0], req.normal[1], req.normal[2]};
    const float nlen = std::sqrt(nrm[0]*nrm[0] + nrm[1]*nrm[1] + nrm[2]*nrm[2]);
    if (nlen < 1e-4f) { nrm[0] = 0.0f; nrm[1] = 0.0f; nrm[2] = 1.0f; }
    else { nrm[0] /= nlen; nrm[1] /= nlen; nrm[2] /= nlen; }

    std::uint32_t seed = seedFrom(pos, req.damage);
    float tangent[3], bitangent[3];
    surfaceBasis(nrm, tangent, bitangent);

    // 1. Contact/impact sphere (red for an entity hit, gray for the world).
    if (blood || req.hitEntity) {
        const float color[3] = {0.9f, 0.02f, 0.02f};
        spawnSphere(ctx, pos, color, 2.0f, 2.0f, 1.0f, 0.01f);
    }
    if (!blood && !req.hitEntity) {
        const float color[3] = {0.55f, 0.55f, 0.55f};
        spawnSphere(ctx, pos, color, 0.1f, 3.0f, 0.3f, 0.1f);
    }

    if (blood) {
        // 2. Textured blood spray (camera-facing PNG billboards).
        const int sprayCount = 4 + static_cast<int>(rnd01(seed) * 12.0f);
        for (int i = 0; i < sprayCount; ++i) {
            const float a = rnd01(seed) * 6.2831853f;
            const float speed = 3.0f + rnd01(seed) * 15.0f;
            const float elev = 0.2f + rnd01(seed) * 1.2f;
            GameEffectPartV1 p{};
            for (int k = 0; k < 3; ++k)
                p.position[k] = pos[k];
            p.velocity[0] = std::cos(a) * speed;
            p.velocity[1] = std::sin(a) * speed;
            p.velocity[2] = elev * speed;
            p.color[0] = 0.7f; p.color[1] = 0.01f; p.color[2] = 0.01f;
            p.scale = 0.03f + rnd01(seed) * 0.1f;
            p.endScale = p.scale;
            p.alpha = 0.7f + rnd01(seed) * 0.25f;
            p.maxLifetime = 0.5f + rnd01(seed) * 1.1f;
            p.affectedByGravity = 1u;
            p.gravity = 9.8f;
            p.billboardText = 0u;
            setStr(p.replayType, sizeof(p.replayType), "hitfx_particle");
            setStr(p.texturePath, sizeof(p.texturePath), kBloadSprayTexture);
            spawnPart(ctx, p);
        }
        // 3. Textured blood splat decals around the hit point.
        const int splatCount = 3 + static_cast<int>(rnd01(seed) * 5.0f);
        for (int i = 0; i < splatCount; ++i) {
            const float a = rnd01(seed) * 6.2831853f;
            const float spread = rnd01(seed) * 0.35f;
            float dp[3];
            for (int k = 0; k < 3; ++k)
                dp[k] = pos[k] + (tangent[k] * std::cos(a) +
                                  bitangent[k] * std::sin(a)) * spread;
            const float color[3] = {0.7f, 0.01f, 0.01f};
            spawnDecal(ctx, dp, nrm, tangent, kBloodSplatTexture, 1.0f,
                       /*Blood*/ 1u, color, 0.9f,
                       0.12f + rnd01(seed) * 0.23f, 0.05f, 30.0f, 5.0f);
        }
        // 4. Red damage streak + damage number.
        if (req.hitEntity) {
            float dir[3] = {0.0f, 0.0f, 1.0f};
            if (std::fabs(req.normal[0]) + std::fabs(req.normal[1]) +
                    std::fabs(req.normal[2]) > 1e-4f) {
                dir[0] = -nrm[0]; dir[1] = -nrm[1]; dir[2] = -nrm[2];
            }
            spawnDamageStreak(ctx, pos, dir, 0.2f, 0.5f);
            if (req.spawnDamageNumber && req.damage != 0)
                spawnDamageNumber(ctx, pos, req.damage);
        }
    } else {
        // 5. Bullet hole.
        const float holeColor[3] = {0.03f, 0.03f, 0.03f};
        spawnDecal(ctx, pos, nrm, tangent, kBulletHoleTexture, 1.0f,
                   /*BulletHole*/ 2u, holeColor, 1.0f, 0.2f, 0.06f, 30.0f, 5.0f);
        // 6. Cracks: a few jagged strips along the surface.
        int arms = 3;
        for (int arm = 0; arm < arms; ++arm) {
            const float a = rnd01(seed) * 6.2831853f;
            float axis[3];
            for (int k = 0; k < 3; ++k)
                axis[k] = tangent[k] * std::cos(a) + bitangent[k] * std::sin(a);
            const int segments = 1 + static_cast<int>(rnd01(seed) * 4.0f);
            float cursor[3];
            for (int k = 0; k < 3; ++k)
                cursor[k] = pos[k] + nrm[k] * 0.005f;
            for (int s = 0; s < segments; ++s) {
                const float len = 0.2f + rnd01(seed) * 0.3f;
                float center[3];
                for (int k = 0; k < 3; ++k)
                    center[k] = cursor[k] + axis[k] * (len * 0.5f);
                const float crackColor[3] = {0.1f, 0.1f, 0.1f};
                spawnDecal(ctx, center, nrm, axis, kCrackTexture, 1.0f,
                           /*Crack*/ 3u, crackColor, 0.85f,
                           0.165f, len, 8.0f, 2.0f);
                for (int k = 0; k < 3; ++k)
                    cursor[k] += axis[k] * len;
                const float turn = (rnd01(seed) * 2.0f - 1.0f) * 1.0f;
                float perp[3] = {nrm[1]*axis[2] - nrm[2]*axis[1],
                                 nrm[2]*axis[0] - nrm[0]*axis[2],
                                 nrm[0]*axis[1] - nrm[1]*axis[0]};
                const float plen = std::sqrt(perp[0]*perp[0] + perp[1]*perp[1] +
                                             perp[2]*perp[2]);
                if (plen > 1e-4f)
                    for (int k = 0; k < 3; ++k)
                        perp[k] /= plen;
                for (int k = 0; k < 3; ++k)
                    axis[k] = axis[k] * std::cos(turn) + perp[k] * std::sin(turn);
                const float alen = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] +
                                             axis[2]*axis[2]);
                if (alen > 1e-4f)
                    for (int k = 0; k < 3; ++k)
                        axis[k] /= alen;
            }
        }
    }

    // 7. Tick-based impact burst (lifetime defined in ticks, like hitfx.json
    //    core.lifetimeTicks = 60).
    float burstDir[3] = {-nrm[0], -nrm[1], -nrm[2]};
    createBurst(ctx, pos, burstDir, nrm, req.damage, req.hitEntity ? 1u : 0u, 60u);
}

const MimitaHotPackage::SchemaRegistrar s_hitBurstSchema{
    {HOT_HIT_BURST_COMPONENT, gameHash("HitBurstState.v1"),
     sizeof(HotHitBurstV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "HitBurstState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_hitBurstSystem{
    {gameHash("hot.hit-burst"), GAME_DOMAIN_RENDER, 4, 0, hitBurstTick,
     "hot.hit-burst"}};

// Tick-based explosion timeline: create the state entity; the client-only
// 60 Hz system ages it and spawns the primitive layers.
void hotSpawnExplosionTimeline(GameplayContextV1* ctx, const float pos[3],
                               float scale, bool grenade)
{
    if (!ctx || !explosionHasLocalView(ctx))
        return;
    if (!ctx->entityCreate || !ctx->writeComponent || !ctx->dynamicWriteComponent)
        return;
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    for (int k = 0; k < 3; ++k)
        tf.position[k] = pos[k];
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    HotExplosionV1 e{};
    for (int k = 0; k < 3; ++k)
        e.position[k] = pos[k];
    e.scale = scale > 0.0f ? scale : 1.0f;
    e.spawnTick = static_cast<std::uint32_t>(ctx->tick);
    e.totalTicks = kExplosionTotalTicks;
    e.grenade = grenade ? 1u : 0u;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_EXPLOSION_COMPONENT, &e,
                               sizeof(e));
}

const MimitaHotPackage::SchemaRegistrar s_explosionSchema{
    {HOT_EXPLOSION_COMPONENT, gameHash("ExplosionState.v1"),
     sizeof(HotExplosionV1), 4, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ExplosionState", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_explosionSystem{
    {gameHash("hot.explosion"), GAME_DOMAIN_CLIENT_TICK, 10, 0,
     explosionTimelineTick, "hot.explosion"}};

#endif
