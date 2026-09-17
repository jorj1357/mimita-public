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
#include <glm/gtc/matrix_transform.hpp>

#include "ecs/actor-entities.h"
#include "ecs/components.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/prediction-registry.h"
#include "entities/player.h"
#include "hot-reload/hot-action.h"
#include "hot-reload/hot-animation.h"
#include "hot-reload/hot-prediction.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/hot-projectile.h"
#include "hot-reload/hot-ui.h"
#include "network/actor-state.h"
#include "network/community-match-client.h"
#include "network/server-browser.h"
#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "terminal/terminal-state.h"
#include "ecs/entity-types.h"
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

    // Generic action facts the hot animation state machine consumes. Cold side
    // publishes facts only; the hot module owns the animation decision. The hot
    // package (animation-policy) registers the identical schema at activation.
    MimitaRuntime::DynamicComponentSchema actionSchema;
    actionSchema.typeId = HOT_ACTOR_ACTION_COMPONENT;
    actionSchema.schemaHash = gameHash("ActorActionState.v1");
    actionSchema.version = HOT_ACTION_STATE_VERSION;
    actionSchema.size = sizeof(HotActorActionStateV1);
    actionSchema.align = 8;
    actionSchema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    actionSchema.networkPolicy = GAME_NET_ALL;
    actionSchema.name = "ActorActionState";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(actionSchema);
}

// Publish the generic action facts for one actor from existing cold state. This
// is a bridge, not a policy owner: it never selects an animation.
void writeActionState(EntityId entity, const ::Player& player)
{
    if (entity == kInvalidEntityId)
        return;
    ensureSchema();
    HotActorActionStateV1 st{};
    st.version = HOT_ACTION_STATE_VERSION;
    st.byteSize = static_cast<std::uint32_t>(sizeof(HotActorActionStateV1));
    st.lifecycleGeneration = player.spawnGeneration;
    st.speed = std::sqrt(player.vel.x * player.vel.x +
                         player.vel.y * player.vel.y);
    if (player.ground.onGround)
        st.flags |= HOT_ACTION_FLAG_GROUNDED;
    if (player.dead)
        st.flags |= HOT_ACTION_FLAG_DEAD;

    // Generic movement intent/runtime facts when the shared components exist.
    EntityRegistry& registry = EntityRegistry::instance();
    if (const MovementIntentComponent* mi =
            registry.tryGet<MovementIntentComponent>(entity)) {
        if (mi->jump)
            st.flags |= HOT_ACTION_FLAG_JUMPING;
        if (mi->dash)
            st.flags |= HOT_ACTION_FLAG_DASHING;
        if (mi->downDash)
            st.flags |= HOT_ACTION_FLAG_DOWN_DASH;
        if (mi->freeze)
            st.flags |= HOT_ACTION_FLAG_FREEZING;
    }
    if (const MovementRuntimeStateComponent* mr =
            registry.tryGet<MovementRuntimeStateComponent>(entity)) {
        if (mr->grounded)
            st.flags |= HOT_ACTION_FLAG_GROUNDED;
        else
            st.flags &= ~HOT_ACTION_FLAG_GROUNDED;
    }

    // Replicated network weapon-state bits (the only action data remote actors
    // currently carry; timers below are local-authoritative detail).
    const std::uint8_t ns = player.networkWeaponState;
    if (ns & MimitaNet::NET_WEAPON_STATE_FIRING)
        st.flags |= HOT_ACTION_FLAG_SHOOTING;
    if (ns & MimitaNet::NET_WEAPON_STATE_RELOADING)
        st.flags |= HOT_ACTION_FLAG_RELOADING;
    if (ns & MimitaNet::NET_WEAPON_STATE_EQUIPPING)
        st.flags |= HOT_ACTION_FLAG_EQUIPPING;
    st.isReloading = (ns & MimitaNet::NET_WEAPON_STATE_RELOADING) ? 1u : 0u;

    // The generic equips-item relationship is the source of truth for whether
    // a tool is actually in the actor's hands.  Typed Player fields can retain
    // a previous weapon during spawn/unequip reconciliation; using them alone
    // makes the hot pose path apply a weapon carry stance to an empty-handed
    // actor.
    std::uint64_t equippedToolEntity = 0;
    std::uint64_t equippedToolKey = 0;
    const bool hasEquippedTool = MimitaNet::actorStateGetEquippedTool(
        static_cast<std::uint64_t>(entity), &equippedToolEntity,
        &equippedToolKey);
    if (hasEquippedTool && equippedToolEntity != 0 && equippedToolKey != 0)
        st.weaponKey = equippedToolKey;

    auto it = player.weaponRuntimes.find(player.equippedWeaponId);
    if (it != player.weaponRuntimes.end()) {
        const WeaponRuntime& rt = it->second;
        st.reloadTimer = rt.reloadTimer;
        st.fireCooldown = rt.fireCooldown;
        st.shootEffectTimer = rt.shootEffectTimer;
        st.ammo = rt.currentAmmo > 0 ? (std::uint32_t)rt.currentAmmo : 0u;
        st.reserve = rt.reserveAmmo > 0 ? (std::uint32_t)rt.reserveAmmo : 0u;
        if (rt.isReloading)
            st.isReloading = 1u;
        auto et = rt.customFloats.find("equipTimer");
        if (et != rt.customFloats.end())
            st.equipTimer = et->second;
        auto sp = rt.customFloats.find("swordPoseState");
        if (sp != rt.customFloats.end())
            st.meleeAction = (std::uint32_t)(sp->second > 0.0f ? sp->second : 0.0f);
        if (st.meleeAction != 0)
            st.flags |= HOT_ACTION_FLAG_MELEE;
    }

    MimitaRuntime::DynamicComponentStore::instance().write(
        entity, HOT_ACTOR_ACTION_COMPONENT, &st, sizeof(st));
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

std::uint64_t actorEntityFor(std::uint32_t actorId, bool isPlayer)
{
    if (actorId == 0)
        return 0;
    if (isPlayer)
        return static_cast<std::uint64_t>(Ecs::ensure(EntityRealm::ClientReplicated,
                                                      EntityDomain::Player, actorId));
    return static_cast<std::uint64_t>(Ecs::ensure(EntityRealm::ClientReplicated,
                                                  EntityDomain::Npc, actorId));
}

void projectScoreboardVisible()
{
    const std::uint64_t entity = actorEntityFor(MP_CONTEXT.localPlayerId, true);
    if (entity == 0)
        return;
    HotScoreboardVisibleV1 v{};
    v.visible = MP_CONTEXT.showPlayerList ? 1u : 0u;
    MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), HOT_SCOREBOARD_VISIBLE_COMPONENT, &v,
        sizeof(v));
}

