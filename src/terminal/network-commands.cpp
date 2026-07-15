#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <shellapi.h>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "network/net_mode.h"
#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/disagreement-visuals.h"
#include "gui/gui-main.h"
#include "auth/auth-system.h"

extern bool gNetPresentationDebug;
extern bool gRoomCodeShow;

void registerNetworkCommands()
{
    Terminal::instance().registerCommand({
        "startserver", "Start the local game server",
        "startserver",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            if (ls && ls->active) {
                Terminal::instance().addLog("[SERVER] already running");
                return;
            }
            if (MimitaNet::startListenServer(*ls, MimitaNet::DEFAULT_PORT))
                Terminal::instance().addLog("[SERVER] started on port " +
                    std::to_string(MimitaNet::DEFAULT_PORT) + " code=" + ls->serverCode);
            else
                Terminal::instance().addLog("[SERVER] failed to start");
        }
    });
    Terminal::instance().registerCommand({
        "stopserver", "Stop the local game server",
        "stopserver",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            if (!ls || !ls->active) {
                Terminal::instance().addLog("[SERVER] not running");
                return;
            }
            if (MP_CONTEXT.active)
                MimitaNet::mpShutdown(MP_CONTEXT);
            MimitaNet::stopListenServer(*ls);
            Terminal::instance().addLog("[SERVER] stopped");
        }
    });
    Terminal::instance().registerCommand({
        "server_help", "List server commands",
        "server_help",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("=== SERVER COMMANDS ===");
            Terminal::instance().addLog("startserver              - Start local server");
            Terminal::instance().addLog("stopserver               - Stop local server");
            Terminal::instance().addLog("server_info              - Show server status");
            Terminal::instance().addLog("server_leave             - Host client disconnects, server continues");
            Terminal::instance().addLog("server_help              - This help");
            Terminal::instance().addLog("serverconnect <ip>       - Connect to server (legacy)");
            Terminal::instance().addLog("disconnectserver         - Disconnect from server");
            Terminal::instance().addLog("fakelag_mode <0|1|2>    - Set fake lag mode");
            Terminal::instance().addLog("fakelag_amount_static <ms> - Static fake lag");
            Terminal::instance().addLog("fakelag_amount_min <ms>  - Min random fake lag");
            Terminal::instance().addLog("fakelag_amount_max <ms>  - Max random fake lag");
            Terminal::instance().addLog("net_debug_presentation [0|1] - Debug overlay");
            Terminal::instance().addLog("net_damage_debug [0|1]   - Damage pipeline debug");
            Terminal::instance().addLog("net_hit_debug [0|1]      - Hit raycast debug");
            Terminal::instance().addLog("net_compare              - Client/server state compare");
            Terminal::instance().addLog("net_entity_dump          - Full entity state dump");
            Terminal::instance().addLog("net_disagreement_debug [0|1] - Disagreement visuals debug");
        }
    });
    Terminal::instance().registerCommand({
        "server_info", "Show server status, players, uptime",
        "server_info",
        [](const std::vector<std::string>&) {
            MimitaNet::ListenServerState* ls = getListenServerState();
            ExternalServerProcessStatus external = getExternalServerProcessStatus();

            const bool listenRunning = ls && ls->active;
            const bool externalRunning = external.running;

            if (!listenRunning && !externalRunning)
            {
                Terminal::instance().addLog("[SERVER] not running");
                return;
            }

            if (listenRunning && externalRunning)
                Terminal::instance().addLog("[SERVER WARNING] external and listen servers are both active");

            char buf[256];

            if (externalRunning)
            {
                snprintf(buf, sizeof(buf), "[SERVER] type=external-headless running=1");
                Terminal::instance().addLog(buf);

                uint64_t uptimeSec = external.uptimeMs / 1000;
                snprintf(buf, sizeof(buf), "[SERVER] pid=%u code=%s port=%u uptime=%llus",
                         external.processId, external.roomCode.c_str(),
                         external.port, (unsigned long long)uptimeSec);
                Terminal::instance().addLog(buf);

                snprintf(buf, sizeof(buf), "[SERVER] name=\"%s\" map=%s",
                         external.serverName.c_str(), external.mapName.c_str());
                Terminal::instance().addLog(buf);

                Terminal::instance().addLog("[SERVER] player count is owned by the child process and unavailable locally");
            }

            if (listenRunning)
            {
                snprintf(buf, sizeof(buf), "[SERVER] type=listen running=1");
                Terminal::instance().addLog(buf);

                snprintf(buf, sizeof(buf), "[SERVER] code=%s port=%u players=%zu/%d tick=%u",
                         ls->serverCode.c_str(), ls->port, ls->players.size(),
                         MimitaNet::MAX_PLAYERS, ls->tick);
                Terminal::instance().addLog(buf);

                for (const auto& kv : ls->players) {
                    snprintf(buf, sizeof(buf), "  player id=%u name=\"%s\" hp=%d dead=%d",
                             kv.second.id, kv.second.name.c_str(), kv.second.health, (int)kv.second.dead);
                    Terminal::instance().addLog(buf);
                }
            }
        }
    });
    Terminal::instance().registerCommand({
        "server_leave", "Host client disconnects but server continues running",
        "server_leave",
        [](const std::vector<std::string>&) {
            if (MP_CONTEXT.active && MP_CONTEXT.localPlayerId) {
                MimitaNet::mpShutdown(MP_CONTEXT);
                Terminal::instance().addLog("[SERVER] host client disconnected, server continues");
            } else {
                Terminal::instance().addLog("[SERVER] not connected to any server");
            }
        }
    });
    Terminal::instance().registerCommand({
        "serverconnect", "Connect to a server IP", "serverconnect <ip> [args...]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: serverconnect <ip> [args...]");
                return;
            }
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            std::string address = args[0];
            if (MimitaNet::mpInit(mpContext, address, AuthSystem::instance().displayName())) {
                Terminal::instance().addLog("[SERVER] connecting to " + address);
            } else {
                Terminal::instance().addLog("[ERROR] failed to connect to " + address);
            }
        }
    });
    Terminal::instance().registerCommand({
        "disconnectserver", "Disconnect from server",
        "disconnectserver",
        [](const std::vector<std::string>&) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (mpContext.active) {
                MimitaNet::mpShutdown(mpContext);
                Terminal::instance().addLog("[SERVER] disconnected");
            } else {
                Terminal::instance().addLog("[SERVER] not connected");
            }
        }
    });
    Terminal::instance().registerCommand({
        "server_showghost", "Show/hide ghost of authoritative server position",
        "server_showghost [0|1]",
        [](const std::vector<std::string>& args) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[SERVER GHOST] " + std::string(mpContext.showServerGhost ? "enabled" : "disabled"));
                Terminal::instance().addLog("Usage: server_showghost [0|1]");
                return;
            }
            mpContext.showServerGhost = args[0] == "1";
            Terminal::instance().addLog(mpContext.showServerGhost
                ? "[SERVER GHOST] Server position ghost enabled"
                : "[SERVER GHOST] Server position ghost disabled");
        }
    });
    Terminal::instance().registerCommand({
        "net_disagreement_debug", "Toggle server disagreement debug logging",
        "net_disagreement_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MimitaNet::gDisagreementDebug = !MimitaNet::gDisagreementDebug;
            } else {
                MimitaNet::gDisagreementDebug = args[0] == "1";
            }
            Terminal::instance().addLog(MimitaNet::gDisagreementDebug
                ? "[NET] Disagreement debug ON"
                : "[NET] Disagreement debug OFF");
        }
    });

    Terminal::instance().registerCommand({
        "server_disagreement_test", "Spawn a test server disagreement event at look position",
        "server_disagreement_test [hit|damage|movement|teleport|state]",
        [](const std::vector<std::string>& args) {
            Player& player = THE_PLAYER;

            using namespace MimitaNet;

            DisagreementReason reason = DISAGREEMENT_POSITION_CORRECTION;
            const char* desc = "test correction";
            glm::vec3 correction(0.5f, 0.3f, 0.2f);

            if (!args.empty())
            {
                if (args[0] == "hit") {
                    reason = DISAGREEMENT_OCCLUDED_SHOT;
                    desc = "test occluded shot";
                    correction = glm::vec3(0.0f);
                } else if (args[0] == "damage") {
                    reason = DISAGREEMENT_INVALID_DAMAGE;
                    desc = "test invalid damage";
                    correction = glm::vec3(0.0f);
                } else if (args[0] == "movement") {
                    reason = DISAGREEMENT_INVALID_MOVEMENT;
                    desc = "test invalid movement";
                    correction = glm::vec3(1.2f, 0.5f, 0.0f);
                } else if (args[0] == "teleport") {
                    reason = DISAGREEMENT_POSITION_CORRECTION;
                    desc = "test teleport correction";
                    correction = glm::vec3(5.0f, 0.0f, 2.0f);
                } else if (args[0] == "state") {
                    reason = DISAGREEMENT_INVALID_STATE;
                    desc = "test invalid state";
                    correction = glm::vec3(0.0f);
                }
            }

            glm::vec3 front(std::cos(player.yaw), std::sin(player.yaw), 0.0f);

            DisagreementEvent event;
            event.timeMs = nowMs();
            event.reason = reason;
            event.position = player.pos + front * 3.0f;
            event.correction = correction;
            event.description = desc;
            event.lifetime = 3.0f;

            spawnDisagreementEffect(event);
            logDisagreement(event);

            Terminal::instance().addLog(
                "[NET] server_disagreement_test: reason=" + std::string(reason == DISAGREEMENT_OCCLUDED_SHOT ? "hit" :
                    reason == DISAGREEMENT_INVALID_DAMAGE ? "damage" :
                    reason == DISAGREEMENT_INVALID_MOVEMENT ? "movement" :
                    reason == DISAGREEMENT_POSITION_CORRECTION ? "teleport" : "state") +
                " pos=(" + std::to_string((int)event.position.x) + "," +
                           std::to_string((int)event.position.y) + "," +
                           std::to_string((int)event.position.z) + ")" +
                " correction=(" + std::to_string(event.correction.x) + "," +
                                  std::to_string(event.correction.y) + "," +
                                  std::to_string(event.correction.z) + ")");
        }
    });

    Terminal::instance().registerCommand({
        "fakelag_mode", "Set fake lag mode (0=off, 1=random, 2=static)",
        "fakelag_mode <0|1|2>",
        [](const std::vector<std::string>& args) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (args.empty()) {
                Terminal::instance().addLog(
                    "[FAKELAG] mode=" + std::to_string(mpContext.fakeLagMode));
                return;
            }
            MimitaNet::mpSetFakeLagMode(mpContext, std::stoi(args[0]));
            Terminal::instance().addLog(
                "[FAKELAG] mode=" + std::to_string(mpContext.fakeLagMode));
        }
    });
    Terminal::instance().registerCommand({
        "fakelag_amount_static", "Set static fake lag in milliseconds",
        "fakelag_amount_static <ms>",
        [](const std::vector<std::string>& args) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!args.empty())
                MimitaNet::mpSetFakeLagStatic(mpContext, std::stoi(args[0]));
            Terminal::instance().addLog(
                "[FAKELAG] static=" + std::to_string(mpContext.fakeLagStaticMs));
        }
    });
    Terminal::instance().registerCommand({
        "fakelag_amount_min", "Set random fake lag minimum",
        "fakelag_amount_min <ms>",
        [](const std::vector<std::string>& args) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!args.empty()) {
                mpContext.fakeLagMinMs = std::clamp(std::stoi(args[0]), 0, 5000);
                if (mpContext.fakeLagMaxMs < mpContext.fakeLagMinMs)
                    mpContext.fakeLagMaxMs = mpContext.fakeLagMinMs;
                mpContext.fakeLagNextRandomizeMs = 0;
            }
            Terminal::instance().addLog(
                "[FAKELAG] min=" + std::to_string(mpContext.fakeLagMinMs));
        }
    });
    Terminal::instance().registerCommand({
        "fakelag_amount_max", "Set random fake lag maximum",
        "fakelag_amount_max <ms>",
        [](const std::vector<std::string>& args) {
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!args.empty()) {
                mpContext.fakeLagMaxMs = std::clamp(std::stoi(args[0]), 0, 5000);
                if (mpContext.fakeLagMinMs > mpContext.fakeLagMaxMs)
                    mpContext.fakeLagMinMs = mpContext.fakeLagMaxMs;
                mpContext.fakeLagNextRandomizeMs = 0;
            }
            Terminal::instance().addLog(
                "[FAKELAG] max=" + std::to_string(mpContext.fakeLagMaxMs));
        }
    });

    Terminal::instance().registerCommand({
        "net_debug_presentation", "Toggle remote player presentation debug overlay", "net_debug_presentation [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                gNetPresentationDebug = !gNetPresentationDebug;
            } else {
                gNetPresentationDebug = args[0] == "1";
            }
            printf("[NET PRESENTATION DEBUG] %s\n", gNetPresentationDebug ? "ON" : "OFF");
            Terminal::instance().addLog(gNetPresentationDebug
                ? "[NET] Presentation debug ON"
                : "[NET] Presentation debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "net_damage_debug", "Log damage pipeline: shooter/target/health/accept/reject", "net_damage_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MimitaNet::gNetDamageDebug = !MimitaNet::gNetDamageDebug;
            } else {
                MimitaNet::gNetDamageDebug = args[0] == "1";
            }
            printf("[NET DAMAGE DEBUG] %s\n", MimitaNet::gNetDamageDebug ? "ON" : "OFF");
            Terminal::instance().addLog(MimitaNet::gNetDamageDebug
                ? "[NET] Damage debug ON"
                : "[NET] Damage debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "net_hit_debug", "Log raycast hit details: origin/direction/entity/distance", "net_hit_debug [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                MimitaNet::gNetHitDebug = !MimitaNet::gNetHitDebug;
            } else {
                MimitaNet::gNetHitDebug = args[0] == "1";
            }
            printf("[NET HIT DEBUG] %s\n", MimitaNet::gNetHitDebug ? "ON" : "OFF");
            Terminal::instance().addLog(MimitaNet::gNetHitDebug
                ? "[NET] Hit debug ON"
                : "[NET] Hit debug OFF");
        }
    });
    Terminal::instance().registerCommand({
        "net_compare", "Show client/server/remote state side-by-side", "net_compare",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!mpContext.active) {
                Terminal::instance().addLog("[NET] Not connected to server");
                return;
            }
            Terminal::instance().addLog("=== NET COMPARE ===");
            Terminal::instance().addLog("LOCAL CLIENT VIEW:");
            Terminal::instance().addLog("  id=" + std::to_string(mpContext.localPlayerId) +
                " hp=" + std::to_string(player.currentHp) +
                " dead=" + std::to_string((int)player.dead) +
                " pos=(" + std::to_string((int)player.pos.x) + "," +
                           std::to_string((int)player.pos.y) + "," +
                           std::to_string((int)player.pos.z) + ")" +
                " weapon=" + player.equippedWeaponId);
            Terminal::instance().addLog("SERVER SNAPSHOT:");
            Terminal::instance().addLog("  id=" + std::to_string(mpContext.localPlayerId) +
                " hp=" + std::to_string(mpContext.localServerHealth) +
                " pos=(" + std::to_string((int)mpContext.localServerPosition.x) + "," +
                           std::to_string((int)mpContext.localServerPosition.y) + "," +
                           std::to_string((int)mpContext.localServerPosition.z) + ")" +
                " awaitingExplode=" + std::to_string((int)mpContext.awaitingExplodeDeath) +
                " reconciled=" + std::to_string((int)mpContext.localPlayerReconciled));
            for (const auto& kv : mpContext.remotePlayers) {
                auto it = mpContext.remotePlayerInterpolation.find(kv.first);
                int snapAge = it != mpContext.remotePlayerInterpolation.end()
                    ? (int)(MimitaNet::nowMs() - it->second.target.receivedMs)
                    : -1;
                Terminal::instance().addLog("REMOTE PLAYER id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")" +
                    " weapon=" + kv.second.equippedWeaponId +
                    " snapAge=" + std::to_string(snapAge) + "ms");
            }
            for (const auto& kv : mpContext.remoteNpcs) {
                Terminal::instance().addLog("REMOTE NPC id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")");
            }
            Terminal::instance().addLog("=== END ===");
        }
    });
    Terminal::instance().registerCommand({
        "net_entity_dump", "Dump all replicated entities with full state", "net_entity_dump",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;
            if (!mpContext.active) {
                Terminal::instance().addLog("[NET] Not connected to server");
                return;
            }
            Terminal::instance().addLog("=== NET ENTITY DUMP ===");
            Terminal::instance().addLog("LOCAL id=" + std::to_string(mpContext.localPlayerId) +
                " hp=" + std::to_string(player.currentHp) +
                " maxHp=" + std::to_string(player.maxHp) +
                " dead=" + std::to_string((int)player.dead) +
                " onGround=" + std::to_string((int)player.ground.onGround) +
                " slot=" + std::to_string(player.equippedSlot) +
                " weapon=" + player.equippedWeaponId +
                " pos=(" + std::to_string(player.pos.x) + "," + std::to_string(player.pos.y) + "," + std::to_string(player.pos.z) + ")" +
                " vel=(" + std::to_string(player.vel.x) + "," + std::to_string(player.vel.y) + "," + std::to_string(player.vel.z) + ")" +
                " yaw=" + std::to_string(player.yaw) +
                " respawnTimer=" + std::to_string(player.respawnTimer));
            Terminal::instance().addLog("SERVER health=" + std::to_string(mpContext.localServerHealth) +
                " ping=" + std::to_string(mpContext.localPingMs) +
                " tick=" + std::to_string(mpContext.tick) +
                " snapshotsReceived=" + std::to_string(mpContext.snapshotsReceived) +
                " snapshotsMissed=" + std::to_string(mpContext.snapshotsMissed));
            for (const auto& kv : mpContext.remotePlayers) {
                auto it = mpContext.remotePlayerInterpolation.find(kv.first);
                std::string age = it != mpContext.remotePlayerInterpolation.end()
                    ? std::to_string(MimitaNet::nowMs() - it->second.target.receivedMs) + "ms"
                    : "none";
                Terminal::instance().addLog("PLAYER id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " onGround=" + std::to_string((int)kv.second.ground.onGround) +
                    " slot=" + std::to_string(kv.second.equippedSlot) +
                    " weapon=" + kv.second.equippedWeaponId +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")" +
                    " yaw=" + std::to_string((int)kv.second.yaw) +
                    " snapAge=" + age);
            }
            for (const auto& kv : mpContext.remoteNpcs) {
                Terminal::instance().addLog("NPC id=" + std::to_string(kv.first) +
                    " hp=" + std::to_string(kv.second.currentHp) +
                    " dead=" + std::to_string((int)kv.second.dead) +
                    " pos=(" + std::to_string((int)kv.second.pos.x) + "," +
                               std::to_string((int)kv.second.pos.y) + "," +
                               std::to_string((int)kv.second.pos.z) + ")" +
                    " yaw=" + std::to_string((int)kv.second.yaw));
            }
            Terminal::instance().addLog("=== END DUMP ===");
        }
    });
    Terminal::instance().registerCommand({
        "roomcodeshow", "Show/hide room code HUD overlay",
        "roomcodeshow 0|1",
        [](const std::vector<std::string>& args) {
            if (args.size() == 1 && args[0] == "0") {
                gRoomCodeShow = false;
                printf("[ROOM CODE] HUD hidden\n");
                Terminal::instance().addLog("[ROOM CODE] HUD hidden");
            } else if (args.size() == 1 && args[0] == "1") {
                gRoomCodeShow = true;
                printf("[ROOM CODE] HUD shown\n");
                Terminal::instance().addLog("[ROOM CODE] HUD shown");
            } else {
                printf("usage: roomcodeshow 0|1\n");
                Terminal::instance().addLog("usage: roomcodeshow 0|1");
            }
        }
    });
}
