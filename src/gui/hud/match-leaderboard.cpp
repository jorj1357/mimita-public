// 09 01 2026, 00 00
/* purpose
* Implements the reusable online match leaderboard HUD for FFA and TDM modes.
* Draws top-3 FFA players with gold/silver/bronze colors and TDM team scores.
* Renders +1 score gain animations on confirmed kills.
* Does NOT own match state, scoring logic, or network replication.
* Does NOT render chat, nameplates, or menu UI.
*/

#include "match-leaderboard.h"

#include <algorithm>
#include <cstdio>

#include "gui/ui-system.h"
#include "gui/gui-coord.h"

MatchLeaderboard& MatchLeaderboard::instance()
{
    static MatchLeaderboard mgr;
    return mgr;
}

void MatchLeaderboard::updateFFA(const std::vector<LeaderboardEntry>& top3)
{
    mFFATop3 = top3;
}

void MatchLeaderboard::updateTDM(int redKills, int blueKills, bool isRedTeam)
{
    mRedKills = redKills;
    mBlueKills = blueKills;
    mIsRedTeam = isRedTeam;
}

void MatchLeaderboard::onScoreGain(float x, float y)
{
    ScoreGainAnim anim;
    anim.startX = x;
    anim.startY = y;
    anim.age = 0.0f;
    mScoreGains.push_back(anim);
}

void MatchLeaderboard::update(float dt)
{
    // Update score gain animations
    for (auto it = mScoreGains.begin(); it != mScoreGains.end();) {
        it->age += dt * 60.0f;  // convert to ticks
        if (it->age >= it->lifetimeTicks) {
            it = mScoreGains.erase(it);
        } else {
            ++it;
        }
    }
}

void MatchLeaderboard::render()
{
    const float fontScale = 0.36f;
    const float smallScale = 0.30f;

    // FFA leaderboard: show top 3 horizontally
    if (!mFFATop3.empty()) {
        float x = 20.0f;
        const float y = 20.0f;

        // Rank colors: gold, silver, bronze
        glm::vec4 rankColors[3] = {
            {1.0f, 0.85f, 0.0f, 1.0f},   // gold
            {0.75f, 0.75f, 0.80f, 1.0f},  // silver
            {0.80f, 0.50f, 0.20f, 1.0f}   // bronze
        };

        for (int i = 0; i < 3 && i < (int)mFFATop3.size(); ++i) {
            const auto& entry = mFFATop3[i];
            glm::vec4 color = rankColors[i];

            char buf[128];
            snprintf(buf, sizeof(buf), "%d. %s: %dK", i + 1, entry.name.c_str(), entry.score);
            uiDrawText(buf, uiScaleX(x), uiScaleY(y), fontScale, color);

            float w = uiMeasureText(buf, fontScale);
            x += w + 40.0f;  // spacing between entries
        }
    }

    // TDM leaderboard: Red left, Blue right
    if (mRedKills > 0 || mBlueKills > 0 || (!mFFATop3.empty() && mFFATop3[0].isLocalPlayer)) {
        const float y = 20.0f;

        // Red team (left side)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "RED: %dK", mRedKills);
            glm::vec4 redColor = {1.0f, 0.3f, 0.3f, 1.0f};
            uiDrawText(buf, uiScaleX(20.0f), uiScaleY(y), fontScale, redColor);

            // YOUR TEAM indicator
            if (mIsRedTeam) {
                uiDrawText("^ YOUR TEAM", uiScaleX(20.0f), uiScaleY(y + 18.0f),
                          smallScale, redColor);
            }
        }

        // Blue team (right side)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "BLUE: %dK", mBlueKills);
            float w = uiMeasureText(buf, fontScale);
            float x = uiScreenW() - w - 20.0f;
            glm::vec4 blueColor = {0.3f, 0.5f, 1.0f, 1.0f};
            uiDrawText(buf, uiScaleX(x), uiScaleY(y), fontScale, blueColor);

            // YOUR TEAM indicator
            if (!mIsRedTeam) {
                float iw = uiMeasureText("^ YOUR TEAM", smallScale);
                uiDrawText("^ YOUR TEAM", uiScaleX(x + w * 0.5f - iw * 0.5f),
                          uiScaleY(y + 18.0f), smallScale, blueColor);
            }
        }
    }

    // Render +1 score gain animations
    for (const auto& anim : mScoreGains) {
        float progress = anim.age / anim.lifetimeTicks;
        float alpha = 1.0f - progress;  // lerp from 1.0 to 0.0
        float offsetY = progress * 18.0f;  // move down 18px
        glm::vec4 color = {0.0f, 1.0f, 0.0f, alpha};  // green
        uiDrawText("+1", uiScaleX(anim.startX), uiScaleY(anim.startY + offsetY),
                  smallScale, color);
    }
}
