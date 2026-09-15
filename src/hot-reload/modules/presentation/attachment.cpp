// 09 15 2026
/* purpose
* Hot generic attachment + tool presentation. Two render.frame systems:
*  - `hot.tool-presentation` (order 3) consumes the REAL generic equip state
*    (`relationship.equips-item` + the tool entity's `ToolRefState.toolKey`) and
*    writes `PresentationState` + `AttachmentState` onto the REAL tool EntityId.
*    Hot policy chooses the logical mesh for a tool key (hot C++, not a kernel
*    database). Local possessed actor -> VIEW context, other actors -> WORLD.
*    Unmigrated tools are untouched (cold fallback). No typed player/weapon
*    pointer is used.
*  - `hot.attachment` (order 4) resolves every `AttachmentState` through the
*    generic `socket.query` capability and writes the presentation world
*    transform back; `hot.presentation-mesh` (order 5) consumes it.
* Raw hot C++ can also register arbitrary logical mesh resources through
* `resource.register`; `hottool` uses the SAME real path (equip relationship).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const std::uint64_t kRightArm = gameHash("rightArm");
const std::uint64_t kEquipsItemRel = gameHash("relationship.equips-item");
const std::uint64_t kToolRefState = gameHash("ToolRefState");

using SocketQueryFn = bool (MIMITA_GAME_CALL *)(void*, GameSocketQueryV1*);
using ResourceRegisterFn = bool (MIMITA_GAME_CALL *)(void*, GameResourceRegisterV1*);

struct ToolRefStateV1 {
    std::uint64_t toolKey;
    std::uint64_t reserved;
};

// ── Hot tool-presentation policy data (hot C++, no kernel weapon database) ──
struct ToolMeshBinding {
    std::uint64_t toolKey;
    std::uint64_t meshId;
    const char* path;
    bool registered;
};
ToolMeshBinding g_bindings[] = {
    // Real shipping swordsword: logical tool key -> logical mesh + GLB. The key
    // is gameHash(weapon id), the same generic key the standard equip bridge and
    // runtime tools write into ToolRefState.toolKey (no enum, mod-friendly).
    {gameHash("swordsword"), gameHash("mesh.tool.swordsword"),
     "assets/objects/weapons/mimita-hafs-v1.glb", false},
    // Real shipping revolver: a different presentation behavior (firearm) on the
    // same substrate - no new ABI, just a logical-mesh binding.
    {gameHash("revolver"), gameHash("mesh.tool.revolver"),
     "assets/objects/weapons/mimita-revolver-v1.glb", false},
    // Remaining standard weapons: mechanical batch, same substrate.
    {gameHash("shotgun"), gameHash("mesh.tool.shotgun"),
     "assets/objects/weapons/mimita-shotgun-v1.glb", false},
    {gameHash("rocket_launcher"), gameHash("mesh.tool.rocket_launcher"),
     "assets/objects/weapons/mimita-rpg-v3.glb", false},
    {gameHash("grenade_launcher"), gameHash("mesh.tool.grenade_launcher"),
     "assets/objects/weapons/mimita-nadelauncher-v1.glb", false},
};
constexpr std::uint32_t kBindingCount =
    (std::uint32_t)(sizeof(g_bindings) / sizeof(g_bindings[0]));
// Runtime-unknown tool binding (used by the `hottool` proof; same real path).
ToolMeshBinding g_runtimeBinding{gameHash("tool.runtime.unknown"),
                                 gameHash("mesh.runtime.tool"),
                                 "assets/objects/weapons/mimita-hafs-v1.glb",
                                 false};

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

void registerBindingMesh(GameplayContextV1* ctx, ToolMeshBinding& b)
{
    if (b.registered || !ctx->resolveCapability)
        return;
    auto reg = reinterpret_cast<ResourceRegisterFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RESOURCE_REGISTER));
    if (!reg)
        return;
    GameResourceRegisterV1 req{};
    req.logicalId = b.meshId;
    req.kind = GAME_RESOURCE_MESH;
    req.applyNow = 1;
    std::snprintf(req.path, sizeof(req.path), "%s", b.path);
    reg(ctx->host, &req);
    b.registered = true;
}

// Hot policy: logical mesh id for a tool key (0 = not hot-migrated).
std::uint64_t toolMeshFor(GameplayContextV1* ctx, std::uint64_t toolKey)
{
    for (std::uint32_t i = 0; i < kBindingCount; ++i) {
        if (g_bindings[i].toolKey == toolKey) {
            registerBindingMesh(ctx, g_bindings[i]);
            return g_bindings[i].meshId;
        }
    }
    if (g_runtimeBinding.toolKey == toolKey) {
        registerBindingMesh(ctx, g_runtimeBinding);
        return g_runtimeBinding.meshId;
    }
    return 0;
}

void writeToolPresentation(GameplayContextV1* ctx, std::uint64_t toolEntity,
                           std::uint64_t meshId, std::uint64_t actor,
                           std::uint32_t context)
{
    HotPresentationStateV1 present{};
    present.meshResourceId = meshId;
    present.textureResourceId = HOT_TEX_DEFAULT;
    present.scale = 1.0f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, toolEntity,
                               HOT_PRESENTATION_COMPONENT, &present,
                               sizeof(present));

    HotAttachmentStateV1 att{};
    att.parentEntity = actor;
    att.socket = kRightArm;
    att.context = context;
    att.flags = HOT_ATTACHMENT_FLAG_VISIBLE;
    att.localRotation[3] = 1.0f;
    att.localScale[0] = att.localScale[1] = att.localScale[2] = 1.0f;
    if (context == HOT_ATTACHMENT_CONTEXT_VIEW) {
        att.localPosition[0] = 0.35f;
        att.localPosition[1] = 0.10f;
        att.localPosition[2] = -0.45f;
    } else {
        att.localPosition[0] = 0.30f;
    }
    ctx->dynamicWriteComponent(ctx->host, toolEntity, HOT_ATTACHMENT_COMPONENT,
                               &att, sizeof(att));
}

// Resolve an actor's equipped tools through the REAL generic equip substrate.
void presentActorTools(GameplayContextV1* ctx, std::uint64_t actor,
                       std::uint32_t context, bool claimOnActor)
{
    std::uint64_t tools[4] = {0, 0, 0, 0};
    const std::uint32_t n = ctx->relationshipQuery(
        ctx->host, kEquipsItemRel, actor, tools, nullptr, 4);
    std::uint64_t claimed = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (tools[i] == 0)
            continue;
        ToolRefStateV1 ref{};
        if (!ctx->dynamicReadComponent(ctx->host, tools[i], kToolRefState, &ref,
                                       sizeof(ref)))
            continue;
        const std::uint64_t meshId = toolMeshFor(ctx, ref.toolKey);
        if (meshId == 0)
            continue;   // unmigrated: cold owns it
        writeToolPresentation(ctx, tools[i], meshId, actor, context);
        claimed = ref.toolKey;
    }
    if (claimOnActor) {
        HotToolClaimV1 claim{};
        claim.toolKey = claimed;
        claim.context = context;
        claim.migrated = claimed != 0 ? 1u : 0u;
        ctx->dynamicWriteComponent(ctx->host, actor, HOT_TOOL_CLAIM_COMPONENT,
                                   &claim, sizeof(claim));
    }
}

void MIMITA_GAME_CALL toolPresentationTick(void* host, std::uint64_t /*tick*/,
                                           float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->relationshipQuery ||
        !ctx->dynamicReadComponent || !ctx->dynamicWriteComponent ||
        !ctx->resolveCapability)
        return;

    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t localActor = shared ? shared->localPlayerEntity : 0;

    // Local possessed actor: first-person (VIEW) presentation context.
    if (localActor != 0)
        presentActorTools(ctx, localActor, HOT_ATTACHMENT_CONTEXT_VIEW, true);

    // Other actors that participate in hot animation: third-person (WORLD).
    std::uint64_t actors[128];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ANIMATION_STATE_COMPONENT, actors, 128);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (actors[i] == localActor || actors[i] == 0)
            continue;
        presentActorTools(ctx, actors[i], HOT_ATTACHMENT_CONTEXT_WORLD, false);
    }
}

