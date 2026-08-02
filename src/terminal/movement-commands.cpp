// 08 02 2026, 00 00
/* purpose
* Registers terminal commands for the movement tuning presets.
* Lets users switch presets, list presets, reload, and print active tuning values.
* Reuses the MovementJsonConfig singleton for all preset loading and persistence.
* Does NOT run movement physics, parse movement formulas, or own tuning defaults.
* Does NOT edit movement preset files or the selector file except through savePresetSelection.
*/

#include "terminal/movement-commands.h"

#include <cstdio>
#include <string>
#include <vector>

#include "config/movement-config.h"
#include "devtools/terminal.h"

void registerMovementCommands()
{
    Terminal::instance().registerCommand({
        "movement_presets",
        "List available movement presets from config/movement/",
        "movement_presets",
        [](const std::vector<std::string>&) {
            const auto presets = MovementJsonConfig::instance().availablePresets();
            if (presets.empty()) {
                Terminal::instance().addLog(
                    "[MOVEMENT] No presets found in config/movement/");
                return;
            }
            std::string list = "[MOVEMENT] Available presets:";
            for (const auto& name : presets)
                list += " " + name;
            Terminal::instance().addLog(list);
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_preset",
        "Load a movement preset by name and persist the selection to config/movement.json",
        "movement_preset <name>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[MOVEMENT] Usage: movement_preset <name>. Active: " +
                    MovementJsonConfig::instance().activePresetName());
                return;
            }
            if (!MovementJsonConfig::instance().savePresetSelection(args[0])) {
                Terminal::instance().addLog(
                    "[MOVEMENT] Failed to load preset: " + args[0]);
                return;
            }
            Terminal::instance().addLog(
                "[MOVEMENT] Active preset: " +
                MovementJsonConfig::instance().activePresetName());
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_reload",
        "Reload the active movement preset from disk",
        "movement_reload",
        [](const std::vector<std::string>&) {
            MovementJsonConfig::instance().load(
                MovementJsonConfig::instance().selectorPath());
            Terminal::instance().addLog(
                "[MOVEMENT] Reloaded. Active preset: " +
                MovementJsonConfig::instance().activePresetName());
        }
    }, CommandCategory::Physics);

    Terminal::instance().registerCommand({
        "movement_print",
        "Print the active movement tuning values",
        "movement_print",
        [](const std::vector<std::string>&) {
            const auto& cfg = MovementJsonConfig::instance().config();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] preset=%s mode=%s air_strafing=%d bhop=%d",
                MovementJsonConfig::instance().activePresetName().c_str(),
                cfg.walkMode == MovementWalkMode::Accel ? "accel" : "override",
                (int)cfg.airControlEnabled, (int)cfg.bunnyHopEnabled);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] ground_speed=%.1f air_speed=%.1f ground_accel=%.1f air_accel=%.1f",
                cfg.groundSpeed, cfg.airSpeed,
                cfg.groundAcceleration, cfg.airAcceleration);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] gravity=%.1f jump=%.1f max_fall=%.1f air_jumps=%d",
                cfg.gravityZ, cfg.jumpVerticalSpeed,
                cfg.maximumFallSpeed, cfg.maximumAirJumps);
            Terminal::instance().addLog(buf);
            std::snprintf(buf, sizeof(buf),
                "[MOVEMENT] ground_friction=%.1f air_friction=%.1f "
                "ground_dash=%.1f air_dash=%.1f down_dash=%.1f",
                cfg.groundFrictionAmount, cfg.airFrictionAmount,
                cfg.groundDashImpulse, cfg.airDashImpulse,
                cfg.downDashVerticalSpeed);
            Terminal::instance().addLog(buf);
        }
    }, CommandCategory::Physics);
}
