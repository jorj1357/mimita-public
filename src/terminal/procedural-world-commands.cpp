// 09 28 2026, 00 00
/* purpose
* Implements the procedural-world terminal commands.
* forwards host/player intent to the authoritative server through the existing
* PACKET_SERVER_COMMAND path and prints replicated state for diagnostics.
* does not generate rooms, spawn NPCs, or mutate server state locally.
*/

#include "terminal/procedural-world-commands.h"

#include <cstdio>
#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "terminal/terminal-state.h"

namespace {

bool sendToServer(const std::string& command)
{
    if (::gpMpContext && ::gpMpContext->active)
    {
        MimitaNet::mpSendServerCommand(*::gpMpContext, command);
        Terminal::instance().addLog("[PROCEDURAL] sent: " + command);
        return true;
    }
    Terminal::instance().addLog("[PROCEDURAL] HOST ONLY (not connected to a server)");
    return false;
}

} // anonymous namespace

void registerProceduralWorldCommands()
{
    Terminal::instance().registerCommand({
        "procedural_world_help",
        "List procedural-world (Infinite Dungeon Slayer) commands",
        "procedural_world_help",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(
                "[PROCEDURAL] commands:\n"
                "  procedural_world_start <mode> [seed] - host: start a procedural mode\n"
                "  pwsids - start infinite_dungeon_slayer\n"
                "  procedural_world_info - print replicated room state\n"
                "  procedural_world_generate_next - host/dev: generate next room\n"
                "  procedural_world_teleport_highest - teleport to highest accessible room\n"
                "  procedural_world_stop - host: disable and remove procedural state");
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "pwsids",
        "Start Infinite Dungeon Slayer",
        "pwsids",
        [](const std::vector<std::string>&) {
            sendToServer("procedural_world_start infinite_dungeon_slayer");
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "procedural_world_start",
        "Host: start a procedural-world mode (e.g. infinite_dungeon_slayer)",
        "procedural_world_start <mode> [seed]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog(
                    "[PROCEDURAL] usage: procedural_world_start <mode> [seed]");
                return;
            }
            std::string command = "procedural_world_start " + args[0];
            if (args.size() >= 2)
                command += " " + args[1];
            sendToServer(command);
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "procedural_world_info",
        "Print the replicated procedural-world room state",
        "procedural_world_info",
        [](const std::vector<std::string>&) {
            const MimitaNet::ProceduralWorldNetworkState& p =
                MimitaNet::CommunityMatchClient::instance().procedural();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "[PROCEDURAL] enabled=%d roomState=%u exitLocked=%d seed=%u "
                "currentRoom=%u generatedRooms=%u highestAccessibleRoom=%u "
                "aliveEncounterActors=%u stateVersion=%u",
                (int)p.enabled, (unsigned)p.roomState, (int)p.exitLocked,
                p.seed, p.currentRoom, p.generatedRooms,
                p.highestAccessibleRoom, p.aliveEncounterActors, p.stateVersion);
            Terminal::instance().addLog(buf);
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "procedural_world_generate_next",
        "Host/dev: generate the next room when the current encounter is clear",
        "procedural_world_generate_next",
        [](const std::vector<std::string>&) {
            sendToServer("procedural_world_generate_next");
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "procedural_world_teleport_highest",
        "Teleport to the highest accessible procedural room entrance",
        "procedural_world_teleport_highest",
        [](const std::vector<std::string>&) {
            sendToServer("procedural_world_teleport_highest");
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "procedural_world_stop",
        "Host: disable procedural mode and remove only its rooms, NPCs, barriers",
        "procedural_world_stop",
        [](const std::vector<std::string>&) {
            sendToServer("procedural_world_stop");
        },
        "2026-09-28",
        CommandCategory::Debug
    });
}
