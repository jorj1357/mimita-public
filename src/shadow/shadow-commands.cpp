#include "shadow-commands.h"
#include "shadow-config.h"
#include "shadow-render.h"

#include <cstdio>
#include "devtools/terminal.h"

void registerShadowCommands()
{
    Terminal::instance().registerCommand({
        "shadow_info", "Print shadow config values", "shadow_info",
        [](const std::vector<std::string>&) {
            const auto& d = ShadowConfig::instance().data();
            char buf[512];
            snprintf(buf, sizeof(buf),
                "[SHADOWS] enabled=%d mapSize=%d distance=%.1f bias=%.4f darkness=%.3f softness=%.2f tint=(%.2f,%.2f,%.2f) stabilize=%d",
                (int)d.enabled, d.shadowMapSize, d.shadowDistance, d.shadowBias,
                d.shadowDarkness, d.shadowSoftness,
                d.shadowTint.r, d.shadowTint.g, d.shadowTint.b, (int)d.stabilize);
            Terminal::instance().addLog(buf);
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "shadow_reload", "Reload config/shadows.json", "shadow_reload",
        [](const std::vector<std::string>&) {
            ShadowConfig::instance().load("config/shadows.json");
            Terminal::instance().addLog("[SHADOWS] Reloaded config/shadows.json");
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "shadow_debug", "Toggle shadow debug overlay", "shadow_debug [0|1]",
        [](const std::vector<std::string>& args) {
            bool on = args.empty() ? !ShadowConfig::instance().data().debugDrawShadowFrustum : (args[0] != "0");
            ShadowConfig::instance().data().debugDrawShadowFrustum = on;
            Terminal::instance().addLog(std::string("[SHADOWS] debug frustum=") + (on ? "1" : "0"));
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "shadow_showmap", "Display shadow map on screen", "shadow_showmap [0|1]",
        [](const std::vector<std::string>& args) {
            bool on = args.empty() ? !showShadowMap() : (args[0] != "0");
            setShowShadowMap(on);
            Terminal::instance().addLog(std::string("[SHADOWS] show shadow map=") + (on ? "1" : "0"));
        },
        "2026-06-15", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "shadow_preset", "Set shadow quality preset", "shadow_preset <off|low|medium|high>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[SHADOWS] Usage: shadow_preset <off|low|medium|high>");
                return;
            }
            int size = 0;
            bool enabled = true;
            if (args[0] == "off") { enabled = false; size = 0; }
            else if (args[0] == "low") { size = 512; }
            else if (args[0] == "medium") { size = 1024; }
            else if (args[0] == "high") { size = 2048; }
            else {
                Terminal::instance().addLog("[SHADOWS] Unknown preset: " + args[0]);
                return;
            }
            auto& cfg = ShadowConfig::instance();
            cfg.data().enabled = enabled;
            cfg.data().shadowMapSize = size;
            Terminal::instance().addLog(std::string("[SHADOWS] Preset applied: ") + args[0] +
                " (size=" + std::to_string(size) + ")");
        },
        "2026-06-15", CommandCategory::Debug
    });
}
