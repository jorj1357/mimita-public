// 09 14 2026
/* purpose
* Implements the client generic presentation-entity bridge for projectiles.
* Ties presentation lifetime to projectile identity and hands drawing to the hot
* presentation system. Does NOT own prediction/interpolation or networking.
*/
#include "render/presentation-entities.h"

#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include <glm/glm.hpp>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/prediction-registry.h"
#include "entities/player.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-projectile.h"
#include "network/packets.h"
#include "project/presentation-resource.h"
#include "render/presentation-render.h"
#include "render/skeleton-instances.h"

namespace PresentationEntities {
namespace {

std::unordered_map<std::uint32_t, std::uint64_t> s_live;
std::unordered_set<std::uint32_t> s_touched;
bool s_syncActive = false;

std::unordered_map<std::uint32_t, std::uint64_t> s_actorLive;
std::unordered_set<std::uint32_t> s_actorTouched;
bool s_actorSyncActive = false;

void ensureSchema()
{
    static bool ready = false;
    if (ready)
        return;
    ready = true;
    MimitaRuntime::DynamicComponentSchema schema;
    schema.typeId = HOT_PRESENTATION_COMPONENT;
    schema.schemaHash = gameHash("PresentationState.v1");
    schema.version = 1;
    schema.size = sizeof(HotPresentationStateV1);
    schema.align = 8;
    schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    schema.networkPolicy = GAME_NET_ALL;
    schema.name = "PresentationState";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(schema);
}

glm::vec3 safeForward(const float velocity[3])
{
    const float x = velocity[0], y = velocity[1], z = velocity[2];
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-4f)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::vec3(x / len, y / len, z / len);
}

} // namespace

bool resourcesForWeapon(std::uint32_t networkWeaponId, std::uint64_t& meshId,
                        std::uint64_t& textureId, float& scale)
{
    if (networkWeaponId == MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER) {
        meshId = HOT_MESH_ROCKET;
        textureId = HOT_TEX_ROCKET;
        scale = 1.0f;
        return true;
    }
    if (networkWeaponId == MimitaNet::NETWORK_WEAPON_GRENADE_LAUNCHER) {
        meshId = HOT_MESH_GRENADE;
        textureId = HOT_TEX_GRENADE;
        scale = 1.0f;
        return true;
    }
    return false;
}

std::uint64_t ensure(std::uint32_t projectileId, const float position[3],
                     const float velocity[3], std::uint64_t meshId,
                     std::uint64_t textureId, float scale)
{
    if (projectileId == 0 || meshId == 0)
        return 0;
    ensureSchema();
    const EntityId entity = Ecs::ensure(EntityRealm::ClientReplicated,
                                        EntityDomain::Projectile, projectileId);
    Ecs::setTransform(entity,
                      glm::vec3(position[0], position[1], position[2]),
                      safeForward(velocity), 0.0f, 0.0f);
    Ecs::setVelocity(entity,
                     glm::vec3(velocity[0], velocity[1], velocity[2]),
                     glm::vec3(0.0f));

    HotPresentationStateV1 present{};
    present.meshResourceId = meshId;
    present.textureResourceId = textureId;
    present.scale = scale > 0.0f ? scale : 1.0f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    MimitaRuntime::DynamicComponentStore::instance().write(
        entity, HOT_PRESENTATION_COMPONENT, &present, sizeof(present));

    s_live[projectileId] = static_cast<std::uint64_t>(entity);
    if (s_syncActive)
        s_touched.insert(projectileId);
    return static_cast<std::uint64_t>(entity);
}

bool has(std::uint32_t projectileId)
{
    return s_live.find(projectileId) != s_live.end();
}

void beginSync()
{
    s_syncActive = true;
    s_touched.clear();
}

void endSync()
{
    for (auto it = s_live.begin(); it != s_live.end();) {
        if (s_touched.find(it->first) != s_touched.end()) {
            ++it;
            continue;
        }
        Ecs::despawn(static_cast<EntityId>(it->second));
        it = s_live.erase(it);
    }
    s_touched.clear();
    s_syncActive = false;
}

