#include "bomb-tag-config.h"
#include "../gui/gui-back.h"
#include "../gui/gui-button.h"
#include "../gui/gui-label.h"
#include "../gui/gui-layout.h"
#include "../gui/ui-system.h"
#include <cstdio>
#include <algorithm>

static int sNumNpcs = 3;
static int sLives = 0; // 0 = infinite
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

    guiLabel("Bomb Tag Settings", uiScaleX(700.0f), uiScaleY(160.0f));

    // NPC Count
    {
        char text[128];
        snprintf(text, sizeof(text), "NPC Count: %d", sNumNpcs);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(260.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("NPC Count -", {1050.0f, 255.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f}))
            sNumNpcs = std::max(1, sNumNpcs - 1);
        UIRect pr = layout.getRectDesign("NPC Count +", {1110.0f, 255.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f}))
            sNumNpcs = std::min(20, sNumNpcs + 1);
    }

    // NPC Difficulty
    {
        char text[64];
        snprintf(text, sizeof(text), "Difficulty: %.0f/10", sNpcDifficulty);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(320.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("Difficulty -", {1050.0f, 315.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f}))
            sNpcDifficulty = std::max(1.0f, sNpcDifficulty - 1.0f);
        UIRect pr = layout.getRectDesign("Difficulty +", {1110.0f, 315.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f}))
            sNpcDifficulty = std::min(10.0f, sNpcDifficulty + 1.0f);
    }

    // Lives
    {
        int idx = 0;
        for (int i = 0; i < kNumLivesOptions; ++i) {
            if (kLivesValues[i] == sLives) { idx = i; break; }
        }
        char text[128];
        snprintf(text, sizeof(text), "Lives: %s", kLivesOptions[idx]);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(380.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("Lives -", {1050.0f, 375.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f})) {
            idx = (idx - 1 + kNumLivesOptions) % kNumLivesOptions;
            sLives = kLivesValues[idx];
        }
        UIRect pr = layout.getRectDesign("Lives +", {1110.0f, 375.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f})) {
            idx = (idx + 1) % kNumLivesOptions;
            sLives = kLivesValues[idx];
        }
    }

    // Time Limit
    {
        int idx = 0;
        for (int i = 0; i < kNumTimeOptions; ++i) {
            if (kTimeValues[i] == sTimeLimit) { idx = i; break; }
        }
        char text[128];
        snprintf(text, sizeof(text), "Time Limit: %s", kTimeOptions[idx]);
        uiDrawText(text, uiScaleX(700.0f), uiScaleY(440.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("Time Limit -", {1050.0f, 435.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f})) {
            idx = (idx - 1 + kNumTimeOptions) % kNumTimeOptions;
            sTimeLimit = kTimeValues[idx];
        }
        UIRect pr = layout.getRectDesign("Time Limit +", {1110.0f, 435.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f})) {
            idx = (idx + 1) % kNumTimeOptions;
            sTimeLimit = kTimeValues[idx];
        }
    }

    // Start
    {
        UIRect sd = layout.getRectDesign("START", {820.0f, 540.0f, 300.0f, 80.0f});
        if (guiButton(win, "START BOMB TAG", sd.x, sd.y, sd.w, sd.h, {0.9f, 0.25f, 0.1f, 1.0f}))
            r.start = true;
    }

    if (guiBackButton(win, layout.getRectDesign("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
        r.goBack = true;

    return r;
}
