// 07 30 2026, 13 26
/* purpose
* Schedules and sends chat tips on the server at configurable random intervals.
* Reads tips from config/tips.json and timing config from config/tipsconfig.json.
* Tips are formatted as "[system] Tip #N: text" and sent via ChatMessageEventPacket.
* Does NOT own networking, player state, or the main game tick.
* Does NOT handle hot reload or file watching.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ChatRateLimitConfig;

struct TipSchedulerConfig
{
    bool enabledByDefault = true;
    uint64_t minimumTicksBetweenTips = 900;
    uint64_t maximumTicksBetweenTips = 1800;
    bool showStartupDisableMessage = true;
    std::string startupMessage = "run chattips 0 in console to disable chat tips";
};

class TipScheduler
{
public:
    TipScheduler();

    void loadTips(const char* tipsPath);
    void loadConfig(const char* configPath);
    void setEnabled(bool enabled) { mEnabled = enabled; }
    bool isEnabled() const { return mEnabled; }

    // Call once per server tick. Sends a tip when the tick matches.
    // Returns the formatted tip string if one was sent, empty otherwise.
    // sendFn is called with the formatted tip text for the actual network send.
    void maybeSendTip(uint64_t currentTick,
                      void (*sendFn)(const std::string& formatted));

    uint64_t getNextTipTick() const { return mNextTipTick; }

private:
    void scheduleNext(uint64_t currentTick);

    bool mEnabled = true;
    uint64_t mNextTipTick = 0;
    int mLastTipIndex = -1;
    TipSchedulerConfig mConfig;
    std::vector<std::string> mTips;
};
