// 09 15 2026
/* purpose
* Headless self-test for real ECS Tool Entity resource-version continuity.
*
* It creates a REAL tool entity + actor through the generic capability context
* (entity.create), records the REAL ownership/equip relationships
* (relationship.owns-tool + actorStateEquipTool -> ToolRefState/contains-item/
* equips-item), and writes the generic PresentationState with the production
* logical mesh id HOT_MESH_ROCKET. A logical resource version A is published,
* then B (and later D) only through the canonical content path
* (ContentArtifact -> ArtifactCache -> validation -> publishContentArtifact ->
* PresentationResourceProvider). After every publication the SAME production
* render path (hot.presentation-mesh -> render.mesh -> submitMesh -> handleOf)
* is run and resolved for that exact EntityId.
*
* HEADLESS/SELFTEST EVIDENCE ONLY: this does not observe a rendered frame.
* Does NOT own gameplay, transport, or rendering policy.
*/
#include "hot-reload/tool-entity-continuity-selftest.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/artifact-cache.h"
#include "hot-reload/content-artifact.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generation-verify.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-reload-system.h"
#include "hot-reload/migration-prep.h"
#include "live-code/live-behavior.h"
#include "network/actor-state.h"
#include "project/presentation-resource.h"
#include "render/presentation-render.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::vector<unsigned char> readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
}

std::vector<unsigned char> glb(unsigned char fill, std::size_t extra = 32)
{
    std::vector<unsigned char> v(extra, fill);
    v[0] = 'g'; v[1] = 'l'; v[2] = 'T'; v[3] = 'F';
    v[4] = 2; v[5] = 0; v[6] = 0; v[7] = 0;
    return v;
}

// Headless mesh loader/retire: a debugOnly GpuMesh (no GPU), distinct per apply.
std::vector<void*> g_retired;
bool continuityLoad(void*, void** outHandle)
{
    void* mesh = PresentationRender::debugCreateMesh();
    if (!mesh)
        return false;
    *outHandle = mesh;
    return true;
}
void continuityRetire(void*, void* handle)
{
    g_retired.push_back(handle);
    PresentationRender::debugRetireMesh(handle);
}

const std::uint64_t kOwnsToolRel = gameHash("relationship.owns-tool");
const std::uint64_t kContainsItemRel = gameHash("relationship.contains-item");
const std::uint64_t kEquipsItemRel = gameHash("relationship.equips-item");
const std::uint64_t kToolRefState = gameHash("ToolRefState");
const std::uint64_t kToolContinuityState = gameHash("ToolContinuityState");
const std::uint64_t kToolKey = gameHash("tool.rocket-continuity");

struct ToolRefStateV1 {
    std::uint64_t toolKey;
    std::uint64_t reserved;
};

struct ToolContinuityStateV1 {
    std::uint32_t shotsFired;
    std::uint32_t reserved;
};

// Publish immutable bytes as a candidate of a logical resource through the
// canonical path. Never calls provider.apply directly.
bool publishCanonical(std::uint64_t logicalId, ResourceKind kind,
                      const std::vector<unsigned char>& bytes, std::string& error)
{
    const std::uint64_t hash = hashArtifactBytes(bytes.data(), bytes.size());
    ArtifactCache::instance().store(hash, bytes.data(), bytes.size());
    ResourceRegistry::instance().announceCandidate(
        {logicalId, (std::uint32_t)kind, hash, (std::uint32_t)bytes.size()});
    return publishContentArtifact(logicalId, hash, bytes.data(), bytes.size(),
                                  error);
}

std::uint32_t relationshipCount(GameplayContextV1* ctx, std::uint64_t rel,
                                std::uint64_t source, std::uint64_t* out,
                                std::uint32_t max)
{
    if (!ctx || !ctx->relationshipQuery)
        return 0;
    return ctx->relationshipQuery(ctx->host, rel, source, out, nullptr, max);
}

