// 09 22 2026
/* purpose
* Thin cold entry point for the hitscan consequence pipeline. The logic lives in
* the hot header `hot-reload/hot-consequences.h` (built into the EXE and the
* game DLL from one source), so a consequence bug is a hot fix. This cold
* function only maps the wide cold trace/types into the shared hot call and runs
* it with the same host context the hot behavior uses.
* Does NOT own the trace or weapon values.
*/
#include "network/server-hitscan-outcome.h"

#include <algorithm>
#include <vector>

#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-consequences.h"
#include "network/server-context.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"
#include "ecs/actor-entities.h"
#include "ecs/entity-types.h"
#include "debug/debug-log.h"

namespace MimitaNet {

namespace {

void* MIMITA_GAME_CALL resolveColdCapability(void*, std::uint64_t id)
{
    return MimitaRuntime::GenericRuntime::instance().capability(id);
}

} // namespace

std::uint8_t serverResolveHitscanOutcome(
    SOCKET sock,
    std::unordered_map<std::uint32_t, ServerPlayer>& players,
    std::unordered_map<std::uint32_t, ServerNpc>& npcs,
    const ServerPlayer& shooter,
    const WeaponDefinition& def,
    const WeaponExecution::HitscanTraceResult& trace,
    const glm::vec3& origin,
    const glm::vec3& direction,
    const glm::vec3& worldHit,
    const glm::vec3& worldNormal,
    float maxRange,
    float worldBlockDistance,
    std::uint32_t requestId,
    std::uint32_t clientSimulationTick,
    std::uint32_t claimedTargetId,
    std::uint32_t tick,
    std::uint64_t& totalPacketsOut)
{
    (void)npcs;
    ServerContextV1* context = activeServerContext();
    if (!context || context->players != &players)
    {
        // No server context (or a different player map): build a local context
        // so the shared hot pipeline can still run against these containers.
        static ServerContextV1 local{};
        local.players = &players;
        local.npcs = &npcs;
        local.sock = (std::uintptr_t)sock;
        local.tick = &tick;
        local.totalPacketsOut = &totalPacketsOut;
        setActiveServerContext(&local);
        context = activeServerContext();
    }
    else
    {
        // Ensure the context points at the live containers/tick for this call.
        context->players = &players;
        context->npcs = &npcs;
        context->sock = (std::uintptr_t)sock;
        context->tick = &tick;
        context->totalPacketsOut = &totalPacketsOut;
    }

    GameplayContextV1 gameplay{};
    gameplay.abiVersion = MIMITA_GAME_API_VERSION;
    gameplay.host = context;
    gameplay.tick = tick;
    gameplay.resolveCapability = &resolveColdCapability;
    gameplay.permanentStorage = nullptr;
    gameplay.permanentStorageSize = 0;

    const std::uint32_t weaponNetworkId = networkWeaponTypeForDefinition(def);
    const std::uint32_t weaponDefNetworkId = weaponDefNetworkIdFor(def.id);
    const std::uint64_t attackerEntity = (std::uint64_t)Ecs::ensure(
        EntityRealm::Server, EntityDomain::Player, shooter.id);

    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {direction.x, direction.y, direction.z};
    const float wh[3] = {worldHit.x, worldHit.y, worldHit.z};
    const float wn[3] = {worldNormal.x, worldNormal.y, worldNormal.z};

    const std::uint32_t verdict = HotConsequences::resolveHitscanTrace(
        &gameplay, attackerEntity, def, weaponNetworkId, weaponDefNetworkId, trace,
        requestId, clientSimulationTick, claimedTargetId, o, d, wh, wn, maxRange,
        worldBlockDistance);

    Debug::log(Debug::Category::Weapons,
        "[ATTACK HITSCAN ACCEPT] playerId=%u requestId=%u weapon=%s pellets=%d targets=%zu\n",
        shooter.id, requestId, def.id.c_str(), trace.pelletCount,
        trace.aggregates.size());
    return (std::uint8_t)verdict;
}

} // namespace MimitaNet
