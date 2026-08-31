// 08 09 2026, 14 30
/* purpose
* Registers terminal commands for NPC spawning, selection, deletion, difficulty
* overrides, and NPC difficulty config inspection/saving.
* Does NOT implement NPC AI, movement, combat, or config loading itself.
* Does NOT modify NPC behavior at runtime beyond what the config supports.
*/

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "npc/npc-difficulty-config.h"
#include "devtools/dev-npc-selection.h"
#include "physics/config.h"
#include "network/net_mode.h"
#include "network/server.h"
#include "network/multiplayer-context.h"
#include "game/spawn-utils.h"

void registerNpcCommands()
{
    Terminal::instance().registerCommand({
        "npc_spawn", "Spawn NPCs at map spawn points", "npc_spawn <count>",
        [](const std::vector<std::string>& args) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            World& world = THE_WORLD;
            int count = args.empty() ? 1 : std::clamp(std::stoi(args[0]), 1, 100);
            for (int i = 0; i < count; ++i) {
                MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
                if (mpContext.active) {
                    glm::vec3 spawnPos = getSpawnPosition(world, i);
                    MimitaNet::mpRequestNpcSpawn(mpContext, spawnPos, 1.0f);
                } else {
                    uint32_t id = npcSystem.nextNpcId();
                    spawnNpcAtSafePosition(npcSystem, id, 1.0f, world, i);
                }
            }
            Terminal::instance().addLog("[NPC COMMAND] npc_spawn count=" + std::to_string(count));
        }
    });
    Terminal::instance().registerCommand({
        "npc_select_all", "Select every NPC", "npc_select_all",
        [](const std::vector<std::string>&) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            NpcSelectionManager::instance().selectAll(npcSystem);
            Terminal::instance().addLog("[NPC COMMAND] npc_select_all");
        }
    });
    Terminal::instance().registerCommand({
        "npc_force_hit", "Force NPC raycast to always hit (saved to config/npc-difficulty.json)", "npc_force_hit <0|1>",
        [](const std::vector<std::string>& args) {
            NpcDifficultyConfig& cfg = NpcDifficultyConfig::instance();
            cfg.settings().forceHit = args.empty() ? !cfg.settings().forceHit : args[0] != "0";
            cfg.save();
            Terminal::instance().addLog(std::string("[NPC] force hit ") +
                (cfg.settings().forceHit ? "ENABLED" : "disabled") + " (saved to config/npc-difficulty.json)");
        }
    });
    Terminal::instance().registerCommand({
        "npc_delete_selected", "Delete selected NPCs", "npc_delete_selected",
        [](const std::vector<std::string>&) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            std::vector<std::uint32_t> ids(
                NpcSelectionManager::instance().selectedIds().begin(),
                NpcSelectionManager::instance().selectedIds().end());
            npcSystem.destroySelected(ids);
            Terminal::instance().addLog("[NPC COMMAND] npc_delete_selected");
        }
    });
    Terminal::instance().registerCommand({
        "npc_delete_all", "Delete every NPC", "npc_delete_all",
        [](const std::vector<std::string>&) {
            if (!MimitaNet::isServerHost()) {
                if (::gpMpContext && ::gpMpContext->active) {
                    MimitaNet::mpSendServerCommand(*::gpMpContext, "npc_delete_all");
                    Terminal::instance().addLog("[NPC COMMAND] sent to host");
                } else {
                    Terminal::instance().addLog("[NPC COMMAND] HOST ONLY");
                }
                return;
            }
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            npcSystem.destroyAll();
            Terminal::instance().addLog("[NPC COMMAND] npc_delete_all");
        }
    });
    Terminal::instance().registerCommand({
        "npcdifflist", "List JSON NPC difficulty presets", "npcdifflist",
        [](const std::vector<std::string>&) {
            std::vector<std::filesystem::path> presets;
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator("config/npcpresets", ec))
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json") presets.push_back(entry.path());
            std::sort(presets.begin(), presets.end());
            for (size_t i = 0; i < presets.size(); ++i)
                Terminal::instance().addLog(std::to_string(i + 1) + " = " + presets[i].filename().string());
        }
    });
    Terminal::instance().registerCommand({
        "npcdiffload", "Load an NPC difficulty preset; host only", "npcdiffload <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[NPC DIFFICULTY] Usage: npcdiffload <number>"); return; }
            if (!MimitaNet::isServerHost()) {
                if (::gpMpContext && ::gpMpContext->active) {
                    MimitaNet::mpSendServerCommand(*::gpMpContext, "npcdiffload " + args[0]);
                    Terminal::instance().addLog("[NPC DIFFICULTY] sent to host");
                } else Terminal::instance().addLog("[NPC DIFFICULTY] HOST ONLY");
                return;
            }
            std::vector<std::filesystem::path> presets;
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator("config/npcpresets", ec))
                if (entry.is_regular_file(ec) && entry.path().extension() == ".json") presets.push_back(entry.path());
            std::sort(presets.begin(), presets.end());
            const int index = std::atoi(args[0].c_str()) - 1;
            if (index < 0 || index >= (int)presets.size()) {
                Terminal::instance().addLog("[NPC DIFFICULTY] Invalid preset number");
                return;
            }
            if (NpcDifficultyConfig::instance().load(presets[(size_t)index].string()))
            {
                NpcSystem& npcSystem = THE_NPC_SYSTEM;
                npcSystem.refreshDifficultyTuning();
                Terminal::instance().addLog("[NPC DIFFICULTY] loaded " + presets[(size_t)index].filename().string());
            }
            else Terminal::instance().addLog("[NPC DIFFICULTY] load failed");
        }
    });
    Terminal::instance().registerCommand({
        "npc_aim_acc", "Set NPC aim accuracy (0 = decent, 50 = very good, 100 = perfect, negative = terrible). Saved to config/npc-difficulty.json.", "npc_aim_acc <float>",
        [](const std::vector<std::string>& args) {
            NpcDifficultyConfig& cfg = NpcDifficultyConfig::instance();
            if (args.empty()) {
                Terminal::instance().addLog("[NPC] maxAngularErrorDegrees = "
                    + std::to_string(cfg.settings().maxAngularErrorDegrees) + " deg");
                return;
            }
            cfg.settings().maxAngularErrorDegrees = NpcCombat::maxAngularErrorForAccuracy(std::stof(args[0]));
            cfg.save();
            Terminal::instance().addLog("[NPC] maxAngularErrorDegrees set to "
                + std::to_string(cfg.settings().maxAngularErrorDegrees) + " deg (saved to config/npc-difficulty.json)");
        }
    });
    Terminal::instance().registerCommand({
        "npc_diff_status", "Show current NPC difficulty config", "npc_diff_status",
        [](const std::vector<std::string>&) {
            const auto& s = NpcDifficultyConfig::instance().settings();
            char buf[640];
            snprintf(buf, sizeof(buf),
                "[NPC DIFFICULTY] maxErr=%.1fdeg diffScale=%.2f dmg=%.2fx fireDelay=[%.2f,%.2f] aggressionBonus=%.2f forceHit=%d turnSpeed=%.0fdeg/s aim=[%.1f,%.1f]s move=[%.1f,%.1f]s movementPreset=%s",
                s.maxAngularErrorDegrees, s.difficultyErrorScale, s.damageMultiplier,
                s.fireDelayMin, s.fireDelayMax, s.aggressionBonus, (int)s.forceHit,
                s.turnSpeed, s.aimAtTargetMin, s.aimAtTargetMax,
                s.faceMovementMin, s.faceMovementMax, s.movementPreset.c_str());
            Terminal::instance().addLog(buf);
        }
    });
    Terminal::instance().registerCommand({
        "npc_difficulty_all", "Set difficulty for all NPCs (1-10)", "npc_difficulty_all <1-10>",
        [](const std::vector<std::string>& args) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            if (args.empty()) {
                Terminal::instance().addLog("[NPC COMMAND] usage: npc_difficulty_all <1-10> (current: " + std::to_string((int)npcSystem.globalDifficulty()) + ")");
                return;
            }
            float d = std::clamp(std::stof(args[0]), 1.0f, 10.0f);
            npcSystem.setGlobalDifficulty(d);
            Terminal::instance().addLog("[NPC COMMAND] npc_difficulty_all set to " + std::to_string((int)d));
        }
    });
    Terminal::instance().registerCommand({
        "npc_move_preset", "Set NPC movement preset (follow/default/source/counterstrike). Saved to config/npc-difficulty.json.", "npc_move_preset <name>",
        [](const std::vector<std::string>& args) {
            NpcDifficultyConfig& cfg = NpcDifficultyConfig::instance();
            if (args.empty()) {
                Terminal::instance().addLog("[NPC] movementPreset = " + cfg.settings().movementPreset
                    + " (follow/default/source/counterstrike)");
                return;
            }
            cfg.settings().movementPreset = args[0];
            cfg.save();
            cfg.load();
            Terminal::instance().addLog("[NPC] movementPreset set to " + args[0] + " (saved to config/npc-difficulty.json)");
        }
    });
}
