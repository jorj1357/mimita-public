// 09 14 2026
/* purpose
* Implements generic authoritative actor/entity health as a dynamic component.
* Does NOT own spawn/destroy policy, networking, or the renderer.
*/
#include "network/actor-health.h"

#include "ecs/dynamic-components.h"
#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace {

constexpr std::uint64_t kActorHealthId = gameHash("ActorHealthState");

} // namespace

void actorHealthEnsureSchema()
{
    // Always (re)register: a store clear must not leave the schema missing.
    MimitaRuntime::DynamicComponentSchema schema;
    schema.typeId = kActorHealthId;
    schema.schemaHash = gameHash("ActorHealthState.v1");
    schema.version = 1;
    schema.size = sizeof(ActorHealthStateV1);
    schema.align = 4;
    schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    schema.networkPolicy = GAME_NET_ALL;
    schema.name = "ActorHealthState";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(schema);
}

bool actorHealthHas(std::uint64_t entity)
{
    actorHealthEnsureSchema();
    return MimitaRuntime::DynamicComponentStore::instance().has(
        static_cast<EntityId>(entity), kActorHealthId);
}

bool actorHealthInit(std::uint64_t entity, std::int32_t maxHp)
{
    actorHealthEnsureSchema();
    ActorHealthStateV1 state{};
    state.current = maxHp > 0 ? maxHp : 0;
    state.max = maxHp > 0 ? maxHp : 0;
    state.dead = state.current <= 0 ? 1u : 0u;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kActorHealthId, &state, sizeof(state));
}

bool actorHealthRead(std::uint64_t entity, std::int32_t* current, std::int32_t* maxHp,
                     bool* dead)
{
    actorHealthEnsureSchema();
    ActorHealthStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kActorHealthId, &state, sizeof(state)))
        return false;
    if (current)
        *current = state.current;
    if (maxHp)
        *maxHp = state.max;
    if (dead)
        *dead = state.dead != 0;
    return true;
}

bool actorHealthApplyDamage(std::uint64_t entity, std::int32_t amount,
                            std::int32_t* outAfter, bool* outDead)
{
    ActorHealthStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kActorHealthId, &state, sizeof(state)))
        return false;
    const std::int32_t dmg = amount > 0 ? amount : 1;
    state.current = state.current - dmg > 0 ? state.current - dmg : 0;
    state.dead = state.current <= 0 ? 1u : 0u;
    const bool written = MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kActorHealthId, &state, sizeof(state));
    if (outAfter)
        *outAfter = state.current;
    if (outDead)
        *outDead = state.dead != 0;
    return written;
}

bool actorHealthSet(std::uint64_t entity, std::int32_t current)
{
    ActorHealthStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(entity), kActorHealthId, &state, sizeof(state)))
        return false;
    state.current = current;
    state.dead = current <= 0 ? 1u : 0u;
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(entity), kActorHealthId, &state, sizeof(state));
}

} // namespace MimitaNet
