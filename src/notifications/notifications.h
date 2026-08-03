// 07 31 2026, 15 00
/* purpose
* Provides the general-purpose in-game notification popup system.
* Shows up to N notifications stacked at the bottom-right of the screen,
* newest at the bottom, oldest at the top, with tick-based (FPS-independent)
* durations, an optional action button, a close button, and a hover brighten.
* Schedules periodic gameplay tips from config/tips.json at random tick
* intervals configured in config/tipsconfig.json, each with a "NEW TIP"
* action button. Keeps an in-memory history of the last 100 notifications.
* Loads behavior from config/notifications.json and styling from the
* config/gui/notifications.json GUI layout; all configs hot-reload.
* Does NOT own the music player, chat, killfeed, or DevOverlay notifications.
* Does NOT render 3D world UI or handle network events.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "gui/hud/ui-tick-clock.h"
#include "gui/ui-system.h"

class NotificationSystem
{
public:
    static NotificationSystem& instance();

    enum class ActionType { None, OpenUrl, Callback };

    struct Action {
        ActionType type = ActionType::None;
        std::string payload;               // e.g. the URL to open
        std::string label;                 // action button text, e.g. "mimita.fun"
        std::function<void()> callback;    // invoked when the action button is clicked
    };

    struct Notification {
        std::string title;
        std::string message;
        uint64_t startTick = 0;
        uint64_t durationTicks = 300;
        Action action;
    };

    // In-memory log of pushed notifications (title, message, tick).
    // Capped at the last 100 so it can later grow into a message history.
    struct HistoryEntry {
        std::string title;
        std::string message;
        uint64_t tick = 0;
    };

    static constexpr size_t kMaxHistory = 100;

    // Lifecycle
    void loadConfig();
    void loadGuiConfig();
    void loadTipsConfig();
    void pollReload();
    void advanceTicks(); // step the 60 Hz UI tick clock
    void updateTips();   // fire a periodic tip when the scheduled tick arrives
    void render(bool inGameplay);

    // API
    void push(const std::string& title, const std::string& message,
              uint64_t durationTicks, const Action& action);
    void pushBuildNotice();
    void pushTip(bool force = false);
    void clear() { mNotifications.clear(); }

    const std::vector<HistoryEntry>& history() const { return mHistory; }

    // Settings
    bool enabled() const { return mEnabled; }
    void setEnabled(bool on);
    bool showInGame() const { return mShowInGame; }
    void setShowInGame(bool on);
    bool tempMuted() const { return mTempMuteUntilTick > nowTick(); }
    void setTempMuteHours(float hours);
    uint64_t tempMuteRemainingTicks() const;
    uint64_t nowTick() const { return mClock.getTick(); }

    // Periodic tip scheduling (config/tipsconfig.json)
    bool tipsEnabled() const { return mTipsEnabled; }
    void setTipsEnabled(bool on);

    const std::string& configPath() const { return mConfigPath; }

private:
    NotificationSystem() = default;
    NotificationSystem(const NotificationSystem&) = delete;
    NotificationSystem& operator=(const NotificationSystem&) = delete;

    void saveConfig();
    void saveTipsConfig();
    void pruneExpired();
    void recordHistory(const Notification& n);
    void scheduleNextTip();

    UiTickClock mClock;
    std::vector<Notification> mNotifications;
    std::vector<HistoryEntry> mHistory;

    std::string mConfigPath = "config/notifications.json";
    std::filesystem::file_time_type mConfigLastWrite;
    std::string mGuiConfigPath = "config/gui/notifications.json";
    std::filesystem::file_time_type mGuiConfigLastWrite;
    std::string mTipsConfigPath = "config/tipsconfig.json";
    std::filesystem::file_time_type mTipsConfigLastWrite;

    int mMaxCount = 3;
    uint64_t mDefaultDurationTicks = 300;
    uint64_t mTipDurationTicks = 0;
    uint64_t mFadeInTicks = 12;
    uint64_t mFadeOutTicks = 30;
    std::string mAnchor = "bottom_right"; // bottom_right | top_right | bottom_left | top_left
    float mOffsetRight = 20.0f;
    float mOffsetBottom = 10.0f;
    float mOffsetTop = 10.0f;
    float mOffsetLeft = 20.0f;
    float mSpacing = 8.0f;
    float mSlideInPx = 200.0f;
    float mPanelAlpha = 0.95f;
    float mHoverBrighten = 0.15f;
    bool mEnabled = true;
    bool mShowInGame = true;
    uint64_t mTempMuteUntilTick = 0;

    bool mTipsEnabled = true;
    uint64_t mNextTipTick = 0;
    uint64_t mTipMinTicks = 60;
    uint64_t mTipMaxTicks = 1800;
};
