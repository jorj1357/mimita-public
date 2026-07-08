#include "ragdoll/ragdoll.h"
#include "ragdoll/ragdoll-config.h"

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
}
