// 08 10 2026, 14 34
/* purpose
* Implements the duels queue/match HUD rendering using the shared UI system.
* Draws the queue timer + status, the match-found banner, in-duel scoreboard
* and countdown, the win/lose + rematch screen, the recent-duels panel with
* rematch buttons, and the enemy-spawn tracer (world-space beam + label).
* Does NOT contain matchmaking, network, or queue-state logic.
*/

#include "duel/duel-ui.h"
#include "duel/duel-queue.h"
#include "duel/duel-history.h"

#include <algorithm>
#include <cstdio>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "terminal/terminal-state.h"
#include "gui/ui-system.h"
#include "input/mouse-lock.h"
#include "network/net_common.h"
#include "camera.h"
#include "debug/debug-visuals.h"
#include "game/game-state.h"

namespace {

void drawCenteredText(const char* text, float y, float scale, glm::vec4 color)
{
    const float w = uiMeasureText(text, scale);
    uiDrawText(text, uiScreenW() * 0.5f - w * 0.5f, y, scale, color);
}

std::string formatElapsed(float seconds)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", seconds);
    return buf;
}

std::string formatAgeAgo(uint64_t unixMs)
{
    const uint64_t nowMsVal = MimitaNet::nowMs();
    const int64_t diffMs = (int64_t)nowMsVal - (int64_t)unixMs;
    const int64_t diffS = diffMs / 1000;
    char buf[48];
    if (diffS < 60) snprintf(buf, sizeof(buf), "%llds ago", (long long)std::max<int64_t>(0, diffS));
    else if (diffS < 3600) snprintf(buf, sizeof(buf), "%lldm ago", (long long)(diffS / 60));
    else if (diffS < 86400) snprintf(buf, sizeof(buf), "%lldh ago", (long long)(diffS / 3600));
    else snprintf(buf, sizeof(buf), "%lldd ago", (long long)(diffS / 86400));
    return buf;
}

void drawRecentDuelsPanel(GLFWwindow* win, bool mouseUnlocked)
{
    const auto& entries = DuelHistory::instance().entries();
    if (entries.empty())
        return;

    const float sw = uiScreenW();
    const float sh = uiScreenH();

    if (!mouseUnlocked)
    {
        // Slim collapsed strip so the HUD stays uncluttered while fighting.
        UIRect strip = {sw - 34.0f, sh * 0.5f - 60.0f, 26.0f, 120.0f};
        uiDrawRect(strip, {0.0f, 0.0f, 0.0f, 0.45f}, "duels-strip-bg");
        uiDrawText("D", strip.x + 6.0f, strip.y + 16.0f, 0.4f, {0.4f, 0.9f, 0.5f, 1.0f});
        uiDrawText("U", strip.x + 6.0f, strip.y + 48.0f, 0.4f, {0.4f, 0.9f, 0.5f, 1.0f});
        uiDrawText("E", strip.x + 6.0f, strip.y + 80.0f, 0.4f, {0.4f, 0.9f, 0.5f, 1.0f});
        uiDrawText("L", strip.x + 6.0f, strip.y + 100.0f, 0.4f, {0.4f, 0.9f, 0.5f, 1.0f});
        return;
    }

    const float panelW = 300.0f;
    const float panelX = sw - panelW - 12.0f;
    const float panelY = sh * 0.12f;
    UIRect panel = {panelX, panelY, panelW, 0.0f};
    const float headerH = 30.0f;
    const float rowH = 62.0f;
    const int show = std::min((int)entries.size(), 6);
    panel.h = headerH + rowH * (float)show;

    uiDrawRect(panel, {0.03f, 0.03f, 0.04f, 0.82f}, "duels-recent-bg");
    uiDrawRectOutline(panel, {0.25f, 0.7f, 0.45f, 0.6f}, "duels-recent-outline");
    uiDrawText("RECENT DUELS", panelX + 12.0f, panelY + 6.0f, 0.34f, {0.5f, 0.95f, 0.6f, 1.0f});

    for (int i = 0; i < show; ++i)
    {
        const DuelHistoryEntry& e = entries[i];
        const float rowY = panelY + headerH + rowH * (float)i;
        char line[128];
        snprintf(line, sizeof(line), "%s  %d - %d", e.won ? "WIN " : "LOSE", e.myScore, e.oppScore);
        uiDrawText(line, panelX + 12.0f, rowY + 4.0f, 0.32f,
                   e.won ? glm::vec4(0.35f, 0.95f, 0.4f, 1.0f) : glm::vec4(0.95f, 0.35f, 0.35f, 1.0f));
        uiDrawText(e.opponentName.c_str(), panelX + 12.0f, rowY + 26.0f, 0.26f, {0.85f, 0.85f, 0.85f, 1.0f});
        uiDrawText(formatAgeAgo(e.unixMs).c_str(), panelX + 12.0f, rowY + 44.0f, 0.22f, {0.6f, 0.6f, 0.6f, 1.0f});

        // Rematch button for this opponent.
        UIRect btn = {panelX + panelW - 96.0f, rowY + 12.0f, 80.0f, 30.0f};
        UIButtonState s = uiButton(win, "Rematch", btn, {0.15f, 0.55f, 0.25f, 1.0f}, "duels-rematch");
        if (s.clicked)
            DuelQueue::instance().requestRematchWith(e.opponentName);
    }
}

} // namespace

