#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "network/net_mode.h"

extern bool gNetPresentationDebug;

void registerNetworkCommands()
{
    Terminal::instance().registerCommand({
        "serverconnect", "Print a server connection request", "serverconnect <ip> [args...]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: serverconnect <ip> [args...]");
                return;
            }
            std::string text = "[SERVER] would connect to " + args[0];
            for (size_t i = 1; i < args.size(); ++i) text += " " + args[i];
            Terminal::instance().addLog(text);
        }
    });
    Terminal::instance().registerCommand({
        "disconnectserver", "Print a server disconnect request", "disconnectserver",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[SERVER] would disconnect");
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
                " onGround=" + std::to_string((int)player.onGround) +
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
                    " onGround=" + std::to_string((int)kv.second.onGround) +
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
}
