// 09 12 2026
/* purpose
* Implements the entity inspection terminal command.
* Reports live entities, domains, legacy ids, control sources, and health.
* Does NOT modify registry state or run the destructive self-test.
*/
#include "terminal/entity-commands.h"

#include "devtools/terminal.h"
#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"

#include <string>
#include <vector>

void registerEntityCommands()
{
    Terminal::instance().registerCommand(
        {
            "entity_list", "List live entities and their control sources",
            "entity_list",
            [](const std::vector<std::string>&) {
                auto& registry = EntityRegistry::instance();
                const std::vector<EntityId> ids = registry.all();
                Terminal::instance().addLog(
                    "[ENTITY] count=" + std::to_string(ids.size()));
                for (EntityId id : ids) {
                    const EntityIdentity* identity = registry.identity(id);
                    const ControlSourceComponent* control =
                        registry.tryGet<ControlSourceComponent>(id);
                    const HealthComponent* health =
                        registry.tryGet<HealthComponent>(id);
                    std::string line =
                        "[ENTITY] id=" + std::to_string(
                            static_cast<unsigned long long>(id)) +
                        " domain=" + (identity ? entityDomainName(identity->domain) : "?") +
                        " legacy=" + std::to_string(identity ? identity->legacyId : 0) +
                        " control=" + (control ? controlSourceName(control->source) : "unset");
                    if (health)
                        line += " hp=" + std::to_string(health->current) + "/" +
                                std::to_string(health->max);
                    Terminal::instance().addLog(line);
                }
            },
        },
        "2026-09-12", CommandCategory::Debug);
}
