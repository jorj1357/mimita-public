#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "render/lighting-config.h"

void registerLightingCommands()
{
    Terminal::instance().registerCommand({
        "lighting_reload", "Reload config/lighting.json from disk", "lighting_reload",
        [](const std::vector<std::string>&) {
            if (LightingConfig::instance().pollReload())
                Terminal::instance().addLog("[LIGHTING] Reloaded");
            else
                Terminal::instance().addLog("[LIGHTING] No changes or failed to load");
        }
    });
    Terminal::instance().registerCommand({
        "lighting_info", "Print current lighting config values", "lighting_info",
        [](const std::vector<std::string>&) {
            const auto& d = LightingConfig::instance().data();
            Terminal::instance().addLog("[LIGHTING] ambient=" +
                std::to_string(d.ambientColor.r) + "," +
                std::to_string(d.ambientColor.g) + "," +
                std::to_string(d.ambientColor.b) + " intensity=" +
                std::to_string(d.ambientIntensity));
            Terminal::instance().addLog("[LIGHTING] post brightness=" +
                std::to_string(d.brightness) + " contrast=" +
                std::to_string(d.contrast) + " saturation=" +
                std::to_string(d.saturation) + " gamma=" +
                std::to_string(d.gamma) + " hueShift=" +
                std::to_string(d.hueShift));
        }
    });
    Terminal::instance().registerCommand({
        "lighting_reset", "Reset lighting config to defaults", "lighting_reset",
        [](const std::vector<std::string>&) {
            LightingConfig::instance().reset();
            Terminal::instance().addLog("[LIGHTING] Reset to defaults");
        }
    });
}
