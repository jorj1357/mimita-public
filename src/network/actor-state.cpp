// 09 14 2026
/* purpose
* Implements generic authoritative actor team/role/profile state and target
* relationship. Does NOT own spawn/destroy, AI policy, or the renderer.
*/
#include "network/actor-state.h"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace {

constexpr std::uint64_t kTeamId = gameHash("ActorTeamState");
constexpr std::uint64_t kRoleId = gameHash("ActorRoleState");
constexpr std::uint64_t kProfileId = gameHash("ActorProfileState");
constexpr std::uint64_t kIdentityId = gameHash("ActorIdentityState");
constexpr std::uint64_t kOriginId = gameHash("ActorOriginState");
constexpr std::uint64_t kLifecycleId = gameHash("ActorLifecycleState");
constexpr std::uint64_t kAvatarId = gameHash("ActorAvatarState");
constexpr std::uint64_t kNetStateId = gameHash("ActorNetState");
constexpr std::uint64_t kWeaponStateId = gameHash("ActorWeaponState");
constexpr std::uint64_t kTargetsRel = gameHash("relationship.targets");
constexpr std::uint64_t kToolRefId = gameHash("ToolRefState");
constexpr std::uint64_t kContainsItemRel = gameHash("relationship.contains-item");
constexpr std::uint64_t kEquipsItemRel = gameHash("relationship.equips-item");

struct ToolRefStateV1 {
    std::uint64_t toolKey;   // runtime tool key (gameHash or network id)
    std::uint64_t reserved;
};

std::unordered_map<std::uint64_t, std::string>& roleIdByHash()
{
    static std::unordered_map<std::uint64_t, std::string> map;
    return map;
}

void ensureSchema(std::uint64_t id, std::uint64_t hash, std::uint32_t size,
                  std::uint32_t align, const char* name)
{
    // Always (re)register: a store clear must not leave the schema missing.
    MimitaRuntime::DynamicComponentSchema schema;
    schema.typeId = id;
    schema.schemaHash = hash;
    schema.version = 1;
    schema.size = size;
    schema.align = align;
    schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    schema.networkPolicy = GAME_NET_ALL;
    schema.name = name;
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(schema);
}

} // namespace

void actorStateEnsureSchemas()
{
    ensureSchema(kTeamId, gameHash("ActorTeamState.v1"), sizeof(ActorTeamStateV1), 4,
                 "ActorTeamState");
    ensureSchema(kRoleId, gameHash("ActorRoleState.v1"), sizeof(ActorRoleStateV1), 8,
                 "ActorRoleState");
    ensureSchema(kProfileId, gameHash("ActorProfileState.v1"),
                 sizeof(ActorProfileStateV1), 8, "ActorProfileState");
    ensureSchema(kToolRefId, gameHash("ToolRefState.v1"), sizeof(ToolRefStateV1), 8,
                 "ToolRefState");
    ensureSchema(kIdentityId, gameHash("ActorIdentityState.v1"),
                 sizeof(ActorIdentityStateV1), 4, "ActorIdentityState");
    ensureSchema(kOriginId, gameHash("ActorOriginState.v1"),
                 sizeof(ActorOriginStateV1), 8, "ActorOriginState");
    ensureSchema(kLifecycleId, gameHash("ActorLifecycleState.v1"),
                 sizeof(ActorLifecycleComponentV1), 4, "ActorLifecycleState");
    ensureSchema(kAvatarId, gameHash("ActorAvatarState.v1"),
                 sizeof(ActorAvatarStateV1), 8, "ActorAvatarState");
    ensureSchema(kNetStateId, gameHash("ActorNetState.v1"),
                 sizeof(ActorNetStateV1), 4, "ActorNetState");
    // Hot input only (ActorNetState is the wire carrier): network policy NONE.
    {
        MimitaRuntime::DynamicComponentSchema schema;
        schema.typeId = kWeaponStateId;
        schema.schemaHash = gameHash("ActorWeaponState.v1");
        schema.version = 1;
        schema.size = sizeof(ActorWeaponStateV1);
        schema.align = 4;
        schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
        schema.networkPolicy = GAME_NET_NONE;
        schema.name = "ActorWeaponState";
        MimitaRuntime::DynamicComponentStore::instance().registerSchema(schema);
    }
}

