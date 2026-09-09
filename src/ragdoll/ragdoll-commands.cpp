#include "ragdoll/ragdoll.h"
#include "ragdoll/ragdoll-config.h"
#include "ragdoll/ragdoll-mode.h"
#include "ragdoll/ragdoll-mode-config.h"

#include <cstdio>

#include "config.h"
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"

void registerRagdollCommands()
{
    Terminal::instance().registerCommand({
        "ragdoll_reload",
        "Reload ragdolldeath.json without restart",
        "ragdoll_reload",
        [](const std::vector<std::string>&) {
            if (RagdollConfig::instance().load()) {
                Terminal::instance().addLog(
                    "[RAGDOLL] Config reloaded successfully");
            } else {
                Terminal::instance().addLog(
                    "[RAGDOLL] Config reload failed (see log)");
            }
        },
        "2026-07-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "ragdoll_killme",
        "Kill the player to test ragdoll death",
        "ragdoll_killme",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            if (player.dead) {
                Terminal::instance().addLog(
                    "[RAGDOLL] killme: already dead");
                return;
            }
            player.currentHp = 0;
            Terminal::instance().addLog(
                "[RAGDOLL] killme: player HP set to 0");
        },
        "2026-07-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "ragdoll_status",
        "Print ragdoll system status",
        "ragdoll_status",
        [](const std::vector<std::string>&) {
            const auto& cfg = RagdollConfig::instance().data();
            auto& sys = RagdollDeathSystem::instance();
            char buf[512];
            snprintf(buf, sizeof(buf),
                "[RAGDOLL] enabled=%d active_ragdolls=%zu "
                "parts_defined=%zu lifetime=%.1f",
                (int)cfg.enabled,
                sys.ragdolls().size(),
                cfg.parts.size(),
                cfg.lifetimeSeconds);
            Terminal::instance().addLog(buf);
        },
        "2026-07-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "ragdoll_mode_toggle",
        "Toggle ragdoll mode on/off",
        "ragdoll_mode_toggle",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            if (player.dead) {
                Terminal::instance().addLog(
                    "[RAGDOLL MODE] Cannot toggle while dead");
                return;
            }
            auto& ragdoll = RagdollModeSystem::instance();
            if (ragdoll.isActive()) {
                ragdoll.deactivate(player);
                player.ragdollModeActive = false;
                Terminal::instance().addLog(
                    "[RAGDOLL MODE] Deactivated");
            } else {
                ragdoll.activate(player);
                player.ragdollModeActive = true;
                Terminal::instance().addLog(
                    "[RAGDOLL MODE] Activated");
            }
        },
        "2026-09-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "ragdoll_mode_status",
        "Print ragdoll mode status",
        "ragdoll_mode_status",
        [](const std::vector<std::string>&) {
            const auto& cfg = RagdollModeConfig::instance().data();
            auto& sys = RagdollModeSystem::instance();
            char buf[512];
            snprintf(buf, sizeof(buf),
                "[RAGDOLL MODE] active=%d enabled=%d "
                "capsules=%zu attachments=%zu "
                "left_grab=%d right_grab=%d",
                (int)sys.isActive(), (int)cfg.enabled,
                cfg.capsules.size(), cfg.attachments.size(),
                (int)sys.leftGrab().active, (int)sys.rightGrab().active);
            Terminal::instance().addLog(buf);
            if (sys.isActive()) {
                glm::vec3 head = sys.getHeadPosition();
                glm::vec3 torso = sys.getTorsoPosition();
                snprintf(buf, sizeof(buf),
                    "[RAGDOLL MODE] torso=(%.2f %.2f %.2f) head=(%.2f %.2f %.2f) parts=%zu",
                    torso.x, torso.y, torso.z,
                    head.x, head.y, head.z,
                    sys.parts().size());
                Terminal::instance().addLog(buf);
            }
        },
        "2026-09-08",
        CommandCategory::Debug
    });

    Terminal::instance().registerCommand({
        "ragdoll_mode_reload",
        "Reload ragdoll.json config",
        "ragdoll_mode_reload",
        [](const std::vector<std::string>&) {
            if (RagdollModeConfig::instance().load()) {
                Terminal::instance().addLog(
                    "[RAGDOLL MODE] Config reloaded successfully");
            } else {
                Terminal::instance().addLog(
                    "[RAGDOLL MODE] Config reload failed (see log)");
            }
        },
        "2026-09-08",
        CommandCategory::Debug
    });
}
