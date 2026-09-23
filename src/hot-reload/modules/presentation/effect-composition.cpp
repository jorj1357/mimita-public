// 09 15 2026
/* purpose
* Hot effect composition. Consumes the generic `effect.request` fact and
* composes generic effect entities (flash sphere, rising smoke, debris) using
* Transform/Velocity/PresentationState/EffectLifetime. The cold fallback
* composition yields when a handler sets handled = 1. The kernel only knows
* generic primitives, so a future grenade/plasma/magic effect reuses this.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-audio-policy.h"
#include "hot-reload/hot-effect.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-tool-visual.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void spawnEffect(GameplayContextV1* ctx, std::uint64_t meshId, std::uint64_t texId,
                 const float pos[3], const float vel[3], float colorR,
                 float colorG, float colorB, float scale0, float growth,
                 float lifetime, float fadeStart)
{
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    tf.position[0] = pos[0];
    tf.position[1] = pos[1];
    tf.position[2] = pos[2];
    tf.look[0] = 1.0f;
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    GameVelocityComponentV1 v{};
    v.linear[0] = vel[0];
    v.linear[1] = vel[1];
    v.linear[2] = vel[2];
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_VELOCITY, &v,
                        sizeof(v));
    HotPresentationStateV1 present{};
    present.meshResourceId = meshId;
    present.textureResourceId = texId;
    present.scale = scale0;
    present.color[0] = colorR;
    present.color[1] = colorG;
    present.color[2] = colorB;
    present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));
    HotEffectLifetimeV1 life{};
    life.age = 0.0f;
    life.lifetime = lifetime;
    // The lifecycle overwrites PresentationState.scale as
    // `scale0 * (1 + growth * age)`, so the recipe scale must be scale0 (a value
    // of 1.0 here silently discarded every recipe scale after the first tick).
    life.scale0 = scale0;
    life.growth = growth;
    life.fadeStart = fadeStart;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_EFFECT_LIFETIME_COMPONENT,
                               &life, sizeof(life));
}

// A static, oriented effect (no motion integration): +Z is rotated to `dir`, and
// per-axis scaleXYZ turns the generic cylinder into a beam/tracer of any length
// and thickness.
void spawnEffectOriented(GameplayContextV1* ctx, std::uint64_t meshId,
                         std::uint64_t texId, const float pos[3],
                         const float dir[3], float colorR, float colorG,
                         float colorB, float scale, float sx, float sy, float sz,
                         float lifetime, float fadeStart)
{
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    for (int k = 0; k < 3; ++k) {
        tf.position[k] = pos[k];
        tf.look[k] = dir[k];
    }
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    HotPresentationStateV1 present{};
    present.meshResourceId = meshId;
    present.textureResourceId = texId;
    present.scale = scale;
    present.color[0] = colorR;
    present.color[1] = colorG;
    present.color[2] = colorB;
    present.color[3] = 1.0f;
    present.scaleXYZ[0] = sx;
    present.scaleXYZ[1] = sy;
    present.scaleXYZ[2] = sz;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));
    HotEffectLifetimeV1 life{};
    life.age = 0.0f;
    life.lifetime = lifetime;
    life.scale0 = 1.0f;
    life.growth = 0.0f;
    life.fadeStart = fadeStart;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_EFFECT_LIFETIME_COMPONENT,
                               &life, sizeof(life));
}

using AudioPlayFn = void (MIMITA_GAME_CALL *)(void*, const GameAudioCommandV1*);
using EffectSpawnFn = void (MIMITA_GAME_CALL *)(void*, const GameEffectSpawnV1*);
using RenderDebugFn = void (MIMITA_GAME_CALL *)(void*, const GameRenderDebugCommandV1*);

void emitDynamicLight(GameplayContextV1* ctx, const float pos[3],
                      const float color[3], const float offset[3], float intensity,
                      float radius, float lifetime)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    auto fx = reinterpret_cast<EffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (!fx)
        return;
    GameEffectSpawnV1 d{};
    d.kind = HOT_EFFECT_LIGHT;
    for (int k = 0; k < 3; ++k)
        d.position[k] = pos[k] + (offset ? offset[k] : 0.0f);
    d.color[0] = color ? color[0] : 1.0f;
    d.color[1] = color ? color[1] : 1.0f;
    d.color[2] = color ? color[2] : 1.0f;
    d.color[3] = 1.0f;
    d.scale = intensity > 0.0f ? intensity : 1.0f;   // scale = intensity
    d.endScale = radius > 0.0f ? radius : 5.0f;      // endScale = radius
    d.lifetime = lifetime > 0.0f ? lifetime : 0.1f;
    fx(ctx->host, &d);
}

void emitWorldSound(GameplayContextV1* ctx, const char* sound, const float pos[3],
                    float volume, float pitch, float maxDistance)
{
    if (!ctx || !ctx->resolveCapability || !sound || !*sound)
        return;
    auto audio = reinterpret_cast<AudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!audio)
        return;
    GameAudioCommandV1 c{};
    std::snprintf(c.sound, sizeof(c.sound), "%s", sound);
    for (int k = 0; k < 3; ++k)
        c.position[k] = pos[k];
    c.volume = volume;
    c.pitch = pitch;
    c.maxDistance = maxDistance;
    c.spatial = 1;
    audio(ctx->host, &c);
}

void emitWorldLabel(GameplayContextV1* ctx, const float pos[3], const float color[4],
                    float scale, const char* text)
{
    if (!ctx || !ctx->resolveCapability || !text || !*text)
        return;
    auto render = reinterpret_cast<RenderDebugFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_DEBUG));
    if (!render)
        return;
    GameRenderDebugCommandV1 cmd{};
    cmd.shape = GAME_RENDER_DEBUG_WORLD_LABEL;
    for (int k = 0; k < 3; ++k)
        cmd.a[k] = pos[k];
    cmd.radius = scale;
    for (int k = 0; k < 4; ++k)
        cmd.color[k] = color[k];
    std::snprintf(cmd.text, sizeof(cmd.text), "%s", text);
    render(ctx->host, &cmd);
}

using AudioPlayFn = void (MIMITA_GAME_CALL *)(void*, const GameAudioCommandV1*);
using SurfaceEffectFn = void (MIMITA_GAME_CALL *)(void*,
                                                  const GameSurfaceEffectV1*);
using CameraEffectFn = void (MIMITA_GAME_CALL *)(void*,
                                                 const GameCameraEffectV1*);

// Hot camera-effect policy: choose amplitude/falloff. Cold applies the punch.
void emitCameraEffect(GameplayContextV1* ctx, float pitch, float yaw,
                      float distance, float falloffDistance)
{
    if (!ctx->resolveCapability)
        return;
    auto fx = reinterpret_cast<CameraEffectFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_CAMERA_EFFECT));
    if (!fx)
        return;
    GameCameraEffectV1 c{};
    c.pitch = pitch;
    c.yaw = yaw;
    c.distance = distance;
    c.falloffDistance = falloffDistance;
    c.runtimeKey = gameHash("effect.camera.shake");
    fx(ctx->host, &c);
}

// Hot surface/decal policy: choose color, size, orientation, lifetime. The cold
// backend puts a generic mark on the surface and never learns what it means.
void emitSurfaceEffect(GameplayContextV1* ctx, const float pos[3],
                       const float normal[3], float r, float g, float b,
                       float radius, float lifetime)
{
    if (!ctx->resolveCapability)
        return;
    auto fx = reinterpret_cast<SurfaceEffectFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SURFACE_EFFECT));
    if (!fx)
        return;
    GameSurfaceEffectV1 s{};
    s.position[0] = pos[0];
    s.position[1] = pos[1];
    s.position[2] = pos[2];
    s.normal[0] = normal[0];
    s.normal[1] = normal[1];
    s.normal[2] = normal[2];
    s.color[0] = r; s.color[1] = g; s.color[2] = b; s.color[3] = 1.0f;
    s.radius = radius;
    s.height = radius;
    s.lifetime = lifetime;
    s.fadeTime = lifetime * 0.25f;
    s.flags = 1;  // persistent (fades over lifetime)
    fx(ctx->host, &s);
}

// Hot audio policy: choose the sound, volume, pitch, and spatial falloff. The
// cold backend only plays the resulting command. Editing this changes the sound
// without an EXE rebuild.
void emitExplosionSoundAt(GameplayContextV1* ctx, bool grenade,
                          const float pos[3])
{
    if (!ctx->resolveCapability)
        return;
    auto audio = reinterpret_cast<AudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!audio)
        return;
    GameAudioCommandV1 cmd{};
    std::snprintf(cmd.sound, sizeof(cmd.sound), "%s",
                  grenade ? "grenadelauncher/grenadelauncherexplode"
                          : "rocketlauncher/rocketlauncherexplode");
    cmd.position[0] = pos[0];
    cmd.position[1] = pos[1];
    cmd.position[2] = pos[2];
    cmd.volume = 1.0f;
    cmd.pitch = 1.0f;
    cmd.maxDistance = 50.0f;
    cmd.spatial = 1;
    audio(ctx->host, &cmd);
}

// One explosion recipe: flash / smoke / debris + sound. Shared by the generic
// effect.request path (typed cold callers) and the hot projectile simulation, so
// a detonation looks identical wherever it is composed.
void composeExplosion(GameplayContextV1* ctx, std::uint64_t effectTypeId,
                      const float pos[3], float scale)
{
    const bool grenade = effectTypeId == gameHash("effect.explosion.grenade");
    emitExplosionSoundAt(ctx, grenade, pos);
    // The visual is a data-driven, client-only tick timeline (red sphere stages +
    // smoke), defined in hit-visuals.cpp. No cubes, no JSON.
    hotSpawnExplosionTimeline(ctx, pos, scale, grenade);
}

// ── Server-disagreement presentation (hot recipe; JSON is fallback only) ──
// The cold kernel only forwards the plain event data; all appearance (pulse,
// beam, tracer, text, particles, sound, severity) is owned here and can be edited
// live. Reuses the generic effect/label/audio primitives; no disagreement-
// specific renderer branch exists.
void composeServerDisagreement(GameplayContextV1* ctx, const EffectRequestV1& req)
{
    const ServerDisagreementVisualV1 recipe = makeServerDisagreementVisual();
    const float pos[3] = {req.position[0], req.position[1], req.position[2]};
    const float mag = std::sqrt(req.correction[0] * req.correction[0] +
                                req.correction[1] * req.correction[1] +
                                req.correction[2] * req.correction[2]);
    float t = 0.0f;
    if (recipe.largeCorrectionMag > recipe.smallCorrectionMag &&
        mag > recipe.smallCorrectionMag)
        t = (mag - recipe.smallCorrectionMag) /
            (recipe.largeCorrectionMag - recipe.smallCorrectionMag);
    t = std::clamp(t, 0.0f, 1.0f);
    const float radius = recipe.smallScale + (recipe.largeScale - recipe.smallScale) * t;
    const float* color = serverDisagreementReasonColor(recipe, req.reason);
    const float zero[3] = {0.0f, 0.0f, 0.0f};

    emitWorldSound(ctx, recipe.sound, pos, recipe.soundVolumeMax, 1.0f,
                   recipe.soundRange);

    // Expanding pulse.
    {
        const float start = recipe.pulseStartScale > 0.0f ? recipe.pulseStartScale : 0.1f;
        const float end = radius * (recipe.pulseEndScale > 0.0f ? recipe.pulseEndScale : 1.0f);
        const float life = recipe.pulseLifetime > 0.0f ? recipe.pulseLifetime : 0.6f;
        const float growth = (end / start - 1.0f) / life;
        spawnEffect(ctx, HOT_MESH_SPHERE, 0, pos, zero, color[0], color[1],
                    color[2], start, growth, life, 0.4f);
    }

    // Vertical beam.
    if (recipe.beamHeight > 0.0f) {
        const float dir[3] = {0.0f, 0.0f, 1.0f};
        const float at[3] = {pos[0], pos[1], pos[2] + recipe.beamHeight * 0.5f};
        const float thick = recipe.beamThickness > 0.0f ? recipe.beamThickness : 0.06f;
        spawnEffectOriented(ctx, HOT_MESH_BEAM, 0, at, dir, color[0], color[1],
                            color[2], 1.0f, thick / 0.18f, thick / 0.18f,
                            recipe.beamHeight / 1.5f,
                            recipe.beamLifetime > 0.0f ? recipe.beamLifetime : 0.36f,
                            0.5f);
    }

    // Correction tracer.
    if (mag > 0.01f) {
        const float dir[3] = {req.correction[0] / mag, req.correction[1] / mag,
                              req.correction[2] / mag};
        const float at[3] = {pos[0] + dir[0] * mag * 0.5f,
                             pos[1] + dir[1] * mag * 0.5f,
                             pos[2] + dir[2] * mag * 0.5f};
        const float thick = recipe.tracerThickness > 0.0f ? recipe.tracerThickness : 0.06f;
        spawnEffectOriented(ctx, HOT_MESH_BEAM, 0, at, dir, recipe.tracerColor[0],
                            recipe.tracerColor[1], recipe.tracerColor[2], 1.0f,
                            thick / 0.18f, thick / 0.18f, mag / 1.5f,
                            recipe.tracerLifetime > 0.0f ? recipe.tracerLifetime : 0.4f,
                            0.5f);
    }

    // Text label.
    {
        char label[64] = {0};
        if (req.flags & 1u)
            std::snprintf(label, sizeof(label), "%s", req.text);
        else if (req.text[0] != '\0')
            std::snprintf(label, sizeof(label), "%s%s", recipe.textPrefix, req.text);
        else
            std::snprintf(label, sizeof(label), "%s", recipe.textPrefix);
        const float at[3] = {pos[0], pos[1], pos[2] + recipe.textZOffset};
        const float tc[4] = {color[0], color[1], color[2], 1.0f};
        emitWorldLabel(ctx, at, tc, recipe.textScale, label);
    }

    // Particle burst.
    for (std::uint32_t i = 0; i < recipe.particleCount; ++i) {
        const float a = (float)i * (6.2831853f /
                                    (float)(recipe.particleCount ? recipe.particleCount : 1u));
        const float elev = 0.5f + 0.5f * std::sin((float)i * 2.3f);
        const float speed = recipe.particleMinSpeed +
            (recipe.particleMaxSpeed - recipe.particleMinSpeed) * 0.5f;
        const float vel[3] = {std::cos(a) * speed, std::sin(a) * speed,
                              elev * speed};
        spawnEffect(ctx, HOT_MESH_SPHERE, 0, pos, vel, recipe.particleColor[0],
                    recipe.particleColor[1], recipe.particleColor[2],
                    recipe.particleScale > 0.0f ? recipe.particleScale : 0.1f,
                    0.0f, recipe.particleLifetime > 0.0f ? recipe.particleLifetime : 0.8f,
                    0.4f);
    }
}

// Local-only correction indicator (arrow + label; not replicated).
void composeLocalDisagreement(GameplayContextV1* ctx, const EffectRequestV1& req)
{
    const LocalDisagreementVisualV1 li = makeLocalDisagreementIndicatorVisual();
    const float pos[3] = {req.position[0], req.position[1], req.position[2]};
    const float mag = std::sqrt(req.correction[0] * req.correction[0] +
                                req.correction[1] * req.correction[1] +
                                req.correction[2] * req.correction[2]);
    if (mag > 0.01f) {
        const float dir[3] = {req.correction[0] / mag, req.correction[1] / mag,
                              req.correction[2] / mag};
        const float at[3] = {pos[0] + dir[0] * mag * 0.5f,
                             pos[1] + dir[1] * mag * 0.5f,
                             pos[2] + dir[2] * mag * 0.5f};
        const float thick = li.arrowThickness > 0.0f ? li.arrowThickness : 0.06f;
        spawnEffectOriented(ctx, HOT_MESH_BEAM, 0, at, dir, li.arrowColor[0],
                            li.arrowColor[1], li.arrowColor[2], 1.0f,
                            thick / 0.18f, thick / 0.18f, mag / 1.5f,
                            li.arrowLifetime > 0.0f ? li.arrowLifetime : 1.2f, 0.5f);
    }
    const float at[3] = {pos[0], pos[1], pos[2] + li.textZOffset};
    emitWorldLabel(ctx, at, li.textColor, li.textScale, li.textLabel);
}

void MIMITA_GAME_CALL onEffectRequest(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* req = event ? static_cast<EffectRequestV1*>(event->payload) : nullptr;
    if (!ctx || !req || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent)
        return;
    // NOTE: handled is set only by a branch that actually composes the effect.
    // An unrecognized fact stays unhandled so the cold owner runs (one owner).

    const float pos[3] = {req->position[0], req->position[1], req->position[2]};
    const float scale = req->scale > 0.0f ? req->scale : 1.0f;

    // Hit feedback (blood/world): the hot hit recipe reproduces the full cold
    // composition (textured blood spray/decals, bullet holes, cracks, impact
    // spheres, damage numbers, tick burst) using the existing EffectPart/decal
    // primitives. JSON stays the fallback when this handler is absent.
    if (req->effectTypeId == gameHash("effect.hit.blood") ||
        req->effectTypeId == gameHash("effect.hit.world")) {
        req->handled = 1;
        hotComposeHit(ctx, *req);
        return;
    }

    // Movement presentation is hot policy. The EXE only reports the movement
    // fact; this code chooses the actual shape, geometry, timing, colour, and
    // sound. Editing these recipes must affect the next hot DLL generation
    // without relinking the executable.
    if (req->effectTypeId == gameHash("effect.movement.ground_jump") ||
        req->effectTypeId == gameHash("effect.movement.air_jump") ||
        req->effectTypeId == gameHash("effect.movement.dash") ||
        req->effectTypeId == gameHash("effect.movement.down_dash") ||
        req->effectTypeId == gameHash("effect.movement.landing") ||
        req->effectTypeId == gameHash("effect.movement.freeze") ||
        req->effectTypeId == gameHash("effect.movement.freeze_trail") ||
        req->effectTypeId == gameHash("effect.movement.footstep")) {
        req->handled = 1;

        const float pos[3] = {req->position[0], req->position[1], req->position[2]};
        float dir[3] = {req->normal[0], req->normal[1], req->normal[2]};
        const float dirLen = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] +
                                       dir[2] * dir[2]);
        if (dirLen < 0.001f) {
            dir[0] = 0.0f;
            dir[1] = 0.0f;
            dir[2] = 1.0f;
        } else {
            dir[0] /= dirLen;
            dir[1] /= dirLen;
            dir[2] /= dirLen;
        }

        const std::uint64_t type = req->effectTypeId;
        if (type == gameHash("effect.movement.ground_jump")) {
            const float p[3] = {pos[0], pos[1], pos[2] - 0.5f};
            const float v[3] = {0.0f, 0.0f, 1.5f};
            spawnEffect(ctx, HOT_MESH_SPHERE, HOT_TEX_DEFAULT, p, v,
                        1.0f, 0.85f, 0.2f, 0.25f, 2.5f,
                        10.0f / 60.0f, 0.15f);
            // Sound is emitted once by hot movement on the accepted jump edge.
            return;
        }
        if (type == gameHash("effect.movement.air_jump")) {
            const float p[3] = {pos[0], pos[1], pos[2] - 1.0f};
            const float v[3] = {0.0f, 0.0f, -0.5f};
            spawnEffect(ctx, HOT_MESH_SPHERE, HOT_TEX_DEFAULT, p, v,
                        0.5f, 0.3f, 1.0f, 0.4f, 3.0f,
                        14.0f / 60.0f, 0.2f);
            // Sound is emitted once by hot movement on the accepted fresh press.
            return;
        }
        if (type == gameHash("effect.movement.dash")) {
            const float speed = std::max(req->scale, 1.0f);
            spawnEffectOriented(ctx, HOT_MESH_BEAM, HOT_TEX_DEFAULT, pos, dir,
                                0.2f, 0.6f, 1.0f, 1.0f,
                                0.18f, 0.18f,
                                std::clamp(0.5f + speed * 0.08f, 0.5f, 3.5f),
                                12.0f / 60.0f, 0.35f);
            HotAudioOverrideV1 dashOv{};
            if (req->flags & 1u) {
                dashOv.volumeScale = 1.3f;
                dashOv.pitchScale = 1.2f;
            }
            hotEmitRecipeSound(ctx, gameHash("dash"), pos, 0, true, &dashOv);
            if (req->flags & 1u) {
                HotAudioOverrideV1 lowPitch{};
                lowPitch.pitchBase = 0.25f;
                hotEmitRecipeSound(ctx, gameHash("dash"), pos, 0, true,
                                   &lowPitch);
            }
            return;
        }
        if (type == gameHash("effect.movement.down_dash")) {
            const float down[3] = {0.0f, 0.0f, -1.0f};
            spawnEffectOriented(ctx, HOT_MESH_BEAM, HOT_TEX_DEFAULT, pos, down,
                                0.1f, 0.8f, 0.8f, 1.0f,
                                0.45f, 0.45f, 3.0f,
                                12.0f / 60.0f, 0.3f);
            hotEmitRecipeSound(ctx, gameHash("down_dash"), pos, 0, true,
                               nullptr);
            return;
        }
        if (type == gameHash("effect.movement.landing")) {
            const float p[3] = {pos[0], pos[1], pos[2]};
            spawnEffectOriented(ctx, HOT_MESH_BEAM, HOT_TEX_DEFAULT, p, dir,
                                0.6f, 0.6f, 0.6f, 1.0f,
                                0.6f, 0.12f, 0.12f,
                                12.0f / 60.0f, 0.2f);
            hotEmitRecipeSound(ctx, gameHash("landing"), pos, 0, true, nullptr);
            return;
        }
        if (type == gameHash("effect.movement.freeze")) {
            spawnEffect(ctx, HOT_MESH_SPHERE, HOT_TEX_DEFAULT, pos,
                        nullptr, 0.2f, 1.0f, 0.3f, 0.2f, 0.0f,
                        std::max(req->scale, 0.1f), 0.0f);
            hotEmitRecipeSound(ctx, gameHash("freeze"), pos, 0, true, nullptr);
            return;
        }
        if (type == gameHash("effect.movement.freeze_trail")) {
            const float up[3] = {0.0f, 0.0f, 1.0f};
            spawnEffectOriented(ctx, HOT_MESH_BEAM, HOT_TEX_DEFAULT, pos, up,
                                0.1f, 0.1f, 0.4f, 1.0f,
                                0.4f, 0.4f, 2.0f,
                                3.0f / 60.0f, 0.0f);
            return;
        }

        // Default afad20a-style walk mode: the audio-policy recipe owns the
        // variant and the volume/pitch jitter. Immediate repeats are intentional
        // (walk4, walk4, walk4 is valid), so no repeat suppression is applied.
        const float p[3] = {pos[0], pos[1], pos[2] - 0.6f};
        const float v[3] = {0.0f, 0.0f, 0.0f};
        spawnEffect(ctx, HOT_MESH_SPHERE, HOT_TEX_DEFAULT, p, v,
                    0.8f, 0.8f, 0.8f, 0.08f, 1.0f,
                    6.0f / 60.0f, 0.0f);
        hotEmitRecipeSound(ctx, gameHash("footstep"), pos, 0, true, nullptr);
        return;
    }

    // Generic actor/NPC action audio (hot policy): a logical actor-sound key in,
    // audio.play out. Cold picks no NPC sound.
    if (req->effectTypeId == gameHash("effect.actor.sound")) {
        req->handled = 1;
        if (req->text[0] != '\0') {
            const bool dash = std::strcmp(req->text, "actor.dash") == 0;
            const bool spawn = std::strcmp(req->text, "actor.spawn") == 0;
            HotAudioOverrideV1 ov{};
            ov.sound = dash ? "entity/player/dash"
                            : (spawn ? "npc_spawn" : req->text);
            hotEmitRecipeSound(ctx, gameHash(dash ? "npc.action"
                                                  : (spawn ? "npc.spawn"
                                                           : "npc.action")),
                               req->position, 0, true, &ov);
        }
        return;
    }

    // Air-jump audio (hot policy): same substrate as footstep; movement fact in,
    // audio.play out. Cold playAirJumpSound is fallback only.
    if (req->effectTypeId == gameHash("effect.jump.sound")) {
        req->handled = 1;
        hotEmitRecipeSound(ctx, gameHash("air_jump"), req->position, 0, true,
                           nullptr);
        return;
    }

    // Footstep audio (hot policy): cadence sound choice, volume, pitch, falloff
    // owned here. Movement state is read-only input; this does not own movement.
    if (req->effectTypeId == gameHash("effect.footstep.sound")) {
        req->handled = 1;
        // Legacy Player::updateAudio emits this compatibility fact from the
        // cold EXE. The canonical sound is emitted by effect.movement.footstep;
        // consume this one silently so the old and new paths cannot double-play.
        return;
    }

    // Weapon-fire audio (hot policy): sound choice/volume/pitch/falloff owned
    // here; the cold mixer only plays the resulting command.
    if (req->effectTypeId == gameHash("effect.weapon.fire.sound")) {
        req->handled = 1;
        if (req->text[0] != '\0') {
            HotAudioOverrideV1 ov{};
            ov.sound = req->text;
            hotEmitRecipeSound(ctx, gameHash("weapon.fire"), req->position, 0,
                               true, &ov);
        }
        return;
    }

    // Camera shake (hot policy): amplitude/falloff chosen here; cold applies it.
    if (req->effectTypeId == gameHash("effect.camera.shake")) {
        req->handled = 1;
        const float amp = req->scale > 0.0f ? req->scale : 1.0f;
        emitCameraEffect(ctx, amp * 4.0f, amp * 2.0f, req->distance,
                         req->falloffDistance);
        return;
    }

    // Muzzle flash: a bright, very short-lived recipe-driven effect. The tool/
    // weapon key is carried in req->weaponNetworkId (a runtime hash), never a
    // branch. No blue default texture and no cube: the recipe picks the mesh and
    // color, and may attach a dynamic light through the existing manager.
    if (req->effectTypeId == gameHash("effect.muzzle")) {
        req->handled = 1;
        const ToolVisualRecipeV1* recipe = findToolVisual(req->weaponNetworkId);
        const ToolMuzzleVisualV1* mz = recipe ? &recipe->muzzle : nullptr;
        const float vel[3] = {0.0f, 0.0f, 0.0f};
        const std::uint64_t meshId =
            (mz && mz->meshId) ? mz->meshId : HOT_MESH_SPHERE;
        const std::uint64_t texId = mz ? mz->textureId : 0;
        const float sc = (mz && mz->scale > 0.0f ? mz->scale : 0.16f) * scale;
        const float life =
            (mz && mz->lifetime > 0.0f) ? mz->lifetime : (1.0f / 60.0f);
        spawnEffect(ctx, meshId, texId, pos, vel,
                    mz ? mz->color[0] : 1.0f, mz ? mz->color[1] : 0.92f,
                    mz ? mz->color[2] : 0.62f, sc, mz ? mz->growth : 0.0f, life,
                    mz ? mz->fadeStart : 0.5f);
        if (mz && mz->hasLight)
            emitDynamicLight(ctx, pos, mz->lightColor, mz->lightOffset,
                             mz->lightIntensity, mz->lightRadius,
                             mz->lightLifetime);
        return;
    }

    // Server disagreement: the hot recipe owns pulse/beam/tracer/text/particles/
    // sound; the cold JSON composition only runs when this stays unhandled.
    if (req->effectTypeId == gameHash("effect.disagreement")) {
        req->handled = 1;
        composeServerDisagreement(ctx, *req);
        return;
    }
    if (req->effectTypeId == gameHash("effect.disagreement.local")) {
        req->handled = 1;
        composeLocalDisagreement(ctx, *req);
        return;
    }

    // Explosion: flash / smoke / debris + hot audio policy. Only the real
    // explosion kinds are handled; anything else stays unhandled (cold owner).
    if (req->effectTypeId == gameHash("effect.explosion.rocket") ||
        req->effectTypeId == gameHash("effect.explosion.grenade")) {
        req->handled = 1;
        composeExplosion(ctx, req->effectTypeId, pos, scale);
        return;
    }
}

// Runtime-unknown audio: a hot command plays a logical sound the EXE never knew
// about, through the generic audio.play capability. No audio enum/switch.
void MIMITA_GAME_CALL hotAudioTestCommand(void* host, const char* args)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability)
        return;
    auto audio = reinterpret_cast<AudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!audio)
        return;
    GameAudioCommandV1 cmd{};
    std::snprintf(cmd.sound, sizeof(cmd.sound), "%s",
                  (args && args[0]) ? args : "rocketlauncher/rocketlauncherexplode");
    cmd.volume = 0.8f;
    cmd.pitch = 1.2f;
    cmd.spatial = 0;
    audio(ctx->host, &cmd);
}

// Screen effect via the existing hot UI path (no new compositor). A hot ui.frame
// system owns the fade; the cold UI backend draws the full-screen panel.
struct ScreenFxState {
    bool active = false;
    float remaining = 0.0f;
    float duration = 0.0f;
    float color[4] = {1.0f, 0.1f, 0.1f, 0.6f};
};
ScreenFxState g_screenFx;

using RenderUiFn = void (MIMITA_GAME_CALL *)(void*, const GameUiCommandV1*);

void MIMITA_GAME_CALL screenFxTick(void* host, std::uint64_t /*tick*/, float dt)
{
    if (!g_screenFx.active)
        return;
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->resolveCapability)
        return;
    auto ui = reinterpret_cast<RenderUiFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_UI));
    if (!ui)
        return;
    const float t = g_screenFx.duration > 0.0f
                        ? g_screenFx.remaining / g_screenFx.duration : 1.0f;
    GameUiCommandV1 c{};
    c.kind = GAME_UI_PANEL;
    c.x = 0.0f; c.y = 0.0f; c.w = 100000.0f; c.h = 100000.0f;
    c.color[0] = g_screenFx.color[0];
    c.color[1] = g_screenFx.color[1];
    c.color[2] = g_screenFx.color[2];
    c.color[3] = g_screenFx.color[3] * t;
    ui(ctx->host, &c);
    g_screenFx.remaining -= dt;
    if (g_screenFx.remaining <= 0.0f)
        g_screenFx.active = false;
}