bool actorStateWriteOrigin(std::uint64_t entity, std::uint32_t origin,
                           std::uint64_t startingWeaponHash)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorOriginStateV1 state{};
    state.origin = origin;
    // The hash is both the component value and the caller's key; store the id
    // hash in startingWeaponNameHash so a reader can resolve the name.
    state.startingWeaponHash = startingWeaponHash;
    state.startingWeaponNameHash = startingWeaponHash;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kOriginId, &state, sizeof(state));
}

bool actorStateReadOrigin(std::uint64_t entity, std::uint32_t* origin,
                          std::uint64_t* startingWeaponHash)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorOriginStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kOriginId, &state, sizeof(state)))
        return false;
    if (origin)
        *origin = state.origin;
    if (startingWeaponHash)
        *startingWeaponHash = state.startingWeaponHash;
    return true;
}

bool actorStateWriteLifecycle(std::uint64_t entity, std::uint32_t lifeGeneration,
                              std::uint32_t dead, float respawnTimer)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorLifecycleComponentV1 state{};
    state.lifeGeneration = lifeGeneration ? lifeGeneration : 1u;
    state.dead = dead;
    state.respawnTimer = respawnTimer;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kLifecycleId, &state, sizeof(state));
}

bool actorStateReadLifecycle(std::uint64_t entity, std::uint32_t* lifeGeneration,
                             std::uint32_t* dead, float* respawnTimer)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorLifecycleComponentV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kLifecycleId, &state, sizeof(state)))
        return false;
    if (lifeGeneration)
        *lifeGeneration = state.lifeGeneration;
    if (dead)
        *dead = state.dead;
    if (respawnTimer)
        *respawnTimer = state.respawnTimer;
    return true;
}

bool actorStateWriteAvatar(std::uint64_t entity, std::uint64_t avatarHash,
                           std::uint32_t generation)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorAvatarStateV1 state{};
    state.avatarHash = avatarHash;
    state.generation = generation;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kAvatarId, &state, sizeof(state));
}

bool actorStateReadAvatar(std::uint64_t entity, std::uint64_t* avatarHash,
                          std::uint32_t* generation)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorAvatarStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kAvatarId, &state, sizeof(state)))
        return false;
    if (avatarHash)
        *avatarHash = state.avatarHash;
    if (generation)
        *generation = state.generation;
    return true;
}

bool actorStateWriteIdentity(std::uint64_t entity, const char* name)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorIdentityStateV1 state{};
    if (name && name[0])
        std::snprintf(state.name, sizeof(state.name), "%s", name);
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kIdentityId, &state, sizeof(state));
}

bool actorStateReadIdentity(std::uint64_t entity, char* outName,
                            std::uint32_t outSize)
{
    if (entity == 0 || !outName || outSize == 0)
        return false;
    actorStateEnsureSchemas();
    ActorIdentityStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kIdentityId, &state, sizeof(state)))
        return false;
    std::snprintf(outName, outSize, "%s", state.name);
    return true;
}

bool actorStateWriteTeam(std::uint64_t entity, std::int32_t team)
{
    actorStateEnsureSchemas();
    ActorTeamStateV1 state{};
    state.team = team;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kTeamId, &state, sizeof(state));
}

bool actorStateReadTeam(std::uint64_t entity, std::int32_t* team)
{
    actorStateEnsureSchemas();
    ActorTeamStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kTeamId, &state, sizeof(state)))
        return false;
    if (team)
        *team = state.team;
    return true;
}

bool actorStateWriteRole(std::uint64_t entity, const char* roleId)
{
    actorStateEnsureSchemas();
    ActorRoleStateV1 state{};
    if (roleId && roleId[0]) {
        state.roleHash = gameHash(roleId);
        roleIdByHash()[state.roleHash] = roleId;
    }
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kRoleId, &state, sizeof(state));
}

bool actorStateReadRoleHash(std::uint64_t entity, std::uint64_t* roleHash)
{
    actorStateEnsureSchemas();
    ActorRoleStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kRoleId, &state, sizeof(state)))
        return false;
    if (roleHash)
        *roleHash = state.roleHash;
    return true;
}

const char* actorStateRoleIdForHash(std::uint64_t roleHash)
{
    auto it = roleIdByHash().find(roleHash);
    return it == roleIdByHash().end() ? nullptr : it->second.c_str();
}

