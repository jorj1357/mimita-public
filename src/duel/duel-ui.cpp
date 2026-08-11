// 08 10 2026, 14 34
/* purpose
* Renders the duels queue and in-match HUD from hot-reloadable GUI layout JSON:
* the wait timer, match-found banner, connect status (with ping), scoreboard
* (player names), countdown, win/lose overlay, rematch hints, recent-duels panel,
* and the enemy-spawn tracer.
* Reads state from DuelQueue and DuelHistory; all positions/colors/fonts come
* from config/gui/duel-queue-hud.json and config/gui/duel-match-hud.json.
* Does NOT run matchmaking, connect to servers, or mutate queue state.
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
#include "gui/gui-layout.h"
#include "input/mouse-lock.h"
#include "network/net_common.h"
#include "camera.h"
#include "debug/debug-visuals.h"
#include "game/game-state.h"

namespace {

// Draw one text element from a layout at its JSON position with its JSON
// font/color (same pattern as engine-tick-ui-game-hud.cpp).
void hudText(GuiLayout& layout, const std::string& id, const std::string& text)
{
    const GuiElement* el = layout.get(id);
    if (!el) return;
    float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
    glm::vec4 color = el->getTextColorVec();
    uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), scale, color);
}

// Downtime stopwatch line: "wait X.XXs" while not fighting, "last wait X.XXs"
// during an active fight (the gap you just waited).
void drawDowntime(GuiLayout& layout, const DuelQueue& dq, bool fighting)
{
    char buf[64];
    if (fighting)
        snprintf(buf, sizeof(buf), "last wait %.2fs", dq.lastDowntime());
    else
        snprintf(buf, sizeof(buf), "wait %.2fs", dq.downtime());
    hudText(layout, "downtimeText", buf);
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

void drawRecentDuelsPanel(GLFWwindow* win, GuiLayout& layout, bool mouseUnlocked)
{
    const auto& entries = DuelHistory::instance().entries();
    if (entries.empty())
        return;

    const GuiElement* panelEl = layout.get("recentPanel");
    if (!panelEl) return;

    const float panelW = panelEl->w > 0.0f ? panelEl->w : 300.0f;
    const float panelX = uiScaleX(panelEl->x);
    const float panelY = uiScaleY(panelEl->y);

    if (!mouseUnlocked)
    {
        // Slim collapsed strip so the HUD stays uncluttered while fighting.
        UIRect strip = {panelX + panelW - 26.0f, panelY, 26.0f, 120.0f};
        uiDrawRect(strip, {0.0f, 0.0f, 0.0f, 0.45f}, "duels-strip-bg");
        uiDrawText("DUELS", strip.x + 3.0f, strip.y + 40.0f, 0.28f, {0.4f, 0.9f, 0.5f, 1.0f});
        return;
    }

    const float headerH = 30.0f;
    const float rowH = 62.0f;
    const int show = std::min((int)entries.size(), 6);
    UIRect panel = {panelX, panelY, panelW, headerH + rowH * (float)show};

    const glm::vec4 bg = panelEl->getBackgroundColorVec();
    uiDrawRect(panel, bg, "duels-recent-bg");
    uiDrawRectOutline(panel, {0.25f, 0.7f, 0.45f, 0.6f}, "duels-recent-outline");
    hudText(layout, "recentHeader", "RECENT DUELS");

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

    const bool mouseUnlocked = !MouseLock::locked();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/duel-queue-hud.json");

    switch (dq.state())
    {
    case DuelQueueState::Queuing:
    {
        hudText(layout, "queueTitle", "QUEUED");
        hudText(layout, "queueTimer", formatElapsed(dq.queueElapsed()));
        hudText(layout, "queueStatus", dq.statusText());
        std::string mapLabel = "Map: " + (dq.chosenMap().empty() ? "?" : dq.chosenMap());
        hudText(layout, "queueMap", mapLabel);
        hudText(layout, "queueEscHint", "Esc = leave queue");
        drawRecentDuelsPanel(win, layout, mouseUnlocked);
        break;
    }

    case DuelQueueState::MatchFound:
    case DuelQueueState::Connecting:
    {
        static float pulse = 0.0f;
        pulse += dt;
        const GuiElement* banner = layout.get("matchFoundText");
        if (banner)
        {
            float scale = banner->fontSize > 0.0f ? banner->fontSize : 0.85f;
            glm::vec4 color = banner->getTextColorVec();
            color.a = 0.75f + 0.25f * sinf(pulse * 6.0f);
            uiDrawText("match found!!!!!!!!", uiScaleX(banner->x), uiScaleY(banner->y), scale, color);
        }
        hudText(layout, "matchFoundOpponent", dq.opponentName());

        // Live connect status with ping once it's measured.
        char status[160];
        if (MP_CONTEXT.active)
            snprintf(status, sizeof(status), "%s - ping: %ums", dq.statusText().c_str(),
                     MP_CONTEXT.localPingMs);
        else
            snprintf(status, sizeof(status), "%s - ping: %ums", dq.statusText().c_str(),
                     MP_CONTEXT.localPingMs);
        hudText(layout, "connectStatus", status);
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

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/duel-match-hud.json");

    if (dq.countdownActive())
    {
        char num[8];
        snprintf(num, sizeof(num), "%.0f", std::ceil(std::max(0.0f, dq.countdownLeft())));
        hudText(layout, "countdownNum", num);
        drawDowntime(layout, dq, false);
        return;
    }

    if (dq.matchOver())
    {
        const GuiElement* overlay = layout.get("endOverlay");
        if (overlay)
        {
            UIRect full = {uiScaleX(overlay->x), uiScaleY(overlay->y),
                           uiScaleX(overlay->w), uiScaleY(overlay->h)};
            uiDrawRect(full, overlay->getBackgroundColorVec(), "duel-end-dim");
        }

        if (dq.won())
            hudText(layout, "winText", "YOU WIN");
        else
            hudText(layout, "loseText", "YOU LOSE");

        char score[64];
        snprintf(score, sizeof(score), "%d - %d", dq.myScore(), dq.oppScore());
        hudText(layout, "endScore", score);

        char rematch[80];
        snprintf(rematch, sizeof(rematch), "rematch in %.0f...", std::max(0.0f, dq.rematchLeft()));
        hudText(layout, "rematchText", rematch);

        hudText(layout, "endHintContinue", "keep fighting - scores locked");
        hudText(layout, "endHintSpace", "press space to rematch now");
        hudText(layout, "endHintEsc", "esc = back to the queue");

        drawDowntime(layout, dq, false);
        drawRecentDuelsPanel(win, layout, true);
        return;
    }

    // Active fight: scoreboard with player names (me on the left, them on the right).
    char score[128];
    snprintf(score, sizeof(score), "%s %d - %d %s",
             dq.playerName().c_str(), dq.myScore(), dq.oppScore(),
             dq.opponentName().c_str());
    hudText(layout, "scoreText", score);

    char goal[64];
    snprintf(goal, sizeof(goal), "first to %d", dq.goal());
    hudText(layout, "goalText", goal);

    drawDowntime(layout, dq, true);
    drawRecentDuelsPanel(win, layout, !MouseLock::locked());
}

void renderDuelTracer(const Camera& camera)
{
    DuelQueue& dq = DuelQueue::instance();
    if (!dq.tracerActive() || !dq.inDuel())
        return;

    // Beam from the player's CURRENT position to the enemy's spawn, so it
    // always points "over there" wherever you're standing.
    const glm::vec3 from = THE_PLAYER.pos;
    const glm::vec3 to = dq.tracerPos();
    const glm::vec4 color = {0.3f, 1.0f, 0.35f, 0.9f};

    DebugVis::drawFilledBeam(camera, from, to, 0.18f, color);
    // Bright marker at the enemy's spawn so the end of the beam is easy to spot.
    DebugVis::drawFilledBeam(camera,
        glm::vec3(to.x, to.y + 8.0f, to.z),
        glm::vec3(to.x, to.y - 1.0f, to.z),
        0.25f, color);
    DebugVis::drawWorldLabel(to + glm::vec3(0.0f, 9.0f, 0.0f), "ENEMY", color);
}
