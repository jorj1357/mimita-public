#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
extern float gNpcMaxInaccuracyDegrees;
#include "devtools/dev-npc-selection.h"
#include "physics/config.h"
#include "network/net_mode.h"

void registerNpcCommands()
{
    Terminal::instance().registerCommand({
        "npc_spawn", "Spawn NPCs at the global spawn point", "npc_spawn <count>",
        [](const std::vector<std::string>& args) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            int count = args.empty() ? 1 : std::clamp(std::stoi(args[0]), 1, 100);
            for (int i = 0; i < count; ++i) {
                npcSystem.spawnNpc(1.0f);
                MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
                if (mpContext.active)
                    MimitaNet::mpRequestNpcSpawn(mpContext, npcSpawnPoint, 1.0f);
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
        "npc_force_hit", "Force NPC raycast to always hit (debug)", "npc_force_hit <0|1>",
        [](const std::vector<std::string>& args) {
            gNpcForceHit = args.empty() ? !gNpcForceHit : args[0] != "0";
            Terminal::instance().addLog(std::string("[DEBUG] NPC force hit ") +
                (gNpcForceHit ? "ENABLED" : "disabled"));
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
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            npcSystem.destroyAll();
            Terminal::instance().addLog("[NPC COMMAND] npc_delete_all");
        }
    });
    Terminal::instance().registerCommand({
        "npc_revolver_inaccuracy", "Set NPC revolver aim inaccuracy in degrees (-1 = use difficulty default)", "npc_revolver_inaccuracy <degrees>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[NPC] npc_revolver_inaccuracy = " + std::to_string(gNpcMaxInaccuracyDegrees) + " (-1 = difficulty-based)");
                return;
            }
            gNpcMaxInaccuracyDegrees = std::stof(args[0]);
            Terminal::instance().addLog("[NPC] npc_revolver_inaccuracy set to " + std::to_string(gNpcMaxInaccuracyDegrees));
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
}
