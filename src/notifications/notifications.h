// 08 16 2026, 01 35
/* purpose
* Provides the general-purpose in-game notification popup system: up to N
* notifications stacked at an anchored screen edge with tick-based durations,
* an optional action button, a close button, and hover brighten. Panels size
* themselves to content and long messages type in character by character, then
* scroll inside a capped box (wheel + scrollbar). Schedules periodic gameplay
* tips from config/tips.json. Loads behavior from config/notifications.json and
* styling from config/gui/notifications.json; all configs hot-reload.
* Does NOT own the music player, chat, killfeed, or DevOverlay notifications.
* Does NOT render 3D world UI or handle network events.
*/
#pragma once

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "gui/hud/ui-tick-clock.h"
#include "gui/ui-system.h"

class NotificationSystem
{
public:
    static NotificationSystem& instance();

    enum class ActionType { None, OpenUrl, Callback };
    enum class Accent { Default, Critical, Important, HostCommand, DevTimer };

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
        uint64_t revealTicks = 0;   // ticks over which the message types out; 0 = instant
        Action action;
        UIScrollState scroll;       // vertical scroll state for the message box
        Accent accent = Accent::Default;
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
    void loadDevTimerTipsConfig();
    void pollReload();
    void advanceTicks(); // step the 60 Hz UI tick clock
    void updateTips();   // fire a periodic tip when the scheduled tick arrives
    void updateDevTimerTips();
    void render(bool inGameplay);

    // API
    void push(const std::string& title, const std::string& message,
              uint64_t durationTicks, const Action& action);
    // Urgent (red) notification for connection state changes and other
    // critical, time-sensitive events. Same tip UI, red accent.
    void pushCritical(const std::string& title, const std::string& message,
                      uint64_t durationTicks = 0);
    // Host-command results use a dedicated light-purple title and message accent.
    void pushHostCommand(const std::string& title, const std::string& message,
                         uint64_t durationTicks = 0);
    void pushImportant(const std::string& title, const std::string& message,
                       uint64_t durationTicks = 0);
    void pushBuildNotice();
    void pushTip(bool force = false);
    bool devTimerTipsEnabled() const { return mDevTimerTipsEnabled; }
    bool setDevTimerTipsEnabled(bool on);
    bool activateLatestJoinAction();
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

    // Per-notification layout computed each frame: panel height grows with the
    // wrapped message lines (capped by max_text_height) plus title and optional
    // action row. Reveal progress drives the typewriter.
    struct NotificationLayout {
        float alpha = 1.0f;
        float slide = 0.0f;
        float panelH = 76.0f;
        float textY = 0.0f;      // design Y offset of the message area within the panel
        float textH = 20.0f;     // design height of the message area
        float contentHPx = 0.0f; // wrapped message height in screen px
        float lineHPx = 0.0f;
        float fontSize = 0.0f;
        float boxW = 0.0f;       // message box width (design)
        float padX = 12.0f;
        bool scrollable = false;
        bool hasAction = false;
        std::vector<std::string> lines;
    };

    NotificationLayout computeLayout(size_t index, uint64_t elapsed);
    void drawMessage(size_t index, const UIRect& box, const NotificationLayout& layout,
                     bool mouseUnlocked, float cursorX, float cursorY);

    void saveConfig();
    bool pushWithAccent(const std::string& title, const std::string& message,
                        uint64_t durationTicks, const Action& action, Accent accent,
                        bool instantText = false);
    void saveTipsConfig();
    bool saveDevTimerTipsConfig();
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
    std::string mDevTimerTipsConfigPath = "config/devtimertips.json";
    std::filesystem::file_time_type mDevTimerTipsConfigLastWrite;
    std::chrono::steady_clock::time_point mSessionStart = std::chrono::steady_clock::now();
    std::mt19937 mDevTimerRng{std::random_device{}()};
    bool mDevTimerTipsEnabled = false;
    uint64_t mLastDevTimerSecond = 0;
    unsigned mDevTimerLoggedTips = 0;
    uint64_t mDevTimerDurationTicks = 60;
    std::string mDevTimerTitle;
    std::string mDevTimerTextFormat;
    std::string mDevTimerMode = "random";
    std::string mDevTimerRepeatMessage;
    std::vector<std::string> mDevTimerMessages;
    int mLastDevTimerMessage = -1;
    std::vector<float> mDevTimerTitleColor{1.0f, 1.0f, 1.0f};
    std::vector<float> mDevTimerTextColor{1.0f, 1.0f, 1.0f};

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

    // Typewriter (config/notifications.json)
    bool mTypewriterEnabled = true;
    uint64_t mCharsPerTick = 1;
    uint64_t mTypewriterDelayTicks = 4;

    // Dynamic layout (config/gui/notifications.json "render")
    float mPaddingX = 12.0f;
    float mPaddingY = 8.0f;
    float mGapTitleText = 6.0f;
    float mGapTextAction = 8.0f;
    float mMaxTextHeight = 120.0f;

    bool mTipsEnabled = true;
    uint64_t mNextTipTick = 0;
    uint64_t mTipMinTicks = 60;
    uint64_t mTipMaxTicks = 1800;
};
