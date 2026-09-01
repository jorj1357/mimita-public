// 09 01 2026, 13 25
/* purpose
* Owns client-side replicated state for the community FFA and TDM modes.
* Accepts generic match snapshots and exposes phase, score, timer, and leaderboard data.
* Feeds confirmed score changes to the community match HUD.
* Does not own Duel matchmaking or the authoritative server simulation.
* Does not decide win conditions or mutate player health.
* Does not render UI directly.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "network/packets.h"
#include "gui/hud/match-leaderboard.h"

namespace MimitaNet {

class CommunityMatchClient
{
public:
    static CommunityMatchClient& instance();
    void onState(const DuelStatePacket& packet);
    bool active() const { return mMode == "ffa" || mMode == "tdm"; }
    const std::string& mode() const { return mMode; }
    uint8_t phase() const { return mPhase; }
    float phaseTimer() const { return mPhaseTimer; }
    uint32_t matchStartTick() const { return mMatchStartTick; }
    uint32_t serverTick() const { return mServerTick; }
    int timeLimitSeconds() const { return mTimeLimitSeconds; }
    int goal() const { return mGoal; }
    int redScore() const { return mRedScore; }
    int blueScore() const { return mBlueScore; }

private:
    std::string mMode;
    uint8_t mPhase = DUEL_PHASE_WAITING;
    float mPhaseTimer = 0.0f;
    uint32_t mMatchStartTick = 0;
    uint32_t mServerTick = 0;
    int mTimeLimitSeconds = 0;
    int mGoal = 0;
    int mRedScore = 0;
    int mBlueScore = 0;
    int mLocalScore = 0;
    uint32_t mMatchId = 0;
    uint32_t mStateVersion = 0;
};

}
