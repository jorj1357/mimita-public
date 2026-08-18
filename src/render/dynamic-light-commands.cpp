// aug 18 2026, 14 30
/* purpose
* Registers terminal commands for dynamic light debugging and hot-reload.
* Commands: dlight_info, dlight_reload, dlight_debug.
* Does NOT own the DynamicLightManager or DynamicLightConfig singletons.
* Does NOT spawn or render lights.
*/
#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "render/dynamic-light-config.h"
#include "render/dynamic-light.h"

void registerDynamicLightCommands()
{
    Terminal::instance().registerCommand({
        "dlight_info", "Print dynamic light config and active light count", "dlight_info",
        [](const std::vector<std::string>&) {
            const auto& cfg = DynamicLightConfig::instance().data();
            auto& mgr = DynamicLightManager::instance();
            Terminal::instance().addLog("[DLIGHT] maxActive=" + std::to_string(cfg.maxActive) +
                " maxPerFrame=" + std::to_string(cfg.maxPerFrame) +
                " quality=" + cfg.quality);
            Terminal::instance().addLog("[DLIGHT] active=" + std::to_string(mgr.activeCount()) +
                " submitted=" + std::to_string(mgr.submittedCount()));
            int weaponCount = 0;
            for (auto& [wid, effects] : cfg.weaponLights)
                for (auto& [eid, s] : effects)
                    if (s.enabled) ++weaponCount;
            Terminal::instance().addLog("[DLIGHT] weapon light configs enabled=" + std::to_string(weaponCount));
        }
    });
    Terminal::instance().registerCommand({
        "dlight_reload", "Reload dynamic light config from lighting.json", "dlight_reload",
        [](const std::vector<std::string>&) {
            if (DynamicLightConfig::instance().pollReload())
                Terminal::instance().addLog("[DLIGHT] Reloaded config/lighting.json");
            else
                Terminal::instance().addLog("[DLIGHT] No changes or failed to load");
        }
    });
    Terminal::instance().registerCommand({
        "dlight_debug", "Toggle dynamic light debug overlay", "dlight_debug [0|1]",
        [](const std::vector<std::string>& args) {
            auto& mgr = DynamicLightManager::instance();
            if (args.empty()) {
                mgr.enableDebugOverlay(!mgr.debugOverlay());
            } else {
                mgr.enableDebugOverlay(args[0] == "1");
            }
            const bool on = mgr.debugOverlay();
            printf("[DLIGHT] debug=%s\n", on ? "ON" : "OFF");
            Terminal::instance().addLog(std::string("[DLIGHT] debug=") + (on ? "ON" : "OFF"));
        }
    });
}
