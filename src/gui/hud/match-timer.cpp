// 09 01 2026, 00 00
/* purpose
* Implements the fast interpolated match timer for online matches.
* Displays elapsed match time as HH:MM:SS.mmm with smooth millisecond interpolation.
* Uses authoritative server tick references for timing, not networked formatted strings.
* Does NOT own match state, win conditions, or network replication.
* Does NOT render menus, chat, or scoreboard UI.
*/

#include "match-timer.h"

#include <cstdio>
#include <cmath>

MatchTimer& MatchTimer::instance()
{
    static MatchTimer timer;
    return timer;
}

void MatchTimer::setTimingReferences(uint32_t matchStartTick, uint32_t serverTick)
{
    mMatchStartTick = matchStartTick;
    mLastServerTick = serverTick;
}

void MatchTimer::update(float dt)
{
    if (mMatchStartTick == 0) return;

    // Estimate elapsed server ticks
    uint32_t elapsedTicks = mLastServerTick - mMatchStartTick;
    float elapsedSec = (float)elapsedTicks / 60.0f;

    // Visual interpolation within current tick for smooth milliseconds
    float tickFraction = fmodf(dt, 1.0f / 60.0f) * 60.0f;
    mElapsedMs = (elapsedSec + tickFraction / 60.0f) * 1000.0f;
}

std::string MatchTimer::formatElapsed() const
{
    if (mMatchStartTick == 0) return "00:00:00.000";

    uint32_t totalMs = (uint32_t)mElapsedMs;
    uint32_t hours = totalMs / 3600000;
    uint32_t minutes = (totalMs % 3600000) / 60000;
    uint32_t seconds = (totalMs % 60000) / 1000;
    uint32_t millis = totalMs % 1000;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u.%03u", hours, minutes, seconds, millis);
    return buf;
}
