// 09 14 2026
/* purpose
* Implements generic authoritative actor team/role/profile state and target
* relationship. Does NOT own spawn/destroy, AI policy, or the renderer.
*/
#include "network/actor-state.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/dynamic-components.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace {

constexpr std::uint64_t kTeamId = gameHash("ActorTeamState");
constexpr std::uint64_t kRoleId = gameHash("ActorRoleState");
constexpr std::uint64_t kProfileId = gameHash("ActorProfileState");
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
    struct ActorActionStateV1 {
        std::uint64_t lastHandledTick;
        std::uint32_t handled;
        std::uint32_t reserved;
    };
    ActorActionStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(actorEntity), gameHash("ActorActionState"), &state,
            sizeof(state)))
        return false;
    return state.handled != 0 && state.lastHandledTick == tick;
}

} // namespace MimitaNet
