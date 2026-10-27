// 2026-10-27

#include "physics/runtime-objects-commands.h"

#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "physics/runtime-objects.h"
#include "vehicles/vehicle-system.h"

void registerRuntimeObjectCommands()
{
    Terminal::instance().registerCommand({
        "runtime_clear",
        "Clear every runtime-only physical object (not authored map data)",
        "runtime_clear",
        [](const std::vector<std::string>&) {
            const RuntimeObjectCounts counts =
                clearRuntimeObjects("terminal");
            Terminal::instance().addLog(
                "[RUNTIME] cleared vehicles=" +
                std::to_string(counts.vehicles) + " constraints=" +
                std::to_string(counts.constraints) + " persistent=" +
                std::to_string(counts.persistentObjects) + " corpses=" +
                std::to_string(counts.corpses) + " entities=" +
                std::to_string(counts.physicalEntities));
        }
    });

    Terminal::instance().registerCommand({
        "runtime_status",
        "Print runtime-only object counts",
        "runtime_status",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(
                "[RUNTIME] vehicles=" +
                std::to_string(VehicleSystem::instance().vehicles().size()));
        }
    });
}