bool actorStateWriteProfile(std::uint64_t entity, const char* movementPreset,
                            const char* behaviorProfile)
{
    actorStateEnsureSchemas();
    ActorProfileStateV1 state{};
    if (movementPreset && movementPreset[0])
        state.movementPresetHash = gameHash(movementPreset);
    if (behaviorProfile && behaviorProfile[0])
        state.behaviorProfileHash = gameHash(behaviorProfile);
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kProfileId, &state, sizeof(state));
}

bool actorStateReadProfile(std::uint64_t entity, std::uint64_t* movementHash,
                           std::uint64_t* behaviorHash)
{
    actorStateEnsureSchemas();
    ActorProfileStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kProfileId, &state, sizeof(state)))
        return false;
    if (movementHash)
        *movementHash = state.movementPresetHash;
    if (behaviorHash)
        *behaviorHash = state.behaviorProfileHash;
    return true;
}

bool actorStateSetTarget(std::uint64_t sourceEntity, std::uint64_t targetEntity)
{
    if (sourceEntity == 0)
        return false;
    MimitaRuntime::RelationshipStore& rel = MimitaRuntime::RelationshipStore::instance();
    // One target per actor: clear any previous target edges.
    std::uint64_t previous[8] = {0};
    const std::size_t count = rel.query(kTargetsRel, static_cast<EntityId>(sourceEntity),
                                        previous, nullptr, 8);
    for (std::size_t i = 0; i < count; ++i)
        rel.remove(kTargetsRel, static_cast<EntityId>(sourceEntity), previous[i]);
    if (targetEntity == 0)
        return true;
    return rel.add(kTargetsRel, static_cast<EntityId>(sourceEntity),
                   static_cast<EntityId>(targetEntity), 0);
}

bool actorStateGetTarget(std::uint64_t sourceEntity, std::uint64_t* targetEntity)
{
    std::uint64_t out[1] = {0};
    const std::size_t count = MimitaRuntime::RelationshipStore::instance().query(
        kTargetsRel, static_cast<EntityId>(sourceEntity), out, nullptr, 1);
    if (count == 0)
        return false;
    if (targetEntity)
        *targetEntity = out[0];
    return true;
}

void actorStateClearTarget(std::uint64_t sourceEntity)
{
    actorStateSetTarget(sourceEntity, 0);
}

bool actorStateEquipTool(std::uint64_t actorEntity, std::uint64_t toolEntity,
                         std::uint64_t toolKey)
{
    actorStateEnsureSchemas();
    if (actorEntity == 0 || toolEntity == 0)
        return false;
    MimitaRuntime::RelationshipStore& rel = MimitaRuntime::RelationshipStore::instance();
    ToolRefStateV1 ref{};
    ref.toolKey = toolKey;
    MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(toolEntity), kToolRefId, &ref, sizeof(ref));
    rel.add(kContainsItemRel, static_cast<EntityId>(actorEntity),
            static_cast<EntityId>(toolEntity), toolKey);
    // One equipped tool per actor.
    std::uint64_t previous[8] = {0};
    const std::size_t count = rel.query(kEquipsItemRel,
                                        static_cast<EntityId>(actorEntity), previous,
                                        nullptr, 8);
    for (std::size_t i = 0; i < count; ++i)
        rel.remove(kEquipsItemRel, static_cast<EntityId>(actorEntity), previous[i]);
    return rel.add(kEquipsItemRel, static_cast<EntityId>(actorEntity),
                   static_cast<EntityId>(toolEntity), toolKey);
}

bool actorStateUnequipTool(std::uint64_t actorEntity)
{
    if (actorEntity == 0)
        return false;
    MimitaRuntime::RelationshipStore& rel = MimitaRuntime::RelationshipStore::instance();
    std::uint64_t previous[8] = {0};
    const std::size_t count = rel.query(kEquipsItemRel,
                                        static_cast<EntityId>(actorEntity), previous,
                                        nullptr, 8);
    for (std::size_t i = 0; i < count; ++i)
        rel.remove(kEquipsItemRel, static_cast<EntityId>(actorEntity), previous[i]);
    return count > 0;
}