// ── Attachment resolution ────────────────────────────────────────────
void MIMITA_GAME_CALL attachmentTick(void* host, std::uint64_t /*tick*/,
                                     float /*dt*/)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->dynamicEnumerateComponent || !ctx->dynamicWriteComponent ||
        !ctx->resolveCapability)
        return;
    auto query = reinterpret_cast<SocketQueryFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SOCKET_QUERY));
    if (!query)
        return;

    std::uint64_t entities[256];
    const std::uint32_t count = ctx->dynamicEnumerateComponent(
        ctx->host, HOT_ATTACHMENT_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        HotAttachmentStateV1 att{};
        if (!ctx->dynamicReadComponent(ctx->host, entities[i],
                                       HOT_ATTACHMENT_COMPONENT, &att,
                                       sizeof(att)))
            continue;
        att.resolved = 0;
        if (att.parentEntity != 0) {
            GameSocketQueryV1 q{};
            q.entity = att.parentEntity;
            q.socket = att.socket;
            for (int k = 0; k < 3; ++k)
                q.localPosition[k] = att.localPosition[k];
            for (int k = 0; k < 4; ++k)
                q.localRotation[k] = att.localRotation[k];
            for (int k = 0; k < 3; ++k)
                q.localScale[k] = att.localScale[k];
            if (query(ctx->host, &q) && q.valid) {
                for (int k = 0; k < 3; ++k) {
                    att.worldPosition[k] = q.position[k];
                    att.worldScale[k] = q.scale[k];
                }
                for (int k = 0; k < 4; ++k)
                    att.worldRotation[k] = q.rotation[k];
                att.resolved = 1;
            }
        }
        ctx->dynamicWriteComponent(ctx->host, entities[i],
                                   HOT_ATTACHMENT_COMPONENT, &att, sizeof(att));
    }
}

