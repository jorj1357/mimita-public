// 2026-10-28
/* purpose
* Register admincmds, player_list, admincmds_grant, and admincmds_revoke.
* All commands travel through the normal client/server command packet.
* Does NOT own grants, visibility, or host-command execution.
*/
#include "terminal/admin-commands.h"

#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "network/multiplayer-context.h"
#include "terminal/terminal-state.h"

namespace {

bool sendServerAdminCommand(const std::string& command, const char* label)
{
    if (::gpMpContext && ::gpMpContext->active)
    {
        MimitaNet::mpSendServerCommand(*::gpMpContext, command);
        Terminal::instance().addLog(std::string("[") + label + "] sent to server");
        return true;
    }
    Terminal::instance().addLog(std::string("[") + label + "] requires a connected server session");
    return false;
}

bool validToggle(const std::string& value)
{
    return value == "0" || value == "1" || value == "on" || value == "off";
}

} // namespace

void registerAdminCommands()
{
    Terminal::instance().registerCommand({
        "admincmds", "Toggle or inspect your server admin-command access",
        "admincmds <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                sendServerAdminCommand("admincmds", "ADMINCMDS");
                return;
            }
            if (args.size() != 1 || !validToggle(args[0])) {
                Terminal::instance().addLog("[ADMINCMDS] usage: admincmds <0|1>");
                return;
            }
            sendServerAdminCommand("admincmds " + args[0], "ADMINCMDS");
        },
        "2026-10-28", CommandCategory::Network
    });

    Terminal::instance().registerCommand({
        "player_list", "List connected human players with host first; admin only",
        "player_list",
        [](const std::vector<std::string>&) {
            sendServerAdminCommand("player_list", "PLAYER_LIST");
        },
        "2026-10-28", CommandCategory::Network
    });

    Terminal::instance().registerCommand({
        "admincmds_grant", "Grant session admin commands by player_list number; host only",
        "admincmds_grant <player-number>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 1 ||
                args[0].find_first_not_of("0123456789") != std::string::npos) {
                Terminal::instance().addLog("[ADMINCMDS_GRANT] usage: admincmds_grant <player-number>");
                return;
            }
            sendServerAdminCommand("admincmds_grant " + args[0], "ADMINCMDS_GRANT");
        },
        "2026-10-28", CommandCategory::Network
    });

    Terminal::instance().registerCommand({
        "admincmds_revoke", "Revoke session admin commands by player_list number; host only",
        "admincmds_revoke <player-number>",
        [](const std::vector<std::string>& args) {
            if (args.size() != 1 ||
                args[0].find_first_not_of("0123456789") != std::string::npos) {
                Terminal::instance().addLog("[ADMINCMDS_REVOKE] usage: admincmds_revoke <player-number>");
                return;
            }
            sendServerAdminCommand("admincmds_revoke " + args[0], "ADMINCMDS_REVOKE");
        },
        "2026-10-28", CommandCategory::Network
    });
}
