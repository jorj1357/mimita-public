// 09 10 2026
/* purpose
* Register the "actorlist" diagnostic so match identity is testable without HUD work.
* Prints the server's authoritative descriptors when present, otherwise the
* client's replicated team/role/state identity.
* Does NOT mutate match state or own assignment.
*/

#include "terminal/actor-commands.h"
#include "game/resident-state-module.h"

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "network/server-gamemode.h"
#include "network/server.h"
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "gamemode/match-roles.h"
#include "gamemode/gamemode.h"
#include "config/player-settings.h"
#include "debug/structured-log.h"

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
        try {
            const unsigned long long index = std::stoull(token);
            return index >= 1 && index <= presets.size() ? presets[(size_t)index - 1] : nullptr;
        } catch (...) {
            return nullptr;
        }
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
        "List match actors with controller, role, team, avatar, state, and policy",
        "actorlist",
        [](const std::vector<std::string>&) {
            using namespace MimitaNet;

            const ServerGamemodeState& d = serverGamemodeState();
            char buf[512];
            nlohmann::json auditActors = nlohmann::json::array();

            // Prefer the server-owned descriptors (host/server process).
            if (!d.matchActors.empty()) {
                for (uint32_t id : d.participants) {
                    if (auditActors.size() >= 64) break;
                    auto it = d.matchActors.find(id);
                    if (it == d.matchActors.end()) continue;
                    const ActorMatchDescriptor& a = it->second;
                    const ActorSpawnProfile profile = serverResolveActorSpawnProfile(id);
                    const std::string policy = !profile.allowAvatarLiveChange
                        ? "live_change_disabled"
                        : !profile.allowedAvatars.empty() ? "curated_allowlist" : "unrestricted";
                    const bool avatarAllowed = profile.avatarName.empty() ||
                        isAvatarAllowedByPolicy(profile.avatarName,
                            profile.allowedAvatars, profile.deniedAvatars);
                    snprintf(buf, sizeof(buf),
                        "actor=%u controller=%s role=%s preset_override=%s team=%d state=%s movement=%s avatar=%s avatar_policy=%s",
                        id,
                        a.controller == ActorController::Npc ? "npc" : "human",
                        a.roleId.empty() ? "none" : a.roleId.c_str(),
                        d.actorPresetOverridesByActorId.count(id)
                            ? d.actorPresetOverridesByActorId.at(id).c_str() : "none",
                        a.teamId,
                        actorStateName(a.state),
                        a.movementProfileId.empty() ? "none" : a.movementProfileId.c_str(),
                        profile.avatarName.empty() ? "unknown" : profile.avatarName.c_str(),
                        policy.c_str());
                    Terminal::instance().addLog(buf);
                    auditActors.push_back({{"actor_id", id}, {"controller", a.controller == ActorController::Npc ? "npc" : "human"},
                        {"team", a.teamId}, {"role", a.roleId},
                        {"selected_avatar", profile.avatarName}, {"avatar_policy", policy},
                        {"avatar_policy_result", avatarAllowed ? "allowed_or_unresolved" : "rejected"}});
                }
                if (d.participants.empty())
                    Terminal::instance().addLog("actorlist: no participants");
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "actor.presentation.audit", d.matchMode,
                    "explicit actor/avatar audit requested", d.currentServerTick,
                    {{"mode", d.matchMode}, {"actor_count", auditActors.size()},
                     {"actors", auditActors}});
                return;
            }

            // Otherwise show what this client received from the server.
            const auto& actors = CommunityMatchClient::instance().actorIdentities();
            const Gamemode& activeMode = GamemodeRegistry::instance().get(
                CommunityMatchClient::instance().mode());
            if (actors.empty()) {
                Terminal::instance().addLog("actorlist: no server or replicated actors");
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "actor.presentation.audit", CommunityMatchClient::instance().mode(),
                    "explicit actor/avatar audit requested; no replicated actors are available",
                    CommunityMatchClient::instance().serverTick(),
                    {{"mode", CommunityMatchClient::instance().mode()},
                     {"actor_count", 0}, {"actors", nlohmann::json::array()}});
                return;
            }
            for (const auto& a : actors) {
                const std::string& role = MatchRoleRegistry::instance().idForIndex(a.roleIndex);
                const MatchRoleDefinition* roleDefinition = role.empty()
                    ? nullptr : MatchRoleRegistry::instance().get(role);
                std::string avatar = "unknown";
                std::string controller = "replicated";
                if (::mimita::game::mpContextPtr()) {
                    if (a.actorId == ::mimita::game::mpContextPtr()->localPlayerId) {
                        avatar = GetPlayerSettings().avatarName;
                        controller = "local_human";
                    }
                    auto player = ::mimita::game::mpContextPtr()->remotePlayers.find(a.actorId);
                    auto npc = ::mimita::game::mpContextPtr()->remoteNpcs.find(a.actorId);
                    if (player != ::mimita::game::mpContextPtr()->remotePlayers.end()) {
                        avatar = player->second.avatarName();
                        controller = "human";
                    } else if (npc != ::mimita::game::mpContextPtr()->remoteNpcs.end()) {
                        avatar = npc->second.avatarName();
                        controller = "npc";
                    }
                }
                bool avatarAllowed = !avatar.empty() && avatar != "unknown";
                if (activeMode.avatar.configured)
                    avatarAllowed = avatarAllowed && isAvatarAllowedByPolicy(
                        avatar, activeMode.avatar.allowed, activeMode.avatar.denied);
                if (roleDefinition) {
                    avatarAllowed = avatarAllowed && isAvatarAllowedByPolicy(
                        avatar, roleDefinition->allowedAvatars,
                        roleDefinition->deniedAvatars);
                    if (roleDefinition->avatarForced && !roleDefinition->avatarName.empty())
                        avatarAllowed = avatarAllowed && avatar == roleDefinition->avatarName;
                }
                snprintf(buf, sizeof(buf),
                    "actor=%u controller=%s role=%s preset_index=%u team=%d state=%s avatar=%s avatar_policy_result=%s",
                    a.actorId,
                    controller.c_str(),
                    role.empty() ? "none" : role.c_str(),
                    a.actorPresetIndex,
                    a.team == 0xFF ? -1 : (int)a.team,
                    actorStateName((ActorState)a.state), avatar.c_str(),
                    !roleDefinition && !activeMode.avatar.configured
                        ? "unknown_policy" : (avatarAllowed ? "allowed" : "rejected_or_unresolved"));
                Terminal::instance().addLog(buf);
                const char* avatarPolicyResult =
                    (!roleDefinition && !activeMode.avatar.configured)
                        ? "unknown_policy" : (avatarAllowed ? "allowed" : "rejected_or_unresolved");
                auditActors.push_back({{"actor_id", a.actorId}, {"controller", controller},
                    {"team", a.team == 0xFF ? -1 : (int)a.team}, {"role", role},
                    {"selected_avatar", avatar},
                    {"avatar_policy_result", avatarPolicyResult}});
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "actor.presentation.audit", CommunityMatchClient::instance().mode(),
                "explicit actor/avatar audit requested", CommunityMatchClient::instance().serverTick(),
                {{"mode", CommunityMatchClient::instance().mode()},
                 {"actor_count", auditActors.size()}, {"actors", auditActors}});
        },
        "2026-09-10",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "actor_list",
        "List match actors with controller, role, team, state, and movement profile",
        "actor_list",
        [](const std::vector<std::string>&) {
            Terminal::instance().execute("actorlist");
        },
        "2026-10-23",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "actor_preset_actor",
        "Assign an actor preset to one match actor until that actor leaves",
        "actor_preset_actor <actor-id> <preset-id|index>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2 || args[0].empty() ||
                args[0].find_first_not_of("0123456789") != std::string::npos) {
                Terminal::instance().addLog("usage: actor_preset_actor <actor-id> <preset-id|index>");
                return;
            }
            uint32_t actorId = 0;
            try {
                const unsigned long long parsed = std::stoull(args[0]);
                if (parsed > std::numeric_limits<uint32_t>::max()) throw std::out_of_range("actor id");
                actorId = (uint32_t)parsed;
            } catch (...) {
                Terminal::instance().addLog("actor_preset_actor: invalid actor id");
                return;
            }
            const std::vector<std::string> presetArgs{args[1]};
            const MatchRoleDefinition* preset = resolvePreset(presetArgs);
            if (!preset) {
                Terminal::instance().addLog("actor_preset_actor: unknown preset or index");
                return;
            }
            if (!(::mimita::game::mpContextPtr() && ::mimita::game::mpContextPtr()->active) && MimitaNet::isServerHost()) {
                if (!MimitaNet::serverSetActorPreset(actorId, preset->id)) {
                    Terminal::instance().addLog(
                        "actor_preset_actor: actor not in active server match or preset unavailable");
                    return;
                }
                Terminal::instance().addLog(
                    "actor_preset_actor: assigned " + preset->id + " to actor " +
                    std::to_string(actorId) + "; applies on next spawn and persists until actor removal");
                return;
            }
            if (::mimita::game::mpContextPtr() && ::mimita::game::mpContextPtr()->active) {
                MimitaNet::mpSendServerCommand(*::mimita::game::mpContextPtr(),
                    "actor_preset_actor " + std::to_string(actorId) + " " + preset->id);
                Terminal::instance().addLog("actor_preset_actor: sent to server host for validation");
                return;
            }
            Terminal::instance().addLog("actor_preset_actor: connect to a server or run this on the host");
        },
        "2026-10-23",
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
                const auto& directory = MatchRoleRegistry::instance().actorPresetDirectory();
                std::error_code ec;
                size_t jsonFiles = 0;
                if (std::filesystem::is_directory(directory, ec)) {
                    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
                        if (ec) break;
                        if (entry.is_regular_file(ec) && entry.path().extension() == ".json")
                            ++jsonFiles;
                    }
                }
                char diagnostic[512];
                snprintf(diagnostic, sizeof(diagnostic),
                    "actor_preset_list: no presets loaded (directory=%s exists=%d json_files=%zu)",
                    directory.c_str(), (int)std::filesystem::is_directory(directory, ec), jsonFiles);
                Terminal::instance().addLog(diagnostic);
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
