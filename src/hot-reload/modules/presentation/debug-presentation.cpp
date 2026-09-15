// 09 14 2026
/* purpose
* Hot generic presentation. Two hot render.frame systems own presentation
* policy using only generic capabilities:
*  - `hot.presentation-mesh`: any entity carrying the shared
*    `PresentationState` dynamic component (rocket, grenade, future item) is
*    drawn through the generic `render.mesh` capability using logical
*    mesh/texture resource ids. Orientation follows Transform.look, or velocity
*    when a Velocity component is present.
*  - `hot.debug-presentation`: any entity carrying `PresentationDebug` is drawn
*    as a wire shape through `render.debug`, and a generic HUD panel is composed
*    through `render.debug` HUD text.
* No RocketPresentation/GrenadePresentation/NpcProjectilePresentation and no
* Player/NPC/Projectile switch. Editing this file and saving changes the running
* client's presentation.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-effect.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-presentation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const std::uint64_t kPresentationDebug = gameHash("PresentationDebug");
const std::uint64_t kPresentationDebugV1 = gameHash("PresentationDebug.v1");

// Generic presentation descriptor for wire/debug shapes.
struct PresentationDebugV1 {
    float scale;
    float color[4];
    std::uint32_t shape;   // GameRenderDebugShape
    std::uint32_t reserved;
};

using RenderDebugFn = void (MIMITA_GAME_CALL *)(void*, const GameRenderDebugCommandV1*);
using RenderMeshFn = void (MIMITA_GAME_CALL *)(void*, const GameRenderMeshCommandV1*);

RenderDebugFn resolveRenderDebug(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->resolveCapability)
        return nullptr;
    return reinterpret_cast<RenderDebugFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_DEBUG));
}

RenderMeshFn resolveRenderMesh(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->resolveCapability)
        return nullptr;
    return reinterpret_cast<RenderMeshFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RENDER_MESH));
}

// The local possessed actor's BODY is drawn by the cold typed body mechanism
// (hot pose policy -> SkeletonInstances -> cold body draw). The generic mesh
// path must not also draw it, or the body is submitted twice (two owners). This
// only skips the local actor's own PresentationState; tools, attachments,
// effects, and arbitrary runtime entities on other EntityIds still draw here.
std::uint64_t localPossessedActor(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return 0;
    const auto* shared =
        reinterpret_cast<const GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared->localPlayerEntity : 0;
}

void quatFromForward(float x, float y, float z, float out[4])
{
    const float len = std::sqrt(x * x + y * y + z * z);
    out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 1.0f;
    if (len < 1e-4f)
        return;
    x /= len; y /= len; z /= len;
    const float dot = z;  // +Z forward
    if (dot < -0.9999f) {
        out[0] = 1.0f; out[1] = 0.0f; out[2] = 0.0f; out[3] = 0.0f;
        return;
    }
    out[0] = -y;
    out[1] = x;
    out[2] = 0.0f;
    out[3] = 1.0f + dot;
    const float qlen = std::sqrt(out[0]*out[0] + out[1]*out[1] + out[2]*out[2] + out[3]*out[3]);
    if (qlen > 1e-6f) {
        out[0] /= qlen; out[1] /= qlen; out[2] /= qlen; out[3] /= qlen;
    }
}

// Present every entity that carries the generic mesh presentation component.
void MIMITA_GAME_CALL presentationMeshTick(void* host, std::uint64_t /*tick*/,
                                           float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->readComponent)
        return;
    RenderMeshFn renderMesh = resolveRenderMesh(ctx);
    if (!renderMesh)
        return;

    const std::uint64_t localActor = localPossessedActor(ctx);
    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_PRESENTATION_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (localActor != 0 && entities[i] == localActor)
            continue;   // local body: cold body mechanism is the one owner
        HotPresentationStateV1 state{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_PRESENTATION_COMPONENT, &state,
                                       sizeof(state)))
            continue;
        // An attached entity follows a named socket on its parent. The resolved
        // presentation transform overrides the entity transform; the entity's
        // authoritative Transform is never written. Unresolved => hidden.
        HotAttachmentStateV1 att{};
        const bool hasAtt = ctx->dynamicReadComponent(
            ctx->host, entities[i], HOT_ATTACHMENT_COMPONENT, &att, sizeof(att));
        if (hasAtt && att.resolved == 0)
            continue;

        GameTransformComponentV1 tf{};
        if (!hasAtt &&
            !ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_TRANSFORM,
                                &tf, sizeof(tf)))
            continue;

        GameRenderMeshCommandV1 cmd{};
        cmd.entity = entities[i];
        cmd.meshResourceId = state.meshResourceId ? state.meshResourceId : HOT_MESH_CUBE;
        cmd.textureResourceId = state.textureResourceId;
        float scale = state.scale > 0.0f ? state.scale : 1.0f;
        if (hasAtt) {
            cmd.position[0] = att.worldPosition[0];
            cmd.position[1] = att.worldPosition[1];
            cmd.position[2] = att.worldPosition[2];
            cmd.rotation[0] = att.worldRotation[0];
            cmd.rotation[1] = att.worldRotation[1];
            cmd.rotation[2] = att.worldRotation[2];
            cmd.rotation[3] = att.worldRotation[3];
            scale *= att.worldScale[0] > 0.0f ? att.worldScale[0] : 1.0f;
            if (att.context == HOT_ATTACHMENT_CONTEXT_VIEW)
                cmd.flags |= GAME_RENDER_MESH_SPACE_VIEW;
        } else {
            float forward[3] = {tf.look[0], tf.look[1], tf.look[2]};
            GameVelocityComponentV1 vel{};
            if (ctx->readComponent(ctx->host, entities[i],
                                   GAME_COMPONENT_VELOCITY, &vel, sizeof(vel))) {
                const float speed = std::sqrt(vel.linear[0]*vel.linear[0] +
                                              vel.linear[1]*vel.linear[1] +
                                              vel.linear[2]*vel.linear[2]);
                if (speed > 0.05f) {
                    forward[0] = vel.linear[0];
                    forward[1] = vel.linear[1];
                    forward[2] = vel.linear[2];
                }
            }
            cmd.position[0] = tf.position[0];
            cmd.position[1] = tf.position[1];
            cmd.position[2] = tf.position[2];
            quatFromForward(forward[0], forward[1], forward[2], cmd.rotation);
        }
        cmd.scale[0] = cmd.scale[1] = cmd.scale[2] = scale;
        cmd.color[0] = state.color[0];
        cmd.color[1] = state.color[1];
        cmd.color[2] = state.color[2];
        cmd.color[3] = state.color[3] > 0.0f ? state.color[3] : 1.0f;
        renderMesh(ctx->host, &cmd);
    }
}

