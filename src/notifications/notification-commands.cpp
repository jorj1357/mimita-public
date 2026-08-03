// 07 31 2026, 15 00
/* purpose
* Registers terminal commands for the notification system.
* notifs toggles notifications on/off; notifsingame toggles showing them during
* gameplay; notifstempmute mutes for N hours; notifs_test pushes a sample popup;
* notiftest pushes a random tip immediately; tips toggles periodic gameplay tips;
* notifs_history prints recent popups.
* Does NOT render, load config, or persist anything itself.
*/
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "devtools/terminal.h"
#include "notifications/notifications.h"

void registerNotificationCommands()
{
    Terminal::instance().registerCommand({
        "notifs", "Enable or disable in-game notifications (1 = on, 0 = off)", "notifs [0|1]",
        [](const std::vector<std::string>& args) {
            NotificationSystem& ns = NotificationSystem::instance();
            if (args.empty()) {
                Terminal::instance().addLog(ns.enabled()
                    ? "[NOTIFS] enabled"
                    : "[NOTIFS] disabled");
                return;
            }
            bool on = args[0] != "0";
            ns.setEnabled(on);
            Terminal::instance().addLog(on
                ? "[NOTIFS] enabled"
                : "[NOTIFS] disabled");
        },
        "2026-07-31", CommandCategory::UI
    });

    Terminal::instance().registerCommand({
        "notifsingame", "Show notifications during gameplay (1 = show, 0 = hide)", "notifsingame [0|1]",
        [](const std::vector<std::string>& args) {
            NotificationSystem& ns = NotificationSystem::instance();
            if (args.empty()) {
                Terminal::instance().addLog(ns.showInGame()
                    ? "[NOTIFS] shown in game"
                    : "[NOTIFS] hidden in game");
                return;
            }
            bool on = args[0] != "0";
            ns.setShowInGame(on);
            Terminal::instance().addLog(on
                ? "[NOTIFS] shown in game"
                : "[NOTIFS] hidden in game");
        },
        "2026-08-03", CommandCategory::UI
    });

    Terminal::instance().registerCommand({
        "notifstempmute", "Mute notifications for N hours (converted to ticks)", "notifstempmute [hours]",
        [](const std::vector<std::string>& args) {
            NotificationSystem& ns = NotificationSystem::instance();
            if (args.empty()) {
                uint64_t rem = ns.tempMuteRemainingTicks();
                if (rem == 0) {
                    Terminal::instance().addLog("[NOTIFS] not muted");
                    return;
                }
                char buf[80];
                snprintf(buf, sizeof(buf), "[NOTIFS] muted for %.2f hours (%llu ticks)",
                         (double)rem / (3600.0 * 60.0), (unsigned long long)rem);
                Terminal::instance().addLog(buf);
                return;
            }
            float hours = std::stof(args[0]);
            ns.setTempMuteHours(hours);
            char buf[80];
            snprintf(buf, sizeof(buf), "[NOTIFS] notifications muted for %.2f hours (%llu ticks)",
                     hours, (unsigned long long)(hours * 3600.0 * 60.0));
            Terminal::instance().addLog(buf);
        },
        "2026-07-31", CommandCategory::UI
    });

    Terminal::instance().registerCommand({
        "notifs_test", "Push a test notification", "notifs_test",
        [](const std::vector<std::string>&) {
            NotificationSystem::instance().push("test", "this is a test notification", 0, {});
            Terminal::instance().addLog("[NOTIFS] pushed test notification");
        },
        "2026-07-31", CommandCategory::UI
    });

    Terminal::instance().registerCommand({
        "notiftest", "Push a random gameplay tip immediately", "notiftest",
        [](const std::vector<std::string>&) {
            NotificationSystem::instance().pushTip(true);
            Terminal::instance().addLog("[NOTIFS] pushed random tip");
        },
        "2026-08-03", CommandCategory::UI
    });

    Terminal::instance().registerCommand({
        "notifs_history", "Print the last notifications (default 10)", "notifs_history [count]",
        [](const std::vector<std::string>& args) {
            const auto& hist = NotificationSystem::instance().history();
            if (hist.empty()) {
                Terminal::instance().addLog("[NOTIFS] history is empty");
                return;
            }
            int count = 10;
            if (!args.empty())
                count = std::atoi(args[0].c_str());
            if (count <= 0) count = 10;
            char buf[512];
            int shown = 0;
            for (auto it = hist.rbegin(); it != hist.rend() && shown < count; ++it, ++shown) {
                snprintf(buf, sizeof(buf), "[NOTIFS HISTORY] tick=%llu  %s: %s",
                         (unsigned long long)it->tick, it->title.c_str(), it->message.c_str());
                Terminal::instance().addLog(buf);
            }
        },
        "2026-08-03", CommandCategory::UI
    });

    Terminal::instance().registerCommand({
        "tips", "Enable or disable periodic gameplay tips (1 = on, 0 = off)", "tips [0|1]",
        [](const std::vector<std::string>& args) {
            NotificationSystem& ns = NotificationSystem::instance();
            if (args.empty()) {
                Terminal::instance().addLog(ns.tipsEnabled()
                    ? "[TIPS] enabled"
                    : "[TIPS] disabled");
                return;
            }
            bool on = args[0] != "0";
            ns.setTipsEnabled(on);
            Terminal::instance().addLog(on
                ? "[TIPS] enabled"
                : "[TIPS] disabled");
        },
        "2026-08-03", CommandCategory::UI
    });
}
