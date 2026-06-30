#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include "devtools/terminal.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/validate-assets.h"
#include "physics/movement/physics-collision.h"
#include "config/player-settings.h"
#include "audio/audio.h"
#include "terminal/terminal-state.h"

void registerDebugCommands()
{
    auto registerDebugToggle = [](const char* name, bool& flag) {
        Terminal::instance().registerCommand({
            name, std::string("Toggle ") + name, std::string(name) + " [0|1]",
            [&flag, name](const std::vector<std::string>& args) {
                flag = args.empty() ? !flag : args[0] != "0";
                Terminal::instance().addLog(std::string("[DEBUG] ") + name + "=" + (flag ? "1" : "0"));
            }
        });
    };
    registerDebugToggle("debug_ticks", DebugConfig::DEBUG_TICKS);
    registerDebugToggle("debug_input", DebugConfig::DEBUG_INPUT);
    registerDebugToggle("debug_collision", DebugConfig::COLLISION_VERBOSE);
    registerDebugToggle("debug_npc", DebugConfig::DEBUG_NPC);
    registerDebugToggle("debug_commands", DebugConfig::DEBUG_COMMANDS);
    registerDebugToggle("debug_blood_rays", DebugConfig::DEBUG_BLOOD_RAYS);
    registerDebugToggle("debug_blood_hits", DebugConfig::DEBUG_BLOOD_HITS);
    registerDebugToggle("debug_blood_force", DebugConfig::DEBUG_BLOOD_FORCE);
    registerDebugToggle("debug_debris", DebugConfig::DEBUG_DEBRIS);
    registerDebugToggle("godball_debug", DebugConfig::DEBUG_GODBALL);
    registerDebugToggle("final_kill_debug", DebugConfig::DEBUG_NPC_DEATH);
    registerDebugToggle("collision_debug", DebugConfig::DEBUG_COLLISION_SYSTEM);
    registerDebugToggle("collision_trace", DebugConfig::DEBUG_COLLISION_TRACE);
    registerDebugToggle("collision_debug_player", DebugConfig::DEBUG_COLLISION_PLAYER);
    registerDebugToggle("collision_debug_limb", DebugConfig::DEBUG_COLLISION_LIMB);
    registerDebugToggle("show_body_colliders", DebugConfig::DEBUG_COLLISION_LIMB);
    registerDebugToggle("body_collision_push", DebugConfig::DEBUG_COLLISION_BODY_PUSH);
    registerDebugToggle("show_body_contacts", DebugConfig::DEBUG_COLLISION_SYSTEM);
    registerDebugToggle("debug_collisions", DebugConfig::DEBUG_COLLISION_GRID);
    registerDebugToggle("collision_validate", DebugConfig::DEBUG_COLLISION_VALIDATE);
    registerDebugToggle("collision_draw_triangles", DebugConfig::DEBUG_COLLISION_SYSTEM);
    registerDebugToggle("collision_draw_contacts", DebugConfig::DEBUG_COLLISION_SYSTEM);
    registerDebugToggle("collision_draw_capsule", DebugConfig::DEBUG_COLLISION_PLAYER);
    registerDebugToggle("collision_draw_sweep", DebugConfig::DEBUG_COLLISION_SYSTEM);
    registerDebugToggle("npc_damage_debug", DebugConfig::DEBUG_NPC_COMBAT);
    registerDebugToggle("npc_movement_debug", DebugConfig::DEBUG_NPC_MOVEMENT);
    registerDebugToggle("ragdoll_debug", DebugConfig::DEBUG_RAGDOLL);
    registerDebugToggle("replay_debug", DebugConfig::DEBUG_REPLAY);
    registerDebugToggle("bombtag_debug", DebugConfig::DEBUG_BOMBTAG);
    registerDebugToggle("networking_debug", DebugConfig::DEBUG_NETWORKING);
    registerDebugToggle("duel_debug", DebugConfig::DEBUG_DUEL);
    registerDebugToggle("animation_debug", DebugConfig::DEBUG_ANIMATION);
    registerDebugToggle("debug_perf_model", DebugConfig::DEBUG_PERF_MODEL);
    registerDebugToggle("ui_debug", DebugConfig::DEBUG_UI);
    registerDebugToggle("physics_debug", DebugConfig::DEBUG_PHYSICS);
    registerDebugToggle("combat_debug", DebugConfig::DEBUG_NPC_COMBAT);
    registerDebugToggle("render_debug", DebugConfig::DEBUG_RENDER);

    Terminal::instance().registerCommand({
        "collision_dump_frame", "Print the last GLB collision trace summary", "collision_dump_frame",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(collisionLastTraceSummary());
        },
        "2026-06-21", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "collision_stress_run", "Run a deterministic synthetic collision stress case",
        "collision_stress_run <wedge1|wedge5|wedge10|wedge20|dash|cone>",
        [](const std::vector<std::string>& args) {
            const std::string caseName = args.empty() ? "wedge5" : args[0];
            Terminal::instance().addLog(collisionStressRun(caseName));
        },
        "2026-06-21", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "debug_combat", "Enable combat calculation logging", "debug_combat <true|false>",
        [](const std::vector<std::string>& args) {
            bool& enabled = GetPlayerSettings().debugCombat;
            enabled = args.empty() ? !enabled : (args[0] == "true" || args[0] == "1");
            SavePlayerSettings();
            Terminal::instance().addLog(std::string("[DEBUG] debug_combat=") + (enabled ? "true" : "false"));
        }
    });
    Terminal::instance().registerCommand({
        "sound_debug", "Toggle centralized sound logs", "sound_debug <0|1>",
        [](const std::vector<std::string>& args) {
            bool enabled = args.empty() ? !AudioManager::instance().debug() : args[0] != "0";
            AudioManager::instance().setDebug(enabled);
            Terminal::instance().addLog(std::string("[SOUND] debug ") + (enabled ? "enabled" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "dbgvis", "Master toggle for all debug visuals", "dbgvis <0|1>",
        [](const std::vector<std::string>& args) {
            bool enabled = args.empty() ? !DebugVis::masterEnabled() : args[0] != "0";
            DebugVis::setMasterEnabled(enabled);
            DebugVis::saveConfig();
            Terminal::instance().addLog(std::string("[DEBUG VISUALS] ") + (enabled ? "enabled" : "disabled"));
        }
    });
    Terminal::instance().registerCommand({
        "debugvis_status", "Show debug visualization status", "debugvis_status",
        [](const std::vector<std::string>&) {
            int enabled = DebugVis::masterEnabled() ? 1 : 0;
            std::string configPath = "config/debug/debug-settings.json";
            int configLoaded = std::filesystem::exists(configPath) ? 1 : 0;
            char buf[256];
            snprintf(buf, sizeof(buf),
                "debugVisualizationEnabled=%d\n"
                "configLoaded=%d\n"
                "configPath=%s",
                enabled, configLoaded, configPath.c_str());
            Terminal::instance().addLog(buf);
        }
    });
    Terminal::instance().registerCommand({
        "validate_assets", "Validate all game assets", "validate_assets",
        [](const std::vector<std::string>&) {
            int failed = validateAllAssets();
            char buf[64];
            snprintf(buf, sizeof(buf), "[VALIDATE] %s", failed ? "SOME ASSETS FAILED" : "All assets OK");
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "forcedash", "Force the dash pose on/off for testing", "forcedash <0|1>",
        [](const std::vector<std::string>& args) {
            if (!gpPlayer) {
                Terminal::instance().addLog("[ERROR] no player");
                return;
            }
            if (args.empty()) {
                gpPlayer->forceDashPose = !gpPlayer->forceDashPose;
            } else {
                gpPlayer->forceDashPose = args[0] != "0";
            }
            Terminal::instance().addLog(
                std::string("[DEBUG] forcedash=") + (gpPlayer->forceDashPose ? "1" : "0"));
        },
        "2026-06-28", CommandCategory::Debug
    });
}