// Present generic wire shapes and compose a generic HUD panel.
void MIMITA_GAME_CALL debugPresentationTick(void* host, std::uint64_t /*tick*/,
                                            float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicReadComponent ||
        !ctx->readComponent)
        return;
    RenderDebugFn render = resolveRenderDebug(ctx);
    if (!render)
        return;

    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, kPresentationDebug, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        PresentationDebugV1 state{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i], kPresentationDebug,
                                       &state, sizeof(state)))
            continue;
        GameTransformComponentV1 tf{};
        if (!ctx->readComponent(ctx->host, entities[i], GAME_COMPONENT_TRANSFORM,
                                &tf, sizeof(tf)))
            continue;

        GameRenderDebugCommandV1 cmd{};
        cmd.shape = state.shape ? state.shape : GAME_RENDER_DEBUG_WIRE_BOX;
        cmd.ownerEntity = entities[i];
        cmd.a[0] = tf.position[0];
        cmd.a[1] = tf.position[1];
        cmd.a[2] = tf.position[2];
        const float scale = state.scale > 0.0f ? state.scale : 0.5f;
        cmd.half[0] = scale;
        cmd.half[1] = scale;
        cmd.half[2] = scale;
        cmd.radius = scale;
        cmd.color[0] = state.color[0];
        cmd.color[1] = state.color[1];
        cmd.color[2] = state.color[2];
        cmd.color[3] = state.color[3];
        render(ctx->host, &cmd);
    }

    // Hot HUD composition: a generic screen-space panel, no gamemode switch.
    GameRenderDebugCommandV1 hud{};
    hud.shape = GAME_RENDER_DEBUG_HUD_TEXT;
    hud.a[0] = 12.0f;
    hud.a[1] = 40.0f;
    hud.radius = 0.30f;
    hud.color[0] = 0.5f; hud.color[1] = 1.0f; hud.color[2] = 0.9f; hud.color[3] = 1.0f;
    std::snprintf(hud.text, sizeof(hud.text), "HOT HUD  generic-entities=%u",
                  (unsigned)count);
    render(ctx->host, &hud);
}

