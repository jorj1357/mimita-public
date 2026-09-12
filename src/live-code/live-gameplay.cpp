// 09 12 2026
/* purpose
* Implements the EXE-side bridge to the hot gameplay policy module.
* Owns ABI-safe module invocation only.
*/
#include "live-code/live-gameplay.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-journal.h"
#include "live-code/live-modules.h"

#include <string>

namespace {

const GameGameplayModuleV1* gameplayModule()
{
    return static_cast<const GameGameplayModuleV1*>(
        LiveModules::findFunctions("gameplay", sizeof(GameGameplayModuleV1)));
}

std::string floatField(const char* name, float value)
{
    return std::string("\"") + name + "\":" + std::to_string(value);
}

} // namespace

namespace LiveGameplay {

bool rocketFlight(const RocketFlightStateV1& state,
                  const RocketFlightParamsV1& base,
                  RocketFlightParamsV1& out)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->adjustRocketFlight)
        return false;
    out = base;
    return module->adjustRocketFlight(
        &state, &base, &out, &HotReloadSystem::instance().gameMemory());
}

void journalPolicy(const char* side, const char* kind,
                   std::uint64_t projectileId, std::uint64_t targetId,
                   float baseSpeed, float outSpeed,
                   float baseDamage, float outDamage)
{
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    LiveEventJournal::Fields fields;
    fields.projectileId = std::to_string(projectileId);
    fields.actorId = side ? side : "unknown";
    fields.result = kind ? kind : "policy";
    fields.extra = std::string("\"policy\":\"") + (kind ? kind : "") + "\"" +
        ",\"generation\":" + std::to_string(status.activeGeneration) +
        ",\"code_hash\":\"" + status.activeHash + "\"" +
        ",\"target_entity_id\":" + std::to_string(targetId) +
        "," + floatField("base_speed", baseSpeed) +
        "," + floatField("out_speed", outSpeed) +
        "," + floatField("base_damage", baseDamage) +
        "," + floatField("out_damage", outDamage);
    LiveEventJournal::instance().record("gameplay_policy", fields);
}

} // namespace LiveGameplay
