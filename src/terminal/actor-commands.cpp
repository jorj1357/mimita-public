// 09 10 2026
/* purpose
* Register the "actorlist" diagnostic so match identity is testable without HUD work.
* Prints the server's authoritative descriptors when present, otherwise the
* client's replicated team/role/state identity.
* Does NOT mutate match state or own assignment.
*/

#include "terminal/actor-commands.h"

#include <cstdio>
#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "network/server-gamemode.h"
#include "network/community-match-client.h"
#include "gamemode/match-roles.h"

namespace {

const char* actorStateName(MimitaNet::ActorState state)
{
    switch (state) {
        case MimitaNet::ActorState::Alive:      return "alive";
        case MimitaNet::ActorState::Dead:       return "dead";
        case MimitaNet::ActorState::Respawning: return "respawning";
        case MimitaNet::ActorState::Spectating: return "spectating";
    }
    return "unknown";
}

} // namespace

void registerActorCommands()
{
    Terminal::instance().registerCommand({
        "actorlist",
        "List match actors with controller, role, team, state, and movement profile",
        "actorlist",
        [](const std::vector<std::string>&) {
            using namespace MimitaNet;

            const ServerGamemodeState& d = serverGamemodeState();
            char buf[512];

            // Prefer the server-owned descriptors (host/server process).
            if (!d.matchActors.empty()) {
                for (uint32_t id : d.participants) {
                    auto it = d.matchActors.find(id);
                    if (it == d.matchActors.end()) continue;
                    const ActorMatchDescriptor& a = it->second;
                    snprintf(buf, sizeof(buf),
                        "actor=%u controller=%s role=%s team=%d state=%s movement=%s",
                        id,
                        a.controller == ActorController::Npc ? "npc" : "human",
                        a.roleId.empty() ? "none" : a.roleId.c_str(),
                        a.teamId,
                        actorStateName(a.state),
                        a.movementProfileId.empty() ? "none" : a.movementProfileId.c_str());
                    Terminal::instance().addLog(buf);
                }
                if (d.participants.empty())
                    Terminal::instance().addLog("actorlist: no participants");
                return;
            }

            // Otherwise show what this client received from the server.
            const auto& actors = CommunityMatchClient::instance().actorIdentities();
            if (actors.empty()) {
                Terminal::instance().addLog("actorlist: no server or replicated actors");
                return;
            }
            for (const auto& a : actors) {
                const std::string& role = MatchRoleRegistry::instance().idForIndex(a.roleIndex);
                snprintf(buf, sizeof(buf),
                    "actor=%u controller=replicated role=%s team=%d state=%s",
                    a.actorId,
                    role.empty() ? "none" : role.c_str(),
                    a.team == 0xFF ? -1 : (int)a.team,
                    actorStateName((ActorState)a.state));
                Terminal::instance().addLog(buf);
            }
        },
        "2026-09-10",
        CommandCategory::Debug
    });
}
