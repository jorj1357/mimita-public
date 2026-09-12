// 09 12 2026
/* purpose
* Implements the authoritative damage policy resolver and safety bound.
* Does NOT own gameplay policy; that lives in the hot behavior module.
*/
#include "network/server-damage-policy.h"

#include "hot-reload/game-api.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-behavior.h"
#include "live-code/live-identity.h"
#include "live-code/live-journal.h"

#include <string>

int serverAuthoritativeDamageLimit()
{
    // Private development: unlimited. The bound exists so production can set a
    // finite cap without touching gameplay code or the hot behavior module.
    return 0;
}

int serverResolveDamagePolicy(const ServerDamagePolicyInput& input,
                              int baseDamage, glm::vec3& knockback)
{
    DamagePolicyV1 payload{};
    payload.attackerEntity = input.attackerEntity;
    payload.victimEntity = input.victimEntity;
    payload.projectileEntity = input.projectileEntity;
    payload.source = input.source;
    payload.victimIsNpc = input.victimIsNpc;
    payload.weaponNetworkId = input.weaponNetworkId;
    payload.distance = input.distance;
    payload.baseDamage = baseDamage;
    payload.outDamage = baseDamage;
    payload.knockbackX = knockback.x;
    payload.knockbackY = knockback.y;
    payload.knockbackZ = knockback.z;

    const bool handled = LiveBehavior::dispatchDamagePolicy(payload, input.tick);
    int finalDamage = handled ? payload.outDamage : baseDamage;
    if (handled) {
        knockback = glm::vec3(payload.knockbackX, payload.knockbackY, payload.knockbackZ);
    }

    // Explicit kernel safety bound, separate from gameplay tuning.
    const int limit = serverAuthoritativeDamageLimit();
    if (finalDamage < 1)
        finalDamage = 1;
    if (limit > 0 && finalDamage > limit)
        finalDamage = limit;

    // Attach the exact active generation/hash so the damage record proves which
    // hot code produced the result.
    const HotReloadSystem::Status liveStatus = HotReloadSystem::instance().status();

    LiveEventJournal::Fields fields;
    fields.actorId = "server";
    fields.projectileId = std::to_string(input.projectileEntity);
    fields.result = handled ? "hot" : "fallback";
    fields.generation = liveStatus.activeGeneration;
    fields.hasGeneration = true;
    fields.codeHash = liveStatus.activeHash;
    fields.extra = std::string("\"event\":\"hot_damage_policy_result\"") +
        ",\"source\":" + std::to_string(input.source) +
        ",\"base_damage\":" + std::to_string(baseDamage) +
        ",\"out_damage\":" + std::to_string(finalDamage) +
        ",\"attacker_entity\":" + std::to_string(input.attackerEntity) +
        ",\"victim_entity\":" + std::to_string(input.victimEntity) +
        ",\"weapon_network_id\":" + std::to_string(input.weaponNetworkId) +
        ",\"distance\":" + std::to_string(input.distance) +
        ",\"module\":\"gameplay\"" +
        ",\"source_file\":\"src/hot-reload/modules/rocket-behavior.cpp\"";
    LiveEventJournal::instance().record("hot_damage_policy_result", fields);
    return finalDamage;
}
