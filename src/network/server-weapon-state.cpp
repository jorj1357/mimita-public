// 09 14 2026
/* purpose
* Implements component-authoritative weapon runtime state on the tool entity.
* Does NOT own networking, input, or the legacy maps.
*/
#include "network/server-weapon-state.h"

#include <cstddef>

#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "ecs/relationship-store.h"
#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace {

struct WeaponToolStateV1 {
    std::int32_t magazineAmmo;
    std::int32_t reserveAmmo;
    std::uint64_t nextAllowedFireTick;
    std::uint32_t reloading;
    std::uint32_t reloadCompleteTick;
    std::uint32_t stateRevision;
    std::uint32_t initialized;
};

const std::uint64_t kStateId = gameHash("WeaponToolState");
const std::uint64_t kToolIdId = gameHash("WeaponToolId");
const std::uint64_t kRelContains = gameHash("relationship.contains-item");

void ensureSchemas()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    MimitaRuntime::DynamicComponentSchema state;
    state.typeId = kStateId;
    state.schemaHash = gameHash("WeaponToolState.v1");
    state.version = 1;
    state.size = sizeof(WeaponToolStateV1);
    state.align = 8;
    state.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    state.name = "WeaponToolState";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(state);

    MimitaRuntime::DynamicComponentSchema id;
    id.typeId = kToolIdId;
    id.schemaHash = gameHash("WeaponToolId.v1");
    id.version = 1;
    id.size = sizeof(std::uint64_t);
    id.align = 8;
    id.copyPolicy = GAME_COPY_IDENTITY_ONLY;
    id.name = "WeaponToolId";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(id);
}

} // namespace

bool serverWeaponIsMigrated(const std::string& weaponId)
{
    // Exactly one weapon migrated this pass; the rest stay on the legacy map.
    return weaponId == "revolver";
}

std::uint64_t serverWeaponToolEntity(ServerPlayer& player, const std::string& weaponId,
                                     bool create)
{
    ensureSchemas();
    const std::uint64_t key = gameHash(weaponId.c_str());
    const EntityId actor =
        Ecs::ensure(EntityRealm::Server, EntityDomain::Player, player.id);
    MimitaRuntime::RelationshipStore& rel = MimitaRuntime::RelationshipStore::instance();
    std::uint64_t items[32] = {0};
    const std::size_t count = rel.query(kRelContains, actor, items, nullptr, 32);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t id = 0;
        if (MimitaRuntime::DynamicComponentStore::instance().read(
                static_cast<EntityId>(items[i]), kToolIdId, &id, sizeof(id)) &&
            id == key)
            return items[i];
    }
    if (!create)
        return 0;

    const EntityId tool = EntityRegistry::instance().createGeneric(EntityRealm::Server);
    MimitaRuntime::DynamicComponentStore::instance().write(tool, kToolIdId, &key,
                                                           sizeof(key));
    rel.add(kRelContains, actor, tool, key);

    // One-time import of the current legacy runtime values.
    WeaponToolStateV1 state{};
    auto it = player.weaponRuntimes.find(weaponId);
    if (it != player.weaponRuntimes.end()) {
        state.magazineAmmo = it->second.magazineAmmo;
        state.reserveAmmo = it->second.reserveAmmo;
        state.nextAllowedFireTick = it->second.nextAllowedFireTick;
        state.reloading = it->second.reloading ? 1u : 0u;
        state.reloadCompleteTick = it->second.reloadCompleteTick;
        state.stateRevision = it->second.stateRevision;
        state.initialized = it->second.initialized ? 1u : 0u;
    }
    MimitaRuntime::DynamicComponentStore::instance().write(tool, kStateId, &state,
                                                           sizeof(state));
    return static_cast<std::uint64_t>(tool);
}

void serverWeaponStateLoad(ServerPlayer& player, const std::string& weaponId)
{
    if (!serverWeaponIsMigrated(weaponId))
        return;
    const std::uint64_t tool = serverWeaponToolEntity(player, weaponId, true);
    if (tool == 0)
        return;
    WeaponToolStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(tool), kStateId, &state, sizeof(state)))
        return;
    ServerPlayer::ServerWeaponRuntime& rt = player.weaponRuntimes[weaponId];
    rt.magazineAmmo = state.magazineAmmo;
    rt.reserveAmmo = state.reserveAmmo;
    rt.nextAllowedFireTick = state.nextAllowedFireTick;
    rt.reloading = state.reloading != 0;
    rt.reloadCompleteTick = state.reloadCompleteTick;
    rt.stateRevision = state.stateRevision;
    rt.initialized = state.initialized != 0;
}

void serverWeaponStateStore(ServerPlayer& player, const std::string& weaponId)
{
    if (!serverWeaponIsMigrated(weaponId))
        return;
    const std::uint64_t tool = serverWeaponToolEntity(player, weaponId, true);
    if (tool == 0)
        return;
    auto it = player.weaponRuntimes.find(weaponId);
    if (it == player.weaponRuntimes.end())
        return;
    WeaponToolStateV1 state{};
    state.magazineAmmo = it->second.magazineAmmo;
    state.reserveAmmo = it->second.reserveAmmo;
    state.nextAllowedFireTick = it->second.nextAllowedFireTick;
    state.reloading = it->second.reloading ? 1u : 0u;
    state.reloadCompleteTick = it->second.reloadCompleteTick;
    state.stateRevision = it->second.stateRevision;
    state.initialized = it->second.initialized ? 1u : 0u;
    MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(tool), kStateId, &state, sizeof(state));
}

bool serverWeaponStateReadComponent(ServerPlayer& player, const std::string& weaponId,
                                    std::int32_t* magazineAmmo,
                                    std::int32_t* reserveAmmo,
                                    std::uint64_t* nextAllowedFireTick)
{
    const std::uint64_t tool = serverWeaponToolEntity(player, weaponId, false);
    if (tool == 0)
        return false;
    WeaponToolStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(tool), kStateId, &state, sizeof(state)))
        return false;
    if (magazineAmmo)
        *magazineAmmo = state.magazineAmmo;
    if (reserveAmmo)
        *reserveAmmo = state.reserveAmmo;
    if (nextAllowedFireTick)
        *nextAllowedFireTick = state.nextAllowedFireTick;
    return true;
}

bool serverWeaponStateWriteComponent(ServerPlayer& player, const std::string& weaponId,
                                     std::int32_t magazineAmmo,
                                     std::int32_t reserveAmmo,
                                     std::uint64_t nextAllowedFireTick)
{
    const std::uint64_t tool = serverWeaponToolEntity(player, weaponId, true);
    if (tool == 0)
        return false;
    WeaponToolStateV1 state{};
    MimitaRuntime::DynamicComponentStore::instance().read(
        static_cast<EntityId>(tool), kStateId, &state, sizeof(state));
    state.magazineAmmo = magazineAmmo;
    state.reserveAmmo = reserveAmmo;
    state.nextAllowedFireTick = nextAllowedFireTick;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(tool), kStateId, &state, sizeof(state));
}

} // namespace MimitaNet