void clear()
{
    for (const auto& entry : s_live)
        Ecs::despawn(static_cast<EntityId>(entry.second));
    s_live.clear();
    s_touched.clear();
    s_syncActive = false;
    for (const auto& entry : s_actorLive)
        Ecs::despawn(static_cast<EntityId>(entry.second));
    s_actorLive.clear();
    s_actorTouched.clear();
    s_actorSyncActive = false;
}

std::uint64_t ensureActor(std::uint32_t actorId, const float position[3],
                          const float look[3], std::uint64_t meshResourceId,
                          std::uint64_t textureResourceId, float scale,
                          const float color[4])
{
    if (actorId == 0 || meshResourceId == 0)
        return 0;
    ensureSchema();
    const EntityId entity = Ecs::ensure(EntityRealm::ClientReplicated,
                                        EntityDomain::Npc, actorId);
    const glm::vec3 forward = safeForward(look);
    Ecs::setTransform(entity,
                      glm::vec3(position[0], position[1], position[2]), forward,
                      0.0f, 0.0f);
    Ecs::setVelocity(entity, glm::vec3(0.0f), glm::vec3(0.0f));

    HotPresentationStateV1 present{};
    present.meshResourceId = meshResourceId;
    present.textureResourceId = textureResourceId;
    present.scale = scale > 0.0f ? scale : 1.0f;
    present.color[0] = color ? color[0] : 1.0f;
    present.color[1] = color ? color[1] : 1.0f;
    present.color[2] = color ? color[2] : 1.0f;
    present.color[3] = color ? color[3] : 1.0f;
    MimitaRuntime::DynamicComponentStore::instance().write(
        entity, HOT_PRESENTATION_COMPONENT, &present, sizeof(present));

    s_actorLive[actorId] = static_cast<std::uint64_t>(entity);
    if (s_actorSyncActive)
        s_actorTouched.insert(actorId);
    return static_cast<std::uint64_t>(entity);
}

void beginActorSync()
{
    s_actorSyncActive = true;
    s_actorTouched.clear();
}

void endActorSync()
{
    for (auto it = s_actorLive.begin(); it != s_actorLive.end();) {
        if (s_actorTouched.find(it->first) != s_actorTouched.end()) {
            ++it;
            continue;
        }
        Ecs::despawn(static_cast<EntityId>(it->second));
        it = s_actorLive.erase(it);
    }
    s_actorTouched.clear();
    s_actorSyncActive = false;
}

bool actorMeshReady()
{
    return MimitaRuntime::PresentationResourceProvider::instance().handleOf(
               HOT_MESH_ACTOR) != nullptr;
}

void projectLocalPlayer(Player& player)
{
    ensureSchema();
    const EntityId entity = Ecs::ensureLocalPlayerEntity();
    const glm::vec3 look(std::cos(player.yaw), std::sin(player.yaw), 0.0f);
    Ecs::setTransform(entity, player.pos, look, player.yaw, 0.0f);
    Ecs::setVelocity(entity, player.vel, player.externalImpulse);
    Ecs::setHealth(entity, player.currentHp, player.maxHp, player.dead);

    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    if (!store.has(entity, HOT_PRESENTATION_COMPONENT)) {
        HotPresentationStateV1 present{};
        present.meshResourceId = HOT_MESH_ACTOR;
        present.textureResourceId = HOT_TEX_DEFAULT;
        present.scale = 1.0f;
        present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
        store.write(entity, HOT_PRESENTATION_COMPONENT, &present, sizeof(present));
    }
    if (!store.has(entity, HOT_ANIMATION_STATE_COMPONENT)) {
        HotAnimationStateV1 anim{};
        anim.clipId = HOT_ANIM_IDLE;
        anim.playbackRate = 1.0f;
        anim.loop = 1;
        store.write(entity, HOT_ANIMATION_STATE_COMPONENT, &anim, sizeof(anim));
    }
}