void renderDuelQueueHud(GLFWwindow* win, float dt)
{
    DuelQueue& dq = DuelQueue::instance();
    if (!dq.isActive())
        return;

    const float sw = uiScreenW();
    const float sh = uiScreenH();
    const bool mouseUnlocked = !MouseLock::locked();

    switch (dq.state())
    {
    case DuelQueueState::Queuing:
    {
        drawCenteredText("QUEUED", sh * 0.14f, 0.4f, {0.45f, 0.9f, 0.55f, 1.0f});
        drawCenteredText(formatElapsed(dq.queueElapsed()).c_str(), sh * 0.20f, 1.1f, {0.6f, 1.0f, 0.65f, 1.0f});
        drawCenteredText(dq.statusText().c_str(), sh * 0.32f, 0.3f, {0.7f, 0.75f, 0.8f, 0.9f});
        drawCenteredText("Esc = leave queue", sh * 0.92f, 0.26f, {0.5f, 0.5f, 0.5f, 0.8f});
        drawRecentDuelsPanel(win, mouseUnlocked);
        break;
    }

    case DuelQueueState::MatchFound:
    case DuelQueueState::HostLaunching:
    case DuelQueueState::Connecting:
    {
        static float pulse = 0.0f;
        pulse += dt;
        const float a = 0.75f + 0.25f * sinf(pulse * 6.0f);
        drawCenteredText("match found!!!!!!!!", sh * 0.42f, 0.85f, {0.3f, 1.0f, 0.35f, a});
        drawCenteredText(dq.statusText().c_str(), sh * 0.55f, 0.3f, {0.8f, 0.8f, 0.8f, 0.9f});
        break;
    }

    default:
        break;
    }
}

void renderDuelMatchHud(GLFWwindow* win, float dt)
{
    DuelQueue& dq = DuelQueue::instance();
    if (!dq.inDuel())
        return;

    const float sw = uiScreenW();
    const float sh = uiScreenH();

    if (dq.countdownActive())
    {
        char num[8];
        snprintf(num, sizeof(num), "%.0f", std::ceil(std::max(0.0f, dq.countdownLeft())));
        drawCenteredText(num, sh * 0.35f, 1.6f, {1.0f, 1.0f, 1.0f, 1.0f});
        return;
    }

    if (dq.matchOver())
    {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.55f}, "duel-end-dim");

        const char* text = dq.won() ? "YOU WIN" : "YOU LOSE";
        const glm::vec4 color = dq.won()
            ? glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
            : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
        drawCenteredText(text, sh * 0.34f, 1.3f, color);

        char score[64];
        snprintf(score, sizeof(score), "%d - %d", dq.myScore(), dq.oppScore());
        drawCenteredText(score, sh * 0.48f, 0.7f, {1.0f, 0.85f, 0.25f, 1.0f});

        char rematch[80];
        snprintf(rematch, sizeof(rematch), "rematch in %.0f...", std::max(0.0f, dq.rematchLeft()));
        drawCenteredText(rematch, sh * 0.58f, 0.42f, {0.7f, 0.9f, 0.75f, 1.0f});

        drawCenteredText("keep fighting - scores locked", sh * 0.66f, 0.28f, {0.65f, 0.65f, 0.65f, 0.9f});
        drawCenteredText("Esc = back to queue", sh * 0.74f, 0.28f, {0.55f, 0.55f, 0.55f, 0.9f});

        drawRecentDuelsPanel(win, true);
        return;
    }

    // Active fight: scoreboard with team names.
    char score[128];
    snprintf(score, sizeof(score), "%s  %d - %d  %s",
             dq.teamName(true).c_str(), dq.myScore(), dq.oppScore(),
             dq.teamName(false).c_str());
    drawCenteredText(score, sh * 0.06f, 0.55f, {1.0f, 0.9f, 0.35f, 1.0f});

    char goal[64];
    snprintf(goal, sizeof(goal), "first to %d", dq.goal());
    drawCenteredText(goal, sh * 0.11f, 0.24f, {0.7f, 0.75f, 0.8f, 0.8f});

    drawRecentDuelsPanel(win, !MouseLock::locked());
}

void renderDuelTracer(const Camera& camera)
{
    DuelQueue& dq = DuelQueue::instance();
    if (!dq.tracerActive() || !dq.inDuel())
        return;

    const glm::vec3 base = dq.tracerPos();
    const glm::vec4 color = {0.3f, 1.0f, 0.35f, 0.9f};

    // Tall bright beam above the enemy's spawn.
    DebugVis::drawFilledBeam(camera,
        glm::vec3(base.x, base.y + 12.0f, base.z),
        glm::vec3(base.x, base.y - 1.0f, base.z),
        0.25f, color);
    DebugVis::drawFilledBeam(camera,
        glm::vec3(base.x, base.y + 9.0f, base.z),
        glm::vec3(base.x, base.y + 11.0f, base.z),
        1.2f, color);
    DebugVis::drawWorldLabel(base + glm::vec3(0.0f, 13.0f, 0.0f), "ENEMY", color);
}