void runProductionFrame(GenericRuntime& runtime, std::uint64_t tick)
{
    // The real frame order: hot.tool-presentation (post-movement) then the
    // render domain (attachment + hot.presentation-mesh).
    runtime.runDomain(GAME_DOMAIN_POST_MOVEMENT, tick, 0.016f,
                      LiveBehavior::hostContext(tick));
    runtime.runDomain(GAME_DOMAIN_RENDER, tick, 0.016f,
                      LiveBehavior::hostContext(tick));
}

} // namespace

bool runToolEntityContinuitySelfTest(std::string& report)
{
    bool ok = true;

    EntityRegistry::instance().destroyAll();
    DynamicComponentStore::instance().clear();
    RelationshipStore::instance().clear();
    MimitaNet::actorStateEnsureSchemas();

    ArtifactCache::instance().setRoot(
        std::filesystem::temp_directory_path() / "mimita-tool-continuity");
    ResourceRegistry::instance().clear();
    PresentationResourceProvider& provider =
        PresentationResourceProvider::instance();
    provider.clear();
    provider.setLoader(HOT_MESH_ROCKET, &continuityLoad, &continuityRetire,
                       nullptr);

    HotReloadSystem& hrs = HotReloadSystem::instance();
    hrs.startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active (real hot systems run)",
                report);
    if (!runtime.active()) {
        report += "[info] build/mimita-game.dll is required for the production "
                  "render path\n";
        report += "[TOOL ENTITY CONTINUITY] FAIL\n";
        return false;
    }

    MimitaNet::actorStateEnsureSchemas();
    GameplayContextV1* ctx = LiveBehavior::hostContext(1);
    ok &= check(ctx && ctx->entityCreate && ctx->dynamicWriteComponent &&
                    ctx->dynamicReadComponent && ctx->readComponent &&
                    ctx->writeComponent && ctx->relationshipAdd &&
                    ctx->relationshipQuery,
                "generic capability context present", report);
    if (!ctx) {
        report += "[TOOL ENTITY CONTINUITY] FAIL\n";
        return false;
    }

    // Defensive: keep the local-possessed-actor path from touching our actors.
    if (ctx->permanentStorage && ctx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
        auto* shared = reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
        if (shared->magic == GAME_SHARED_MAGIC) {
            shared->localPlayerEntity = 0;
            shared->selectedEntity = 0;
        }
    }

    // Minimal gameplay-state schema (test component) so continuity is visible.
    {
        DynamicComponentSchema s;
        s.typeId = kToolContinuityState;
        s.schemaHash = gameHash("ToolContinuityState.v1");
        s.version = 1;
        s.size = sizeof(ToolContinuityStateV1);
        s.align = 4;
        s.copyPolicy = GAME_COPY_AUTHORING;
        s.networkPolicy = GAME_NET_ALL;
        s.name = "ToolContinuityState";
        DynamicComponentStore::instance().registerSchema(s);
    }

    // ── Real ECS Tool Entity + actor through the generic path ──────────
    std::uint64_t P = 0;
    std::uint64_t E = 0;
    ok &= check(ctx->entityCreate(ctx->host, 0u, &P) && P != 0,
                "actor entity created via entity.create", report);
    ok &= check(ctx->entityCreate(ctx->host, 0u, &E) && E != 0,
                "tool entity created via entity.create", report);
    if (P == 0 || E == 0) {
        report += "[TOOL ENTITY CONTINUITY] FAIL\n";
        return false;
    }

    GameTransformComponentV1 tfP{};
    tfP.position[0] = 1.0f;
    ctx->writeComponent(ctx->host, P, GAME_COMPONENT_TRANSFORM, &tfP, sizeof(tfP));
    GameTransformComponentV1 tfE{};
    tfE.position[0] = 1.2f;
    ctx->writeComponent(ctx->host, E, GAME_COMPONENT_TRANSFORM, &tfE, sizeof(tfE));

    // Real relationships: owns-tool (hot tool path) and the real equip API
    // (writes ToolRefState + contains-item + equips-item).
    const bool ownsAdded =
        ctx->relationshipAdd(ctx->host, kOwnsToolRel, P, E, kToolKey);
    const bool equipped = MimitaNet::actorStateEquipTool(P, E, kToolKey);
    ok &= check(ownsAdded && equipped,
                "owns-tool + real equip relationships established", report);

    ToolRefStateV1 ref{};
    ok &= check(ctx->dynamicReadComponent(ctx->host, E, kToolRefState, &ref,
                                          sizeof(ref)) &&
                    ref.toolKey == kToolKey,
                "ToolRefState carries the runtime tool key", report);

    HotPresentationStateV1 present{};
    present.meshResourceId = HOT_MESH_ROCKET;
    present.textureResourceId = HOT_TEX_DEFAULT;
    present.scale = 1.0f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    ctx->dynamicWriteComponent(ctx->host, E, HOT_PRESENTATION_COMPONENT, &present,
                               sizeof(present));

    ToolContinuityStateV1 play{7u, 0u};
    ctx->dynamicWriteComponent(ctx->host, E, kToolContinuityState, &play,
                               sizeof(play));

    // ── Establish resource A and observe the production resolution ─────
    const std::vector<unsigned char> A = glb(0x30);
    const std::vector<unsigned char> B = glb(0x60);
    const std::uint64_t hashA = hashArtifactBytes(A.data(), A.size());
    const std::uint64_t hashB = hashArtifactBytes(B.data(), B.size());
    ok &= check(hashA != hashB, "GLB A and B are distinct versions", report);

    std::string err;
    ok &= check(publishCanonical(HOT_MESH_ROCKET, ResourceKind::Glb, A, err),
                "resource A published via canonical content path", report);
    void* handleA = provider.handleOf(HOT_MESH_ROCKET);
    runProductionFrame(runtime, 10);
    ok &= check(provider.current(HOT_MESH_ROCKET) != nullptr &&
                    provider.current(HOT_MESH_ROCKET)->contentHash == hashA &&
                    handleA != nullptr &&
                    PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET,
                "production render path resolves A for E", report);

    // ── BEFORE state ───────────────────────────────────────────────────
    std::uint64_t ownsBefore[4] = {0, 0, 0, 0};
    std::uint64_t containsBefore[4] = {0, 0, 0, 0};
    std::uint64_t equipsBefore[4] = {0, 0, 0, 0};
    const std::uint32_t ownsBeforeCount =
        relationshipCount(ctx, kOwnsToolRel, P, ownsBefore, 4);
    const std::uint32_t containsBeforeCount =
        relationshipCount(ctx, kContainsItemRel, P, containsBefore, 4);
    const std::uint32_t equipsBeforeCount =
        relationshipCount(ctx, kEquipsItemRel, P, equipsBefore, 4);
    ToolContinuityStateV1 playBefore{};
    ctx->dynamicReadComponent(ctx->host, E, kToolContinuityState, &playBefore,
                              sizeof(playBefore));
    report += "  [info] REAL ECS TOOL ENTITY tool=" + std::to_string(E) +
              " actor=" + std::to_string(P) + " toolKey=" +
              std::to_string(kToolKey) + " mesh=" +
              std::to_string(HOT_MESH_ROCKET) +
              " owns=" + std::to_string(ownsBeforeCount) +
              " equips=" + std::to_string(equipsBeforeCount) + "\n";
    report += "  [info] BEFORE hashA=" + std::to_string(hashA) + " handleA=" +
              std::to_string(reinterpret_cast<std::uintptr_t>(handleA)) +
              " shotsFired=" + std::to_string(playBefore.shotsFired) + "\n";

    // ── A -> B only through the canonical content path ─────────────────
    const std::size_t retiredBeforeB = g_retired.size();
    ok &= check(publishCanonical(HOT_MESH_ROCKET, ResourceKind::Glb, B, err),
                "resource B published via canonical content path", report);
    void* handleB = provider.handleOf(HOT_MESH_ROCKET);
    ok &= check(provider.current(HOT_MESH_ROCKET) != nullptr &&
                    provider.current(HOT_MESH_ROCKET)->contentHash == hashB &&
                    handleB != nullptr && handleB != handleA,
                "provider current == B and handle B != handle A", report);

    // The SAME production render path must now resolve B for the SAME entity.
    runProductionFrame(runtime, 11);
    ok &= check(PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET &&
                    provider.handleOf(HOT_MESH_ROCKET) == handleB,
                "production render path resolves B for the SAME EntityId E",
                report);

    // ── Entity continuity (no recreation / respawn / re-equip) ─────────
    {
        std::uint64_t ownsAfter[4] = {0, 0, 0, 0};
        std::uint64_t containsAfter[4] = {0, 0, 0, 0};
        std::uint64_t equipsAfter[4] = {0, 0, 0, 0};
        const std::uint32_t ownsAfterCount =
            relationshipCount(ctx, kOwnsToolRel, P, ownsAfter, 4);
        const std::uint32_t containsAfterCount =
            relationshipCount(ctx, kContainsItemRel, P, containsAfter, 4);
        const std::uint32_t equipsAfterCount =
            relationshipCount(ctx, kEquipsItemRel, P, equipsAfter, 4);
        ToolRefStateV1 refAfter{};
        ctx->dynamicReadComponent(ctx->host, E, kToolRefState, &refAfter,
                                  sizeof(refAfter));
        HotPresentationStateV1 meshAfter{};
        ctx->dynamicReadComponent(ctx->host, E, HOT_PRESENTATION_COMPONENT,
                                  &meshAfter, sizeof(meshAfter));
        ToolContinuityStateV1 playAfter{};
        ctx->dynamicReadComponent(ctx->host, E, kToolContinuityState, &playAfter,
                                  sizeof(playAfter));
        std::uint64_t equippedTool = 0;
        std::uint64_t equippedKey = 0;
        MimitaNet::actorStateGetEquippedTool(P, &equippedTool, &equippedKey);

        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(E)) &&
                        EntityRegistry::instance().alive(static_cast<EntityId>(P)),
                    "Tool EntityId E and actor P still alive (no recreation)",
                    report);
        ok &= check(ownsAfterCount == 1 && ownsAfter[0] == E,
                    "owns-tool relationship unchanged", report);
        ok &= check(containsAfterCount == 1 && containsAfter[0] == E,
                    "contains-item relationship unchanged", report);
        ok &= check(equipsAfterCount == 1 && equipsAfter[0] == E &&
                        equippedTool == E && equippedKey == kToolKey,
                    "equip relationship unchanged (same E, no re-equip)", report);
        ok &= check(refAfter.toolKey == kToolKey, "ToolRefState tool key unchanged",
                    report);
        ok &= check(meshAfter.meshResourceId == HOT_MESH_ROCKET,
                    "PresentationState.meshResourceId logical id unchanged", report);
        ok &= check(playAfter.shotsFired == playBefore.shotsFired,
                    "gameplay state value unchanged", report);
    }

    // ── Malformed C: rejected, last-good B, entity graph unchanged ─────
    {
        std::vector<unsigned char> bad(32, 0x11);  // not a GLB
        const std::uint64_t hashBad = hashArtifactBytes(bad.data(), bad.size());
        ArtifactCache::instance().store(hashBad, bad.data(), bad.size());
        ResourceRegistry::instance().announceCandidate(
            {HOT_MESH_ROCKET, (std::uint32_t)ResourceKind::Glb, hashBad,
             (std::uint32_t)bad.size()});
        const bool rejected = !publishContentArtifactFromCache(HOT_MESH_ROCKET, err);
        runProductionFrame(runtime, 12);
        std::uint64_t equipsC[4] = {0, 0, 0, 0};
        const std::uint32_t equipsCCount =
            relationshipCount(ctx, kEquipsItemRel, P, equipsC, 4);
        ToolContinuityStateV1 playC{};
        ctx->dynamicReadComponent(ctx->host, E, kToolContinuityState, &playC,
                                  sizeof(playC));
        ok &= check(rejected &&
                        provider.current(HOT_MESH_ROCKET)->contentHash == hashB &&
                        provider.handleOf(HOT_MESH_ROCKET) == handleB,
                    "malformed C rejected; provider still B", report);
        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(E)) &&
                        equipsCCount == 1 && equipsC[0] == E &&
                        playC.shotsFired == playBefore.shotsFired,
                    "malformed C leaves E/equip/gameplay state unchanged", report);
        ok &= check(PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET &&
                        provider.handleOf(HOT_MESH_ROCKET) == handleB,
                    "render path still resolves B after malformed C", report);
    }

    // ── Runtime preparation failure: publication fails, B stays ────────
    {
        struct Failing { static bool load(void*, void**) { return false; } };
        provider.setLoader(HOT_MESH_ROCKET, &Failing::load, &continuityRetire,
                           nullptr);
        const std::vector<unsigned char> C = glb(0x90);
        const std::uint64_t hashC = hashArtifactBytes(C.data(), C.size());
        ArtifactCache::instance().store(hashC, C.data(), C.size());
        ResourceRegistry::instance().announceCandidate(
            {HOT_MESH_ROCKET, (std::uint32_t)ResourceKind::Glb, hashC,
             (std::uint32_t)C.size()});
        const bool prepFailed = !publishContentArtifactFromCache(HOT_MESH_ROCKET, err);
        provider.setLoader(HOT_MESH_ROCKET, &continuityLoad, &continuityRetire,
                           nullptr);
        runProductionFrame(runtime, 13);
        ok &= check(prepFailed &&
                        provider.current(HOT_MESH_ROCKET)->contentHash == hashB &&
                        provider.handleOf(HOT_MESH_ROCKET) == handleB,
                    "runtime preparation failure keeps last-good B", report);
        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(E)) &&
                        PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET,
                    "prep failure leaves the entity graph intact", report);
    }

    // ── Retirement safety ──────────────────────────────────────────────
    // The provider retires the previous handle synchronously at the committed
    // swap boundary. The production render path stores only the LOGICAL id per
    // entity (g_entityMeshId) and resolves the handle at use, so no raw handle
    // can outlive the swap in the single-threaded frame model. No explicit GPU
    // fence exists for a hypothetical future multi-threaded renderer.
    {
        const bool aRetired = g_retired.size() > retiredBeforeB &&
                              g_retired[retiredBeforeB] == handleA;
        ok &= check(aRetired,
                    "old handle A retired exactly at the A -> B swap",
                    report);
        ok &= check(PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET,
                    "render path keeps a logical id, never the retired handle",
                    report);
    }

    // ── F -> G with B active (real production hot-reload transaction) ───
    std::uint32_t F = hrs.status().activeGeneration;
    std::uint32_t G = 0;
    bool fToG = false;
    {
        const std::vector<unsigned char> dllBytes = readFile("build/mimita-game.dll");
        if (dllBytes.empty()) {
            report += "  [info] build/mimita-game.dll missing; F->G skipped\n";
        } else {
            G = F + 1;
            const std::uint64_t dllHash =
                hashArtifactBytes(dllBytes.data(), dllBytes.size());
            GenerationManifestV1 manifest{};
            manifest.logicalGenerationId = G;
            manifest.platformArtifactHash = dllHash;
            manifest.platformArtifactSize = (std::uint32_t)dllBytes.size();
            manifest.hotAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
            GenerationLocalFactsV1 facts{};
            facts.artifactHash = dllHash;
            facts.artifactSize = (std::uint32_t)dllBytes.size();
            facts.coldAbiVersion = (std::uint32_t)MIMITA_GAME_API_VERSION;
            ok &= check(verifyGeneration(manifest, facts) == VerifyFailure::None,
                        "F->G candidate manifest verifies", report);

            MigrationPrepareFactsV1 mf{};
            mf.candidateSchemaCount = manifest.requiredSchemaCount;
            for (std::uint32_t i = 0; i < manifest.requiredSchemaCount; ++i) {
                mf.candidateSchemaIds[i] = manifest.requiredSchemas[i];
                mf.candidateSchemaVersions[i] = manifest.requiredSchemaVersions[i];
            }
            mf.storedSchemaVersion = [](void*, std::uint64_t id) {
                return DynamicComponentStore::instance().maxStoredVersion(id);
            };
            mf.hasMigrationPath = [](void*, std::uint64_t id, std::uint32_t from,
                                     std::uint32_t to) {
                return DynamicComponentStore::instance().hasMigration(id, from, to);
            };
            const MigrationPlanV1 plan = prepareMigration(F, G, mf);
            hrs.setCandidateMigrationPlan(plan);

            std::string gerr;
            const bool installed =
                hrs.installCandidateArtifact(dllBytes, G, dllHash, gerr) &&
                hrs.hasInstalledCandidate() &&
                hrs.installedCandidateGeneration() == G;
            ok &= check(installed, "F->G artifact installed as real candidate",
                        report);

            hrs.requestSwitchAtTick(1000);
            for (std::uint32_t t = 1000; t < 1010 && !fToG; ++t)
                fToG = hrs.pollAndAdvance(t);
            ok &= check(fToG && hrs.status().activeGeneration == G &&
                            hrs.lastSwitchRejection() == SwitchRejection::None,
                        "F -> G activated through the real switch transaction",
                        report);
        }
    }

    if (fToG) {
        runProductionFrame(runtime, 100);
        std::uint64_t equipsG[4] = {0, 0, 0, 0};
        const std::uint32_t equipsGCount =
            relationshipCount(ctx, kEquipsItemRel, P, equipsG, 4);
        ToolContinuityStateV1 playG{};
        ctx->dynamicReadComponent(ctx->host, E, kToolContinuityState, &playG,
                                  sizeof(playG));
        HotPresentationStateV1 meshG{};
        ctx->dynamicReadComponent(ctx->host, E, HOT_PRESENTATION_COMPONENT, &meshG,
                                  sizeof(meshG));
        std::uint64_t equippedG = 0, keyG = 0;
        MimitaNet::actorStateGetEquippedTool(P, &equippedG, &keyG);
        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(E)) &&
                        EntityRegistry::instance().alive(static_cast<EntityId>(P)),
                    "F->G: E and P EntityIds unchanged", report);
        ok &= check(equipsGCount == 1 && equipsG[0] == E && equippedG == E &&
                        keyG == kToolKey,
                    "F->G: equip relationship unchanged", report);
        ok &= check(playG.shotsFired == playBefore.shotsFired &&
                        meshG.meshResourceId == HOT_MESH_ROCKET,
                    "F->G: gameplay state and logical mesh id unchanged", report);
        ok &= check(provider.current(HOT_MESH_ROCKET)->contentHash == hashB &&
                        PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET &&
                        provider.handleOf(HOT_MESH_ROCKET) == handleB,
                    "F->G: B preserved and render path still resolves B", report);

        // ── D after G on the same logical id / same entity ─────────────
        const std::vector<unsigned char> D = glb(0xC0);
        const std::uint64_t hashD = hashArtifactBytes(D.data(), D.size());
        ok &= check(publishCanonical(HOT_MESH_ROCKET, ResourceKind::Glb, D, err),
                    "D published after G via canonical content path", report);
        void* handleD = provider.handleOf(HOT_MESH_ROCKET);
        runProductionFrame(runtime, 101);
        std::uint64_t equipsD[4] = {0, 0, 0, 0};
        const std::uint32_t equipsDCount =
            relationshipCount(ctx, kEquipsItemRel, P, equipsD, 4);
        ok &= check(equipsDCount == 1 && equipsD[0] == E &&
                        provider.current(HOT_MESH_ROCKET)->contentHash == hashD &&
                        provider.handleOf(HOT_MESH_ROCKET) == handleD &&
                        PresentationRender::entityMeshResourceId(E) == HOT_MESH_ROCKET,
                    "D after G: same E/equip, B -> D works", report);
    } else {
        report += "  [info] F->G not run (D-after-G skipped)\n";
    }

    // ── Unknown logical resource (never known at EXE build time) ───────
    {
        const std::uint64_t unknownId = resourceIdFromLogicalName("mesh.user.test-object");
        provider.setLoader(unknownId, &continuityLoad, &continuityRetire, nullptr);
        const std::vector<unsigned char> U = glb(0xAB);
        std::string uerr;
        const bool published =
            publishCanonical(unknownId, ResourceKind::Glb, U, uerr);
        std::uint64_t E2 = 0;
        ctx->entityCreate(ctx->host, 0u, &E2);
        if (E2 != 0) {
            GameTransformComponentV1 tf2{};
            ctx->writeComponent(ctx->host, E2, GAME_COMPONENT_TRANSFORM, &tf2,
                                sizeof(tf2));
            HotPresentationStateV1 up{};
            up.meshResourceId = unknownId;
            up.textureResourceId = HOT_TEX_DEFAULT;
            up.scale = 1.0f;
            up.color[0] = up.color[1] = up.color[2] = up.color[3] = 1.0f;
            ctx->dynamicWriteComponent(ctx->host, E2, HOT_PRESENTATION_COMPONENT, &up,
                                       sizeof(up));
        }
        runProductionFrame(runtime, 110);
        ok &= check(published && E2 != 0 &&
                        provider.handleOf(unknownId) != nullptr &&
                        PresentationRender::entityMeshResourceId(E2) == unknownId,
                    "runtime-unknown logical resource resolves in the render path",
                    report);
    }

    // ── Unresolved resource fallback ───────────────────────────────────
    {
        const std::uint64_t missingId = resourceIdFromLogicalName("mesh.does.not.exist");
        std::uint64_t E3 = 0;
        ctx->entityCreate(ctx->host, 0u, &E3);
        if (E3 != 0) {
            GameTransformComponentV1 tf3{};
            ctx->writeComponent(ctx->host, E3, GAME_COMPONENT_TRANSFORM, &tf3,
                                sizeof(tf3));
            HotPresentationStateV1 mp{};
            mp.meshResourceId = missingId;
            mp.textureResourceId = HOT_TEX_DEFAULT;
            mp.scale = 1.0f;
            mp.color[0] = mp.color[1] = mp.color[2] = mp.color[3] = 1.0f;
            ctx->dynamicWriteComponent(ctx->host, E3, HOT_PRESENTATION_COMPONENT, &mp,
                                       sizeof(mp));
        }
        const std::uint64_t submissionsBefore =
            PresentationRender::submittedMeshCount();
        runProductionFrame(runtime, 111);
        ok &= check(E3 != 0 &&
                        provider.handleOf(missingId) == nullptr &&
                        PresentationRender::entityMeshResourceId(E3) == missingId &&
                        PresentationRender::submittedMeshCount() > submissionsBefore &&
                        EntityRegistry::instance().alive(static_cast<EntityId>(E3)),
                    "unresolved resource is a safe skipped draw (no crash)",
                    report);
    }

    report += "  [info] F=" + std::to_string(F) + " G=" + std::to_string(G) +
              " providerHash=" + std::to_string(hashB) + "\n";
    return ok;
}