void applyHotPoseToPlayer(Player& player)
{
    const EntityId entity = Ecs::ensureLocalPlayerEntity();
    const SkeletonInstances::Instance* inst = SkeletonInstances::get(entity);
    if (!inst || player.physicalBody.parts.empty())
        return;
    for (auto& part : player.physicalBody.parts) {
        const SkeletonInstances::BonePose* bone =
            SkeletonInstances::findBone(inst, gameHash(part.name.c_str()));
        if (!bone)
            continue;
        part.pose.translation = glm::vec3(bone->translation[0], bone->translation[1],
                                          bone->translation[2]);
        part.pose.rotationEuler = glm::vec3(
            bone->rotationEuler[0], bone->rotationEuler[1], bone->rotationEuler[2]);
    }
}

std::uint64_t ensurePredicted(std::uint64_t predictionKey,
                              std::uint32_t projectileId,
                              const float position[3], const float velocity[3],
                              std::uint64_t meshId, std::uint64_t textureId,
                              float scale, std::uint64_t tick)
{
    if (projectileId == 0 || meshId == 0)
        return 0;
    ensureSchema();
    const EntityId entity = Ecs::ensure(EntityRealm::ClientPredicted,
                                        EntityDomain::Projectile, projectileId);
    Ecs::setTransform(entity,
                      glm::vec3(position[0], position[1], position[2]),
                      safeForward(velocity), 0.0f, 0.0f);
    Ecs::setVelocity(entity,
                     glm::vec3(velocity[0], velocity[1], velocity[2]),
                     glm::vec3(0.0f));

    HotPresentationStateV1 present{};
    present.meshResourceId = meshId;
    present.textureResourceId = textureId;
    present.scale = scale > 0.0f ? scale : 1.0f;
    present.color[0] = present.color[1] = present.color[2] = present.color[3] = 1.0f;
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    store.write(entity, HOT_PRESENTATION_COMPONENT, &present, sizeof(present));

    HotPredictionLinkV1 link{};
    link.predictionKey = predictionKey;
    store.write(entity, HOT_PREDICTION_LINK_COMPONENT, &link, sizeof(link));

    s_live[projectileId] = static_cast<std::uint64_t>(entity);
    if (s_syncActive)
        s_touched.insert(projectileId);

    // The registry may retire this provisional immediately (authority first).
    const EntityId canonicalEntity =
        Ecs::PredictionRegistry::instance().registerProvisional(predictionKey,
                                                                entity, tick);
    return static_cast<std::uint64_t>(canonicalEntity);
}

void associateByLink(std::uint64_t tick)
{
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    EntityRegistry& registry = EntityRegistry::instance();
    EntityId entities[256] = {};
    const std::uint32_t count =
        store.enumerate(HOT_PREDICTION_LINK_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        const EntityId entity = entities[i];
        if (!registry.alive(entity))
            continue;
        HotPredictionLinkV1 link{};
        if (!store.read(entity, HOT_PREDICTION_LINK_COMPONENT, &link, sizeof(link)))
            continue;
        if (link.predictionKey == 0)
            continue;
        // Skip the provisional itself; only the authoritative counterpart
        // associates.
        if (Ecs::PredictionRegistry::instance().provisionalOf(link.predictionKey) ==
            entity)
            continue;
        Ecs::PredictionRegistry::instance().associate(link.predictionKey, entity,
                                                      tick);
    }
}

void projectReplicatedProjectiles()
{
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    EntityRegistry& registry = EntityRegistry::instance();
    EntityId entities[256] = {};
    const std::uint32_t count =
        store.enumerate(HOT_PROJECTILE_COMPONENT, entities, 256);
    for (std::uint32_t i = 0; i < count; ++i) {
        const EntityId entity = entities[i];
        if (!registry.alive(entity))
            continue;  // replicate component but no adopted entity shell yet
        HotProjectileStateV1 state{};
        if (!store.read(entity, HOT_PROJECTILE_COMPONENT, &state, sizeof(state)))
            continue;
        Ecs::setTransform(entity,
                          glm::vec3(state.position[0], state.position[1],
                                    state.position[2]),
                          safeForward(state.velocity), 0.0f, 0.0f);
        Ecs::setVelocity(entity,
                         glm::vec3(state.velocity[0], state.velocity[1],
                                   state.velocity[2]),
                         glm::vec3(0.0f));
        // The authoritative entity presents through its own replicated
        // PresentationState; nothing else is created here.
    }
}

} // namespace PresentationEntities
