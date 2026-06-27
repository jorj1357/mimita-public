#include "competitive.h"
#include "competitive-ui.h"

#include "../gui/ui-system.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-element-render.h"
#include "../gui/gui-coord.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace {

// ── State for MMR animation ─────────────────────────────────────────
bool gAnimating = false;
float gAnimTime = 0.0f;
int gAnimStartMmr = 0;
int gAnimEndMmr = 0;
int gAnimChange = 0;
float gAnimDuration = 2.0f;

CompetitiveMatchResult gLastMatchResult{};

int animatedMmr()
{
    if (!gAnimating || gAnimDuration <= 0.0f)
        return gAnimEndMmr;
    float t = std::min(1.0f, gAnimTime / gAnimDuration);
    float ease = 1.0f - std::pow(1.0f - t, 3.0f);
    return (int)std::round((float)gAnimStartMmr + (float)(gAnimEndMmr - gAnimStartMmr) * ease);
}

}

void startMmrAnimation(int startMmr, int endMmr, int change)
{
    gAnimating = true;
    gAnimTime = 0.0f;
    gAnimStartMmr = startMmr;
    gAnimEndMmr = endMmr;
    gAnimChange = change;
}

bool isMmrAnimating()
{
    return gAnimating;
}

void stopMmrAnimation()
{
    gAnimating = false;
}

void updateMmrAnimation(float dt)
{
    if (!gAnimating) return;
    gAnimTime += dt;
    if (gAnimTime >= gAnimDuration)
        gAnimating = false;
}

void setLastCompetitiveMatchResult(const CompetitiveMatchResult& result)
{
    gLastMatchResult = result;
    startMmrAnimation(result.mmrBefore, result.mmrAfter, result.mmrChange);
}

const CompetitiveMatchResult& getLastCompetitiveMatchResult()
{
    return gLastMatchResult;
}

CompetitiveResultAction drawCompetitiveResultScreen(GLFWwindow* win)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    float sw = uiScreenW();
    float sh = uiScreenH();

    uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.65f}, "comp-result-dim");

    const char* banner = gLastMatchResult.won ? "VICTORY" : "DEFEAT";
    glm::vec4 bannerColor = gLastMatchResult.won
        ? glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
        : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
    float bannerW = uiMeasureText(banner, 1.2f);
    uiDrawText(banner, sw * 0.5f - bannerW * 0.5f, sh * 0.25f, 1.2f, bannerColor);

    int displayMmr = animatedMmr();
    const auto& dispTier = tierInfoForMmr(displayMmr);

    char mmrBuf[64];
    snprintf(mmrBuf, sizeof(mmrBuf), "MMR: %d", displayMmr);
    float mmrW = uiMeasureText(mmrBuf, 0.6f);
    uiDrawText(mmrBuf, sw * 0.5f - mmrW * 0.5f, sh * 0.35f, 0.6f, {1.0f, 1.0f, 1.0f, 1.0f});

    char changeBuf[32];
    snprintf(changeBuf, sizeof(changeBuf), "%s%d",
             gLastMatchResult.mmrChange >= 0 ? "+" : "", gLastMatchResult.mmrChange);
    glm::vec4 changeColor = gLastMatchResult.mmrChange >= 0
        ? glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
        : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
    float changeW = uiMeasureText(changeBuf, 0.45f);
    uiDrawText(changeBuf, sw * 0.5f - changeW * 0.5f, sh * 0.35f + 40.0f, 0.45f, changeColor);

    const auto& beforeTier = tierInfoForMmr(gLastMatchResult.mmrBefore);
    const auto& afterTier = tierInfoForMmr(gLastMatchResult.mmrAfter);
    char tierBuf[64];
    snprintf(tierBuf, sizeof(tierBuf), "Rank: %s -> %s",
             beforeTier.displayName, afterTier.displayName);
    float tierW = uiMeasureText(tierBuf, 0.38f);
    uiDrawText(tierBuf, sw * 0.5f - tierW * 0.5f, sh * 0.35f + 80.0f, 0.38f, {0.8f, 0.85f, 0.95f, 1.0f});

    char scoreBuf[32];
    snprintf(scoreBuf, sizeof(scoreBuf), "%d - %d",
             gLastMatchResult.playerKills, gLastMatchResult.opponentKills);
    float scoreW = uiMeasureText(scoreBuf, 0.5f);
    uiDrawText(scoreBuf, sw * 0.5f - scoreW * 0.5f, sh * 0.5f, 0.5f, {1.0f, 0.85f, 0.25f, 1.0f});

    float achY = sh * 0.55f;
    for (const auto& a : gLastMatchResult.newAchievements) {
        for (int i = 0; i < kRankAchievementCount; ++i) {
            if (kRankAchievements[i].achievementId == a) {
                char achBuf[128];
                snprintf(achBuf, sizeof(achBuf), "Achievement Unlocked: %s", kRankAchievements[i].title);
                float aw = uiMeasureText(achBuf, 0.30f);
                uiDrawText(achBuf, sw * 0.5f - aw * 0.5f, achY, 0.30f, {1.0f, 0.85f, 0.15f, 1.0f});
                achY += 24.0f;
                float dw = uiMeasureText(kRankAchievements[i].description, 0.22f);
                uiDrawText(kRankAchievements[i].description, sw * 0.5f - dw * 0.5f, achY,
                           0.22f, {0.6f, 0.7f, 0.8f, 1.0f});
                achY += 30.0f;
                break;
            }
        }
    }

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/competitive-result.json");

    const GuiElement* playAgain = layout.get("playAgainButton");
    if (playAgain && drawGuiElement(win, *playAgain).clicked)
        return CompetitiveResultAction::PlayAgain;

    const GuiElement* exitBtn = layout.get("exitButton");
    if (exitBtn && drawGuiElement(win, *exitBtn).clicked)
        return CompetitiveResultAction::ExitToMenu;

    return CompetitiveResultAction::None;
}
