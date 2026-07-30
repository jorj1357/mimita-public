// 07 30 2026, 13 26
/* purpose
* Implements the server-side tip scheduler.
* Loads tips and config from JSON files, schedules random tip intervals,
* and formats tips as "[system] Tip #N: text".
* Does NOT own networking, player state, or the main game tick.
*/
#include "tip-scheduler.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

TipScheduler::TipScheduler()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

void TipScheduler::loadTips(const char* tipsPath)
{
    mTips.clear();
    std::ifstream file(tipsPath);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::Chat, "[TIPS] could not open %s\n", tipsPath);
        return;
    }

    // Simple parser: expects a JSON array of strings like ["tip1", "tip2", ...]
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    size_t pos = 0;
    while ((pos = content.find('\"', pos)) != std::string::npos)
    {
        size_t start = pos + 1;
        size_t end = content.find('\"', start);
        if (end == std::string::npos)
            break;
        std::string tip = content.substr(start, end - start);
        // Skip JSON structural strings like "tips"
        if (tip == "tips")
        {
            pos = end + 1;
            continue;
        }
        mTips.push_back(tip);
        pos = end + 1;
    }

    Debug::log(Debug::Category::Chat, "[TIPS] loaded %zu tips from %s\n",
               mTips.size(), tipsPath);
}

void TipScheduler::loadConfig(const char* configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        Debug::warn(Debug::Category::Chat, "[TIPS] could not open config %s, using defaults\n", configPath);
        return;
    }

    // Simple key-value parser for the small config
    std::string line;
    while (std::getline(file, line))
    {
        // Skip non-numeric lines for simplicity; the config is well-known
        if (line.find("\"minimumTicksBetweenTips\"") != std::string::npos)
        {
            auto colon = line.find(':');
            if (colon != std::string::npos)
                mConfig.minimumTicksBetweenTips =
                    static_cast<uint64_t>(std::atoll(line.c_str() + colon + 1));
        }
        else if (line.find("\"maximumTicksBetweenTips\"") != std::string::npos)
        {
            auto colon = line.find(':');
            if (colon != std::string::npos)
                mConfig.maximumTicksBetweenTips =
                    static_cast<uint64_t>(std::atoll(line.c_str() + colon + 1));
        }
        else if (line.find("\"enabledByDefault\"") != std::string::npos)
        {
            mConfig.enabledByDefault = line.find("true") != std::string::npos;
        }
    }
    file.close();

    // Validate
    if (mConfig.minimumTicksBetweenTips < 1)
        mConfig.minimumTicksBetweenTips = 1;
    if (mConfig.maximumTicksBetweenTips < mConfig.minimumTicksBetweenTips)
        mConfig.maximumTicksBetweenTips = mConfig.minimumTicksBetweenTips;

    mEnabled = mConfig.enabledByDefault;

    Debug::log(Debug::Category::Chat, "[TIPS] config loaded: min=%llu max=%llu enabled=%d\n",
               (unsigned long long)mConfig.minimumTicksBetweenTips,
               (unsigned long long)mConfig.maximumTicksBetweenTips,
               (int)mEnabled);
}

void TipScheduler::scheduleNext(uint64_t currentTick)
{
    if (mConfig.maximumTicksBetweenTips <= mConfig.minimumTicksBetweenTips)
    {
        mNextTipTick = currentTick + mConfig.minimumTicksBetweenTips;
        return;
    }
    uint64_t range = mConfig.maximumTicksBetweenTips - mConfig.minimumTicksBetweenTips;
    uint64_t extra = static_cast<uint64_t>(
        (static_cast<double>(std::rand()) / RAND_MAX) * static_cast<double>(range));
    mNextTipTick = currentTick + mConfig.minimumTicksBetweenTips + extra;
}

void TipScheduler::maybeSendTip(uint64_t currentTick,
                                 void (*sendFn)(const std::string& formatted))
{
    if (!mEnabled || mTips.empty())
        return;

    if (currentTick < mNextTipTick)
        return;

    // Pick a random tip, avoiding the last one
    int idx;
    if (mTips.size() == 1)
    {
        idx = 0;
    }
    else
    {
        do {
            idx = static_cast<int>(
                (static_cast<double>(std::rand()) / RAND_MAX) * static_cast<double>(mTips.size()));
        } while (idx == mLastTipIndex && mTips.size() > 1);
    }
    mLastTipIndex = idx;

    // Format: "[system] Tip #N: text"
    // Use the hash of the tip text as a pseudo-ID
    int tipId = 0;
    for (char c : mTips[idx])
        tipId = (tipId * 31 + c) & 0x7FFFFFFF;
    tipId = (tipId % 9999) + 1;

    char formatted[512];
    std::snprintf(formatted, sizeof(formatted), "[system] Tip #%d: %s",
                  tipId, mTips[idx].c_str());

    sendFn(std::string(formatted));

    Debug::log(Debug::Category::Chat, "[TIPS] sent tip at tick=%llu: %s\n",
               (unsigned long long)currentTick, formatted);

    scheduleNext(currentTick);
}
