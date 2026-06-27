#include "competitive.h"
#include "competitive-ui.h"

#include "../game/duel.h"

#include <cstdio>

namespace {
bool gCompetitiveMatchActive = false;
int gCompetitiveOpponentMmr = 5000;
}

bool isCompetitiveMatchActive()
{
    return gCompetitiveMatchActive;
}

void setCompetitiveMatchActive(bool active, int opponentMmr)
{
    gCompetitiveMatchActive = active;
    gCompetitiveOpponentMmr = opponentMmr;
    if (active) {
        printf("[COMP] Competitive match started, opponent MMR=%d\n", opponentMmr);
    }
}

int getCompetitiveOpponentMmr()
{
    return gCompetitiveOpponentMmr;
}

void onCompetitiveMatchEnd(DuelManager& duel, bool playerWon)
{
    if (!gCompetitiveMatchActive) return;

    CompetitiveProfile& profile = GetCompetitiveProfile();
    int playerKills = duel.stats().kills;
    int opponentKills = duel.stats().deaths;

    // Calculate result
    auto result = calculateMatchResult(
        profile.mmr,
        gCompetitiveOpponentMmr,
        playerWon,
        playerKills,
        opponentKills,
        profile.achievements
    );

    // Apply and submit
    SubmitCompetitiveMatch(result);

    // Set for result screen
    setLastCompetitiveMatchResult(result);

    // Log
    const auto& tBefore = tierInfoForMmr(result.mmrBefore);
    const auto& tAfter = tierInfoForMmr(result.mmrAfter);
    printf("[COMP] Match complete: %s | MMR %d->%d (%+d) | %s -> %s\n",
           playerWon ? "WIN" : "LOSS",
           result.mmrBefore, result.mmrAfter, result.mmrChange,
           tBefore.displayName, tAfter.displayName);

    for (const auto& a : result.newAchievements) {
        printf("[COMP] Achievement unlocked: %s\n", a.c_str());
    }

    gCompetitiveMatchActive = false;
}
