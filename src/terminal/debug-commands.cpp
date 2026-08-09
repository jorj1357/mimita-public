#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include "devtools/terminal.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/validate-assets.h"
#include "physics/movement/physics-collision.h"
#include "config/player-settings.h"
#include "audio/audio.h"
#include "terminal/terminal-state.h"
#include "gui/hud/player-nameplates.h"
#include "gui/gui-bindings.h"
#include "network/server.h"

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

    // Camera axis debug (not in DebugConfig, standalone global)
    Terminal::instance().registerCommand({
        "cam_axis_debug", "Toggle camera axis debug visualization (RGB arrows at camera)",
        "cam_axis_debug [0|1]",
        [](const std::vector<std::string>& args) {
            extern bool gCamAxisDebug;
            gCamAxisDebug = args.empty() ? !gCamAxisDebug : args[0] != "0";
            Terminal::instance().addLog(std::string("[DEBUG] cam_axis_debug=") + (gCamAxisDebug ? "1" : "0"));
        }
    });
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
    registerDebugToggle("persistent_physics_debug", DebugConfig::DEBUG_PERSISTENT_PHYSICS);
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
    registerDebugToggle("menu_preview_debug", DebugConfig::DEBUG_MENU_PREVIEW);
    registerDebugToggle("world_xh_enabled", DebugConfig::WORLD_XH_ENABLED);

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

    Terminal::instance().registerCommand({
        "healthbar_debug", "Toggle healthbar aim mode debug overlay", "healthbar_debug [0|1]",
        [](const std::vector<std::string>& args) {
            bool val = args.empty() ? !isHealthbarDebugEnabled() : args[0] != "0";
            setHealthbarDebugEnabled(val);
            Terminal::instance().addLog(std::string("[HEALTHBAR] healthbar_debug=") + (val ? "1" : "0"));
        },
        "2026-07-03", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "healthme",
        "Override only your own HP. Syntax: healthme <hp> | healthme default|reset",
        "healthme <value>",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;

            if (args.empty()) {
                Terminal::instance().addLog("[HEALTHME] Usage: healthme <hp> or healthme default|reset");
                return;
            }

            if (args[0] == "default" || args[0] == "reset") {
                DevOverrides::playerHealthOverrideEnabled = false;
                Debug::warn(Debug::Category::General,
                    "\n==================================\n"
                    "Player Health Override Disabled\n"
                    "Using default player health.\n"
                    "==================================\n");
                Terminal::instance().addLog("[HEALTHME] Player override disabled. Next respawn will use normal HP.");
                return;
            }

            int value;
            try { value = std::stoi(args[0]); }
            catch (...) {
                Terminal::instance().addLog("[HEALTHME] Invalid value. Use a positive integer or 'default'.");
                return;
            }

            if (value < 0) {
                Terminal::instance().addLog("[HEALTHME] Negative values not allowed.");
                return;
            }

            DevOverrides::playerHealthOverrideEnabled = true;
            DevOverrides::playerHealthOverrideValue = value;

            // Apply immediately
            player.maxHp = value;
            player.currentHp = value;

            char buf[256];
            snprintf(buf, sizeof(buf),
                "\n==================================\n"
                "Player Health Override Enabled\n"
                "Current HP: %d\n"
                "Max HP: %d\n"
                "Applies on Respawn: YES\n"
                "==================================",
                value, value);
            Debug::warn(Debug::Category::General, "%s\n", buf);
            Terminal::instance().addLog(std::string("[HEALTHME] Player HP set to ") + std::to_string(value));
        },
        "2026-07-04", CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "healthall",
        "Set spawn HP for ALL entities (player, NPCs). Host only. Syntax: healthall <hp> | healthall default|reset",
        "healthall <value>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[HEALTHALL] Usage: healthall <hp> or healthall default|reset");
                return;
            }

            const bool isReset = (args[0] == "default" || args[0] == "reset");
            int value = 0;
            if (!isReset)
            {
                try { value = std::stoi(args[0]); }
                catch (...) {
                    Terminal::instance().addLog("[HEALTHALL] Invalid value. Use a positive integer or 'default'.");
                    return;
                }
                if (value < 0) {
                    Terminal::instance().addLog("[HEALTHALL] Negative values not allowed.");
                    return;
                }
            }

            // In-process server (dedicated --server or local listen server):
            // apply the override directly. Otherwise send the command to the
            // server, which applies it only if the sender is the host.
            if (MimitaNet::isServerHost())
            {
                DevOverrides::healthOverrideEnabled = !isReset;
                DevOverrides::healthOverrideValue = value;
                MimitaNet::serverGameOverrides().maxHpOverride = isReset ? 0 : value;
                if (gpPlayer && !gpPlayer->dead)
                {
                    const int effectiveMax = (isReset || value <= 0) ? 100 : value;
                    gpPlayer->maxHp = effectiveMax;
                    gpPlayer->currentHp = effectiveMax;
                }
                Terminal::instance().addLog(isReset
                    ? "[HEALTHALL] Override disabled. Future spawns use normal HP (100)."
                    : std::string("[HEALTHALL] All-entities spawn HP set to ") + std::to_string(value));
                return;
            }

            if (::gpMpContext && ::gpMpContext->active)
            {
                MimitaNet::mpSendServerCommand(*::gpMpContext,
                    isReset ? "healthall default"
                            : "healthall " + std::to_string(value));
                Terminal::instance().addLog("[HEALTHALL] Sent to server (host only).");
                return;
            }

            Terminal::instance().addLog("[HEALTHALL] HOST ONLY — run this on the server host (or while connected to your own server).");
        },
        "2026-07-30", CommandCategory::Debug
    });

    // GUI binding debug commands
    Terminal::instance().registerCommand({
        "gui_dump_bindings", "Dump all active GUI data bindings",
        "gui_dump_bindings",
        [](const std::vector<std::string>&) {
            const auto& all = GuiBindings::instance().all();
            Terminal::instance().addLog("=== GUI BINDINGS ===");
            for (const auto& kv : all) {
                Terminal::instance().addLog("  " + kv.first + " = " + kv.second);
            }
            Terminal::instance().addLog("Focused input: " +
                (GuiBindings::instance().focusedId().empty()
                    ? "(none)" : GuiBindings::instance().focusedId()));
            Terminal::instance().addLog("=== END ===");
        },
        "2026-07-12", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "gui_highlight_dynamic", "Toggle highlighting of dynamic GUI elements (not implemented yet)",
        "gui_highlight_dynamic [0|1]",
        [](const std::vector<std::string>& args) {
            static bool highlight = false;
            highlight = args.empty() ? !highlight : args[0] != "0";
            Terminal::instance().addLog(std::string("[GUI] highlight_dynamic=") + (highlight ? "1" : "0"));
        },
        "2026-07-12", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "sim.catchup",
        "Set the max fixed simulation ticks that can be caught up per frame after a stall (default 6)",
        "sim.catchup <ticks>",
        [](const std::vector<std::string>& args) {
            extern int gSimMaxCatchupTicks;
            if (args.empty()) {
                Terminal::instance().addLog(
                    std::string("[SIM] gSimMaxCatchupTicks=") + std::to_string(gSimMaxCatchupTicks));
                return;
            }
            int value = 6;
            try { value = std::stoi(args[0]); }
            catch (...) { value = 6; }
            gSimMaxCatchupTicks = std::clamp(value, 1, 120);
            Terminal::instance().addLog(
                std::string("[SIM] gSimMaxCatchupTicks=") + std::to_string(gSimMaxCatchupTicks));
        },
        "2026-08-03", CommandCategory::Debug
    });
}