// Command: create a local typeless actor entity that runs the FULL generic
// presentation chain (mesh.actor + AnimationState + PresentationState) so the
// hot pose -> render.mesh path can be checked visually without a remote NPC.
void MIMITA_GAME_CALL hotactorCommand(void* host, const char* /*args*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent)
        return;
    static float s_x = 2.0f;
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    tf.position[0] = s_x;
    tf.position[1] = 1.0f;
    tf.position[2] = 6.0f;
    tf.look[0] = 1.0f;
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    GameVelocityComponentV1 vel{};
    vel.linear[0] = 2.0f;  // moving -> move clip
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_VELOCITY, &vel,
                        sizeof(vel));
    HotAnimationStateV1 anim{};
    anim.clipId = HOT_ANIM_MOVE;
    anim.playbackRate = 1.0f;
    anim.loop = 1;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_ANIMATION_STATE_COMPONENT,
                               &anim, sizeof(anim));
    HotPresentationStateV1 present{};
    present.meshResourceId = HOT_MESH_ACTOR;
    present.textureResourceId = HOT_TEX_DEFAULT;
    present.scale = 1.0f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));
    std::printf("[HOT ACTOR] entity=%llu at x=%.1f\n",
                (unsigned long long)entity, s_x);
    s_x += 1.5f;
}

// Command: spawn a runtime-unknown generic effect - an entity that rises while
// growing and fading, then expires. No EXE enum/switch; the hot
// `hot.effect-lifecycle` system owns its lifetime.
void MIMITA_GAME_CALL hoteffectCommand(void* host, const char* /*args*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent)
        return;
    static float s_x = 2.0f;
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    tf.position[0] = s_x;
    tf.position[1] = 1.0f;
    tf.position[2] = 5.0f;
    tf.look[0] = 1.0f;
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    GameVelocityComponentV1 vel{};
    vel.linear[2] = 2.0f;  // rises
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_VELOCITY, &vel,
                        sizeof(vel));
    HotPresentationStateV1 present{};
    present.meshResourceId = HOT_MESH_CUBE;
    present.textureResourceId = HOT_TEX_DEFAULT;
    present.scale = 0.4f;
    present.color[0] = 0.3f; present.color[1] = 0.7f;
    present.color[2] = 1.0f; present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));
    HotEffectLifetimeV1 life{};
    life.age = 0.0f;
    life.lifetime = 1.5f;
    life.scale0 = 1.0f;
    life.growth = 1.5f;
    life.fadeStart = 0.5f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_EFFECT_LIFETIME_COMPONENT,
                               &life, sizeof(life));
    std::printf("[HOT EFFECT] entity=%llu at x=%.1f\n",
                (unsigned long long)entity, s_x);
    s_x += 1.5f;
}

// Generic predicted-entity tool behavior (non-projectile). Creates an arbitrary
// entity and writes the generic PredictionLink from the action's predictionKey,
// proving the server predicted-spawn path carries no projectile assumptions.
void MIMITA_GAME_CALL predictedTestUse(const ToolUsePolicyV1* use,
                                       GameplayContextV1* ctx)
{
    if (!use || !ctx)
        return;
    auto* mutableUse = const_cast<ToolUsePolicyV1*>(use);
    mutableUse->outFire = 0;
    mutableUse->handled = 1;
    if (!ctx->entityCreate || !ctx->dynamicWriteComponent)
        return;
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    HotPredictionLinkV1 link{};
    link.predictionKey = use->predictionKey;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PREDICTION_LINK_COMPONENT,
                               &link, sizeof(link));
}

// Command: create a local runtime entity presented only through generic mesh
// presentation data (Transform + PresentationState). No conceptual type.
void MIMITA_GAME_CALL hotpresentCommand(void* host, const char* /*args*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent)
        return;

    static float s_x = 2.0f;
    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;

    GameTransformComponentV1 tf{};
    tf.position[0] = s_x;
    tf.position[1] = 1.0f;
    tf.position[2] = 6.0f;
    tf.look[0] = 1.0f;
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));

    HotPresentationStateV1 present{};
    present.meshResourceId = HOT_MESH_CUBE;
    present.textureResourceId = HOT_TEX_DEFAULT;
    present.scale = 0.7f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));

    std::printf("[HOT PRESENT] mesh entity=%llu at x=%.1f\n",
                (unsigned long long)entity, s_x);
    s_x += 1.5f;
}

const MimitaHotPackage::SchemaRegistrar s_presentationDebugSchema{
    {kPresentationDebug, kPresentationDebugV1, sizeof(PresentationDebugV1), 4,
     GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL, "PresentationDebug", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_presentationStateSchema{
    {HOT_PRESENTATION_COMPONENT, gameHash("PresentationState.v1"),
     sizeof(HotPresentationStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "PresentationState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_predictionLinkSchema{
    {HOT_PREDICTION_LINK_COMPONENT, gameHash("PredictionLink.v1"),
     sizeof(HotPredictionLinkV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_ALL,
     "PredictionLink", 1, 0}};
const MimitaHotPackage::SystemRegistrar s_presentationMeshSystem{
    {gameHash("hot.presentation-mesh"), GAME_DOMAIN_RENDER, 5, 0,
     presentationMeshTick, "hot.presentation-mesh"}};
const MimitaHotPackage::SystemRegistrar s_presentationDebugSystem{
    {gameHash("hot.debug-presentation"), GAME_DOMAIN_RENDER, 10, 0,
     debugPresentationTick, "hot.debug-presentation"}};
const MimitaHotPackage::ToolBehaviorRegistrar s_predictedTestTool{
    gameHash("hot.predicted-test"), predictedTestUse};
const MimitaHotPackage::CommandRegistrar s_presentationCommand{
    {"hotpresent", "hotpresent - create a generic-presented runtime entity", 0,
     hotpresentCommand}};
const MimitaHotPackage::CommandRegistrar s_actorCommand{
    {"hotactor", "hotactor - spawn a typeless actor entity (mesh.actor + pose)", 0,
     hotactorCommand}};
const MimitaHotPackage::CommandRegistrar s_effectCommand{
    {"hoteffect", "hoteffect - spawn a runtime-unknown generic effect entity", 0,
     hoteffectCommand}};

} // namespace

#endif
