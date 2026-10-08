// 09 01 2026, 13 10
/* purpose
* Registers developer and host terminal commands for community modes and debug tools.
* Provides mode selection and the intermission/countdown match start commands.
* Keeps host authority in the network server and forwards client requests to it.
* Does not own match simulation, scoring, or HUD rendering.
* Does not bypass host-only command validation.
* Does not launch or manage the game executable.
*/

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
#include "network/server-gamemode.h"
#include "network/community-server-config.h"
#include "map/map-catalog.h"
#include "gamemode/map-config.h"
#include "gamemode/gamemode.h"
#include "entities/player.h"
#include "npc/npc.h"
#include <glm/glm.hpp>
#include <cstdlib>

void registerDebugCommands()
{
    Terminal::instance().registerCommand({
        "modestartnow", "Start a community mode at the 3-2-1 countdown; host only.", "modestartnow <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[MODESTARTNOW] Usage: modestartnow <number>"); return; }
            auto& cfg = MimitaNet::CommunityServerConfig::instance();
            if (cfg.modes().empty()) cfg.load();
            const int index = std::atoi(args[0].c_str()) - 1;
            if (index < 0 || index >= (int)cfg.modes().size()) { Terminal::instance().addLog("[MODESTARTNOW] Invalid mode number"); return; }
            const auto& mode = cfg.modes()[(size_t)index];
            if (MimitaNet::isServerHost()) {
                MimitaNet::serverCommunitySetMode(mode.id);
                MimitaNet::serverCommunityStartMatch(true, mode.id);
                Terminal::instance().addLog("[MODESTARTNOW] started " + mode.id + " at countdown");
            } else if (::gpMpContext && ::gpMpContext->active) {
                MimitaNet::mpSendServerCommand(*::gpMpContext, "modestartnow " + std::to_string(index + 1));
                Terminal::instance().addLog("[MODESTARTNOW] sent to server");
            } else Terminal::instance().addLog("[MODESTARTNOW] HOST ONLY");
        }
    });
    Terminal::instance().registerCommand({
        "modelist", "List community modes from config/onlinemodes.json", "modelist",
        [](const std::vector<std::string>&) {
            auto& cfg = MimitaNet::CommunityServerConfig::instance();
            if (cfg.modes().empty()) cfg.load();
            int index = 1;
            for (const auto& mode : cfg.modes())
                Terminal::instance().addLog(std::to_string(index++) + " = " + mode.id + " - " + mode.name);
        }
    });
    Terminal::instance().registerCommand({
        "modestart", "Start or live-switch to a community mode at intermission; host only.", "modestart <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[MODESTART] Usage: modestart <number>"); return; }
            auto& cfg = MimitaNet::CommunityServerConfig::instance();
            if (cfg.modes().empty()) cfg.load();
            const int index = std::atoi(args[0].c_str()) - 1;
            if (index < 0 || index >= (int)cfg.modes().size()) { Terminal::instance().addLog("[MODESTART] Invalid mode number"); return; }
            const auto& mode = cfg.modes()[(size_t)index];
            if (MimitaNet::isServerHost()) {
                MimitaNet::serverCommunitySetMode(mode.id);
                MimitaNet::serverCommunityStartMatch(false, mode.id);
                Terminal::instance().addLog("[MODESTART] started " + mode.id + " (" + mode.name + ")");
            } else if (::gpMpContext && ::gpMpContext->active) {
                MimitaNet::mpSendServerCommand(*::gpMpContext, "modestart " + std::to_string(index + 1));
                Terminal::instance().addLog("[MODESTART] sent to server");
            } else {
                Terminal::instance().addLog("[MODESTART] HOST ONLY");
            }
        }
    });
    Terminal::instance().registerCommand({
        "juggernaut_skip", "Force the current Juggernaut round to draw; host only.", "juggernaut_skip",
        [](const std::vector<std::string>&) {
            if (MimitaNet::isServerHost()) {
                MimitaNet::serverGamemodeRequestJuggernautSkip();
                Terminal::instance().addLog("[JUGGERNAUT_SKIP] host requested a draw; actors will respawn for the next 3-2-1");
            } else if (::gpMpContext && ::gpMpContext->active) {
                MimitaNet::mpSendServerCommand(*::gpMpContext, "juggernaut_skip");
                Terminal::instance().addLog("[JUGGERNAUT_SKIP] sent as a skip vote request; voting is not enabled yet");
            } else {
                Terminal::instance().addLog("[JUGGERNAUT_SKIP] HOST ONLY");
            }
        }
    });
    Terminal::instance().registerCommand({
        "maplist", "List maps from assets/maps", "maplist",
        [](const std::vector<std::string>&) {
            const auto catalog = scanMapCatalog();
            int index = 1;
            for (const auto& map : catalog.maps)
                Terminal::instance().addLog(std::to_string(index++) + " = " + map.displayName);
        }
    });
    Terminal::instance().registerCommand({
        "team_list", "List the active gamemode's teams in JSON order", "team_list",
        [](const std::vector<std::string>&) {
            if (::gpMpContext && ::gpMpContext->active)
                MimitaNet::mpSendServerCommand(*::gpMpContext, "team_list");
            else if (MimitaNet::isServerHost())
                Terminal::instance().addLog("[TEAM_LIST] " + MimitaNet::serverActiveTeamList());
            else Terminal::instance().addLog("[TEAM_LIST] not connected");
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "team_pick", "Request an authoritative team change", "team_pick <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[TEAM_PICK] Usage: team_pick <number>"); return; }
            const std::string command = "team_pick " + args[0];
            if (::gpMpContext && ::gpMpContext->active)
                MimitaNet::mpSendServerCommand(*::gpMpContext, command);
            else if (MimitaNet::isServerHost())
                Terminal::instance().addLog("[TEAM_PICK] host must be connected to its server session");
            else Terminal::instance().addLog("[TEAM_PICK] not connected");
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "change_team", "Request an authoritative team change", "change_team <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[TEAM_PICK] Usage: change_team <number>"); return; }
            const std::string command = "team_pick " + args[0];
            if (::gpMpContext && ::gpMpContext->active)
                MimitaNet::mpSendServerCommand(*::gpMpContext, command);
            else if (MimitaNet::isServerHost())
                Terminal::instance().addLog("[TEAM_PICK] host must be connected to its server session");
            else Terminal::instance().addLog("[TEAM_PICK] not connected");
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "team_change", "Request an authoritative team change", "team_change <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[TEAM_PICK] Usage: team_change <number>"); return; }
            const std::string command = "team_pick " + args[0];
            if (::gpMpContext && ::gpMpContext->active)
                MimitaNet::mpSendServerCommand(*::gpMpContext, command);
            else if (MimitaNet::isServerHost())
                Terminal::instance().addLog("[TEAM_PICK] host must be connected to its server session");
            else Terminal::instance().addLog("[TEAM_PICK] not connected");
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "npc_inspect", "Inspect one NPC's team/brain/perception/navigation state", "npc_inspect [id]",
        [](const std::vector<std::string>& args) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            const uint32_t wantId = args.empty() ? 0 : (uint32_t)std::atoi(args[0].c_str());
            int shown = 0;
            for (const Npc& n : npcSystem.all()) {
                if (wantId != 0 && n.id != wantId) continue;
                char buf[512];
                snprintf(buf, sizeof(buf),
                    "[NPC INSPECT] id=%u epoch=%u team=%d hp=%d/%d preset=%s behavior=%s "
                    "target=%u belief=%d vis=%d conf=%.2f dist=%.1f goal=%s action=%s",
                    n.id, (unsigned)n.transformEpoch, n.body.matchTeam,
                    n.body.currentHp, n.body.maxHp,
                    n.movementProfileId.empty() ? "default" : n.movementProfileId.c_str(),
                    n.behaviorProfileId.empty() ? "default" : n.behaviorProfileId.c_str(),
                    n.serverTargetId, (int)n.belief.hasTarget, (int)n.belief.hasVisibleTarget,
                    n.belief.confidence, n.belief.distance,
                    utilityGoalName(n.utility.currentGoal),
                    utilityActionName(n.utility.currentAction));
                Terminal::instance().addLog(buf);
                snprintf(buf, sizeof(buf),
                    "[NPC INSPECT]   navDest=(%.1f %.1f %.1f) pathNodes=%d perceptionFov=%d los=%d "
                    "react=%.2f objKnown=%d objPos=(%.1f %.1f %.1f)",
                    n.navigator.goal.targetPos.x, n.navigator.goal.targetPos.y, n.navigator.goal.targetPos.z,
                    (int)n.navigator.path.size(),
                    (int)n.perception.withinFov, (int)n.perception.hasLineOfSight,
                    n.reactionTimer, (int)n.utilityContext.objectiveKnown,
                    n.utilityContext.objectivePos.x, n.utilityContext.objectivePos.y,
                    n.utilityContext.objectivePos.z);
                Terminal::instance().addLog(buf);
                snprintf(buf, sizeof(buf),
                    "[NPC INSPECT]   goalScore=%.2f actionCooldown=%.2f goalTimer=%.2f",
                    n.utility.currentScore.total, n.utility.actionCooldown, n.utility.goalTimer);
                Terminal::instance().addLog(buf);
                ++shown;
            }
            if (shown == 0) Terminal::instance().addLog("[NPC INSPECT] no matching NPC (count=" +
                std::to_string(npcSystem.all().size()) + ")");
        },
        "", CommandCategory::NPC
    });
    Terminal::instance().registerCommand({
        "npc_brain", "Show one NPC's utility goal scores (all goals)", "npc_brain [id]",
        [](const std::vector<std::string>& args) {
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            const uint32_t wantId = args.empty() ? 0 : (uint32_t)std::atoi(args[0].c_str());
            int shown = 0;
            for (const Npc& n : npcSystem.all()) {
                if (wantId != 0 && n.id != wantId) continue;
                const UtilityGoalKind goals[] = {
                    UtilityGoalKind::KillTarget, UtilityGoalKind::Survive,
                    UtilityGoalKind::HoldPosition, UtilityGoalKind::TakeCover,
                    UtilityGoalKind::MoveToObjective, UtilityGoalKind::DefendSite,
                    UtilityGoalKind::RotateToSite, UtilityGoalKind::PlantObjective,
                    UtilityGoalKind::DefuseObjective, UtilityGoalKind::RetakeSite,
                };
                char header[128];
                snprintf(header, sizeof(header), "[NPC BRAIN] id=%u current=%s", n.id,
                         utilityGoalName(n.utility.currentGoal));
                Terminal::instance().addLog(header);
                for (UtilityGoalKind g : goals) {
                    const UtilityGoalScore s = scoreUtilityGoal(g, n.utilityContext);
                    char buf[160];
                    snprintf(buf, sizeof(buf), "[NPC BRAIN]   %-16s total=%.2f rel=%.2f los=%.2f hp=%.2f",
                             utilityGoalName(g), s.total, s.relevance, s.lineOfSight, s.health);
                    Terminal::instance().addLog(buf);
                }
                ++shown;
            }
            if (shown == 0) Terminal::instance().addLog("[NPC BRAIN] no matching NPC");
        },
        "", CommandCategory::NPC
    });
    Terminal::instance().registerCommand({
        "team_status", "Show teams, rosters, assignments, and round score", "team_status",
        [](const std::vector<std::string>&) {
            const MimitaNet::ServerGamemodeState& d = MimitaNet::serverGamemodeState();
            const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
            char buf[320];
            snprintf(buf, sizeof(buf), "[TEAM STATUS] mode=%s round=%u/%d wins=%d-%d locked=%d",
                     d.matchMode.c_str(), d.roundNumber, d.roundsToWin,
                     d.roundWins[0], d.roundWins[1], (int)d.rosterLocked);
            Terminal::instance().addLog(buf);
            for (size_t t = 0; t < gm.teams.size(); ++t) {
                Terminal::instance().addLog("[TEAM STATUS] " + std::to_string(t + 1) + ". " +
                    gm.teams[t].displayName + " (id=" + gm.teams[t].id + " cap=" +
                    std::to_string(gm.teams[t].capacity) + ")");
            }
            int counts[2] = {0, 0};
            for (const auto& kv : d.matchTeams)
                if (kv.second >= 0 && kv.second < 2) counts[kv.second]++;
            Terminal::instance().addLog("[TEAM STATUS] members: team0=" + std::to_string(counts[0]) +
                " team1=" + std::to_string(counts[1]) + " participants=" +
                std::to_string(d.participants.size()));
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "objective_status", "Show the mode objective (bomb) state", "objective_status",
        [](const std::vector<std::string>&) {
            const MimitaNet::ServerGamemodeState& d = MimitaNet::serverGamemodeState();
            const auto& o = d.objective;
            static const char* states[] = {"Inactive","Carried","Dropped","Planted","Defused","Exploded"};
            const int si = (int)o.state;
            char buf[320];
            snprintf(buf, sizeof(buf),
                "[OBJECTIVE] id=%s active=%d state=%s carrier=%u team=%d pos=(%.1f %.1f %.1f) site=%s",
                o.id.empty() ? "-" : o.id.c_str(), (int)o.valid(),
                (si >= 0 && si < 6) ? states[si] : "?",
                o.carrierActorId, o.allowedCarrierTeam,
                o.position.x, o.position.y, o.position.z,
                o.plantedSiteId.empty() ? "-" : o.plantedSiteId.c_str());
            Terminal::instance().addLog(buf);
            snprintf(buf, sizeof(buf),
                "[OBJECTIVE] plant=%d/%d defuse=%d/%d explosionDeadline=%u pickups=%u drops=%u",
                o.plantTicksElapsed, o.plantTicksRequired,
                o.defuseTicksElapsed, o.defuseTicksRequired,
                o.explosionDeadlineTick, d.objectivePickupCounter, d.objectiveDropCounter);
            Terminal::instance().addLog(buf);
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "grenade_spawn", "Spawn a grenade area effect at your position: <frag|smoke|darkbang|fire>",
        "grenade_spawn <id>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[GRENADE] usage: grenade_spawn <frag|smoke|darkbang|fire>");
                return;
            }
            const uint32_t id = MimitaNet::serverSpawnGrenadeAreaEffect(
                args[0], /*ownerActorId=*/0, THE_PLAYER.matchTeam,
                THE_PLAYER.pos);
            if (id == 0)
                Terminal::instance().addLog("[GRENADE] no area effect for " + args[0] +
                    " (frag uses the direct explosion path)");
            else
                Terminal::instance().addLog("[GRENADE] spawned " + args[0] +
                    " area effect id=" + std::to_string(id));
        },
        "", CommandCategory::Weapon
    });
    Terminal::instance().registerCommand({
        "site_debug", "Inspect/edit bomb sites: show|hide|select <id>|move [x y z]|print|save",
        "site_debug <show|hide|select <id>|move [x y z]|print|save>",
        [](const std::vector<std::string>& args) {
            auto& reg = MapConfigRegistry::instance();
            if (reg.current().mapId.empty())
                reg.load(MimitaNet::serverGamemodeState().mapId);
            const auto& cfg = reg.current();
            if (args.empty()) {
                Terminal::instance().addLog("[SITE] usage: site_debug <show|hide|select <id>|move [x y z]|print|save>");
                return;
            }
            const std::string& cmd = args[0];
            if (cmd == "show" || cmd == "hide" || cmd == "on" || cmd == "off") {
                const bool visible = (cmd == "show" || cmd == "on");
                for (const auto& site : cfg.bombSites)
                    reg.setSiteVisibility(site.id, visible);
                Terminal::instance().addLog(std::string("[SITE] ") + cmd + " " +
                    std::to_string(cfg.bombSites.size()) + " site(s)");
            } else if (cmd == "select") {
                if (args.size() < 2) { Terminal::instance().addLog("[SITE] select <id>"); return; }
                const BombSite* site = reg.findSite(args[1]);
                if (!site) { Terminal::instance().addLog("[SITE] unknown site " + args[1]); return; }
                Terminal::instance().addLog("[SITE] selected " + site->id);
            } else if (cmd == "move") {
                if (args.size() < 2) { Terminal::instance().addLog("[SITE] move <id> [x y z]"); return; }
                const BombSite* site = reg.findSite(args[1]);
                if (!site) { Terminal::instance().addLog("[SITE] unknown site " + args[1]); return; }
                glm::vec3 pos;
                if (args.size() >= 5) {
                    pos = glm::vec3(std::stof(args[2]), std::stof(args[3]), std::stof(args[4]));
                } else {
                    pos = THE_PLAYER.pos;  // move to current player position
                }
                reg.setSitePosition(site->id, pos);
                Terminal::instance().addLog("[SITE] moved " + site->id + " to (" +
                    std::to_string(pos.x) + " " + std::to_string(pos.y) + " " +
                    std::to_string(pos.z) + ")");
            } else if (cmd == "print") {
                if (cfg.bombSites.empty()) { Terminal::instance().addLog("[SITE] no sites"); return; }
                for (const auto& site : cfg.bombSites) {
                    Terminal::instance().addLog("[SITE] " + site.id + " pos=(" +
                        std::to_string(site.position.x) + " " + std::to_string(site.position.y) +
                        " " + std::to_string(site.position.z) + ") r=" +
                        std::to_string(site.radius) + (site.hasPosition ? "" : " UNVERIFIED"));
                }
            } else if (cmd == "save") {
                Terminal::instance().addLog(reg.save() ? "[SITE] saved" : "[SITE] save failed");
            } else {
                Terminal::instance().addLog("[SITE] unknown subcommand " + cmd);
            }
        },
        "", CommandCategory::Duel
    });
    Terminal::instance().registerCommand({
        "respawn_all", "Respawn every server player and NPC", "respawn_all",
        [](const std::vector<std::string>&) {
            if (::gpMpContext && ::gpMpContext->active)
                MimitaNet::mpSendServerCommand(*::gpMpContext, "respawn_all");
            else Terminal::instance().addLog("[RESPAWN_ALL] not connected");
        }
    });
    Terminal::instance().registerCommand({
        "mapchange", "Change the community server map; host only", "mapchange <number>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("[MAPCHANGE] Usage: mapchange <number>"); return; }
            const auto catalog = scanMapCatalog();
            const int index = std::atoi(args[0].c_str()) - 1;
            if (index < 0 || index >= (int)catalog.maps.size()) { Terminal::instance().addLog("[MAPCHANGE] Invalid map number"); return; }
            const std::string mapId = std::filesystem::path(catalog.maps[(size_t)index].assetPath).stem().string();
            if (MimitaNet::isServerHost()) MimitaNet::serverGamemodeRequestMapChange(mapId);
            else if (::gpMpContext && ::gpMpContext->active) MimitaNet::mpSendServerCommand(*::gpMpContext, "mapchange " + std::to_string(index + 1));
            else { Terminal::instance().addLog("[MAPCHANGE] HOST ONLY"); return; }
            Terminal::instance().addLog("[MAPCHANGE] requested " + mapId);
        }
    });
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
    registerDebugToggle("spyknife_debug", DebugConfig::DEBUG_SPYKNIFE);
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
                if (::gpMpContext && ::gpMpContext->active)
                    MimitaNet::mpSendServerCommand(*::gpMpContext, "healthme default");
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

            if (value <= 0) {
                Terminal::instance().addLog("[HEALTHME] Value must be a positive integer.");
                return;
            }

            // In a networked session the server owns health and respawn.
            // Keep the local value responsive, then send the self-only command
            // through the authoritative server path.
            if (::gpMpContext && ::gpMpContext->active)
            {
                DevOverrides::playerHealthOverrideEnabled = true;
                DevOverrides::playerHealthOverrideValue = value;
                player.maxHp = value;
                player.currentHp = value;
                MimitaNet::mpSendServerCommand(*::gpMpContext,
                    "healthme " + std::to_string(value));
                Terminal::instance().addLog(std::string("[HEALTHME] Sent self-only HP override to server: ") +
                                             std::to_string(value));
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

    Terminal::instance().registerCommand({
        "changemap",
        "Change the map live without restarting the server. Host only. Syntax: changemap <map>",
        "changemap <map>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[CHANGEMAP] Usage: changemap <map>  (e.g. changemap funworld3)");
                return;
            }
            const std::string mapId = args[0];

            if (MimitaNet::isServerHost())
            {
                MimitaNet::serverGamemodeRequestMapChange(mapId);
                Terminal::instance().addLog("[CHANGEMAP] Changing map to " + mapId);
                return;
            }

            if (::gpMpContext && ::gpMpContext->active)
            {
                MimitaNet::mpSendServerCommand(*::gpMpContext, "changemap " + mapId);
                Terminal::instance().addLog("[CHANGEMAP] Sent to server (host only).");
                return;
            }

            Terminal::instance().addLog("[CHANGEMAP] HOST ONLY — run this on the server host.");
        },
        "2026-08-10", CommandCategory::Debug
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
