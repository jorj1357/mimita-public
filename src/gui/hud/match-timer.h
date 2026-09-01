// 09 01 2026, 00 00
/* purpose
* Declares the fast interpolated match timer for online matches.
* Displays elapsed match time as HH:MM:SS.mmm with smooth millisecond interpolation.
* Uses authoritative server tick references for timing, not networked formatted strings.
* Does NOT own match state, win conditions, or network replication.
* Does NOT render menus, chat, or scoreboard UI.
*/

#pragma once

#include <cstdint>
#include <string>

class MatchTimer {
public:
    static MatchTimer& instance();

    void setTimingReferences(uint32_t matchStartTick, uint32_t serverTick);
    void update(float dt);
    std::string formatElapsed() const;
    bool isActive() const { return mMatchStartTick > 0; }

private:
    uint32_t mMatchStartTick = 0;
    uint32_t mLastServerTick = 0;
    float mElapsedMs = 0.0f;
};
