// 09 01 2026, 00 00
/* purpose
* Declares the reusable online match leaderboard HUD for FFA and TDM modes.
* Displays top-3 FFA players with gold/silver/bronze colors and TDM team scores.
* Provides +1 score gain animation on confirmed kills.
* Does NOT own match state, scoring logic, or network replication.
* Does NOT render chat, nameplates, or menu UI.
*/

#pragma once

#include <string>
#include <vector>

struct LeaderboardEntry {
    std::string name;
    int score = 0;
    int rank = 0;  // 0=1st, 1=2nd, 2=3rd
    bool isLocalPlayer = false;
};

struct ScoreGainAnim {
    float age = 0.0f;
    float lifetimeTicks = 30.0f;
    float startX = 0.0f;
    float startY = 0.0f;
};

class MatchLeaderboard {
public:
    static MatchLeaderboard& instance();

    void updateFFA(const std::vector<LeaderboardEntry>& top3);
    void updateTDM(int redKills, int blueKills, bool isRedTeam);
    void onScoreGain(float x, float y);
    void update(float dt);
    void render();

private:
    std::vector<LeaderboardEntry> mFFATop3;
    int mRedKills = 0;
    int mBlueKills = 0;
    bool mIsRedTeam = false;
    std::vector<ScoreGainAnim> mScoreGains;
};