void projectServerListings()
{
    std::vector<MimitaNet::ServerBrowserEntry> entries;
    MimitaNet::serverBrowserEntries(entries);
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    std::unordered_set<EntityId> touched;
    for (const MimitaNet::ServerBrowserEntry& e : entries) {
        const std::uint32_t key =
            (std::uint32_t)(gameHash(e.code.c_str()) & 0xFFFFFFFFu);
        const EntityId entity =
            Ecs::ensure(EntityRealm::ClientReplicated, EntityDomain::None, key);
        HotServerListingV1 lst{};
        lst.listingId = gameHash(e.code.c_str());
        lst.players = e.players;
        lst.maxPlayers = e.maxPlayers;
        lst.pingMs = e.ping.reachable ? (std::int32_t)e.ping.pingMs : -1;
        lst.flags = (e.ping.reachable ? HOT_SERVER_LISTING_REACHABLE : 0u) |
                    (e.passwordProtected ? HOT_SERVER_LISTING_PASSWORD : 0u);
        std::snprintf(lst.code, sizeof(lst.code), "%s", e.code.c_str());
        std::snprintf(lst.name, sizeof(lst.name), "%s", e.serverName.c_str());
        std::snprintf(lst.map, sizeof(lst.map), "%s", e.map.c_str());
        std::snprintf(lst.mode, sizeof(lst.mode), "%s", e.gamemode.c_str());
        store.write(entity, HOT_SERVER_LISTING_COMPONENT, &lst, sizeof(lst));
        touched.insert(entity);
    }
    // Remove listings that disappeared (no stale rows/pointers).
    EntityId live[128] = {};
    const std::uint32_t count =
        store.enumerate(HOT_SERVER_LISTING_COMPONENT, live, 128);
    for (std::uint32_t i = 0; i < count; ++i)
        if (touched.find(live[i]) == touched.end())
            store.remove(live[i], HOT_SERVER_LISTING_COMPONENT);
}

void projectMatchStats()
{
    const auto& actors =
        MimitaNet::CommunityMatchClient::instance().actorIdentities();
    if (actors.empty())
        return;
    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    for (const auto& a : actors) {
        if (a.actorId == 0)
            continue;
        const std::uint64_t entity = actorEntityFor(a.actorId, true);
        if (entity == 0)
            continue;
        if (a.name[0] != '\0')
            MimitaNet::actorStateWriteIdentity(entity, a.name);
        MimitaNet::actorStateWriteTeam(entity, a.team);
        HotActorMatchStatsV1 st{};
        st.score = a.score;
        st.flags = (a.actorId == MP_CONTEXT.localPlayerId) ? 1u : 0u;
        store.write(static_cast<EntityId>(entity), HOT_ACTOR_STATS_COMPONENT, &st,
                    sizeof(st));
    }
}

