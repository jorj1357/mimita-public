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
#include "hot-reload/hot-effect.h"
#include "hot-reload/hot-pose.h"
#include "hot-reload/hot-presentation.h"
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
#include "network/actor-state.h"
#include "camera.h"

extern Camera* gpCamera;

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
    // Hot packages register commands generically (no cold command switch): the
    // visual-proof commands added this session must be present.
    ok &= check(runtime.hasCommand("posedebug") && runtime.hasCommand("hotactor") &&
                    runtime.hasCommand("hotpresent"),
                "hot package registers commands without a cold switch", report);

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

        // Local player (THE_PLAYER) participates in the same generic animation
        // and pose path: its canonical entity is driven by hot pose generation.
        const EntityId localEntity = Ecs::ensureLocalPlayerEntity();
        ok &= check(localEntity != kInvalidEntityId,
                    "local player has a canonical generic entity", report);
        HotAnimationStateV1 localAnim{};
        localAnim.clipId = HOT_ANIM_MOVE;
        localAnim.playbackRate = 1.0f;
        DynamicComponentStore::instance().write(
            localEntity, HOT_ANIMATION_STATE_COMPONENT, &localAnim,
            sizeof(localAnim));
        runtime.runDomain(GAME_DOMAIN_RENDER, 10, 0.016f,
                          LiveBehavior::hostContext(10));
        SkeletonInstances::Instance* localInst = SkeletonInstances::get(localEntity);
        ok &= check(localInst != nullptr && localInst->version > 0,
                    "local player entity is driven by the hot pose path", report);
    }

    // ── Hot generic effect ownership ──────────────────────────────────
    {
        GameplayContextV1* fxHost = LiveBehavior::hostContext(11);
        const bool ran = runtime.runCommand("hoteffect", "", fxHost);
        std::uint64_t fx[8] = {0};
        const std::uint32_t fxCount = DynamicComponentStore::instance().enumerate(
            HOT_EFFECT_LIFETIME_COMPONENT, fx, 8);
        ok &= check(ran && fxCount >= 1,
                    "runtime-unknown effect entity created (no enum/switch)",
                    report);
        if (fxCount >= 1) {
            const EntityId effect = static_cast<EntityId>(fx[0]);
            for (int i = 0; i < 30; ++i)
                runtime.runDomain(GAME_DOMAIN_RENDER, 200 + i, 1.0f / 60.0f,
                                  LiveBehavior::hostContext(200 + i));
            HotEffectLifetimeV1 life{};
            HotPresentationStateV1 pres{};
            const bool aged = DynamicComponentStore::instance().read(
                effect, HOT_EFFECT_LIFETIME_COMPONENT, &life, sizeof(life));
            DynamicComponentStore::instance().read(
                effect, HOT_PRESENTATION_COMPONENT, &pres, sizeof(pres));
            ok &= check(aged && life.age > 0.4f && pres.scale > 0.4f,
                        "hot effect ages, integrates, and grows", report);
            for (int i = 0; i < 90; ++i)
                runtime.runDomain(GAME_DOMAIN_RENDER, 300 + i, 1.0f / 60.0f,
                                  LiveBehavior::hostContext(300 + i));
            ok &= check(!EntityRegistry::instance().alive(effect),
                        "hot effect expires and is destroyed", report);
        }

        // Real shipping explosion fact reaches the hot effect owner, which
        // composes generic effect entities; the cold composition yields.
        std::uint64_t before[16] = {0};
        const std::uint32_t beforeCount = DynamicComponentStore::instance().enumerate(
            HOT_EFFECT_LIFETIME_COMPONENT, before, 16);
        EffectRequestV1 req{};
        req.effectTypeId = gameHash("effect.explosion.rocket");
        req.position[0] = 1.0f;
        req.position[1] = 1.0f;
        req.position[2] = 1.0f;
        req.scale = 1.0f;
        const bool fxHandled = LiveBehavior::dispatchEffectRequest(req, 20);
        std::uint64_t after[16] = {0};
        const std::uint32_t afterCount = DynamicComponentStore::instance().enumerate(
            HOT_EFFECT_LIFETIME_COMPONENT, after, 16);
        ok &= check(fxHandled && afterCount > beforeCount,
                    "real explosion fact reaches the hot effect owner and composes "
                    "generic effects",
                    report);

        // Hot audio policy: the explosion handler emits a generic audio command,
        // and a runtime-unknown sound plays through the same capability.
        const std::uint64_t audioAfterFx = LiveBehavior::audioPlayCount();
        GameplayContextV1* audioHost = LiveBehavior::hostContext(21);
        const std::uint64_t audioBefore = LiveBehavior::audioPlayCount();
        const bool ranAudio = runtime.runCommand("hotaudiotest", "", audioHost);
        ok &= check(audioAfterFx > 0 && ranAudio &&
                        LiveBehavior::audioPlayCount() > audioBefore,
                    "hot audio policy plays sounds via the generic command",
                    report);

        // Real hit/blood fact reaches the hot effect owner.
        std::uint64_t hitBefore[16] = {0};
        const std::uint32_t hitBeforeCount =
            DynamicComponentStore::instance().enumerate(
                HOT_EFFECT_LIFETIME_COMPONENT, hitBefore, 16);
        EffectRequestV1 hitReq{};
        hitReq.effectTypeId = gameHash("effect.hit.blood");
        hitReq.position[0] = 2.0f;
        hitReq.position[1] = 2.0f;
        hitReq.position[2] = 2.0f;
        hitReq.scale = 1.0f;
        const bool hitHandled = LiveBehavior::dispatchEffectRequest(hitReq, 22);
        std::uint64_t hitAfter[16] = {0};
        const std::uint32_t hitAfterCount =
            DynamicComponentStore::instance().enumerate(
                HOT_EFFECT_LIFETIME_COMPONENT, hitAfter, 16);
        ok &= check(hitHandled && hitAfterCount > hitBeforeCount,
                    "real hit/blood fact reaches the hot effect owner", report);

        // Generic surface-effect/decal primitive: the hit path emits a generic
        // mark, and a runtime-unknown effect creates one via the same capability.
        GameplayContextV1* surfHost = LiveBehavior::hostContext(23);
        const std::uint64_t surfBefore = LiveBehavior::surfaceEffectCount();
        const bool ranSurface =
            runtime.runCommand("hotsurfaceeffect", "", surfHost);
        ok &= check(ranSurface && LiveBehavior::surfaceEffectCount() > surfBefore,
                    "hot surface-effect policy creates a generic decal (no enum)",
                    report);

        // Real weapon-fire muzzle flash: the cold `spawnMuzzleFlash` owner emits
        // a generic effect.request; the hot policy composes the flash.
        std::uint64_t muzzleBefore[16] = {0};
        const std::uint32_t muzzleBeforeCount =
            DynamicComponentStore::instance().enumerate(
                HOT_EFFECT_LIFETIME_COMPONENT, muzzleBefore, 16);
        EffectRequestV1 muzzleReq{};
        muzzleReq.effectTypeId = gameHash("effect.muzzle");
        muzzleReq.weaponNetworkId = gameHash("runtime.tool.unknown");
        muzzleReq.position[0] = 3.0f;
        muzzleReq.position[1] = 1.0f;
        muzzleReq.position[2] = 3.0f;
        muzzleReq.scale = 1.0f;
        const bool muzzleHandled = LiveBehavior::dispatchEffectRequest(muzzleReq, 24);
        std::uint64_t muzzleAfter[16] = {0};
        const std::uint32_t muzzleAfterCount =
            DynamicComponentStore::instance().enumerate(
                HOT_EFFECT_LIFETIME_COMPONENT, muzzleAfter, 16);
        ok &= check(muzzleHandled && muzzleAfterCount > muzzleBeforeCount,
                    "real weapon-fire fact reaches the hot muzzle policy (runtime "
                    "tool key, no enum)",
                    report);

        // Real explosion camera shake: the launcher emits a generic camera fact;
        // the hot policy applies it through the generic camera.effect backend.
        EffectRequestV1 shakeReq{};
        shakeReq.effectTypeId = gameHash("effect.camera.shake");
        shakeReq.scale = 1.0f;
        shakeReq.distance = 2.0f;
        shakeReq.falloffDistance = 8.0f;
        const std::uint64_t camBefore = LiveBehavior::cameraEffectCount();
        const bool shakeHandled = LiveBehavior::dispatchEffectRequest(shakeReq, 25);
        ok &= check(shakeHandled && LiveBehavior::cameraEffectCount() > camBefore,
                    "real explosion shake reaches the hot camera policy", report);

        // Runtime-unknown camera effect.
        GameplayContextV1* camHost = LiveBehavior::hostContext(26);
        const std::uint64_t camBefore2 = LiveBehavior::cameraEffectCount();
        const bool ranCam = runtime.runCommand("hotcamerafx", "", camHost);
        ok &= check(ranCam && LiveBehavior::cameraEffectCount() > camBefore2,
                    "hot camera-effect command works (no enum)", report);

        // Hot screen effect via the existing UI path + weapon-fire audio.
        ok &= check(runtime.hasCommand("hotscreenfx"),
                    "hot screen-effect command registered (reuses hot UI)", report);
        EffectRequestV1 fireSound{};
        fireSound.effectTypeId = gameHash("effect.weapon.fire.sound");
        std::snprintf(fireSound.text, sizeof(fireSound.text),
                      "rocketlauncher/rocketlaunchershoot");
        const std::uint64_t audioBefore3 = LiveBehavior::audioPlayCount();
        const bool fireHandled =
            LiveBehavior::dispatchEffectRequest(fireSound, 27);
        ok &= check(fireHandled && LiveBehavior::audioPlayCount() > audioBefore3,
                    "real weapon-fire sound policy is hot (audio.play)", report);

        // Footstep + air-jump audio use the same generic movement-fact substrate.
        EffectRequestV1 stepReq{};
        stepReq.effectTypeId = gameHash("effect.footstep.sound");
        const std::uint64_t stepBefore = LiveBehavior::audioPlayCount();
        const bool stepHandled = LiveBehavior::dispatchEffectRequest(stepReq, 28);
        ok &= check(stepHandled && LiveBehavior::audioPlayCount() > stepBefore,
                    "grounded footstep audio policy is hot (audio.play)", report);

        EffectRequestV1 stepCustom{};
        stepCustom.effectTypeId = gameHash("effect.footstep.sound");
        std::snprintf(stepCustom.text, sizeof(stepCustom.text), "mod/custom_step");
        const std::uint64_t custBefore = LiveBehavior::audioPlayCount();
        const bool custHandled = LiveBehavior::dispatchEffectRequest(stepCustom, 29);
        ok &= check(custHandled && LiveBehavior::audioPlayCount() > custBefore,
                    "arbitrary logical footstep sound id is hot (no enum)", report);

        EffectRequestV1 actorSnd{};
        actorSnd.effectTypeId = gameHash("effect.actor.sound");
        std::snprintf(actorSnd.text, sizeof(actorSnd.text), "%s", "actor.dash");
        const std::uint64_t actorBefore = LiveBehavior::audioPlayCount();
        const bool actorHandled =
            LiveBehavior::dispatchEffectRequest(actorSnd, 30);
        ok &= check(actorHandled && LiveBehavior::audioPlayCount() > actorBefore,
                    "NPC action audio policy is hot (actor.dash -> audio.play)",
                    report);

        EffectRequestV1 spawnSnd{};
        spawnSnd.effectTypeId = gameHash("effect.actor.sound");
        std::snprintf(spawnSnd.text, sizeof(spawnSnd.text), "%s", "actor.spawn");
        const std::uint64_t spawnBefore = LiveBehavior::audioPlayCount();
        const bool spawnHandled = LiveBehavior::dispatchEffectRequest(spawnSnd, 31);
        ok &= check(spawnHandled && LiveBehavior::audioPlayCount() > spawnBefore,
                    "NPC spawn audio policy is hot (actor.spawn -> audio.play)",
                    report);

        // Generic persistent audio slots: SET_SLOT / idempotence / change / STOP,
        // with runtime-unknown slot + sound ids (no cold enum).
        {
            GameplayContextV1* actx = LiveBehavior::hostContext(30);
            auto audio = actx ? reinterpret_cast<GameAudioPlayFn>(
                                    actx->resolveCapability(actx->host,
                                                            GAME_CAP_AUDIO_PLAY))
                              : nullptr;
            ok &= check(audio != nullptr, "audio.play capability resolves", report);
            if (audio) {
                GameAudioCommandV1 c{};
                std::snprintf(c.sound, sizeof(c.sound), "entity/player/dash");
                c.slotId = gameHash("selftest.loop");
                c.op = GAME_AUDIO_SET_SLOT;
                c.loop = 1;
                c.volume = 0.5f;
                c.pitch = 1.0f;
                const std::uint64_t b1 = LiveBehavior::audioPlayCount();
                audio(actx->host, &c);
                ok &= check(LiveBehavior::audioPlayCount() > b1,
                            "audio SET_SLOT starts a persistent slot voice", report);
                const std::uint64_t b2 = LiveBehavior::audioPlayCount();
                audio(actx->host, &c);
                ok &= check(LiveBehavior::audioPlayCount() == b2,
                            "identical SET_SLOT is idempotent (no restart)", report);
                std::snprintf(c.sound, sizeof(c.sound), "entity/player/walk1");
                const std::uint64_t b3 = LiveBehavior::audioPlayCount();
                audio(actx->host, &c);
                ok &= check(LiveBehavior::audioPlayCount() > b3,
                            "slot sound change A->B replaces the voice", report);
                // Runtime-unknown slot + sound id.
                std::snprintf(c.sound, sizeof(c.sound), "audio.user.weird");
                c.slotId = gameHash("user.weird.loop");
                const std::uint64_t b4 = LiveBehavior::audioPlayCount();
                audio(actx->host, &c);
                ok &= check(LiveBehavior::audioPlayCount() > b4,
                            "runtime-unknown audio slot + logical sound accepted",
                            report);
                c.op = GAME_AUDIO_STOP_SLOT;
                audio(actx->host, &c);   // safe stop; no crash
                const std::uint64_t b5 = LiveBehavior::audioPlayCount();
                c.slotId = gameHash("never.existed");
                audio(actx->host, &c);   // stopping a nonexistent slot is a no-op
                ok &= check(LiveBehavior::audioPlayCount() == b5,
                            "STOP_SLOT on a nonexistent slot is a safe no-op",
                            report);
            }
        }

        EffectRequestV1 jumpReq{};
        jumpReq.effectTypeId = gameHash("effect.jump.sound");
        const std::uint64_t jumpBefore = LiveBehavior::audioPlayCount();
        const bool jumpHandled = LiveBehavior::dispatchEffectRequest(jumpReq, 30);
        ok &= check(jumpHandled && LiveBehavior::audioPlayCount() > jumpBefore,
                    "air-jump audio uses the same movement-fact substrate", report);

        // Unhandled movement facts must report unhandled so cold stays the owner.
        EffectRequestV1 unknownReq{};
        unknownReq.effectTypeId = gameHash("effect.landing.sound");
        ok &= check(!LiveBehavior::dispatchEffectRequest(unknownReq, 31),
                    "unmigrated landing audio stays cold-owned (no duplicate owner)", report);
    }

    // ── Generic attachment + logical mesh resources + view space ──────
    {
        // The real equip state's ToolRefState schema is cold-owned.
        MimitaNet::actorStateEnsureSchemas();
        GameplayContextV1* actx = LiveBehavior::hostContext(32);
        // Tool presentation runs in post-movement (before the cold render pass)
        // then the attachment/mesh systems run in the render domain; mirror the
        // real frame order.
        auto runPresentation = [&](std::uint64_t t) {
            runtime.runDomain(GAME_DOMAIN_POST_MOVEMENT, t, 0.016f,
                              LiveBehavior::hostContext(t));
            runtime.runDomain(GAME_DOMAIN_RENDER, t, 0.016f,
                              LiveBehavior::hostContext(t));
        };
        ok &= check(runtime.hasCommand("hotmesh") && runtime.hasCommand("hottool"),
                    "hot attachment/tool commands registered (no cold switch)",
                    report);
        auto sock = actx ? reinterpret_cast<GameSocketQueryFn>(
                               actx->resolveCapability(actx->host,
                                                       GAME_CAP_SOCKET_QUERY))
                         : nullptr;
        auto resReg = actx ? reinterpret_cast<GameResourceRegisterFn>(
                                 actx->resolveCapability(actx->host,
                                                         GAME_CAP_RESOURCE_REGISTER))
                           : nullptr;
        ok &= check(sock != nullptr, "socket.query capability resolves", report);
        ok &= check(resReg != nullptr, "resource.register capability resolves",
                    report);

        // Non-skeletal parent: entity transform + caller local offset.
        std::uint64_t parent = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &parent);
            if (parent != 0) {
                GameTransformComponentV1 tf{};
                tf.position[0] = 10.0f;
                tf.position[1] = 2.0f;
                tf.position[2] = 3.0f;
                actx->writeComponent(actx->host, parent, GAME_COMPONENT_TRANSFORM,
                                     &tf, sizeof(tf));
            }
        }
        ok &= check(parent != 0, "attachment parent entity created", report);

        GameSocketQueryV1 q{};
        q.entity = parent;
        q.socket = gameHash("rightArm");   // no skeleton yet -> fallback
        q.localPosition[0] = 1.0f;
        q.localRotation[3] = 1.0f;
        q.localScale[0] = q.localScale[1] = q.localScale[2] = 1.0f;
        const bool qok = sock && sock(actx->host, &q) && q.valid == 1 &&
                         q.usedFallback == 1 && q.position[0] > 10.9f &&
                         q.position[0] < 11.1f && q.position[2] > 2.9f &&
                         q.position[2] < 3.1f;
        ok &= check(qok,
                    "socket query falls back to entity transform + local offset",
                    report);

        GameSocketQueryV1 qMissing{};
        qMissing.entity = 9999999;
        qMissing.localRotation[3] = 1.0f;
        if (sock)
            sock(actx->host, &qMissing);
        ok &= check(sock && qMissing.valid == 0,
                    "socket query on missing entity fails safe", report);

        // Runtime-unknown logical mesh resource (arbitrary id + path).
        GameResourceRegisterV1 reg{};
        reg.logicalId = gameHash("mesh.selftest.unknown");
        reg.kind = GAME_RESOURCE_MESH;
        reg.applyNow = 1;
        std::snprintf(reg.path, sizeof(reg.path), "%s",
                      "assets/objects/weapons/mimita-hafs-v1.glb");
        const bool regOk = resReg && resReg(actx->host, &reg) && reg.ok == 1;
        ok &= check(regOk, "runtime-unknown logical mesh resource registers",
                    report);

        GameResourceRegisterV1 bad{};
        bad.logicalId = gameHash("mesh.selftest.bad");
        bad.kind = GAME_RESOURCE_MESH;
        bad.applyNow = 1;
        std::snprintf(bad.path, sizeof(bad.path), "%s",
                      "src/hot-reload/hot-modules.json");   // not a GLB
        if (resReg)
            resReg(actx->host, &bad);
        ok &= check(MimitaRuntime::PresentationResourceProvider::instance()
                            .handleOf(bad.logicalId) == nullptr,
                    "malformed mesh keeps no bad generation (last-good only)",
                    report);

        // Local-player duplicate body owner: the local possessed actor's own
        // PresentationState must NOT be submitted by the generic mesh path (the
        // cold body mechanism is the one owner). Compare submission deltas with
        // and without localPlayerEntity pointing at the actor.
        std::uint64_t localActor = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &localActor);
            GameTransformComponentV1 tf{};
            tf.position[0] = 20.0f;
            actx->writeComponent(actx->host, localActor, GAME_COMPONENT_TRANSFORM,
                                 &tf, sizeof(tf));
            HotPresentationStateV1 ps{};
            ps.meshResourceId = gameHash("mesh.cube");
            ps.textureResourceId = gameHash("texture.default");
            ps.scale = 1.0f;
            ps.color[0] = ps.color[1] = ps.color[2] = ps.color[3] = 1.0f;
            actx->dynamicWriteComponent(actx->host, localActor,
                                        HOT_PRESENTATION_COMPONENT, &ps,
                                        sizeof(ps));
        }
        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = 0;   // baseline: nothing skipped
        }
        const std::uint64_t base0 = PresentationRender::submittedMeshCount();
        runPresentation(50);
        const std::uint64_t withActorVisible =
            PresentationRender::submittedMeshCount() - base0;
        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = localActor;
        }
        const std::uint64_t base1 = PresentationRender::submittedMeshCount();
        runPresentation(51);
        const std::uint64_t withActorSkipped =
            PresentationRender::submittedMeshCount() - base1;
        ok &= check(localActor != 0 && withActorVisible >= 1 &&
                        withActorVisible == withActorSkipped + 1,
                    "local possessed body is submitted exactly once (generic path yields)",
                    report);

        // Runtime-unknown tool via the REAL equip substrate: the command only
        // creates the entity + equips-item relationship + ToolRefState; the
        // shared hot.tool-presentation -> hot.attachment -> render.mesh path
        // does the rest (no debug-only rendering shortcut).
        const std::uint64_t viewBefore =
            PresentationRender::viewSpaceSubmissionCount();
        runtime.runCommand("hottool", "", LiveBehavior::hostContext(33));
        std::uint64_t equipped[4] = {0, 0, 0, 0};
        const std::uint32_t eqCount = actx
            ? actx->relationshipQuery(actx->host,
                                      gameHash("relationship.equips-item"),
                                      localActor, equipped, nullptr, 4)
            : 0u;
        ok &= check(eqCount == 1 && equipped[0] != 0,
                    "runtime-unknown tool registered via equips-item relationship",
                    report);

        const std::uint64_t meshesBeforeTool =
            PresentationRender::submittedMeshCount();
        runPresentation(34);
        ok &= check(PresentationRender::submittedMeshCount() > meshesBeforeTool,
                    "equipped tool presents via generic attachment render.mesh",
                    report);

        // The real tool EntityId carries the generic PresentationState.
        HotPresentationStateV1 toolPres{};
        const bool toolPresRead = actx && equipped[0] != 0 &&
            actx->dynamicReadComponent(actx->host, equipped[0],
                                       HOT_PRESENTATION_COMPONENT, &toolPres,
                                       sizeof(toolPres));
        ok &= check(toolPresRead &&
                        toolPres.meshResourceId == gameHash("mesh.runtime.tool"),
                    "tool entity carries its logical mesh resource", report);

        HotAttachmentStateV1 att{};
        const bool attRead = actx && equipped[0] != 0 &&
            actx->dynamicReadComponent(actx->host, equipped[0],
                                       HOT_ATTACHMENT_COMPONENT, &att,
                                       sizeof(att));
        ok &= check(attRead && att.resolved == 1 &&
                        att.parentEntity == localActor,
                    "attachment resolves a socket world transform (fail-safe)",
                    report);

        HotToolClaimV1 claim{};
        const bool claimRead = actx &&
            actx->dynamicReadComponent(actx->host, localActor,
                                       HOT_TOOL_CLAIM_COMPONENT, &claim,
                                       sizeof(claim));
        ok &= check(claimRead && claim.migrated == 1 && claim.toolKey != 0,
                    "hot tool claim declares single presentation owner", report);

        ok &= check(PresentationRender::viewSpaceSubmissionCount() > viewBefore,
                    "local equipped tool uses the generic view-space context",
                    report);

        // REAL equip path: the cold generic equip API is the source of truth;
        // hot tool-presentation consumes it. This is the real swordsword, driven
        // by the real equip state (not a debug-only shortcut).
        std::uint64_t swordActor = 0;
        std::uint64_t swordTool = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &swordActor);
            actx->entityCreate(actx->host, 0u, &swordTool);
            GameTransformComponentV1 tf{};
            tf.position[0] = 30.0f;
            actx->writeComponent(actx->host, swordActor, GAME_COMPONENT_TRANSFORM,
                                 &tf, sizeof(tf));
        }
        const bool equippedReal =
            swordActor != 0 && swordTool != 0 &&
            MimitaNet::actorStateEquipTool(swordActor, swordTool,
                                           gameHash("swordsword"));
        std::uint64_t eqTool = 0, eqKey = 0;
        ok &= check(equippedReal &&
                        MimitaNet::actorStateGetEquippedTool(swordActor, &eqTool,
                                                             &eqKey) &&
                        eqTool == swordTool && eqKey == gameHash("swordsword"),
                    "real equip API resolves the equipped tool EntityId", report);

        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = swordActor;
        }
        runPresentation(60);
        HotPresentationStateV1 swordPres{};
        const bool swordPresRead = actx &&
            actx->dynamicReadComponent(actx->host, swordTool,
                                       HOT_PRESENTATION_COMPONENT, &swordPres,
                                       sizeof(swordPres));
        ok &= check(swordPresRead &&
                        swordPres.meshResourceId == gameHash("mesh.tool.swordsword"),
                    "real swordsword tool carries its logical mesh (hot policy)",
                    report);
        HotAttachmentStateV1 swAtt{};
        const bool swAttRead = actx &&
            actx->dynamicReadComponent(actx->host, swordTool,
                                       HOT_ATTACHMENT_COMPONENT, &swAtt,
                                       sizeof(swAtt));
        ok &= check(swAttRead && swAtt.resolved == 1 &&
                        swAtt.parentEntity == swordActor,
                    "real swordsword attachment resolves to the actor socket",
                    report);

        // Identity survives further presentation changes.
        runPresentation(61);
        std::uint64_t eqTool2 = 0, eqKey2 = 0;
        ok &= check(MimitaNet::actorStateGetEquippedTool(swordActor, &eqTool2,
                                                         &eqKey2) &&
                        eqTool2 == swordTool,
                    "presentation changes preserve the tool EntityId", report);

        // SECOND WEAPON, NO NEW ABI: the real revolver (tool key 1) uses the
        // exact same substrate (binding + attachment + mesh), only hot policy
        // data differs.
        std::uint64_t revActor = 0, revTool = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &revActor);
            actx->entityCreate(actx->host, 0u, &revTool);
            MimitaNet::actorStateEquipTool(revActor, revTool,
                                           gameHash("revolver"));
        }
        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = revActor;
        }
        runPresentation(62);
        HotPresentationStateV1 revPres{};
        const bool revPresRead = actx &&
            actx->dynamicReadComponent(actx->host, revTool,
                                       HOT_PRESENTATION_COMPONENT, &revPres,
                                       sizeof(revPres));
        HotToolClaimV1 revClaim{};
        const bool revClaimRead = actx &&
            actx->dynamicReadComponent(actx->host, revActor,
                                       HOT_TOOL_CLAIM_COMPONENT, &revClaim,
                                       sizeof(revClaim));
        ok &= check(revPresRead &&
                        revPres.meshResourceId == gameHash("mesh.tool.revolver") &&
                        revClaimRead && revClaim.migrated == 1 &&
                        revClaim.toolKey == gameHash("revolver"),
                    "second weapon (revolver) uses the same substrate (no new ABI)",
                    report);

        // Ordering safety: relationship present but the tool component state has
        // not arrived yet -> no crash, no false claim, cold stays the owner.
        std::uint64_t ghostActor = 0, ghostTool = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &ghostActor);
            actx->entityCreate(actx->host, 0u, &ghostTool);
            // Equip edge without a ToolRefState write.
            actx->relationshipAdd(actx->host,
                                  gameHash("relationship.equips-item"),
                                  ghostActor, ghostTool, 4);
        }
        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = ghostActor;
        }
        runPresentation(63);
        HotToolClaimV1 ghostClaim{};
        const bool ghostClaimRead = actx &&
            actx->dynamicReadComponent(actx->host, ghostActor,
                                       HOT_TOOL_CLAIM_COMPONENT, &ghostClaim,
                                       sizeof(ghostClaim));
        HotPresentationStateV1 ghostPres{};
        const bool ghostPresRead = actx &&
            actx->dynamicReadComponent(actx->host, ghostTool,
                                       HOT_PRESENTATION_COMPONENT, &ghostPres,
                                       sizeof(ghostPres));
        ok &= check(ghostClaimRead && ghostClaim.migrated == 0 &&
                        !ghostPresRead,
                    "incomplete tool state waits safely (no false claim/crash)",
                    report);

        // Unmigrated weapon (no hot binding): generic identity exists but no
        // hot claim, so cold fallback stays the owner.
        std::uint64_t sgActor = 0, sgTool = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &sgActor);
            actx->entityCreate(actx->host, 0u, &sgTool);
            MimitaNet::actorStateEquipTool(sgActor, sgTool,
                                           gameHash("weapon.selftest.cold"));
        }
        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = sgActor;
        }
        runPresentation(64);
        HotToolClaimV1 sgClaim{};
        const bool sgClaimRead = actx &&
            actx->dynamicReadComponent(actx->host, sgActor,
                                       HOT_TOOL_CLAIM_COMPONENT, &sgClaim,
                                       sizeof(sgClaim));
        ok &= check(sgClaimRead && sgClaim.migrated == 0,
                    "unmigrated weapon falls back to cold presentation", report);

        // STANDARD WEAPON-SLOT BRIDGE: the exact function the real
        // WeaponSystem::equip calls. It must create the generic tool identity
        // (persistent per actor+key) and equip it, with the typed mirror
        // agreeing. No manual actorStateEquipTool substitution here.
        std::uint64_t bridgeActor = 0;
        if (actx && actx->entityCreate)
            actx->entityCreate(actx->host, 0u, &bridgeActor);
        const std::uint64_t swordTool2 = MimitaNet::actorStateEquipWeaponKey(
            bridgeActor, gameHash("swordsword"),
            static_cast<std::uint32_t>(EntityRealm::Local));
        std::uint64_t bt = 0, bk = 0;
        ok &= check(swordTool2 != 0 && bridgeActor != 0 &&
                        MimitaNet::actorStateGetEquippedTool(bridgeActor, &bt, &bk) &&
                        bt == swordTool2 && bk == gameHash("swordsword"),
                    "standard weapon-slot equip creates the generic tool identity",
                    report);

        if (actx && actx->permanentStorage &&
            actx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(actx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = bridgeActor;
        }
        runPresentation(70);
        HotToolClaimV1 bClaim{};
        HotPresentationStateV1 bSwordPres{};
        const bool bSwordRead = actx &&
            actx->dynamicReadComponent(actx->host, swordTool2,
                                       HOT_PRESENTATION_COMPONENT, &bSwordPres,
                                       sizeof(bSwordPres));
        const bool bClaimRead = actx &&
            actx->dynamicReadComponent(actx->host, bridgeActor,
                                       HOT_TOOL_CLAIM_COMPONENT, &bClaim,
                                       sizeof(bClaim));
        ok &= check(bSwordRead &&
                        bSwordPres.meshResourceId == gameHash("mesh.tool.swordsword") &&
                        bClaimRead && bClaim.migrated == 1 &&
                        bClaim.toolKey == gameHash("swordsword"),
                    "real standard-equip swordsword reaches hot presentation",
                    report);

        // Switch swordsword -> revolver: exactly one equipped tool; the sword
        // tool identity persists but is no longer equipped.
        const std::uint64_t revTool2 = MimitaNet::actorStateEquipWeaponKey(
            bridgeActor, gameHash("revolver"),
            static_cast<std::uint32_t>(EntityRealm::Local));
        std::uint64_t bt2 = 0, bk2 = 0;
        std::uint64_t edges[4] = {0, 0, 0, 0};
        const std::uint32_t edgeCount = actx
            ? actx->relationshipQuery(actx->host,
                                      gameHash("relationship.equips-item"),
                                      bridgeActor, edges, nullptr, 4)
            : 0u;
        ok &= check(revTool2 != 0 && revTool2 != swordTool2 &&
                        MimitaNet::actorStateGetEquippedTool(bridgeActor, &bt2,
                                                             &bk2) &&
                        bk2 == gameHash("revolver") && edgeCount == 1 &&
                        edges[0] == revTool2 &&
                        EntityRegistry::instance().alive(
                            static_cast<EntityId>(swordTool2)),
                    "weapon switch keeps one equipped tool; old identity persists",
                    report);

        // Unequip: edge removed, no claim, tool entities persist.
        MimitaNet::actorStateUnequipTool(bridgeActor);
        runPresentation(71);
        std::uint64_t bt3 = 0, bk3 = 0;
        HotToolClaimV1 uClaim{};
        const bool uClaimRead = actx &&
            actx->dynamicReadComponent(actx->host, bridgeActor,
                                       HOT_TOOL_CLAIM_COMPONENT, &uClaim,
                                       sizeof(uClaim));
        ok &= check(!MimitaNet::actorStateGetEquippedTool(bridgeActor, &bt3, &bk3) &&
                        uClaimRead && uClaim.migrated == 0 &&
                        EntityRegistry::instance().alive(
                            static_cast<EntityId>(swordTool2)),
                    "unequip removes the generic edge; no stale claim", report);
    }

    // ── Generic world->screen projection + hot actor overlays ─────────
    {
        GameplayContextV1* actx = LiveBehavior::hostContext(80);
        auto project = actx ? reinterpret_cast<GameWorldProjectFn>(
                                  actx->resolveCapability(actx->host,
                                                          GAME_CAP_WORLD_PROJECT))
                            : nullptr;
        ok &= check(project != nullptr, "world.project capability resolves",
                    report);
        static Camera testCam;
        testCam.pos = glm::vec3(0.0f);
        testCam.front = glm::vec3(1.0f, 0.0f, 0.0f);
        testCam.up = glm::vec3(0.0f, 0.0f, 1.0f);
        testCam.right = glm::vec3(0.0f, -1.0f, 0.0f);
        testCam.yaw = 0.0f;
        testCam.pitch = 0.0f;
        Camera* const savedCamera = gpCamera;
        gpCamera = &testCam;
        if (project) {
            GameWorldProjectV1 front{};
            front.worldPosition[0] = 5.0f;
            const bool frontOk = project(actx->host, &front) && front.visible == 1;
            GameWorldProjectV1 behind{};
            behind.worldPosition[0] = -5.0f;
            project(actx->host, &behind);
            ok &= check(frontOk && behind.visible == 0,
                        "world.project projects front and rejects behind", report);
        }

        // Hot actor overlays compose through render.ui for a generic actor that
        // has only Transform + Health + PresentationState + identity.
        std::uint64_t overlayActor = 0;
        if (actx && actx->entityCreate) {
            actx->entityCreate(actx->host, 0u, &overlayActor);
            GameTransformComponentV1 tf{};
            tf.position[0] = 5.0f;  // in front of the test camera
            actx->writeComponent(actx->host, overlayActor,
                                 GAME_COMPONENT_TRANSFORM, &tf, sizeof(tf));
            GameHealthComponentV1 hp{};
            hp.current = 42;
            hp.max = 100;
            actx->writeComponent(actx->host, overlayActor, GAME_COMPONENT_HEALTH,
                                 &hp, sizeof(hp));
            HotPresentationStateV1 ps{};
            ps.meshResourceId = HOT_MESH_ACTOR;
            ps.scale = 1.0f;
            actx->dynamicWriteComponent(actx->host, overlayActor,
                                        HOT_PRESENTATION_COMPONENT, &ps,
                                        sizeof(ps));
            MimitaNet::actorStateWriteIdentity(overlayActor, "RuntimeActor");
        }
        ok &= check(runtime.hasCommand("hotoverlays"),
                    "hot overlay command registered (no cold switch)", report);
        const std::uint64_t uiBefore = LiveUi::commandCount();
        runtime.runDomain(GAME_DOMAIN_UI, 82, 0.016f,
                          LiveBehavior::hostContext(82));
        ok &= check(LiveUi::commandCount() > uiBefore,
                    "generic actor gets a hot overlay via render.ui", report);
        // Per-actor ownership: the hot path claims the actor so the cold
        // nameplate/healthbar policy yields for it (exactly one owner).
        HotOverlayClaimV1 claim{};
        const bool claimed = overlayActor != 0 &&
            actx->dynamicReadComponent(actx->host, overlayActor,
                                       HOT_OVERLAY_CLAIM_COMPONENT, &claim,
                                       sizeof(claim));
        ok &= check(claimed && claim.owned == 1,
                    "hot overlay claims the actor (cold yields per-actor)",
                    report);
        gpCamera = savedCamera;
        runtime.runCommand("hotoverlays", "0", LiveBehavior::hostContext(84));
    }

    // ── Generic mode HUD claim (MatchHudState + ModeHudClaim) ─────────
    {
        GameplayContextV1* ctx = LiveBehavior::hostContext(90);
        // Move hot UI off the main menu so only the match HUD can emit here.
        runtime.runCommand("uiscreen", "screen.play", LiveBehavior::hostContext(90));
        std::uint64_t hudEntity = 0;
        if (ctx && ctx->entityCreate)
            ctx->entityCreate(ctx->host, 0u, &hudEntity);
        HotMatchHudStateV1 hud{};
        hud.timerSeconds = 90.0f;
        hud.scoreA = 3;
        hud.scoreB = 5;
        hud.phase = 2;
        std::snprintf(hud.labelA, sizeof(hud.labelA), "RED");
        std::snprintf(hud.labelB, sizeof(hud.labelB), "BLUE");
        HotModeHudClaimV1 claim{};
        claim.owned = 0;
        if (ctx && hudEntity != 0) {
            ctx->dynamicWriteComponent(ctx->host, hudEntity,
                                       HOT_MATCH_HUD_COMPONENT, &hud,
                                       sizeof(hud));
            ctx->dynamicWriteComponent(ctx->host, hudEntity,
                                       HOT_MODE_HUD_CLAIM_COMPONENT, &claim,
                                       sizeof(claim));
        }
        const std::uint64_t beforeCold = LiveUi::commandCount();
        runtime.runDomain(GAME_DOMAIN_UI, 91, 0.016f,
                          LiveBehavior::hostContext(91));
        ok &= check(LiveUi::commandCount() == beforeCold,
                    "cold owns the mode HUD when the claim is not owned", report);

        claim.owned = 1;
        if (ctx && hudEntity != 0)
            ctx->dynamicWriteComponent(ctx->host, hudEntity,
                                       HOT_MODE_HUD_CLAIM_COMPONENT, &claim,
                                       sizeof(claim));
        const std::uint64_t beforeHot = LiveUi::commandCount();
        runtime.runDomain(GAME_DOMAIN_UI, 92, 0.016f,
                          LiveBehavior::hostContext(92));
        ok &= check(LiveUi::commandCount() > beforeHot,
                    "hot mode HUD composes when the claim is owned", report);
        // Cleanup so later "no state" checks are deterministic.
        if (ctx && hudEntity != 0) {
            ctx->dynamicRemoveComponent(ctx->host, hudEntity,
                                        HOT_MATCH_HUD_COMPONENT);
            ctx->dynamicRemoveComponent(ctx->host, hudEntity,
                                        HOT_MODE_HUD_CLAIM_COMPONENT);
        }
    }

    // ── Generic UI action event + hot menu composition ────────────────
    {
        ok &= check(runtime.hasCommand("uiscreen"),
                    "hot UI command registered (no cold switch)", report);
        // The standalone hot main menu is disabled during normal gameplay so
        // it cannot cover the world. Enable it explicitly here to retain a
        // focused hot-UI composition test without changing runtime behavior.
        runtime.runCommand("uiscreen", "screen.main-menu",
                           LiveBehavior::hostContext(94));
        runtime.runCommand("uiscreen", "screen.main-menu",
                           LiveBehavior::hostContext(95));
        GameplayContextV1* uiCtx = LiveBehavior::hostContext(95);
        std::uint64_t uiActor = 0;
        if (uiCtx && uiCtx->entityCreate)
            uiCtx->entityCreate(uiCtx->host, 0u, &uiActor);
        if (uiCtx && uiCtx->permanentStorage &&
            uiCtx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared =
                reinterpret_cast<GameSharedStateV1*>(uiCtx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = uiActor;  // claim carrier
        }
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 96, 0.016f,
                          LiveBehavior::hostContext(96));
        ok &= check(LiveUi::buttonCount() == 3,
                    "hot main menu emits interactive widgets", report);

        // Synthetic click on the PLAY button center.
        const bool consumed = LiveUi::handlePointerClick(640.0f, 212.0f, 96);
        ok &= check(consumed,
                    "cold backend reports the click as a generic ui.action", report);

        HotUiClaimV1 uiClaim{};
        const bool claimRead = uiCtx && uiActor != 0 &&
            uiCtx->dynamicReadComponent(uiCtx->host, uiActor,
                                        HOT_UI_CLAIM_COMPONENT, &uiClaim,
                                        sizeof(uiClaim));
        ok &= check(claimRead && uiClaim.owned == 1 &&
                        uiClaim.screenId == gameHash("screen.main-menu"),
                    "hot UI claims the screen (cold legacy menu yields)", report);

        // Navigation state lives in a migratable dynamic component (not a module
        // static), so it survives a hot generation swap.
        HotUiNavigationStateV1 nav{};
        const bool navRead = uiCtx && uiActor != 0 &&
            uiCtx->dynamicReadComponent(uiCtx->host, uiActor,
                                        HOT_UI_NAV_COMPONENT, &nav, sizeof(nav));
        ok &= check(navRead && nav.screenId == gameHash("screen.server-browser"),
                    "hot navigation state is migratable component state", report);

        // PLAY now routes to the hot server browser (default owner): hot no
        // longer owns the main menu.
        runtime.runCommand("hotoverlays", "0", LiveBehavior::hostContext(97));
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 97, 0.016f,
                          LiveBehavior::hostContext(97));
        ok &= check(!LiveUi::hotOwnsScreen(gameHash("screen.main-menu")),
                    "hot navigation moved off the main menu after the action",
                    report);

        // Generation safety: beginFrame clears any stale claim, so a generation
        // that fails to re-claim falls back to the cold owner (no blank UI).
        LiveUi::beginFrame();
        ok &= check(!LiveUi::hotOwnsScreen(gameHash("screen.main-menu")),
                    "hot screen claim is recomputed per frame (generation-safe)",
                    report);
    }

    // ── Generic setting seam + hot settings screen ────────────────────
    {
        GameplayContextV1* ctx = LiveBehavior::hostContext(100);
        auto get = ctx ? reinterpret_cast<GameSettingGetFn>(
                             ctx->resolveCapability(ctx->host, GAME_CAP_SETTING_GET))
                       : nullptr;
        auto set = ctx ? reinterpret_cast<GameSettingSetFn>(
                             ctx->resolveCapability(ctx->host, GAME_CAP_SETTING_SET))
                       : nullptr;
        ok &= check(get && set, "setting.get/set capabilities resolve", report);
        if (get && set) {
            GameSettingV1 s{};
            s.settingId = gameHash("video.fov");
            s.type = GAME_SETTING_FLOAT;
            s.floatValue = 123.0f;
            set(ctx->host, &s);
            GameSettingV1 g{};
            g.settingId = gameHash("video.fov");
            get(ctx->host, &g);
            ok &= check(g.ok && g.floatValue > 122.9f && g.floatValue < 123.1f,
                        "setting.get reflects setting.set (engine-authoritative)",
                        report);
            // Validation is the kernel's, not presentation's.
            GameSettingV1 bad{};
            bad.settingId = gameHash("video.fov");
            bad.type = GAME_SETTING_FLOAT;
            bad.floatValue = 999999.0f;
            set(ctx->host, &bad);
            GameSettingV1 g2{};
            g2.settingId = gameHash("video.fov");
            get(ctx->host, &g2);
            ok &= check(g2.ok && g2.floatValue <= 140.0f,
                        "setting.set clamps invalid values (kernel validation)",
                        report);
            // Discrete option setting (graphics preset): index <-> option list.
            GameSettingV1 opt{};
            opt.settingId = gameHash("video.graphicsPreset");
            opt.type = GAME_SETTING_OPTION;
            opt.intValue = 0;
            set(ctx->host, &opt);
            GameSettingV1 og{};
            og.settingId = gameHash("video.graphicsPreset");
            get(ctx->host, &og);
            ok &= check(og.ok && og.type == GAME_SETTING_OPTION &&
                            og.intValue == 0 && og.optionCount >= 3 &&
                            og.optionLabel[0] != '\0',
                        "discrete option setting get/set works (kernel list)",
                        report);
        }

        // Hot settings composition + ownership.
        std::uint64_t sActor = 0;
        if (ctx && ctx->entityCreate)
            ctx->entityCreate(ctx->host, 0u, &sActor);
        if (ctx && ctx->permanentStorage &&
            ctx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared = reinterpret_cast<GameSharedStateV1*>(
                ctx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = sActor;
        }
        runtime.runCommand("uiscreen", "screen.settings",
                           LiveBehavior::hostContext(100));
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 101, 0.016f,
                          LiveBehavior::hostContext(101));
        ok &= check(LiveUi::buttonCount() >= 6,
                    "hot settings screen emits controls (sliders/toggles/back)",
                    report);
        HotUiClaimV1 sClaim{};
        const bool sClaimRead = ctx && sActor != 0 &&
            ctx->dynamicReadComponent(ctx->host, sActor,
                                      HOT_UI_CLAIM_COMPONENT, &sClaim,
                                      sizeof(sClaim));
        ok &= check(sClaimRead && sClaim.owned == 1 &&
                        sClaim.screenId == gameHash("screen.settings"),
                    "hot settings claims the screen (cold settings yields)",
                    report);

        // A UI VALUE_CHANGED action writes through to the engine setting.
        GameUiActionV1 change{};
        change.elementId = gameHash("video.fov");
        change.actionType = GAME_UI_ACTION_VALUE_CHANGED;
        change.value = 130.0f;
        LiveBehavior::dispatchGameplayEvent64(gameHash("ui.action"), &change,
                                              sizeof(change), 101);
        GameSettingV1 g3{};
        g3.settingId = gameHash("video.fov");
        if (get)
            get(ctx->host, &g3);
        ok &= check(change.handled == 1 && g3.ok && g3.floatValue > 129.9f &&
                        g3.floatValue < 130.1f,
                    "hot settings action modifies the real setting", report);
        // Leave no hot screen so later "no state" checks stay deterministic.
        runtime.runCommand("uiscreen", "none", LiveBehavior::hostContext(102));
    }

    // ── Hot scoreboard from generic actor stats ───────────────────────
    {
        GameplayContextV1* ctx = LiveBehavior::hostContext(110);
        runtime.runCommand("uiscreen", "none", LiveBehavior::hostContext(110));
        runtime.runCommand("hotoverlays", "0", LiveBehavior::hostContext(110));
        std::uint64_t actor = 0;
        if (ctx && ctx->entityCreate)
            ctx->entityCreate(ctx->host, 0u, &actor);
        if (ctx && actor != 0) {
            // Runtime-unknown actor: only generic identity/team/stats.
            MimitaNet::actorStateWriteIdentity(actor, "RuntimeActor");
            MimitaNet::actorStateWriteTeam(actor, 1);
            HotActorMatchStatsV1 st{};
            st.score = 7;
            st.flags = 1;   // local highlight
            ctx->dynamicWriteComponent(ctx->host, actor,
                                       HOT_ACTOR_STATS_COMPONENT, &st,
                                       sizeof(st));
            HotScoreboardVisibleV1 vis{};
            vis.visible = 1;   // cold Tab input bridged to generic state
            ctx->dynamicWriteComponent(ctx->host, actor,
                                       HOT_SCOREBOARD_VISIBLE_COMPONENT, &vis,
                                       sizeof(vis));
        }
        if (ctx && ctx->permanentStorage &&
            ctx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared = reinterpret_cast<GameSharedStateV1*>(
                ctx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = actor;
        }
        const std::uint64_t before = LiveUi::commandCount();
        runtime.runDomain(GAME_DOMAIN_UI, 111, 0.016f,
                          LiveBehavior::hostContext(111));
        ok &= check(LiveUi::commandCount() > before,
                    "runtime-unknown actor appears in the hot scoreboard", report);
        HotUiClaimV1 sbClaim{};
        const bool sbRead = ctx && actor != 0 &&
            ctx->dynamicReadComponent(ctx->host, actor,
                                      HOT_UI_CLAIM_COMPONENT, &sbClaim,
                                      sizeof(sbClaim));
        ok &= check(sbRead && sbClaim.owned == 1 &&
                        sbClaim.screenId == gameHash("screen.scoreboard"),
                    "hot scoreboard claims ownership (cold leaderboard yields)",
                    report);
        // Cleanup so later checks are deterministic.
        if (ctx && actor != 0) {
            ctx->dynamicRemoveComponent(ctx->host, actor,
                                        HOT_ACTOR_STATS_COMPONENT);
            ctx->dynamicRemoveComponent(ctx->host, actor,
                                        HOT_SCOREBOARD_VISIBLE_COMPONENT);
        }
        LiveUi::beginFrame();
    }

    // ── Hot pause menu (Main view) ────────────────────────────────────
    {
        GameplayContextV1* ctx = LiveBehavior::hostContext(120);
        runtime.runCommand("uiscreen", "none", LiveBehavior::hostContext(120));
        std::uint64_t actor = 0;
        if (ctx && ctx->entityCreate)
            ctx->entityCreate(ctx->host, 0u, &actor);
        if (ctx && actor != 0) {
            HotPauseStateV1 ps{};
            ps.viewHash = gameHash("pause.main");
            ps.visible = 1;
            ctx->dynamicWriteComponent(ctx->host, actor, HOT_PAUSE_STATE_COMPONENT,
                                       &ps, sizeof(ps));
        }
        if (ctx && ctx->permanentStorage &&
            ctx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared = reinterpret_cast<GameSharedStateV1*>(
                ctx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = actor;
        }
        HotPauseStateV1 psRead{};
        const bool psWrote = ctx && actor != 0 &&
            ctx->dynamicReadComponent(ctx->host, actor, HOT_PAUSE_STATE_COMPONENT,
                                      &psRead, sizeof(psRead));
        ok &= check(psWrote && psRead.visible == 1 &&
                        psRead.viewHash == gameHash("pause.main"),
                    "pause state bridge writes generic state", report);
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 121, 0.016f,
                          LiveBehavior::hostContext(121));
        ok &= check(LiveUi::buttonCount() >= 6,
                    "hot pause menu emits its Main-view widgets", report);
        HotUiClaimV1 pClaim{};
        const bool pRead = ctx && actor != 0 &&
            ctx->dynamicReadComponent(ctx->host, actor,
                                      HOT_UI_CLAIM_COMPONENT, &pClaim,
                                      sizeof(pClaim));
        ok &= check(pRead && pClaim.owned == 1 &&
                        pClaim.screenId == gameHash("screen.pause"),
                    "hot pause claims the screen (cold Main view yields)", report);
        // Hot UI-sound policy: a click plays a logical sound via audio.play.
        {
            GameUiActionV1 snd{};
            snd.elementId = gameHash("pause.resume");
            snd.actionType = GAME_UI_ACTION_CLICK;
            const std::uint64_t before = LiveBehavior::audioPlayCount();
            LiveBehavior::dispatchGameplayEvent64(gameHash("ui.action"), &snd,
                                                  sizeof(snd), 122);
            ok &= check(LiveBehavior::audioPlayCount() > before,
                        "hot UI-sound policy plays through audio.play", report);
        }
        // Pause Settings routes to the hot settings screen with a return target.
        {
            GameUiActionV1 go{};
            go.elementId = gameHash("pause.settings");
            go.actionType = GAME_UI_ACTION_CLICK;
            LiveBehavior::dispatchGameplayEvent64(gameHash("ui.action"), &go,
                                                  sizeof(go), 123);
            HotUiNavigationStateV1 nav{};
            const bool navRead = ctx && actor != 0 &&
                ctx->dynamicReadComponent(ctx->host, actor,
                                          HOT_UI_NAV_COMPONENT, &nav, sizeof(nav));
            ok &= check(navRead && nav.screenId == gameHash("screen.settings") &&
                            nav.previousScreenId == gameHash("screen.pause"),
                        "pause Settings routes to hot settings with return", report);
        }
        if (ctx && actor != 0) {
            ctx->dynamicRemoveComponent(ctx->host, actor,
                                        HOT_PAUSE_STATE_COMPONENT);
            ctx->dynamicRemoveComponent(ctx->host, actor, HOT_UI_NAV_COMPONENT);
        }
        LiveUi::beginFrame();
    }

    // ── Hot server browser from generic listing facts ─────────────────
    {
        GameplayContextV1* ctx = LiveBehavior::hostContext(130);
        runtime.runCommand("hotoverlays", "0", LiveBehavior::hostContext(130));
        std::uint64_t actor = 0;
        if (ctx && ctx->entityCreate)
            ctx->entityCreate(ctx->host, 0u, &actor);
        if (ctx && actor != 0) {
            HotServerListingV1 l{};
            l.listingId = gameHash("selftest-room-1");
            l.players = 3;
            l.maxPlayers = 16;
            l.pingMs = 42;
            l.flags = HOT_SERVER_LISTING_REACHABLE;
            std::snprintf(l.code, sizeof(l.code), "ABCD");
            std::snprintf(l.name, sizeof(l.name), "Runtime Server");
            std::snprintf(l.map, sizeof(l.map), "arena");
            std::snprintf(l.mode, sizeof(l.mode), "tdm");
            ctx->dynamicWriteComponent(ctx->host, actor,
                                       HOT_SERVER_LISTING_COMPONENT, &l,
                                       sizeof(l));
        }
        if (ctx && ctx->permanentStorage &&
            ctx->permanentStorageSize >= sizeof(GameSharedStateV1)) {
            auto* shared = reinterpret_cast<GameSharedStateV1*>(
                ctx->permanentStorage);
            if (shared->magic == GAME_SHARED_MAGIC)
                shared->localPlayerEntity = actor;
        }
        runtime.runCommand("uiscreen", "screen.server-browser",
                           LiveBehavior::hostContext(130));
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 131, 0.016f,
                          LiveBehavior::hostContext(131));
        ok &= check(LiveUi::buttonCount() >= 2,
                    "runtime-unknown server listing appears in the hot browser",
                    report);
        HotUiClaimV1 bClaim{};
        const bool bRead = ctx && actor != 0 &&
            ctx->dynamicReadComponent(ctx->host, actor,
                                      HOT_UI_CLAIM_COMPONENT, &bClaim,
                                      sizeof(bClaim));
        ok &= check(bRead && bClaim.owned == 1 &&
                        bClaim.screenId == gameHash("screen.server-browser"),
                    "hot server browser claims the screen (cold yields)", report);
        // Generic text input: focus the join-code field, type, and verify the
        // hot-owned migratable text state updates (backend never owns the text).
        LiveUi::handlePointerClick(100.0f, 610.0f, 132);
        LiveUi::handleTextChar('A');
        LiveUi::handleTextChar('B');
        HotUiTextStateV1 t{};
        const bool tRead = ctx && actor != 0 &&
            ctx->dynamicReadComponent(ctx->host, actor, HOT_UI_TEXT_COMPONENT, &t,
                                      sizeof(t));
        ok &= check(tRead && std::strcmp(t.text, "AB") == 0,
                    "generic text input updates hot-owned bounded text state",
                    report);
        if (ctx && actor != 0) {
            ctx->dynamicRemoveComponent(ctx->host, actor, HOT_UI_TEXT_COMPONENT);
            ctx->dynamicRemoveComponent(ctx->host, actor,
                                        HOT_SERVER_LISTING_COMPONENT);
        }
        runtime.runCommand("uiscreen", "none", LiveBehavior::hostContext(132));
        LiveUi::beginFrame();
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

        runtime.runCommand("hotoverlays", "0", LiveBehavior::hostContext(1));
        LiveUi::beginFrame();
        runtime.runDomain(GAME_DOMAIN_UI, 1, 0.016f, LiveBehavior::hostContext(1));
        ok &= check(!LiveUi::hotOwnsScreen(0),
                    "hot ui.frame fails safe without match HUD state (no hot owner)",
                    report);

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
        HotModeHudClaimV1 hudClaim{};
        hudClaim.owned = 1;   // hot composition owns this mode's HUD
        const bool wroteHud =
            hudMatch != 0 &&
            DynamicComponentStore::instance().write(hudMatch, HOT_MATCH_HUD_COMPONENT,
                                                     &hud, sizeof(hud)) &&
            DynamicComponentStore::instance().write(
                hudMatch, HOT_MODE_HUD_CLAIM_COMPONENT, &hudClaim,
                sizeof(hudClaim));
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
