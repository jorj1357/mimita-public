// 09 15 2026
/* purpose
* Hot generic attachment + runtime tool presentation. A `render.frame` system
* resolves every entity carrying `AttachmentState` through the generic
* `socket.query` capability (parent entity + socket/bone hash + hot local
* offset) and writes the presentation world transform back; `hot.presentation-mesh`
* consumes it and emits `render.mesh`, optionally in VIEW space. Hot code also
* registers arbitrary logical mesh resources through `resource.register`.
* This is presentation-only: gameplay Transform/authority is never overwritten,
* and no weapon/tool enum or per-tool ABI slot exists.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-presentation.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

const std::uint64_t kRightArm = gameHash("rightArm");
const std::uint64_t kRuntimeMesh = gameHash("mesh.runtime.tool");
const char* const kDefaultToolGlb = "assets/objects/weapons/mimita-hafs-v1.glb";

using SocketQueryFn = bool (MIMITA_GAME_CALL *)(void*, GameSocketQueryV1*);
using ResourceRegisterFn = bool (MIMITA_GAME_CALL *)(void*, GameResourceRegisterV1*);

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

// Register a logical mesh resource from hot code (no per-tool kernel code).
std::uint32_t registerMesh(GameplayContextV1* ctx, std::uint64_t logicalId,
                           const char* path)
{
    if (!ctx || !ctx->resolveCapability || !path || !*path)
        return 0;
    auto reg = reinterpret_cast<ResourceRegisterFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_RESOURCE_REGISTER));
    if (!reg)
        return 0;
    GameResourceRegisterV1 req{};
    req.logicalId = logicalId;
    req.kind = GAME_RESOURCE_MESH;
    req.applyNow = 1;
    std::snprintf(req.path, sizeof(req.path), "%s", path);
    reg(ctx->host, &req);
    return req.ok ? req.generation : 0;
}

// Resolve every AttachmentState entity through the generic socket query. The
// resolved transform is stored in the component's out fields; the child's
// authoritative Transform is untouched. A missing parent/socket fails safe.
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

// Create a runtime-unknown tool entity: arbitrary logical mesh + attachment to a
// parent socket. `context` selects world (third-person) or view (first-person).
std::uint64_t spawnTool(GameplayContextV1* ctx, const char* path,
                        std::uint32_t context)
{
    if (!ctx || !ctx->entityCreate || !ctx->writeComponent ||
        !ctx->dynamicWriteComponent || !ctx->resolveCapability)
        return 0;
    GameSharedStateV1* shared = sharedState(ctx);
    const std::uint64_t parent =
        shared ? (shared->localPlayerEntity ? shared->localPlayerEntity
                                            : shared->selectedEntity)
               : 0;
    if (parent == 0)
        return 0;
    registerMesh(ctx, kRuntimeMesh, (path && *path) ? path : kDefaultToolGlb);

    std::uint64_t entity = 0;
    if (!ctx->entityCreate(ctx->host, 0u, &entity) || entity == 0)
        return 0;

    GameTransformComponentV1 tf{};
    ctx->writeComponent(ctx->host, entity, GAME_COMPONENT_TRANSFORM, &tf,
                        sizeof(tf));

    HotPresentationStateV1 present{};
    present.meshResourceId = kRuntimeMesh;
    present.textureResourceId = HOT_TEX_DEFAULT;
    present.scale = 1.0f;
    present.color[0] = present.color[1] = present.color[2] = 1.0f;
    present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_PRESENTATION_COMPONENT,
                               &present, sizeof(present));

    HotAttachmentStateV1 att{};
    att.parentEntity = parent;
    att.socket = kRightArm;
    att.context = context;
    att.flags = HOT_ATTACHMENT_FLAG_VISIBLE;
    if (context == HOT_ATTACHMENT_CONTEXT_VIEW) {
        // Eye-space offset for the first-person presentation context.
        att.localPosition[0] = 0.35f;
        att.localPosition[1] = 0.10f;
        att.localPosition[2] = -0.45f;
    } else {
        // Hand offset for the third-person presentation context.
        att.localPosition[0] = 0.30f;
        att.localPosition[1] = 0.0f;
        att.localPosition[2] = 0.0f;
    }
    att.localRotation[3] = 1.0f;
    att.localScale[0] = att.localScale[1] = att.localScale[2] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, entity, HOT_ATTACHMENT_COMPONENT, &att,
                               sizeof(att));
    return entity;
}

void MIMITA_GAME_CALL hotMeshCommand(void* host, const char* args)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    const char* path = (args && *args) ? args : kDefaultToolGlb;
    const std::uint32_t gen = registerMesh(ctx, kRuntimeMesh, path);
    std::printf("[TOOL] resource.register id=mesh.runtime.tool gen=%u path=%s\n",
                (unsigned)gen, path);
}

void MIMITA_GAME_CALL hotToolCommand(void* host, const char* args)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    const std::uint64_t e = spawnTool(ctx, args, HOT_ATTACHMENT_CONTEXT_WORLD);
    std::printf("[TOOL] third-person tool entity=%llu\n",
                (unsigned long long)e);
}

void MIMITA_GAME_CALL hotToolFirstPersonCommand(void* host, const char* args)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    const std::uint64_t e = spawnTool(ctx, args, HOT_ATTACHMENT_CONTEXT_VIEW);
    std::printf("[TOOL] first-person tool entity=%llu\n",
                (unsigned long long)e);
}

const MimitaHotPackage::SchemaRegistrar s_attachmentSchema{
    {HOT_ATTACHMENT_COMPONENT, gameHash("AttachmentState.v1"),
     sizeof(HotAttachmentStateV1), 8, GAME_COPY_RUNTIME_ONLY, GAME_NET_NONE,
     "AttachmentState", 1, 0}};
// Order 4: runs after pose-generation (2) and before presentation-mesh (5) so
// the resolved transform is ready when the mesh is submitted.
const MimitaHotPackage::SystemRegistrar s_attachmentSystem{
    {gameHash("hot.attachment"), GAME_DOMAIN_RENDER, 4, 0, attachmentTick,
     "hot.attachment"}};
const MimitaHotPackage::CommandRegistrar s_hotMeshCommand{
    {"hotmesh", "hotmesh <glb path> - register a runtime-unknown logical mesh", 0,
     hotMeshCommand}};
const MimitaHotPackage::CommandRegistrar s_hotToolCommand{
    {"hottool", "hottool [glb path] - runtime-unknown third-person attached tool",
     0, hotToolCommand}};
const MimitaHotPackage::CommandRegistrar s_hotTool1pCommand{
    {"hottool1p",
     "hottool1p [glb path] - runtime-unknown first-person (view-space) tool", 0,
     hotToolFirstPersonCommand}};

} // namespace

#endif
