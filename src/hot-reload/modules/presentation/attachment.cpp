// 09 16 2026
/* purpose
* Hot generic attachment + tool presentation. Two render.frame systems:
*  - `hot.tool-presentation` (order 3) consumes the REAL generic equip state
*    (`relationship.equips-item` + the tool entity's `ToolRefState.toolKey`) and
*    writes `PresentationState` + `AttachmentState` onto the REAL tool EntityId.
*    The visual output is a `ToolVisualRecipeV1` (hot C++, selected by the tool
*    key hash); there is no weapon enum or renderer branch. Local possessed actor
*    -> VIEW context, other actors -> WORLD. A recipe is claimed only when it is
*    complete AND its mesh resource actually resolved, so a claim can never
*    suppress the cold fallback renderer for a tool hot cannot fully draw.
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
#include "hot-reload/hot-tool-visual.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const std::uint64_t kRightArm = gameHash("rightArm");
const std::uint64_t kEquipsItemRel = gameHash("relationship.equips-item");
const std::uint64_t kContainsItemRel = gameHash("relationship.contains-item");
const std::uint64_t kToolRefState = gameHash("ToolRefState");

using SocketQueryFn = bool (MIMITA_GAME_CALL *)(void*, GameSocketQueryV1*);
using ResourceRegisterFn = bool (MIMITA_GAME_CALL *)(void*, GameResourceRegisterV1*);

struct ToolRefStateV1 {
    std::uint64_t toolKey;
    std::uint64_t reserved;
};

// Per-recipe mesh readiness. Reset when the hot DLL generation changes (statics
// re-initialize) and retried with a throttle while a resource cannot load yet.
struct ToolResourceState {
    std::uint64_t toolKey = 0;
    std::uint64_t nextAttemptTick = 0;
    bool meshReady = false;
};
ToolResourceState g_resourceState[16];

ToolResourceState& resourceStateFor(std::uint64_t toolKey)
{
    for (ToolResourceState& s : g_resourceState) {
        if (s.toolKey == toolKey)
            return s;
    }
    for (ToolResourceState& s : g_resourceState) {
        if (s.toolKey == 0) {
            s.toolKey = toolKey;
            return s;
        }
    }
    static ToolResourceState fallback;
    fallback.toolKey = toolKey;
    return fallback;
}

// Runtime-unknown tool recipe (used by the `hottool`/`hotmesh` proof; same real
// equip path). Its path is editable at runtime through the `hotmesh` command.
ToolVisualRecipeV1 g_runtimeRecipe{};
bool g_runtimeInit = false;

void ensureRuntimeRecipe()
{
    if (g_runtimeInit)
        return;
    g_runtimeInit = true;
    g_runtimeRecipe = ToolVisualRecipeV1{};
    g_runtimeRecipe.toolKey = gameHash("tool.runtime.unknown");
    g_runtimeRecipe.modelPath = "assets/objects/weapons/mimita-hafs-v1.glb";
    g_runtimeRecipe.meshId = gameHash("mesh.runtime.tool");
    g_runtimeRecipe.textureId = HOT_TEX_DEFAULT;
    g_runtimeRecipe.socket = kRightArm;
    g_runtimeRecipe.viewPosition[0] = 0.35f;
    g_runtimeRecipe.viewPosition[1] = 0.10f;
    g_runtimeRecipe.viewPosition[2] = -0.45f;
    g_runtimeRecipe.worldPosition[0] = 0.30f;
    g_runtimeRecipe.viewRotation[3] = 1.0f;
    g_runtimeRecipe.worldRotation[3] = 1.0f;
    g_runtimeRecipe.viewScale = 1.0f;
    g_runtimeRecipe.worldScale = 1.0f;
    g_runtimeRecipe.flags = 1u;
}

const ToolVisualRecipeV1* findRecipe(std::uint64_t toolKey)
{
    ensureRuntimeRecipe();
    if (toolKey == g_runtimeRecipe.toolKey)
        return &g_runtimeRecipe;
    return findToolVisual(toolKey);
}

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

// Register the recipe's logical mesh and report true once it actually resolves.
// A failed/malformed/missing asset leaves readiness false, so no claim is
// written and the cold renderer keeps owning the tool (fail-safe).
bool ensureRecipeMesh(GameplayContextV1* ctx, const ToolVisualRecipeV1& recipe)
{
    if (!ctx || !ctx->resolveCapability)
        return false;
    if (recipe.meshId == 0 || !recipe.modelPath || !*recipe.modelPath)
        return false;
    ToolResourceState& st = resourceStateFor(recipe.toolKey);
    if (st.meshReady)
        return true;
    if (ctx->tick < st.nextAttemptTick)
        return false;
    st.nextAttemptTick = ctx->tick + 60;   // throttle retries
    auto reg = reinterpret_cast<ResourceRegisterFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RESOURCE_REGISTER));
    if (!reg)
        return false;
    GameResourceRegisterV1 req{};
    req.logicalId = recipe.meshId;
    req.kind = GAME_RESOURCE_MESH;
    req.applyNow = 1;
    std::snprintf(req.path, sizeof(req.path), "%s", recipe.modelPath);
    if (reg(ctx->host, &req) && req.ok && req.generation != 0)
        st.meshReady = true;
    return st.meshReady;
}

void writeToolPresentation(GameplayContextV1* ctx, std::uint64_t toolEntity,
                           const ToolVisualRecipeV1& recipe, std::uint64_t actor,
                           std::uint32_t context)
{
    HotPresentationStateV1 present{};
    present.meshResourceId = recipe.meshId;
    present.textureResourceId = recipe.textureId;
    present.scale = 1.0f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, toolEntity,
                               HOT_PRESENTATION_COMPONENT, &present,
                               sizeof(present));

    HotAttachmentStateV1 att{};
    att.parentEntity = actor;
    att.socket = recipe.socket ? recipe.socket : kRightArm;
    att.context = context;
    att.flags = HOT_ATTACHMENT_FLAG_VISIBLE;
    att.localRotation[3] = 1.0f;
    if (context == HOT_ATTACHMENT_CONTEXT_VIEW) {
        for (int k = 0; k < 3; ++k)
            att.localPosition[k] = recipe.viewPosition[k];
        for (int k = 0; k < 4; ++k)
            att.localRotation[k] = recipe.viewRotation[k];
        const float s = recipe.viewScale > 0.0f ? recipe.viewScale : 1.0f;
        att.localScale[0] = att.localScale[1] = att.localScale[2] = s;
    } else {
        for (int k = 0; k < 3; ++k)
            att.localPosition[k] = recipe.worldPosition[k];
        for (int k = 0; k < 4; ++k)
            att.localRotation[k] = recipe.worldRotation[k];
        const float s = recipe.worldScale > 0.0f ? recipe.worldScale : 1.0f;
        att.localScale[0] = att.localScale[1] = att.localScale[2] = s;
    }
    ctx->dynamicWriteComponent(ctx->host, toolEntity, HOT_ATTACHMENT_COMPONENT,
                               &att, sizeof(att));
}

// Remove a tool entity's presentation so an unequipped tool stops drawing. The
// tool entity persists across a switch (one identity per actor+key), so without
// this a stale model keeps drawing and its attachment keeps resolving.
void clearToolPresentation(GameplayContextV1* ctx, std::uint64_t toolEntity)
{
    if (!ctx->dynamicRemoveComponent || toolEntity == 0)
        return;
    ctx->dynamicRemoveComponent(ctx->host, toolEntity, HOT_PRESENTATION_COMPONENT);
    ctx->dynamicRemoveComponent(ctx->host, toolEntity, HOT_ATTACHMENT_COMPONENT);
}

// Resolve an actor's equipped tools through the REAL generic equip substrate.
void presentActorTools(GameplayContextV1* ctx, std::uint64_t actor,
                       std::uint32_t context, bool claimOnActor)
{
    std::uint64_t tools[4] = {0, 0, 0, 0};
    const std::uint32_t n = ctx->relationshipQuery(
        ctx->host, kEquipsItemRel, actor, tools, nullptr, 4);

    // Hide owned-but-unequipped tools (stale-switch cleanup). The `contains-item`
    // relation lists every owned tool; anything not currently equipped loses its
    // presentation.
    if (ctx->dynamicRemoveComponent) {
        std::uint64_t owned[8] = {0};
        const std::uint32_t ownedCount = ctx->relationshipQuery(
            ctx->host, kContainsItemRel, actor, owned, nullptr, 8);
        for (std::uint32_t i = 0; i < ownedCount; ++i) {
            if (owned[i] == 0)
                continue;
            bool equipped = false;
            for (std::uint32_t j = 0; j < n; ++j) {
                if (tools[j] == owned[i]) {
                    equipped = true;
                    break;
                }
            }
            if (!equipped)
                clearToolPresentation(ctx, owned[i]);
        }
    }

    std::uint64_t claimedKey = 0;
    std::uint64_t claimedEntity = 0;
    std::uint64_t claimedMesh = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (tools[i] == 0)
            continue;
        ToolRefStateV1 ref{};
        if (!ctx->dynamicReadComponent(ctx->host, tools[i], kToolRefState, &ref,
                                       sizeof(ref)))
            continue;
        const ToolVisualRecipeV1* recipe = findRecipe(ref.toolKey);
        if (!recipe)
            continue;   // no complete recipe: cold owns it
        if (!ensureRecipeMesh(ctx, *recipe))
            continue;   // recipe exists but its mesh is not drawable yet: cold owns
        writeToolPresentation(ctx, tools[i], *recipe, actor, context);
        claimedKey = ref.toolKey;
        claimedEntity = tools[i];
        claimedMesh = recipe->meshId;
    }
    if (claimOnActor) {
        HotToolClaimV1 claim{};
        claim.toolKey = claimedKey;
        claim.context = context;
        claim.migrated = claimedKey != 0 ? 1u : 0u;
        claim.toolEntity = claimedEntity;
        claim.meshResourceId = claimedMesh;
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
    ensureRuntimeRecipe();
    if (args && *args)
        g_runtimeRecipe.modelPath = args;
    ToolResourceState& st = resourceStateFor(g_runtimeRecipe.toolKey);
    st.meshReady = false;
    st.nextAttemptTick = 0;
    if (ctx)
        (void)ensureRecipeMesh(ctx, g_runtimeRecipe);
    std::printf("[TOOL] registered runtime mesh '%s'\n", g_runtimeRecipe.modelPath);
}

// The debug command only creates the entity + generic equip state; presentation
// then flows through the same hot.tool-presentation system as a real tool.
void MIMITA_GAME_CALL hotToolCommand(void* host, const char* args)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent || !ctx->relationshipAdd)
        return;
    ensureRuntimeRecipe();
    if (args && *args) {
        g_runtimeRecipe.modelPath = args;
        ToolResourceState& st = resourceStateFor(g_runtimeRecipe.toolKey);
        st.meshReady = false;
        st.nextAttemptTick = 0;
    }

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
    ref.toolKey = g_runtimeRecipe.toolKey;
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
    {HOT_TOOL_CLAIM_COMPONENT, gameHash("ToolPresentationClaim.v2"),
     sizeof(HotToolClaimV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "ToolPresentationClaim", 2, 0}};
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
