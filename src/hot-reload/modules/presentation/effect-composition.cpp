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
#include "hot-reload/hot-effect.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
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
    life.scale0 = 1.0f;
    life.growth = growth;
    life.fadeStart = fadeStart;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_EFFECT_LIFETIME_COMPONENT,
                               &life, sizeof(life));
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
void emitExplosionSound(GameplayContextV1* ctx, const EffectRequestV1& req)
{
    if (!ctx->resolveCapability)
        return;
    auto audio = reinterpret_cast<AudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!audio)
        return;
    const bool grenade = req.effectTypeId == gameHash("effect.explosion.grenade");
    GameAudioCommandV1 cmd{};
    std::snprintf(cmd.sound, sizeof(cmd.sound), "%s",
                  grenade ? "grenadelauncher/grenadelauncherexplode"
                          : "rocketlauncher/rocketlauncherexplode");
    cmd.position[0] = req.position[0];
    cmd.position[1] = req.position[1];
    cmd.position[2] = req.position[2];
    cmd.volume = 1.0f;
    cmd.pitch = 1.0f;
    cmd.maxDistance = 50.0f;
    cmd.spatial = 1;
    audio(ctx->host, &cmd);
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

    // Hit impacts (blood/world): a small burst of generic effect entities.
    if (req->effectTypeId == gameHash("effect.hit.blood") ||
        req->effectTypeId == gameHash("effect.hit.world")) {
        req->handled = 1;
        const bool blood = req->effectTypeId == gameHash("effect.hit.blood");
        for (int i = 0; i < 4; ++i) {
            const float a = (float)i * 1.5708f;
            const float vel[3] = {std::cos(a) * 1.5f * scale,
                                  std::sin(a) * 1.5f * scale, 1.2f * scale};
            spawnEffect(ctx, HOT_MESH_CUBE, HOT_TEX_DEFAULT, pos, vel,
                        blood ? 0.7f : 0.9f, blood ? 0.05f : 0.7f,
                        blood ? 0.05f : 0.4f, 0.12f * scale, 0.0f,
                        blood ? 0.5f : 0.35f, 0.4f);
        }
        // Surface effect: hot policy picks color/size/lifetime; the cold
        // backend puts a generic mark on the surface.
        emitSurfaceEffect(ctx, pos, req->normal, blood ? 0.7f : 0.9f,
                          blood ? 0.05f : 0.7f, blood ? 0.05f : 0.4f,
                          0.15f * scale, blood ? 20.0f : 30.0f);
        return;
    }

    // Generic actor/NPC action audio (hot policy): a logical actor-sound key in,
    // audio.play out. Cold picks no NPC sound.
    if (req->effectTypeId == gameHash("effect.actor.sound")) {
        req->handled = 1;
        if (ctx->resolveCapability && req->text[0] != '\0') {
            auto audio = reinterpret_cast<AudioPlayFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
            if (audio) {
                const char* sound =
                    std::strcmp(req->text, "actor.dash") == 0
                        ? "entity/player/dash"
                    : std::strcmp(req->text, "actor.spawn") == 0
                        ? "npc_spawn" : req->text;
                GameAudioCommandV1 c{};
                std::snprintf(c.sound, sizeof(c.sound), "%s", sound);
                c.position[0] = req->position[0];
                c.position[1] = req->position[1];
                c.position[2] = req->position[2];
                c.volume = 1.0f;
                c.pitch = 1.0f;
                c.maxDistance = 36.0f;
                c.spatial = 1;
                audio(ctx->host, &c);
            }
        }
        return;
    }

    // Air-jump audio (hot policy): same substrate as footstep; movement fact in,
    // audio.play out. Cold playAirJumpSound is fallback only.
    if (req->effectTypeId == gameHash("effect.jump.sound")) {
        req->handled = 1;
        if (ctx->resolveCapability) {
            auto audio = reinterpret_cast<AudioPlayFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
            if (audio) {
                GameAudioCommandV1 c{};
                std::snprintf(c.sound, sizeof(c.sound), "%s", "entity/player/doublejump");
                c.position[0] = req->position[0];
                c.position[1] = req->position[1];
                c.position[2] = req->position[2];
                c.volume = 1.0f;
                c.pitch = 1.0f;
                c.maxDistance = 22.0f;
                c.spatial = 0;
                audio(ctx->host, &c);
            }
        }
        return;
    }

    // Footstep audio (hot policy): cadence sound choice, volume, pitch, falloff
    // owned here. Movement state is read-only input; this does not own movement.
    if (req->effectTypeId == gameHash("effect.footstep.sound")) {
        req->handled = 1;
        if (ctx->resolveCapability) {
            auto audio = reinterpret_cast<AudioPlayFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
            if (audio) {
                static std::uint32_t s_step = 0;
                GameAudioCommandV1 c{};
                if (req->text[0] != '\0')   // arbitrary logical id (no enum)
                    std::snprintf(c.sound, sizeof(c.sound), "%s", req->text);
                else
                    std::snprintf(c.sound, sizeof(c.sound), "entity/player/walk%d",
                                  1 + (int)(s_step++ % 4u));
                c.position[0] = req->position[0];
                c.position[1] = req->position[1];
                c.position[2] = req->position[2];
                c.volume = 0.8f;
                c.pitch = 1.0f;
                c.maxDistance = 22.0f;
                c.spatial = 1;
                audio(ctx->host, &c);
            }
        }
        return;
    }

    // Weapon-fire audio (hot policy): sound choice/volume/pitch/falloff owned
    // here; the cold mixer only plays the resulting command.
    if (req->effectTypeId == gameHash("effect.weapon.fire.sound")) {
        req->handled = 1;
        if (ctx->resolveCapability && req->text[0] != '\0') {
            auto audio = reinterpret_cast<AudioPlayFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
            if (audio) {
                GameAudioCommandV1 c{};
                std::snprintf(c.sound, sizeof(c.sound), "%s", req->text);
                c.position[0] = req->position[0];
                c.position[1] = req->position[1];
                c.position[2] = req->position[2];
                c.volume = 0.9f;
                c.pitch = 1.0f;
                c.maxDistance = 80.0f;
                c.spatial = 1;
                audio(ctx->host, &c);
            }
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

    // Muzzle flash: a bright, very short-lived generic effect. The tool/weapon
    // key is carried in req->weaponNetworkId (a runtime hash), never a branch.
    if (req->effectTypeId == gameHash("effect.muzzle")) {
        req->handled = 1;
        const float vel[3] = {0.0f, 0.0f, 0.0f};
        spawnEffect(ctx, HOT_MESH_CUBE, HOT_TEX_DEFAULT, pos, vel, 1.0f, 0.9f,
                    0.4f, 0.35f * scale, 0.0f, 0.08f, 0.5f);
        return;
    }

    // Explosion: flash / smoke / debris + hot audio policy. Only the real
    // explosion kinds are handled; anything else stays unhandled (cold owner).
    if (req->effectTypeId != gameHash("effect.explosion.rocket") &&
        req->effectTypeId != gameHash("effect.explosion.grenade"))
        return;
    req->handled = 1;
    emitExplosionSound(ctx, *req);

    // Flash: expanding, fast fade.
    {
        const float vel[3] = {0.0f, 0.0f, 0.0f};
        spawnEffect(ctx, HOT_MESH_CUBE, HOT_TEX_DEFAULT, pos, vel, 1.0f, 0.75f,
                    0.25f, 0.5f * scale, 3.0f, 0.35f, 0.2f);
    }
    // Smoke: rises while growing, slower fade.
    {
        const float vel[3] = {0.0f, 0.0f, 1.5f};
        spawnEffect(ctx, HOT_MESH_CUBE, HOT_TEX_DEFAULT, pos, vel, 0.35f, 0.35f,
                    0.35f, 0.6f * scale, 1.2f, 1.4f, 0.55f);
    }
    // Debris: small, upward, short.
    for (int i = 0; i < 3; ++i) {
        const float a = (float)i * 2.094f;
        const float vel[3] = {std::cos(a) * 2.0f, std::sin(a) * 2.0f, 3.0f};
        spawnEffect(ctx, HOT_MESH_CUBE, HOT_TEX_DEFAULT, pos, vel, 0.8f, 0.5f,
                    0.2f, 0.25f * scale, 0.0f, 0.8f, 0.6f);
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
    {gameHash("effect.request"), gameHash("effect.request.v1"), 0, onEffectRequest,
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

#endif
