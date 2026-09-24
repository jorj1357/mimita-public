// 09 22 2026
/* purpose
* Thin cold entry point for melee/projectile damage consequences. The logic
* lives in the hot header `hot-reload/hot-consequences.h`; this maps the cold
* victim list into the shared hot call so cold and hot run one implementation.
* Kept as a compatibility primitive for cold callers that cannot build packets
* themselves.
* Does NOT own the trace or weapon values.
*/
#include "network/server-damage-outcome.h"

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-consequences.h"
#include "network/server-context.h"
#include "network/network-weapons.h"
#include "ecs/actor-entities.h"
#include "ecs/entity-types.h"

namespace MimitaNet {

namespace {

void* MIMITA_GAME_CALL resolveDamageCapability(void*, std::uint64_t id)
{
    return MimitaRuntime::GenericRuntime::instance().capability(id);
}

} // namespace

void serverResolveDamageOutcome(
    SOCKET sock,
    std::unordered_map<std::uint32_t, ServerPlayer>& players,
    std::unordered_map<std::uint32_t, ServerNpc>& npcs,
    const ServerPlayer* attacker,
    const WeaponDefinition* def,
    std::uint32_t sourceKind,
    std::uint32_t causeSerial,
    std::uint32_t projectileId,
    const ServerOutcomeVictim* victims,
    std::uint32_t victimCount,
    std::uint32_t tick,
    std::uint64_t& totalPacketsOut)
{
    if (!victims || victimCount == 0)
        return;

    ServerContextV1 context{};
    context.players = &players;
    context.npcs = &npcs;
    context.sock = (std::uintptr_t)sock;
    context.tick = &tick;
    context.totalPacketsOut = &totalPacketsOut;
    setActiveServerContext(&context);

    GameplayContextV1 gameplay{};
    gameplay.abiVersion = MIMITA_GAME_API_VERSION;
    gameplay.host = &context;
    gameplay.tick = tick;
    gameplay.resolveCapability = &resolveDamageCapability;

    const std::uint64_t attackerEntity = attacker
        ? (std::uint64_t)Ecs::ensure(EntityRealm::Server, EntityDomain::Player, attacker->id)
        : 0;
    const std::uint32_t weaponNetworkId = def ? networkWeaponTypeForDefinition(*def) : 0;
    const std::uint32_t weaponDefNetworkId = def ? weaponDefNetworkIdFor(def->id) : 0;

    for (std::uint32_t i = 0; i < victimCount; ++i)
    {
        const ServerOutcomeVictim& v = victims[i];
        if (v.entity == 0)
            continue;
        HotConsequences::applyVictim(
            &gameplay, attackerEntity, v.entity, v.spawnGeneration, sourceKind,
            v.damage, v.knockback, v.hitPosition, v.hitNormal, causeSerial,
            projectileId, weaponNetworkId, weaponDefNetworkId);
    }

    // The context above is stack-owned for this synchronous consequence pass.
    // Do not leave the global bridge pointing at it after the hot damage call
    // returns; the next projectile, lifecycle, or network callback would see
    // a dangling server context.
    setActiveServerContext(nullptr);
}

} // namespace MimitaNet