void MIMITA_GAME_CALL hotScreenFxCommand(void* /*host*/, const char* /*args*/)
{
    g_screenFx.active = true;
    g_screenFx.remaining = 0.6f;
    g_screenFx.duration = 0.6f;
}
const MimitaHotPackage::SystemRegistrar s_screenFxSystem{
    {gameHash("hot.screen-fx"), GAME_DOMAIN_UI, 20, 0, screenFxTick,
     "hot.screen-fx"}};
const MimitaHotPackage::CommandRegistrar s_hotScreenFx{
    {"hotscreenfx", "hotscreenfx - runtime screen effect via hot UI", 0,
     hotScreenFxCommand}};

const MimitaHotPackage::EventRegistrar s_effectRequest{
    {gameHash("effect.request"), gameHash("effect.request.v3"), 0, onEffectRequest,
     "hot.effect-composition"}};
const MimitaHotPackage::CommandRegistrar s_hotAudioTest{
    {"hotaudiotest", "hotaudiotest [logical sound] - play a runtime sound", 0,
     hotAudioTestCommand}};

// Runtime-unknown surface effect: a hot command creates a mark the EXE never
// knew about through the generic surface.effect capability. No enum/switch.
void MIMITA_GAME_CALL hotSurfaceEffectCommand(void* host, const char* /*args*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx)
        return;
    const float pos[3] = {2.0f, 1.0f, 1.0f};
    const float nrm[3] = {0.0f, 0.0f, 1.0f};
    emitSurfaceEffect(ctx, pos, nrm, 0.2f, 0.9f, 0.4f, 0.4f, 25.0f);
}
const MimitaHotPackage::CommandRegistrar s_hotSurfaceEffect{
    {"hotsurfaceeffect", "hotsurfaceeffect - create a runtime surface effect", 0,
     hotSurfaceEffectCommand}};

// Runtime-unknown camera effect: a hot command requests a camera perturbation
// the EXE never knew about, through the generic camera.effect capability.
void MIMITA_GAME_CALL hotCameraFxCommand(void* host, const char* /*args*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx)
        return;
    emitCameraEffect(ctx, 6.0f, 3.0f, 0.0f, 0.0f);
}
const MimitaHotPackage::CommandRegistrar s_hotCameraFx{
    {"hotcamerafx", "hotcamerafx - request a runtime camera effect", 0,
     hotCameraFxCommand}};

} // namespace

// External entry for the hot projectile simulation: compose a detonation with
// the same recipe the generic effect.request path uses. No cold call site.
void hotComposeExplosion(GameplayContextV1* ctx, std::uint64_t effectTypeId,
                         const float position[3], float scale)
{
    if (!ctx)
        return;
    composeExplosion(ctx, effectTypeId, position, scale);
}

#endif
