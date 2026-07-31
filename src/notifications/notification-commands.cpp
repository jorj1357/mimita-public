// 07 31 2026, 15 00
/* purpose
* Registers terminal commands for the notification system.
* notifs toggles notifications on/off; notifstempmute mutes for N hours
* (converted internally to 60 Hz ticks); notifs_test pushes a sample popup.
* Does NOT render, load config, or persist anything itself.
*/
#include <cstdio>
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
}
