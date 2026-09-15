// 09 14 2026
/* purpose
* Implements the headless hot-combat policy self-test.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#include "network/hot-combat-selftest.h"

#include <cstdio>
#include <string>

#include <unordered_map>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/prediction-registry.h"
#include "ecs/components.h"
#include "hot-reload/game-api.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-pose.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-reload-system.h"
#include "debug/debug-visuals.h"
#include "live-code/live-behavior.h"
#include "live-code/live-ui.h"
#include "hot-reload/hot-ui.h"
#include "project/presentation-resource.h"
#include "render/presentation-entities.h"
#include "render/presentation-render.h"
#include "render/skeleton-instances.h"
#include "network/server-context.h"
#include "network/server-gamemode.h"
#include "network/server-weapon-state.h"

using namespace MimitaNet;

using namespace MimitaRuntime;
using namespace MimitaNet;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

const std::uint64_t kBananaTool = gameHash("banana.launcher");
const std::uint64_t kBananaProjectile = gameHash("banana.projectile");
const std::uint64_t kBananaState = gameHash("BananaLauncherState");

struct BananaLauncherState {
    std::int32_t shotsFired;
    float power;
    std::uint64_t toolEntity;
};

ToolUsePolicyV1 makeUse(std::uint64_t toolId)
{
    ToolUsePolicyV1 use{};
    use.ownerId = 7;
    use.toolId = toolId;
    use.toolNetworkId = static_cast<std::uint32_t>(toolId);
    use.kind = 0;
    use.baseFire = 1;
    use.outFire = 1;
    use.ammoCost = 1;
    use.tick = 1;
    return use;
}

// Generic presentation-resource provider test hooks (no GPU involved).
bool g_resourceFailNext = false;
int g_resourceLoads = 0;
std::uint64_t g_resourceNextHandle = 1000;

bool resourceTestLoad(void*, void** out)
{
    if (g_resourceFailNext)
        return false;
    *out = reinterpret_cast<void*>(static_cast<std::uintptr_t>(++g_resourceNextHandle));
    ++g_resourceLoads;
    return true;
}

void resourceTestRetire(void*, void*) {}

ProjectileImpactPolicyV1 makeImpact(std::uint64_t typeId)
{
    ProjectileImpactPolicyV1 impact{};
    impact.projectileTypeId = typeId;
    impact.ownerId = 7;
    impact.weaponNetworkId = static_cast<std::uint32_t>(typeId);
    impact.hitKind = 1;  // world
    return impact;
}

} // namespace

bool runHotCombatSelfTest(std::string& report)
{
    bool ok = true;

    EntityRegistry::instance().destroyAll();
    DynamicComponentStore::instance().clear();

    HotReloadSystem::instance().startup();
    GenericRuntime& runtime = GenericRuntime::instance();
    ok &= check(runtime.active(), "hot package active", report);

    // ── Generic hot presentation (render.debug capability + hot system) ──
    // An entity unknown to the EXE (only Transform + a package dynamic
    // component) is presented by a hot render.frame system through the generic
    // render.debug capability. No Player/NPC/Projectile type switch.
    {
        struct PresentationDebugV1 {
            float scale;
            float color[4];
            std::uint32_t shape;
            std::uint32_t reserved;
        };
        DebugVis::setMasterEnabled(true);
        GameplayContextV1* pctx = LiveBehavior::hostContext(1);
        auto renderDebug = pctx
            ? reinterpret_cast<GameRenderDebugFn>(pctx->resolveCapability(
                  pctx->host, GAME_CAP_RENDER_DEBUG))
            : nullptr;
        ok &= check(renderDebug != nullptr,
                    "render.debug capability resolves", report);

        const std::size_t beforeDirect = debugLineVertexCount();
        if (renderDebug) {
            GameRenderDebugCommandV1 cmd{};
            cmd.shape = GAME_RENDER_DEBUG_LINE;
            cmd.b[2] = 1.0f;
            cmd.color[0] = cmd.color[1] = cmd.color[2] = cmd.color[3] = 1.0f;
            renderDebug(pctx->host, &cmd);
        }
        ok &= check(debugLineVertexCount() >= beforeDirect + 2,
                    "hot render.debug command reaches the kernel draw buffer",
                    report);

        std::uint64_t entity = 0;
        const std::uint64_t presentType = gameHash("PresentationDebug");
        if (pctx && pctx->entityCreate && pctx->writeComponent &&
            pctx->dynamicWriteComponent) {
            pctx->entityCreate(pctx->host, 0u, &entity);
            if (entity != 0) {
                GameTransformComponentV1 tf{};
                tf.position[0] = 3.0f;
                tf.position[1] = 1.0f;
                tf.position[2] = 4.0f;
                pctx->writeComponent(pctx->host, entity, GAME_COMPONENT_TRANSFORM,
                                     &tf, sizeof(tf));
                PresentationDebugV1 pd{};
                pd.scale = 0.5f;
                pd.color[0] = pd.color[1] = pd.color[2] = pd.color[3] = 1.0f;
                pd.shape = GAME_RENDER_DEBUG_WIRE_SPHERE;
                pctx->dynamicWriteComponent(pctx->host, entity, presentType, &pd,
                                            sizeof(pd));
            }
        }
        ok &= check(entity != 0, "generic-presented entity created", report);
        const std::size_t beforeHot = debugLineVertexCount();
        runtime.runDomain(GAME_DOMAIN_RENDER, 2, 0.016f,
                          LiveBehavior::hostContext(2));
        ok &= check(debugLineVertexCount() > beforeHot,
                    "hot render.frame system presents a generic entity", report);
        DebugVis::setMasterEnabled(false);

        // Generic mesh presentation: a typeless entity with Transform +
        // PresentationState is presented via the generic render.mesh capability.
        auto renderMesh = pctx
            ? reinterpret_cast<GameRenderMeshFn>(pctx->resolveCapability(
                  pctx->host, GAME_CAP_RENDER_MESH))
            : nullptr;
        ok &= check(renderMesh != nullptr, "render.mesh capability resolves",
                    report);
        struct PresentationStateV1 {
            std::uint64_t meshResourceId;
            std::uint64_t textureResourceId;
            std::uint32_t flags;
            float scale;
            float color[4];
        };
        const std::uint64_t stateType = gameHash("PresentationState");
        std::uint64_t meshEntity = 0;
        if (pctx && pctx->entityCreate) {
            pctx->entityCreate(pctx->host, 0u, &meshEntity);
            if (meshEntity != 0) {
                GameTransformComponentV1 tf{};
                tf.position[0] = 5.0f;
                tf.position[1] = 1.0f;
                tf.position[2] = 5.0f;
                pctx->writeComponent(pctx->host, meshEntity,
                                     GAME_COMPONENT_TRANSFORM, &tf, sizeof(tf));
                // Actor-like generic entity: Transform + Health + team color,
                // no Player/NPC/Monster type.
                GameHealthComponentV1 hp{};
                hp.current = 80;
                hp.max = 100;
                pctx->writeComponent(pctx->host, meshEntity, GAME_COMPONENT_HEALTH,
                                     &hp, sizeof(hp));
                PresentationStateV1 ps{};
                ps.meshResourceId = gameHash("mesh.cube");
                ps.textureResourceId = gameHash("texture.default");
                ps.scale = 1.0f;
                ps.color[0] = 1.0f; ps.color[1] = 0.2f; ps.color[2] = 0.2f;
                ps.color[3] = 1.0f;  // team/actor color is presentation data
                pctx->dynamicWriteComponent(pctx->host, meshEntity, stateType,
                                            &ps, sizeof(ps));
            }
        }
        const std::uint64_t meshesBefore = PresentationRender::submittedMeshCount();
        runtime.runDomain(GAME_DOMAIN_RENDER, 3, 0.016f,
                          LiveBehavior::hostContext(3));
        ok &= check(PresentationRender::submittedMeshCount() > meshesBefore,
                    "typeless actor-like entity presents via render.mesh "
                    "(team color as data)",
                    report);

        // Authoritative replicated projectile: generic replication delivers only
        // HotProjectileState + PresentationState (NO typed Transform). The
        // client projects the state onto the REAL replicated EntityId, which the
        // hot system then presents. No parallel bridge entity.
        std::uint64_t authProjectile = 0;
        if (pctx && pctx->entityCreate) {
            pctx->entityCreate(pctx->host, 0u, &authProjectile);
            if (authProjectile != 0) {
                HotProjectileStateV1 st{};
                st.velocity[0] = 5.0f;
                st.lifetime = 2.0f;
                DynamicComponentStore::instance().write(
                    authProjectile, HOT_PROJECTILE_COMPONENT, &st, sizeof(st));
                PresentationStateV1 aps{};
                aps.meshResourceId = gameHash("mesh.rocket");
                aps.textureResourceId = gameHash("texture.rocket");
                aps.scale = 1.0f;
                aps.color[0] = aps.color[1] = aps.color[2] = aps.color[3] = 1.0f;
                DynamicComponentStore::instance().write(
                    authProjectile, stateType, &aps, sizeof(aps));
            }
        }
        PresentationEntities::projectReplicatedProjectiles();
        GameTransformComponentV1 authTf{};
        ok &= check(authProjectile != 0 &&
                        pctx->readComponent(pctx->host, authProjectile,
                                            GAME_COMPONENT_TRANSFORM, &authTf,
                                            sizeof(authTf)),
                    "replicated projectile projects onto the authoritative entity",
                    report);
        const std::uint64_t authMeshBefore =
            PresentationRender::submittedMeshCount();
        runtime.runDomain(GAME_DOMAIN_RENDER, 5, 0.016f,
                          LiveBehavior::hostContext(5));
        ok &= check(PresentationRender::submittedMeshCount() > authMeshBefore,
                    "authoritative replicated projectile presents via its own "
                    "EntityId",
                    report);
    }

    // ── Generic predicted -> authoritative entity association ─────────
    // Reusable beyond projectiles: exercised here with a non-projectile
    // (WorldObject domain) entity.
    {
        using Ecs::PredictionRegistry;
        PredictionRegistry& pr = PredictionRegistry::instance();
        pr.clear();
        // Prediction first: a provisional non-projectile entity is canonical.
        const EntityId prov = Ecs::ensure(EntityRealm::ClientPredicted,
                                          EntityDomain::WorldObject, 9001);
        ok &= check(pr.registerProvisional(7001, prov, 100) == prov &&
                        pr.statusOf(7001) == Ecs::PREDICTION_PENDING,
                    "prediction first: provisional entity is canonical", report);
        const EntityId auth = Ecs::ensure(EntityRealm::Server,
                                          EntityDomain::WorldObject, 9002);
        ok &= check(pr.associate(7001, auth, 101) == auth &&
                        pr.authoritativeOf(7001) == auth,
                    "authority associates and becomes canonical", report);
        ok &= check(!EntityRegistry::instance().alive(prov),
                    "provisional entity retires on association", report);
        ok &= check(pr.canonical(7001) == auth &&
                        pr.statusOf(7001) == Ecs::PREDICTION_ASSOCIATED,
                    "exactly one canonical entity after handoff", report);

        // Authority first: a late provisional retires immediately.
        const EntityId auth2 = Ecs::ensure(EntityRealm::Server,
                                           EntityDomain::WorldObject, 9003);
        pr.associate(7002, auth2, 100);
        const EntityId prov2 = Ecs::ensure(EntityRealm::ClientPredicted,
                                           EntityDomain::WorldObject, 9004);
        ok &= check(pr.registerProvisional(7002, prov2, 100) == auth2 &&
                        !EntityRegistry::instance().alive(prov2),
                    "authority first: late provisional retires", report);

        // Destroyed authority does not dangle or resurrect.
        const EntityId prov3 = Ecs::ensure(EntityRealm::ClientPredicted,
                                           EntityDomain::WorldObject, 9005);
        pr.registerProvisional(7003, prov3, 100);
        const EntityId auth3 = Ecs::ensure(EntityRealm::Server,
                                           EntityDomain::WorldObject, 9006);
        pr.associate(7003, auth3, 101);
        EntityRegistry::instance().destroy(auth3);
        pr.onEntityDestroyed(auth3);
        ok &= check(pr.statusOf(7003) != Ecs::PREDICTION_ASSOCIATED &&
                        pr.canonical(7003) != auth3,
                    "destroyed authority does not dangle", report);

        // Stale prediction retires.
        const EntityId prov4 = Ecs::ensure(EntityRealm::ClientPredicted,
                                           EntityDomain::WorldObject, 9007);
        pr.registerProvisional(7004, prov4, 10);
        pr.pruneOlderThan(50);
        ok &= check(!EntityRegistry::instance().alive(prov4) &&
                        pr.canonical(7004) == 0,
                    "stale prediction retires", report);
        pr.clear();
    }

    // ── Client prediction-link association (generic) ──────────────────
    {
        std::uint64_t pm = 0, pt = 0;
        float psc = 1.0f;
        PresentationEntities::resourcesForWeapon(
            MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER, pm, pt, psc);
        PresentationEntities::beginSync();
        const float pp[3] = {0.0f, 0.0f, 0.0f};
        const float pv[3] = {1.0f, 0.0f, 0.0f};
        const std::uint64_t provEnt = PresentationEntities::ensurePredicted(
            8801u, 5501u, pp, pv, pm, pt, psc, 100);
        ok &= check(provEnt != 0 &&
                        Ecs::PredictionRegistry::instance().statusOf(8801u) ==
                            Ecs::PREDICTION_PENDING,
                    "predicted entity registered as provisional", report);
        const EntityId authEnt =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Projectile, 5502);
        HotPredictionLinkV1 link{};
        link.predictionKey = 8801u;
        DynamicComponentStore::instance().write(
            authEnt, HOT_PREDICTION_LINK_COMPONENT, &link, sizeof(link));
        PresentationEntities::associateByLink(101);
        ok &= check(Ecs::PredictionRegistry::instance().authoritativeOf(8801u) ==
                            authEnt &&
                        !EntityRegistry::instance().alive(provEnt),
                    "client link associates authority and retires provisional",
                    report);
        PresentationEntities::endSync();
        PresentationEntities::clear();

        // ── Real NPC presentation bridge ─────────────────────────────
        // A client NPC replica projects into a generic presentation entity
        // (Transform + PresentationState) with team color as data.
        const float apos[3] = {1.0f, 1.0f, 1.0f};
        const float alook[3] = {1.0f, 0.0f, 0.0f};
        const float acolor[4] = {1.0f, 0.25f, 0.25f, 1.0f};
        PresentationEntities::beginActorSync();
        const std::uint64_t actor = PresentationEntities::ensureActor(
            7001u, apos, alook, gameHash("mesh.actor"), gameHash("texture.default"),
            1.0f, acolor);
        const bool actorHasTransform =
            actor != 0 &&
            EntityRegistry::instance().tryGet<TransformComponent>(
                static_cast<EntityId>(actor)) != nullptr;
        const bool actorHasPresent =
            actor != 0 && DynamicComponentStore::instance().has(
                              static_cast<EntityId>(actor),
                              gameHash("PresentationState"));
        ok &= check(actorHasTransform && actorHasPresent,
                    "real NPC replica projects to a generic presentation entity",
                    report);
        PresentationEntities::endActorSync();
        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(actor)),
                    "live actor presentation persists across a frame", report);
        PresentationEntities::beginActorSync();
        PresentationEntities::endActorSync();
        ok &= check(!EntityRegistry::instance().alive(static_cast<EntityId>(actor)),
                    "untouched actor presentation retires with the entity", report);
    }

    // ── Generic hot animation policy ──────────────────────────────────
    {
        const EntityId actor = Ecs::ensure(EntityRealm::ClientReplicated,
                                           EntityDomain::Npc, 8101);
        Ecs::setTransform(actor, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.0f,
                          0.0f);
        Ecs::setVelocity(actor, glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));
        HotAnimationStateV1 anim{};
        anim.clipId = HOT_ANIM_IDLE;
        anim.playbackRate = 1.0f;
        DynamicComponentStore::instance().write(actor, HOT_ANIMATION_STATE_COMPONENT,
                                                 &anim, sizeof(anim));
        runtime.runDomain(GAME_DOMAIN_RENDER, 6, 0.016f,
                          LiveBehavior::hostContext(6));
        DynamicComponentStore::instance().read(actor, HOT_ANIMATION_STATE_COMPONENT,
                                                &anim, sizeof(anim));
        ok &= check(anim.clipId == HOT_ANIM_MOVE,
                    "hot animation policy selects move for a moving actor", report);

        Ecs::setVelocity(actor, glm::vec3(0.0f), glm::vec3(0.0f));
        runtime.runDomain(GAME_DOMAIN_RENDER, 7, 0.016f,
                          LiveBehavior::hostContext(7));
        DynamicComponentStore::instance().read(actor, HOT_ANIMATION_STATE_COMPONENT,
                                                &anim, sizeof(anim));
        ok &= check(anim.clipId == HOT_ANIM_IDLE,
                    "hot animation policy selects idle for a still actor", report);

        HealthComponent hp{};
        hp.current = 0;
        hp.max = 100;
        hp.dead = true;
        EntityRegistry::instance().add<HealthComponent>(actor, hp);
        runtime.runDomain(GAME_DOMAIN_RENDER, 8, 0.016f,
                          LiveBehavior::hostContext(8));
        DynamicComponentStore::instance().read(actor, HOT_ANIMATION_STATE_COMPONENT,
                                                &anim, sizeof(anim));
        ok &= check(anim.clipId == HOT_ANIM_DEAD,
                    "hot animation policy selects death for a dead actor", report);
        EntityRegistry::instance().destroy(actor);

        // Hot pose generation publishes generic PoseState via skeleton.apply.
        const EntityId mover = Ecs::ensure(EntityRealm::ClientReplicated,
                                           EntityDomain::Npc, 8201);
        Ecs::setTransform(mover, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.0f,
                          0.0f);
        Ecs::setVelocity(mover, glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f));
        HotAnimationStateV1 moverAnim{};
        moverAnim.clipId = HOT_ANIM_MOVE;
        moverAnim.playbackRate = 1.0f;
        DynamicComponentStore::instance().write(
            mover, HOT_ANIMATION_STATE_COMPONENT, &moverAnim, sizeof(moverAnim));
        const std::uint64_t appliesBefore = LiveBehavior::skeletonApplyCount();
        runtime.runDomain(GAME_DOMAIN_RENDER, 9, 0.016f,
                          LiveBehavior::hostContext(9));
        ok &= check(LiveBehavior::skeletonApplyCount() > appliesBefore,
                    "hot pose generation invokes skeleton.apply", report);
        HotPoseStateV1 pose{};
        ok &= check(DynamicComponentStore::instance().read(
                        mover, HOT_POSE_STATE_COMPONENT, &pose, sizeof(pose)) &&
                        pose.count > 0,
                    "hot pose publishes a generic PoseState on the entity", report);
        // The same pose drives the entity's cold skeleton instance (keyed by
        // EntityId, not by Player/Npc identity).
        SkeletonInstances::Instance* inst = SkeletonInstances::get(mover);
        ok &= check(inst != nullptr && inst->version > 0 && inst->boneCount > 0,
                    "skeleton.apply drives the per-entity skeleton instance",
                    report);
        EntityRegistry::instance().destroy(mover);
        SkeletonInstances::purgeDead();
        ok &= check(SkeletonInstances::get(mover) == nullptr,
                    "destroyed entity skeleton instance is purged", report);

        // Generic render.mesh consumes SkeletonInstances by EntityId, with a
        // static fallback when no instance exists.
        const std::uint64_t boneHashes[2] = {gameHash("leftArm"),
                                             gameHash("rightArm")};
        const std::uint64_t partMesh = gameHash("selftest.part-mesh");
        ok &= check(PresentationRender::debugInstallPartMesh(partMesh, boneHashes, 2,
                                                             nullptr),
                    "part mesh installed for skinned consumption test", report);
        const EntityId skinnedEntity = Ecs::ensure(EntityRealm::ClientReplicated,
                                                   EntityDomain::Npc, 8401);
        SkeletonInstances::ensure(skinnedEntity);
        GameSkeletonPoseV1 skelPose{};
        skelPose.entity = static_cast<std::uint64_t>(skinnedEntity);
        skelPose.count = 1;
        skelPose.parts[0].part = gameHash("leftArm");
        skelPose.parts[0].rotationEuler[2] = 0.5f;
        SkeletonInstances::applyPose(skinnedEntity, skelPose);
        GameRenderMeshCommandV1 skCmd{};
        skCmd.entity = static_cast<std::uint64_t>(skinnedEntity);
        skCmd.meshResourceId = partMesh;
        const std::uint64_t skinnedBefore =
            PresentationRender::skinnedSubmissionCount();
        PresentationRender::submitMesh(skCmd);
        ok &= check(PresentationRender::skinnedSubmissionCount() ==
                        skinnedBefore + 1,
                    "generic render.mesh consumes SkeletonInstances by EntityId",
                    report);
        const EntityId bareEntity = Ecs::ensure(EntityRealm::ClientReplicated,
                                                EntityDomain::Npc, 8402);
        GameRenderMeshCommandV1 bareCmd{};
        bareCmd.entity = static_cast<std::uint64_t>(bareEntity);
        bareCmd.meshResourceId = partMesh;
        const std::uint64_t fallbackBefore =
            PresentationRender::staticFallbackCount();
        PresentationRender::submitMesh(bareCmd);
        ok &= check(PresentationRender::staticFallbackCount() == fallbackBefore + 1,
                    "missing skeleton instance falls back to a static draw",
                    report);
        EntityRegistry::instance().destroy(skinnedEntity);
        EntityRegistry::instance().destroy(bareEntity);
    }

    // ── Generic generation-aware resource provider ────────────────────
    {
        using MimitaRuntime::PresentationResourceProvider;
        PresentationResourceProvider& rp = PresentationResourceProvider::instance();
        const std::uint64_t logical = gameHash("selftest.presentation-resource");
        rp.setLoader(logical, &resourceTestLoad, &resourceTestRetire, nullptr);
        const bool first = rp.apply(logical, 111u);
        ok &= check(first && rp.generationOf(logical) == 1 &&
                        rp.handleOf(logical) != nullptr,
                    "resource provider loads first generation", report);
        const int loadsBefore = g_resourceLoads;
        ok &= check(rp.apply(logical, 111u) && g_resourceLoads == loadsBefore,
                    "same content hash does not reload", report);
        ok &= check(rp.apply(logical, 222u) && rp.generationOf(logical) == 2,
                    "changed content hash swaps generation", report);
        g_resourceFailNext = true;
        void* lastGood = rp.handleOf(logical);
        ok &= check(!rp.apply(logical, 333u) && rp.handleOf(logical) == lastGood &&
                        rp.generationOf(logical) == 2,
                    "failed load preserves last-good resource generation", report);
        g_resourceFailNext = false;
        ok &= check(LiveUi::resolveUiImageHandle(logical) != 0,
                    "ui image resolves a generation-aware resource handle", report);

        // ── Client projectile presentation bridge ─────────────────────
        // A network projectile materializes a generic client entity and is
        // drawn once by the hot system; untouched/terminated entities retire.
        std::uint64_t projMesh = 0, projTex = 0;
        float projScale = 1.0f;
        ok &= check(PresentationEntities::resourcesForWeapon(
                        MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER, projMesh,
                        projTex, projScale),
                    "projectile weapon maps to logical resources", report);
        PresentationEntities::beginSync();
        const float ppos[3] = {1.0f, 2.0f, 3.0f};
        const float pvel[3] = {10.0f, 0.0f, 0.0f};
        const std::uint64_t projEntity = PresentationEntities::ensure(
            4242u, ppos, pvel, projMesh, projTex, projScale);
        ok &= check(projEntity != 0 && PresentationEntities::has(4242u),
                    "network projectile materializes a generic presentation entity",
                    report);
        const std::uint64_t meshesBeforeProj =
            PresentationRender::submittedMeshCount();
        runtime.runDomain(GAME_DOMAIN_RENDER, 4, 0.016f,
                          LiveBehavior::hostContext(4));
        ok &= check(PresentationRender::submittedMeshCount() > meshesBeforeProj,
                    "generic projectile presentation submits a mesh", report);
        // The typed renderer is suppressed for any projectile the bridge owns,
        // so exactly one owner draws it.
        ok &= check(PresentationEntities::has(4242u),
                    "generic presentation suppresses the typed projectile draw",
                    report);
        PresentationEntities::endSync();
        ok &= check(PresentationEntities::has(4242u),
                    "live projectile presentation persists across a frame", report);
        PresentationEntities::beginSync();
        PresentationEntities::endSync();
        ok &= check(!PresentationEntities::has(4242u),
                    "terminated projectile presentation retires with the entity",
                    report);
    }

    // ── GLB loader validate stage (generation-aware resource) ─────────
    {
        std::string error;
        ok &= check(PresentationRender::validateGlbFile(
                        "assets/entity/player/default/mimita-char-no-animations-v4.glb",
                        error),
                    "valid GLB container is accepted", report);
        const char* badPath = "C:/Users/guita/AppData/Local/Temp/opencode/bad.glb";
        if (FILE* f = std::fopen(badPath, "wb")) {
            const char junk[16] = {'N', 'O', 'T', 'G', 'L', 'B', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            std::fwrite(junk, 1, sizeof(junk), f);
            std::fclose(f);
        }
        error.clear();
        ok &= check(!PresentationRender::validateGlbFile(badPath, error) &&
                        !error.empty(),
                    "malformed GLB is rejected (last-good preserved)", report);

        // The real actor GLB is rigid multipart: its named nodes must parse into
        // generic bone-hashed parts so hot PoseState can pose them.
        std::uint64_t partHashes[16] = {0};
        const std::uint32_t partCount = PresentationRender::inspectGlbParts(
            "assets/entity/player/default/mimita-char-no-animations-v4.glb",
            partHashes, 16);
        bool hasTorso = false;
        bool hasLeftArm = false;
        for (std::uint32_t i = 0; i < partCount && i < 16; ++i) {
            if (partHashes[i] == gameHash("torso")) hasTorso = true;
            if (partHashes[i] == gameHash("leftArm")) hasLeftArm = true;
        }
        ok &= check(partCount >= 4 && hasTorso && hasLeftArm,
                    "real actor GLB parses into named body parts", report);
    }

    // ── Hot ui.frame HUD composition ──────────────────────────────────
    {
        GameplayContextV1* uiCtx = LiveBehavior::hostContext(0);
        auto renderUi = uiCtx
            ? reinterpret_cast<GameRenderUiFn>(uiCtx->resolveCapability(
                  uiCtx->host, GAME_CAP_RENDER_UI))
            : nullptr;
        ok &= check(renderUi != nullptr, "render.ui capability resolves", report);
        LiveUi::beginFrame();
        if (renderUi) {
            GameUiCommandV1 probe{};
            probe.kind = GAME_UI_TEXT;
            std::snprintf(probe.text, sizeof(probe.text), "probe");
            renderUi(uiCtx->host, &probe);
        }
        ok &= check(LiveUi::commandCount() == 1,
                    "render.ui capability buffers a widget", report);
        ok &= check(DynamicComponentStore::instance().schema(
                        HOT_MATCH_HUD_COMPONENT) != nullptr,
                    "MatchHudState schema registered", report);

        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 1, 0.016f, LiveBehavior::hostContext(1));
        ok &= check(LiveUi::commandCount() == 0,
                    "hot ui.frame fails safe without match HUD state", report);

        HotMatchHudStateV1 hud{};
        hud.timerSeconds = 95.0f;
        hud.scoreA = 3;
        hud.scoreB = 1;
        hud.phase = 2;
        std::snprintf(hud.phaseText, sizeof(hud.phaseText), "LIVE");
        std::snprintf(hud.labelA, sizeof(hud.labelA), "RED");
        std::snprintf(hud.labelB, sizeof(hud.labelB), "BLUE");
        std::uint64_t hudMatch = 0;
        if (uiCtx && uiCtx->entityCreate)
            uiCtx->entityCreate(uiCtx->host, 0u, &hudMatch);
        const bool wroteHud =
            hudMatch != 0 &&
            DynamicComponentStore::instance().write(hudMatch, HOT_MATCH_HUD_COMPONENT,
                                                     &hud, sizeof(hud));
        std::uint64_t hudFound[4] = {0};
        const std::uint32_t hudStored = DynamicComponentStore::instance().enumerate(
            HOT_MATCH_HUD_COMPONENT, hudFound, 4);
        ok &= check(wroteHud && hudStored > 0, "match HUD state stored", report);

        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 2, 0.016f, LiveBehavior::hostContext(2));
        const std::size_t firstCount = LiveUi::commandCount();
        ok &= check(firstCount > 0,
                    "hot ui.frame system emits HUD widget commands", report);
        // Deterministic ordering: the same input emits the same count/order.
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 3, 0.016f, LiveBehavior::hostContext(3));
        ok &= check(LiveUi::commandCount() == firstCount,
                    "hot HUD composition is deterministic", report);
    }

    MimitaNet::ServerGamemodeState& d = MimitaNet::serverGamemodeState();
    d.enabled = true;
    MimitaNet::serverMatchResetEntity();
    const std::uint64_t match = MimitaNet::serverMatchEntity();
    ok &= check(match != 0, "kernel match entity available", report);

    // ── Generic authoritative server context ─────────────────────────
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer> players;
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc> npcs;
    std::unordered_map<std::uint32_t, MimitaNet::ServerProjectile> projectiles;
    std::uint32_t nextProjectileId = 1;
    std::uint32_t serverTick = 1;
    std::uint64_t totalPacketsOut = 0;

    players.try_emplace(7);
    {
        MimitaNet::ServerPlayer& victim = players[7];
        victim.id = 7;
        victim.spawnState = MimitaNet::ServerPlayer::Active;
        victim.dead = false;
        victim.health = 100;
        victim.maxHealth = 100;
        victim.pos = glm::vec3(5.0f, 0.0f, 0.0f);
    }

    MimitaNet::ServerContextV1 context;
    context.players = &players;
    context.npcs = &npcs;
    context.projectiles = &projectiles;
    context.nextProjectileId = &nextProjectileId;
    context.tick = &serverTick;
    context.totalPacketsOut = &totalPacketsOut;
    MimitaNet::setActiveServerContext(&context);

    // ── Brand-new tool: use is owned by the hot behavior ─────────────
    {
        ToolUsePolicyV1 use = makeUse(kBananaTool);
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        const bool handled = LiveBehavior::dispatchToolUse(use, 1);
        ok &= check(handled && use.handled == 1 && use.outFire == 0,
                    "banana.launcher tool use handled by hot behavior", report);
        ok &= check(projectiles.size() == 0,
                    "hot tool no longer spawns a kernel-container projectile", report);
        std::uint64_t found[8] = {0};
        const std::uint32_t hotProjectileCount =
            DynamicComponentStore::instance().enumerate(
                HOT_PROJECTILE_COMPONENT, found, 8);
        ok &= check(hotProjectileCount >= 1,
                    "hot tool spawned a composition-driven projectile entity", report);

        BananaLauncherState state{};
        const bool has = DynamicComponentStore::instance().read(
            match, kBananaState, &state, sizeof(state));
        ok &= check(has && state.shotsFired == 1 && state.toolEntity != 0,
                    "new tool recorded package state and created a tool entity", report);
    }

    // ── Real rocket uses the canonical hot projectile path ───────────
    {
        std::uint64_t found[8] = {0};
        const std::uint32_t before = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, found, 8);
        ToolUsePolicyV1 use = makeUse(5);  // NETWORK_WEAPON_ROCKET_LAUNCHER
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        const bool handled = LiveBehavior::dispatchToolUse(use, 3);
        std::uint64_t after[8] = {0};
        const std::uint32_t now = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, after, 8);
        ok &= check(handled && use.outFire == 0 && now == before + 1 &&
                        projectiles.empty(),
                    "real rocket uses the canonical hot projectile path", report);
        std::uint64_t newProj = 0;
        for (std::uint32_t i = 0; i < now; ++i) {
            bool seen = false;
            for (std::uint32_t j = 0; j < before; ++j)
                if (after[i] == found[j]) { seen = true; break; }
            if (!seen) newProj = after[i];
        }
        ok &= check(newProj != 0 &&
                        DynamicComponentStore::instance().has(
                            newProj, gameHash("PresentationState")),
                    "rocket projectile carries generic PresentationState", report);
    }

    // ── Grenade launcher uses the same canonical hot projectile path ─
    {
        std::uint64_t found[8] = {0};
        const std::uint32_t before = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, found, 8);
        ToolUsePolicyV1 use = makeUse(7);  // NETWORK_WEAPON_GRENADE_LAUNCHER
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        const bool handled = LiveBehavior::dispatchToolUse(use, 4);
        std::uint64_t after[8] = {0};
        const std::uint32_t now = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, after, 8);
        ok &= check(handled && use.outFire == 0 && now == before + 1,
                    "grenade uses the canonical hot projectile path", report);
        std::uint64_t newProj = 0;
        for (std::uint32_t i = 0; i < now; ++i) {
            bool seen = false;
            for (std::uint32_t j = 0; j < before; ++j)
                if (after[i] == found[j]) { seen = true; break; }
            if (!seen) newProj = after[i];
        }
        ok &= check(newProj != 0 &&
                        DynamicComponentStore::instance().has(
                            newProj, gameHash("PresentationState")),
                    "grenade projectile carries generic PresentationState", report);
    }

    // ── Real predicted handoff: predictionKey -> PredictionLink ───────
    {
        std::uint64_t found[16] = {0};
        const std::uint32_t before = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, found, 16);
        ToolUsePolicyV1 use = makeUse(5);  // rocket
        use.predictionKey = 12345u;
        use.origin[0] = 0.0f; use.origin[1] = 0.0f; use.origin[2] = 1.0f;
        use.direction[0] = 1.0f; use.direction[1] = 0.0f; use.direction[2] = 0.0f;
        LiveBehavior::dispatchToolUse(use, 7);
        std::uint64_t after[16] = {0};
        const std::uint32_t now = DynamicComponentStore::instance().enumerate(
            HOT_PROJECTILE_COMPONENT, after, 16);
        std::uint64_t newProj = 0;
        for (std::uint32_t i = 0; i < now; ++i) {
            bool seen = false;
            for (std::uint32_t j = 0; j < before; ++j)
                if (after[i] == found[j]) { seen = true; break; }
            if (!seen) newProj = after[i];
        }
        HotPredictionLinkV1 link{};
        ok &= check(newProj != 0 &&
                        DynamicComponentStore::instance().read(
                            newProj, HOT_PREDICTION_LINK_COMPONENT, &link,
                            sizeof(link)) &&
                        link.predictionKey == 12345u,
                    "authoritative projectile carries the prediction key",
                    report);
    }

    // ── Generic non-projectile predicted behavior ─────────────────────
    {
        ToolUsePolicyV1 use = makeUse(gameHash("hot.predicted-test"));
        use.predictionKey = 67890u;
        LiveBehavior::dispatchToolUse(use, 8);
        std::uint64_t links[32] = {0};
        const std::uint32_t count = DynamicComponentStore::instance().enumerate(
            HOT_PREDICTION_LINK_COMPONENT, links, 32);
        bool foundKey = false;
        for (std::uint32_t i = 0; i < count; ++i) {
            HotPredictionLinkV1 link{};
            if (DynamicComponentStore::instance().read(
                    links[i], HOT_PREDICTION_LINK_COMPONENT, &link, sizeof(link)) &&
                link.predictionKey == 67890u) {
                foundKey = true;
                // Non-projectile: the entity is not a hot projectile.
                if (DynamicComponentStore::instance().has(
                        links[i], HOT_PROJECTILE_COMPONENT))
                    foundKey = false;
            }
        }
        ok &= check(foundKey && use.outFire == 0,
                    "non-projectile predicted entity carries PredictionLink",
                    report);
    }

    // ── A second use increments the same package state ───────────────
    {
        ToolUsePolicyV1 use = makeUse(kBananaTool);
        LiveBehavior::dispatchToolUse(use, 2);
        BananaLauncherState state{};
        DynamicComponentStore::instance().read(match, kBananaState, &state, sizeof(state));
        ok &= check(state.shotsFired == 2, "tool state persists across uses", report);
    }

    // ── Unknown tool falls back to the cold path (not handled) ───────
    {
        ToolUsePolicyV1 use = makeUse(gameHash("unknown.tool"));
        const bool handled = LiveBehavior::dispatchToolUse(use, 3);
        ok &= check(!handled && use.handled == 0,
                    "unregistered tool leaves the cold path in charge", report);
    }

    // ── Brand-new projectile impact is owned by the hot behavior ─────
    {
        ProjectileImpactPolicyV1 impact = makeImpact(kBananaProjectile);
        const bool handled = LiveBehavior::dispatchProjectileImpact(impact, 4);
        ok &= check(handled && impact.handled == 1 && impact.outExplode == 1,
                    "banana.projectile impact handled by hot behavior", report);
    }

    // ── Real rocket projectile policy is hot (network id 5) ──────────
    {
        ProjectileImpactPolicyV1 impact = makeImpact(5);
        const bool handled = LiveBehavior::dispatchProjectileImpact(impact, 5);
        ok &= check(handled && impact.outExplode == 1,
                    "real rocket projectile impact policy is hot", report);
    }

    // ── Unknown projectile type falls back to cold per-type flags ────
    {
        ProjectileImpactPolicyV1 impact = makeImpact(gameHash("unknown.projectile"));
        const bool handled = LiveBehavior::dispatchProjectileImpact(impact, 6);
        ok &= check(!handled && impact.handled == 0,
                    "unregistered projectile leaves the cold flags in charge", report);
    }

    // ── Per-entity behavior binding owns the tool use ────────────────
    {
        const EntityId toolEntity =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        BehaviorBindingsComponent bindings;
        BehaviorBinding binding;
        binding.eventType = static_cast<std::uint32_t>(gameHash("on.primary-use"));
        binding.behaviorId = gameHash("banana.launcher.use");
        bindings.add(binding);
        EntityRegistry::instance().add<BehaviorBindingsComponent>(toolEntity, bindings);

        ToolUsePolicyV1 use = makeUse(kBananaTool);
        use.toolEntity = static_cast<std::uint64_t>(toolEntity);
        const bool handled = LiveBehavior::dispatchToolUse(use, 7);
        ok &= check(handled && use.handled == 1 && use.outFire == 0,
                    "per-entity behavior binding owns tool use", report);
    }

    // ── Real input path: generic action intent → equipped runtime tool ──
    {
        FireIntentPacket pkt{};
        pkt.header.type = PACKET_FIRE_INTENT_REQUEST;
        pkt.header.playerId = 7;
        pkt.action = FIRE_INTENT_START;
        pkt.intentId = 42;
        pkt.toolId = kBananaTool;  // runtime tool: no network weapon id
        pkt.startTick = serverTick;
        pkt.originX = 0.0f; pkt.originY = 0.0f; pkt.originZ = 1.0f;
        pkt.dirX = 1.0f; pkt.dirY = 0.0f; pkt.dirZ = 0.0f;
        MimitaNet::handleFireIntentPacket(0, reinterpret_cast<const char*>(&pkt),
                                          static_cast<int>(sizeof(pkt)), players,
                                          serverTick);
        auto pit = players.find(7);
        ok &= check(pit != players.end() && pit->second.runtimeToolId == kBananaTool &&
                        pit->second.equippedToolEntity != 0,
                    "generic action intent equipped a runtime tool (no network weapon)", report);
        // The authoritative held-fire tick dispatches the equipped tool's
        // behavior; no registered weapon definition is consulted.
        MimitaNet::tickHeldFireIntents(0, players, npcs, projectiles, nextProjectileId,
                                       serverTick, totalPacketsOut);
        ok &= check(pit != players.end() && pit->second.heldFire.lastEmitTick == serverTick,
                    "held-fire tick dispatched the generic action", report);
    }

    // ── Generic action spawns an authoritative projectile ────────────
    {
        std::uint64_t beforeBuf[64] = {0};
        const std::uint32_t beforeCount = DynamicComponentStore::instance().enumerate(
            gameHash("HotProjectileState"), beforeBuf, 64);
        FireIntentPacket pkt{};
        pkt.header.type = PACKET_FIRE_INTENT_REQUEST;
        pkt.header.playerId = 7;
        pkt.action = FIRE_INTENT_START;
        pkt.intentId = 43;
        pkt.toolId = 5;  // rocket behavior key
        pkt.startTick = serverTick;
        pkt.originX = 0.0f; pkt.originY = 0.0f; pkt.originZ = 1.0f;
        pkt.dirX = 1.0f; pkt.dirY = 0.0f; pkt.dirZ = 0.0f;
        MimitaNet::handleFireIntentPacket(0, reinterpret_cast<const char*>(&pkt),
                                          static_cast<int>(sizeof(pkt)), players,
                                          serverTick);
        MimitaNet::tickHeldFireIntents(0, players, npcs, projectiles, nextProjectileId,
                                       serverTick, totalPacketsOut);
        std::uint64_t afterBuf[64] = {0};
        const std::uint32_t afterCount = DynamicComponentStore::instance().enumerate(
            gameHash("HotProjectileState"), afterBuf, 64);
        ok &= check(afterCount > beforeCount,
                    "generic input spawned an authoritative projectile", report);
    }

    // ── Generic authoritative damage capability ──────────────────────
    {
        const EntityId victimEntity =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Player, 7);
        GameplayContextV1* contextHost = LiveBehavior::hostContext(7);
        GameDamageApplyFn apply = nullptr;
        if (contextHost && contextHost->resolveCapability)
            apply = reinterpret_cast<GameDamageApplyFn>(
                contextHost->resolveCapability(contextHost->host, GAME_CAP_DAMAGE_APPLY));
        ok &= check(apply != nullptr, "damage.apply capability resolved", report);
        if (apply) {
            GameDamageApplyV1 request{};
            request.victimEntity = static_cast<std::uint64_t>(victimEntity);
            request.amount = 30;
            request.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
            apply(contextHost->host, &request);
            auto it = players.find(7);
            ok &= check(request.applied && it != players.end() &&
                            it->second.health < 100,
                        "hot code applied real authoritative damage", report);
        }
    }

    // ── Generic damage across entity types ───────────────────────────
    {
        npcs.try_emplace(3);
        MimitaNet::ServerNpc& npc = npcs[3];
        npc.entityId = 3;
        npc.health = 100;
        npc.pos = glm::vec3(9.0f, 0.0f, 0.0f);
        const EntityId npcEntity =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, 3);
        GameplayContextV1* host = LiveBehavior::hostContext(11);
        GameDamageApplyFn apply = reinterpret_cast<GameDamageApplyFn>(
            host->resolveCapability(host->host, GAME_CAP_DAMAGE_APPLY));
        GameDamageApplyV1 npcReq{};
        npcReq.victimEntity = static_cast<std::uint64_t>(npcEntity);
        npcReq.amount = 25;
        npcReq.sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
        apply(host->host, &npcReq);
        ok &= check(npcReq.applied && npcReq.healthAfter == 75,
                    "damage.apply works on an NPC victim (generic)", report);

        const EntityId obj = EntityRegistry::instance().createGeneric(EntityRealm::Server);
        HealthComponent hc;
        hc.current = 50;
        hc.max = 50;
        EntityRegistry::instance().add<HealthComponent>(obj, hc);
        GameDamageApplyV1 objReq{};
        objReq.victimEntity = static_cast<std::uint64_t>(obj);
        objReq.amount = 20;
        objReq.sourceKind = GAME_DAMAGE_SOURCE_EXPLOSION;
        apply(host->host, &objReq);
        ok &= check(objReq.applied && objReq.healthAfter == 30,
                    "damage.apply works on a non-actor damageable entity", report);
    }

    // ── Generic item containment/equip lifecycle ─────────────────────
    {
        const EntityId item =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        MimitaNet::serverItemEquip(7, static_cast<std::uint64_t>(item));
        ok &= check(MimitaNet::serverItemContains(7, static_cast<std::uint64_t>(item)),
                    "item contained after equip", report);
        MimitaNet::serverItemUnequip(7);
        MimitaNet::serverItemDrop(7, static_cast<std::uint64_t>(item));
        ok &= check(!MimitaNet::serverItemContains(7, static_cast<std::uint64_t>(item)),
                    "item not contained after drop", report);
        MimitaNet::serverItemPickup(7, static_cast<std::uint64_t>(item));
        MimitaNet::serverItemEquip(7, static_cast<std::uint64_t>(item));
        ok &= check(EntityRegistry::instance().alive(item) &&
                        MimitaNet::serverItemContains(7, static_cast<std::uint64_t>(item)),
                    "same item EntityId survives inventory->equip->drop->pickup->re-equip",
                    report);
    }

    // ── Migrated registered weapon: revolver state on its tool entity ─
    {
        std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer> wp;
        wp.try_emplace(7);
        MimitaNet::ServerPlayer& p = wp[7];
        p.id = 7;
        // Seed the legacy map with bogus values that must NOT be authoritative.
        MimitaNet::ServerPlayer::ServerWeaponRuntime scratch{};
        scratch.initialized = true;
        scratch.magazineAmmo = 999;
        scratch.nextAllowedFireTick = 4242;
        p.weaponRuntimes["revolver"] = scratch;

        const std::uint64_t tool =
            MimitaNet::serverWeaponToolEntity(p, "revolver", true);
        ok &= check(tool != 0, "revolver state tool entity created", report);

        MimitaNet::serverWeaponStateWriteComponent(p, "revolver", 6, 12, 100);
        std::int32_t mag = 0, res = 0;
        std::uint64_t cd = 0;
        ok &= check(MimitaNet::serverWeaponStateReadComponent(p, "revolver", &mag, &res,
                                                              &cd) &&
                        mag == 6 && res == 12 && cd == 100,
                    "revolver ammo/cooldown live on the tool entity component", report);

        MimitaNet::serverWeaponStateLoad(p, "revolver");
        ok &= check(p.weaponRuntimes["revolver"].magazineAmmo == 6 &&
                        p.weaponRuntimes["revolver"].nextAllowedFireTick == 100,
                    "legacy weapon map is not authoritative (component wins)", report);

        MimitaNet::serverItemEquip(7, tool);
        MimitaNet::serverItemDrop(7, tool);
        MimitaNet::serverItemPickup(7, tool);
        MimitaNet::serverItemEquip(7, tool);
        std::int32_t mag2 = 0, res2 = 0;
        std::uint64_t cd2 = 0;
        ok &= check(EntityRegistry::instance().alive(static_cast<EntityId>(tool)) &&
                        MimitaNet::serverWeaponToolEntity(p, "revolver", false) == tool &&
                        MimitaNet::serverWeaponStateReadComponent(p, "revolver", &mag2,
                                                                  &res2, &cd2) &&
                        mag2 == 6,
                    "revolver tool EntityId + state survive equip/drop/pickup/re-equip",
                    report);
    }

    MimitaNet::setActiveServerContext(nullptr);
    // ── Hot projectile lifecycle (projectiles.60) owns the entity ────
    {
        const EntityId proj =
            EntityRegistry::instance().createGeneric(EntityRealm::Server);
        HotProjectileStateV1 s{};
        s.velocity[0] = 10.0f;
        s.lifetime = 1.0f;
        s.radius = 0.1f;
        s.impactDamage = 1.0f;
        s.flags = 0;  // pure integration: no explode
        DynamicComponentStore::instance().write(proj, HOT_PROJECTILE_COMPONENT, &s,
                                                sizeof(s));

        const float dt = 1.0f / 60.0f;
        runtime.runDomain(gameHash("projectiles.60"), 100, dt,
                          LiveBehavior::hostContext(100));

        HotProjectileStateV1 after{};
        const bool read = DynamicComponentStore::instance().read(
            proj, HOT_PROJECTILE_COMPONENT, &after, sizeof(after));
        ok &= check(read && after.age > 0.0f && after.position[0] > 0.0f,
                    "hot projectiles.60 system simulates the projectile entity",
                    report);
    }

    GenericRuntime::instance().deactivate();
    HotReloadSystem::instance().unloadGameDLL();
    return ok;
}
