// 07 31 2026, 15 00
/* purpose
* Provides the general-purpose in-game notification popup system.
* Shows up to N notifications stacked at the bottom-right of the screen,
* newest at the bottom, oldest at the top, with tick-based (FPS-independent)
* durations, an optional action button, and a close button per notification.
* Loads behavior from config/notifications.json and styling from the
* config/gui/notifications.json GUI layout; both hot-reload.
* Does NOT own the music player, chat, killfeed, or DevOverlay notifications.
* Does NOT render 3D world UI or handle network events.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "gui/hud/ui-tick-clock.h"
#include "gui/ui-system.h"

class NotificationSystem
{
public:
    static NotificationSystem& instance();

    enum class ActionType { None, OpenUrl };

    struct Action {
        ActionType type = ActionType::None;
        std::string payload; // e.g. the URL to open
        std::string label;   // action button text, e.g. "mimita.fun"
    };

    struct Notification {
        std::string title;
        std::string message;
        uint64_t startTick = 0;
        uint64_t durationTicks = 300;
        Action action;
    };

    // Lifecycle
    void loadConfig();
    void pollReload();
    void advanceTicks(); // step the 60 Hz UI tick clock
    void render();

    // API
    void push(const std::string& title, const std::string& message,
              uint64_t durationTicks, const Action& action);
    void pushBuildNotice();
    void clear() { mNotifications.clear(); }

    // Settings
    bool enabled() const { return mEnabled; }
    void setEnabled(bool on);
    bool tempMuted() const { return mTempMuteUntilTick > nowTick(); }
    void setTempMuteHours(float hours);
    uint64_t tempMuteRemainingTicks() const;
    uint64_t nowTick() const { return mClock.getTick(); }

    const std::string& configPath() const { return mConfigPath; }

private:
    NotificationSystem() = default;
    NotificationSystem(const NotificationSystem&) = delete;
    NotificationSystem& operator=(const NotificationSystem&) = delete;

    void saveConfig();
    void pruneExpired();

    UiTickClock mClock;
    std::vector<Notification> mNotifications;

    std::string mConfigPath = "config/notifications.json";
    std::filesystem::file_time_type mConfigLastWrite;

    int mMaxCount = 3;
    uint64_t mDefaultDurationTicks = 300;
    uint64_t mFadeInTicks = 12;
    uint64_t mFadeOutTicks = 30;
    float mOffsetRight = 20.0f;
    float mOffsetBottom = 10.0f;
    float mSpacing = 8.0f;
    bool mEnabled = true;
    uint64_t mTempMuteUntilTick = 0;
};
