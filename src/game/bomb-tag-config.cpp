#include "bomb-tag-config.h"
#include "../gui/gui-layout.h"
#include "../gui/gui-element-render.h"
#include "../gui/ui-system.h"
#include <cstdio>
#include <algorithm>

static int sNumNpcs = 3;
static int sLives = 0;
static int sTimeLimit = 180;
static float sNpcDifficulty = 5.0f;

static const char* kLivesOptions[] = {"Infinite", "1", "2", "3", "5", "10"};
static const int kLivesValues[] = {0, 1, 2, 3, 5, 10};
static const int kNumLivesOptions = 6;

static const char* kTimeOptions[] = {"1 min", "3 min", "5 min", "10 min", "15 min"};
static const int kTimeValues[] = {60, 180, 300, 600, 900};
static const int kNumTimeOptions = 5;

BombTagConfigResult drawBombTagConfigMenu(GLFWwindow* win)
{
    BombTagConfigResult r{};
    r.numNpcs = sNumNpcs;
    r.lives = sLives;
    r.timeLimitSeconds = sTimeLimit;
    r.npcDifficulty = sNpcDifficulty;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/bomb-tag-config.json");

    // Render +/- buttons and start/back via unified renderer
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "npcCountMinus") sNumNpcs = std::max(1, sNumNpcs - 1);
        else if (id == "npcCountPlus") sNumNpcs = std::min(20, sNumNpcs + 1);
        else if (id == "diffMinus") sNpcDifficulty = std::max(1.0f, sNpcDifficulty - 1.0f);
        else if (id == "diffPlus") sNpcDifficulty = std::min(10.0f, sNpcDifficulty + 1.0f);
        else if (id == "livesMinus" || id == "livesPlus") {
            int idx = 0;
            for (int i = 0; i < kNumLivesOptions; ++i) {
                if (kLivesValues[i] == sLives) { idx = i; break; }
            }
            idx = (id == "livesMinus") ? (idx - 1 + kNumLivesOptions) % kNumLivesOptions
                                       : (idx + 1) % kNumLivesOptions;
            sLives = kLivesValues[idx];
        }
        else if (id == "timeMinus" || id == "timePlus") {
            int idx = 0;
            for (int i = 0; i < kNumTimeOptions; ++i) {
                if (kTimeValues[i] == sTimeLimit) { idx = i; break; }
            }
            idx = (id == "timeMinus") ? (idx - 1 + kNumTimeOptions) % kNumTimeOptions
                                      : (idx + 1) % kNumTimeOptions;
            sTimeLimit = kTimeValues[idx];
        }
        else if (id == "startButton") r.start = true;
        else if (id == "backButton") r.goBack = true;
    }

    // Dynamic labels
    auto drawVal = [&](const char* fmt, auto val, float y) {
        char text[128];
        snprintf(text, sizeof(text), fmt, val);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(y), 0.38f, {1, 1, 1, 1});
    };
    drawVal("NPC Count: %d", sNumNpcs, 260.0f);
    drawVal("Difficulty: %.0f/10", sNpcDifficulty, 320.0f);

    {
        int idx = 0;
        for (int i = 0; i < kNumLivesOptions; ++i)
            if (kLivesValues[i] == sLives) { idx = i; break; }
        char text[128];
        snprintf(text, sizeof(text), "Lives: %s", kLivesOptions[idx]);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(380.0f), 0.38f, {1, 1, 1, 1});
    }
    {
        int idx = 0;
        for (int i = 0; i < kNumTimeOptions; ++i)
            if (kTimeValues[i] == sTimeLimit) { idx = i; break; }
        char text[128];
        snprintf(text, sizeof(text), "Time Limit: %s", kTimeOptions[idx]);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(440.0f), 0.38f, {1, 1, 1, 1});
    }

    return r;
}
