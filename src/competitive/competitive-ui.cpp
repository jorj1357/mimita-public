#include "competitive.h"
#include "competitive-ui.h"

#include "../gui/ui-system.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-element-render.h"
#include "../gui/gui-coord.h"
#include "../gui/gui-main.h"
#include "../auth/auth-system.h"

#include <cstdio>

CompetitiveMenuAction drawCompetitiveMenu(GLFWwindow* win)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    float sw = uiScreenW();
    float sh = uiScreenH();

    uiDrawRect({0, 0, sw, sh}, {0.028f, 0.032f, 0.045f, 1.0f}, "comp-bg");

    AuthSystem& auth = AuthSystem::instance();
    CompetitiveProfile& profile = GetCompetitiveProfile();
    const auto& tier = tierInfoForMmr(profile.mmr);

    // Title
    uiDrawText("COMPETITIVE DUELS", cs.designToScreenX(740.0f), cs.designToScreenY(50.0f),
               0.55f, {0.9f, 0.95f, 1.0f, 1.0f});

    // Profile card
    float cardX = cs.designToScreenX(700.0f);
    float cardY = cs.designToScreenY(110.0f);
    float cardW = cs.designToScreenX(520.0f);
    float cardH = cs.designToScreenY(260.0f);
    uiDrawRect({cardX, cardY, cardW, cardH}, {0.035f, 0.04f, 0.06f, 0.9f}, "comp-card");
    uiDrawRectOutline({cardX, cardY, cardW, cardH}, {0.15f, 0.2f, 0.3f, 0.4f}, "comp-card-border");

    float labelX = cardX + cs.designToScreenX(20.0f);
    float labelY = cardY + cs.designToScreenY(16.0f);
    float lineH = cs.designToScreenY(28.0f);

    auto drawLine = [&](const char* label, const char* value) {
        uiDrawText(label, labelX, labelY, 0.28f, {0.5f, 0.6f, 0.7f, 1.0f});
        float valW = uiMeasureText(value, 0.30f);
        uiDrawText(value, labelX + cardW - valW - cs.designToScreenX(20.0f), labelY,
                   0.30f, {1.0f, 1.0f, 1.0f, 1.0f});
        labelY += lineH;
    };

    char mmrStr[32], highestStr[32], winsStr[32], gamesStr[32], streakStr[32], rankStr[32];
    snprintf(mmrStr, sizeof(mmrStr), "%d", profile.mmr);
    snprintf(highestStr, sizeof(highestStr), "%d", profile.highestMmr);
    snprintf(winsStr, sizeof(winsStr), "%d / %d (%.0f%%)", profile.wins, profile.losses,
             profile.gamesPlayed > 0 ? (100.0f * profile.wins / profile.gamesPlayed) : 0.0f);
    snprintf(gamesStr, sizeof(gamesStr), "%d", profile.gamesPlayed);
    snprintf(streakStr, sizeof(streakStr), "%d", profile.currentStreak);
    snprintf(rankStr, sizeof(rankStr), "#%d", profile.globalRank);

    drawLine("Rank", tier.displayName);
    drawLine("MMR", mmrStr);
    drawLine("Global Rank", rankStr);
    drawLine("Highest MMR", highestStr);
    drawLine("W / L", winsStr);
    drawLine("Games", gamesStr);
    drawLine("Streak", streakStr);

    // Achievements section
    float achY = labelY + cs.designToScreenY(10.0f);
    uiDrawText("Achievements", labelX, achY, 0.26f, {0.5f, 0.6f, 0.7f, 1.0f});
    achY += lineH * 0.8f;
    if (profile.achievements.empty()) {
        uiDrawText("None yet.", labelX, achY, 0.22f, {0.4f, 0.45f, 0.55f, 1.0f});
    } else {
        for (const auto& a : profile.achievements) {
            for (int i = 0; i < kRankAchievementCount; ++i) {
                if (kRankAchievements[i].achievementId == a) {
                    uiDrawText(kRankAchievements[i].title, labelX, achY, 0.22f,
                               {0.6f, 0.85f, 0.5f, 1.0f});
                    achY += lineH * 0.7f;
                    break;
                }
            }
        }
    }

    // Buttons
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/competitive-menu.json");
    for (const std::string& id : layout.elementIds()) {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;
        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;
        if (id == "findMatchButton") {
            return CompetitiveMenuAction::FindMatch;
        } else if (id == "leaderboardButton") {
            printf("[COMP] Leaderboard requested\n");
            gGuiMenuState = GUI_MENU_LEADERBOARD;
        } else if (id == "backButton") {
            return CompetitiveMenuAction::GoBack;
        }
    }

    return CompetitiveMenuAction::None;
}
