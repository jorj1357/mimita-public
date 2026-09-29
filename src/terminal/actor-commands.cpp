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
#include "gamemode/gamemode.h"
#include "config/player-settings.h"

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

const MatchRoleDefinition* resolvePreset(const std::vector<std::string>& args)
{
    if (args.empty()) return nullptr;
    auto presets = MatchRoleRegistry::instance().actorPresets();
    if (presets.empty()) {
        MatchRoleRegistry::instance().loadActorPresets();
        presets = MatchRoleRegistry::instance().actorPresets();
    }
    const std::string& token = args[0];
    if (!token.empty() && token.find_first_not_of("0123456789") == std::string::npos) {
        const int index = std::stoi(token);
        return index >= 1 && index <= (int)presets.size() ? presets[index - 1] : nullptr;
    }
    const MatchRoleDefinition* preset = MatchRoleRegistry::instance().getActorPreset(token);
    if (!preset) {
        MatchRoleRegistry::instance().loadActorPresets();
        preset = MatchRoleRegistry::instance().getActorPreset(token);
    }
    return preset;
}

const MatchRoleDefinition* currentPreset()
{
    const auto& clientPreset = MimitaNet::CommunityMatchClient::instance().actorPresetId();
    if (!clientPreset.empty())
        return MatchRoleRegistry::instance().getActorPreset(clientPreset);
    const auto& d = MimitaNet::serverGamemodeState();
    std::string modeId = d.matchMode;
    if (modeId.empty()) modeId = MimitaNet::CommunityMatchClient::instance().mode();
    if (!modeId.empty()) {
        const Gamemode& mode = GamemodeRegistry::instance().get(modeId);
        if (!mode.actorPresetId.empty())
            return MatchRoleRegistry::instance().getActorPreset(mode.actorPresetId);
    }
    return nullptr;
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

    Terminal::instance().registerCommand({
        "actor_preset_list",
        "List actor presets alphabetically with convenience indices",
        "actor_preset_list",
        [](const std::vector<std::string>&) {
            auto presets = MatchRoleRegistry::instance().actorPresets();
            if (presets.empty()) {
                MatchRoleRegistry::instance().loadActorPresets();
                presets = MatchRoleRegistry::instance().actorPresets();
            }
            if (presets.empty()) {
                Terminal::instance().addLog("actor_preset_list: no presets loaded");
                return;
            }
            char buf[256];
            for (size_t i = 0; i < presets.size(); ++i) {
                const std::string label = presets[i]->displayName.empty()
                    ? std::string() : " - " + presets[i]->displayName;
                snprintf(buf, sizeof(buf), "[%zu] %s%s", i + 1,
                    presets[i]->id.c_str(), label.c_str());
                Terminal::instance().addLog(buf);
            }
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "actor_preset",
        "Resolve an actor preset by stable ID or alphabetical list index",
        "actor_preset <id|index>",
        [](const std::vector<std::string>& args) {
            const MatchRoleDefinition* preset = resolvePreset(args);
            if (!preset) {
                Terminal::instance().addLog("actor_preset: unknown preset or index");
                return;
            }
            const bool serverContext = MimitaNet::serverGamemodeState().enabled;
            const bool applied = serverContext
                ? MimitaNet::serverActivateActorPreset(preset->id)
                : MimitaNet::CommunityMatchClient::instance().applyActorPreset(*preset);
            char buf[512];
            snprintf(buf, sizeof(buf),
                "actor_preset: id=%s scope=%s applied=%d fov=%.0f forcedFov=%d firstPersonForced=%d movement=%s weaponSet=%s health=%d avatar=%s",
                preset->id.c_str(), serverContext ? "server" : "local", (int)applied,
                preset->cameraFov, (int)preset->forceFov,
                (int)preset->forceFirstPerson,
                preset->movementPreset.empty() ? "none" : preset->movementPreset.c_str(),
                preset->weaponSet.empty() ? "none" : preset->weaponSet.c_str(),
                preset->health, preset->avatarName.empty() ? "none" : preset->avatarName.c_str());
            Terminal::instance().addLog(buf);
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "actor_preset_reset",
        "Restore the camera, perspective, and avatar values saved before the actor preset",
        "actor_preset_reset",
        [](const std::vector<std::string>&) {
            if (MimitaNet::serverGamemodeState().enabled) {
                MimitaNet::serverResetActorPreset();
                Terminal::instance().addLog("actor_preset_reset: cleared server preset override");
                return;
            }
            const bool hadPreset = !MimitaNet::CommunityMatchClient::instance().actorPresetId().empty();
            MimitaNet::CommunityMatchClient::instance().resetActorPreset();
            Terminal::instance().addLog(hadPreset
                ? "actor_preset_reset: restored previous settings"
                : "actor_preset_reset: no manual actor preset is active");
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "actor_preset_info",
        "Show one actor preset's effective rule references",
        "actor_preset_info <id|index>",
        [](const std::vector<std::string>& args) {
            const MatchRoleDefinition* preset = resolvePreset(args);
            if (!preset) {
                Terminal::instance().addLog("actor_preset_info: unknown preset or index");
                return;
            }
            char buf[512];
            snprintf(buf, sizeof(buf), "id=%s display=%s movement=%s weaponSet=%s health=%d avatar=%s",
                preset->id.c_str(), preset->displayName.empty() ? preset->id.c_str() : preset->displayName.c_str(),
                preset->movementPreset.c_str(), preset->weaponSet.c_str(), preset->health,
                preset->avatarName.c_str());
            Terminal::instance().addLog(buf);
        },
        "2026-09-28",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "actor_preset_current",
        "Show the actor preset selected by the active gamemode",
        "actor_preset_current",
        [](const std::vector<std::string>&) {
            const MatchRoleDefinition* preset = currentPreset();
            Terminal::instance().addLog(preset
                ? "actor_preset_current: " + preset->id
                : "actor_preset_current: none");
        },
        "2026-09-28",
        CommandCategory::Debug
    });
}