// Standard weapon-slot equip bridge: ensure a generic tool entity exists for the
// weapon/tool key and make it the actor's one equipped tool. The tool entity is
// reused across equips (weapon identity persists), so switching never churns
// EntityIds. Presentation/gameplay/effects all read the same tool identity; the
// typed Player fields remain compatibility mirrors.
std::uint64_t actorStateEquipWeaponKey(std::uint64_t actorEntity,
                                       std::uint64_t toolKey,
                                       std::uint32_t realm)
{
    if (actorEntity == 0 || toolKey == 0)
        return 0;
    // Reuse the already-equipped tool when it already represents this key.
    std::uint64_t current = 0, currentKey = 0;
    if (actorStateGetEquippedTool(actorEntity, &current, &currentKey) &&
        current != 0 && currentKey == toolKey &&
        EntityRegistry::instance().alive(static_cast<EntityId>(current)))
        return current;

    // Reuse a persistent tool entity per (actor, key); create on first use.
    static std::unordered_map<std::uint64_t, std::uint64_t> s_toolByActorKey;
    const std::uint64_t mapKey = actorEntity ^ (toolKey * 0x9E3779B97F4A7C15ull);
    std::uint64_t tool = 0;
    auto it = s_toolByActorKey.find(mapKey);
    if (it != s_toolByActorKey.end() &&
        EntityRegistry::instance().alive(static_cast<EntityId>(it->second)))
        tool = it->second;
    if (tool == 0) {
        tool = static_cast<std::uint64_t>(
            EntityRegistry::instance().createGeneric(static_cast<EntityRealm>(realm)));
        s_toolByActorKey[mapKey] = tool;
    }
    actorStateEquipTool(actorEntity, tool, toolKey);
    return tool;
}

bool actorStateGetEquippedTool(std::uint64_t actorEntity, std::uint64_t* toolEntity,
                               std::uint64_t* toolKey)
{
    std::uint64_t out[1] = {0};
    const std::size_t count = MimitaRuntime::RelationshipStore::instance().query(
        kEquipsItemRel, static_cast<EntityId>(actorEntity), out, nullptr, 1);
    if (count == 0)
        return false;
    if (toolEntity)
        *toolEntity = out[0];
    if (toolKey) {
        ToolRefStateV1 ref{};
        if (MimitaRuntime::DynamicComponentStore::instance().read(
                static_cast<EntityId>(out[0]), kToolRefId, &ref, sizeof(ref)))
            *toolKey = ref.toolKey;
        else
            *toolKey = 0;
    }
    return true;
}

bool actorStateHasEquippedTool(std::uint64_t actorEntity)
{
    std::uint64_t tool = 0;
    return actorStateGetEquippedTool(actorEntity, &tool, nullptr);
}

bool actorStateActionHandled(std::uint64_t actorEntity, std::uint64_t tick)
{
    struct CombatHandledStateV1 {
        std::uint64_t lastHandledTick;
        std::uint32_t handled;
        std::uint32_t reserved;
    };
    CombatHandledStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(actorEntity), gameHash("CombatHandledState"),
            &state, sizeof(state)))
        return false;
    return state.handled != 0 && state.lastHandledTick == tick;
}

bool actorStateWriteNetState(std::uint64_t entity, const ActorNetStateV1& state)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kNetStateId, &state, sizeof(state));
}

bool actorStateReadNetState(std::uint64_t entity, ActorNetStateV1* out)
{
    if (entity == 0 || !out)
        return false;
    actorStateEnsureSchemas();
    return MimitaRuntime::DynamicComponentStore::instance().read(
        static_cast<EntityId>(entity), kNetStateId, out, sizeof(*out));
}

bool actorStateWriteWeaponState(std::uint64_t entity, std::int16_t equippedSlot,
                                std::uint8_t weaponState)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorWeaponStateV1 state{};
    state.equippedSlot = equippedSlot;
    state.weaponState = weaponState;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kWeaponStateId, &state, sizeof(state));
}

bool actorStateReadWeaponState(std::uint64_t entity, std::int16_t* equippedSlot,
                               std::uint8_t* weaponState)
{
    if (entity == 0)
        return false;
    actorStateEnsureSchemas();
    ActorWeaponStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kWeaponStateId, &state, sizeof(state)))
        return false;
    if (equippedSlot)
        *equippedSlot = state.equippedSlot;
    if (weaponState)
        *weaponState = state.weaponState;
    return true;
}

} // namespace MimitaNet