// ── Runtime-unknown tool proof command (same real equip path) ─────────
void MIMITA_GAME_CALL hotMeshCommand(void* host, const char* args)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (args && *args)
        g_runtimeBinding.path = args;
    g_runtimeBinding.registered = false;
    registerBindingMesh(ctx, g_runtimeBinding);
    std::printf("[TOOL] registered runtime mesh '%s'\n", g_runtimeBinding.path);
}

// The debug command only creates the entity + generic equip state; presentation
// then flows through the same hot.tool-presentation system as a real tool.
void MIMITA_GAME_CALL hotToolCommand(void* host, const char* args)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent || !ctx->relationshipAdd)
        return;
    if (args && *args)
        g_runtimeBinding.path = args;
    g_runtimeBinding.registered = false;

    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t actor =
        shared ? (shared->localPlayerEntity ? shared->localPlayerEntity
                                            : shared->selectedEntity)
               : 0;
    if (actor == 0)
        return;

    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return;
    GameTransformComponentV1 tf{};
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));
    ToolRefStateV1 ref{};
    ref.toolKey = g_runtimeBinding.toolKey;
    ctx->dynamicWriteComponent(ctx->host, entity, kToolRefState, &ref,
                               sizeof(ref));
    ctx->relationshipAdd(ctx->host, kEquipsItemRel, actor, entity, 0);
    std::printf("[TOOL] runtime tool entity=%llu equips actor=%llu\n",
                (unsigned long long)entity, (unsigned long long)actor);
}

const MimitaHotPackage::SchemaRegistrar s_attachmentSchema{
    {HOT_ATTACHMENT_COMPONENT, gameHash("AttachmentState.v1"),
     sizeof(HotAttachmentStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "AttachmentState", 1, 0}};
const MimitaHotPackage::SchemaRegistrar s_claimSchema{
    {HOT_TOOL_CLAIM_COMPONENT, gameHash("ToolPresentationClaim.v1"),
     sizeof(HotToolClaimV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ToolPresentationClaim", 1, 0}};
// Runs in post-movement (before the cold render pass) so the owner claim and the
// tool PresentationState/AttachmentState are fresh when the cold WeaponViewModel
// decides whether to yield this same frame (no one-frame double owner). The
// attachment resolver + mesh submission still run later in the RENDER domain.
const MimitaHotPackage::SystemRegistrar s_toolPresentationSystem{
    {gameHash("hot.tool-presentation"), GAME_DOMAIN_POST_MOVEMENT, 1, 0,
     toolPresentationTick, "hot.tool-presentation"}};
const MimitaHotPackage::SystemRegistrar s_attachmentSystem{
    {gameHash("hot.attachment"), GAME_DOMAIN_RENDER, 4, 0, attachmentTick,
     "hot.attachment"}};
const MimitaHotPackage::CommandRegistrar s_hotMeshCommand{
    {"hotmesh", "hotmesh <glb path> - register a runtime-unknown logical mesh", 0,
     hotMeshCommand}};
const MimitaHotPackage::CommandRegistrar s_hotToolCommand{
    {"hottool", "hottool [glb path] - runtime-unknown tool via real equip path",
     0, hotToolCommand}};

} // namespace

#endif