void projectActorOverlayState(std::uint32_t actorId, bool isPlayer,
                              const Player& player)
{
    if (actorId == 0)
        return;
    const EntityId entity =
        static_cast<EntityId>(actorEntityFor(actorId, isPlayer));
    if (entity == kInvalidEntityId)
        return;
    const float yawRad = glm::radians(player.yaw);
    const glm::vec3 look(std::cos(yawRad), std::sin(yawRad), 0.0f);
    Ecs::setTransform(entity, player.pos, look, player.yaw, 0.0f);
    Ecs::setHealth(entity, player.currentHp, player.maxHp, player.dead);
    writeActionState(entity, player);
    if (!player.username.empty())
        MimitaNet::actorStateWriteIdentity(static_cast<std::uint64_t>(entity),
                                           player.username.c_str());
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

static void projectPlayerEntity(Player& player, EntityId entity)
{
    ensureSchema();
    const float yawRad = glm::radians(player.yaw);
    const glm::vec3 look(std::cos(yawRad), std::sin(yawRad), 0.0f);
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
        HotAnimationStateV2 anim{};
        anim.version = HOT_ANIMATION_STATE_VERSION;
        anim.byteSize = static_cast<std::uint32_t>(sizeof(HotAnimationStateV2));
        anim.actionId = HOT_ACTION_IDLE;
        anim.playbackRate = 1.0f;
        anim.loop = 1;
        anim.blendWeight = 1.0f;
        store.write(entity, HOT_ANIMATION_STATE_COMPONENT, &anim, sizeof(anim));
    }
    writeActionState(entity, player);

    // Generic actor identity for hot overlays/chat/scoreboard (no typed Player
    // needed downstream).
    if (!player.username.empty())
        MimitaNet::actorStateWriteIdentity(static_cast<std::uint64_t>(entity),
                                           player.username.c_str());

    // Lifecycle reconciliation for the local actor's generic equip identity: on
    // spawn/respawn/join/reconnect the typed mirror may already name a weapon
    // while the generic `equips-item` edge has not been (re)created yet. Only
    // write when it disagrees, so the generic edge stays the cross-system
    // identity and this never fights it every frame.
    if (player.hasValidWeapon && !player.equippedWeaponId.empty()) {
        const std::uint64_t key = gameHash(player.equippedWeaponId.c_str());
        std::uint64_t tool = 0, tkey = 0;
        if (!MimitaNet::actorStateGetEquippedTool(
                static_cast<std::uint64_t>(entity), &tool, &tkey) ||
            tkey != key) {
            MimitaNet::actorStateEquipWeaponKey(
                static_cast<std::uint64_t>(entity), key,
                static_cast<std::uint32_t>(EntityRealm::Local));
        }
    }
}

void projectLocalPlayer(Player& player)
{
    projectPlayerEntity(player, Ecs::ensureLocalPlayerEntity());
}

void projectPreviewPlayer(Player& player)
{
    projectPlayerEntity(player,
                        Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 2));
}

static void applyHotPoseEntity(Player& player, EntityId entity)
{
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
        // SkeletonInstances stores radians (hot boundary unit); the legacy
        // typed field is degrees, so convert rather than copy.
        part.pose.rotationEuler = glm::degrees(glm::vec3(
            bone->rotationEuler[0], bone->rotationEuler[1], bone->rotationEuler[2]));
        // Write the local node transform so the typed renderer (which uses
        // perfectPoseSkeleton world transforms) reflects the hot pose.
        if (part.nodeIndex >= 0 &&
            part.nodeIndex < (int)player.perfectPoseSkeleton.nodes.size()) {
            const glm::mat4 restM =
                player.perfectPoseSkeleton.restLocalTransforms[part.nodeIndex];
            glm::mat4 m = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(bone->translation[0], bone->translation[1],
                          bone->translation[2]));
            m = glm::rotate(m, bone->rotationEuler[0], glm::vec3(1, 0, 0));
            m = glm::rotate(m, bone->rotationEuler[1], glm::vec3(0, 1, 0));
            m = glm::rotate(m, bone->rotationEuler[2], glm::vec3(0, 0, 1));
            player.perfectPoseSkeleton.nodes[part.nodeIndex].localTransform =
                restM * m;
        }
    }
    player.updateModelWorldTransforms();
}

void applyHotPoseToPlayer(Player& player)
{
    // Ragdoll owns the local body while active; the hot pose must not overwrite.
    if (player.ragdollModeActive)
        return;
    applyHotPoseEntity(player, Ecs::ensureLocalPlayerEntity());
}

void applyHotPoseToPreview(Player& player)
{
    applyHotPoseEntity(player,
                       Ecs::ensure(EntityRealm::Local, EntityDomain::Player, 2));
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
