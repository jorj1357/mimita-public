// 08 05 2026, 00 00
/* purpose
* Implements the in-game ranked leaderboard menu.
* Fetches top rows from the website leaderboard API once per open and renders a
* scrollable table using full VIP name styling.
* Lets the competitive leaderboard button and the `leaderboard` terminal
* command open the same shared surface.
* DOES NOT verify VIP entitlements or mutate account/rank state.
* DOES NOT own website HTTP transport, database, or Stripe state.
* DOES NOT render chat, nameplates, or HUD combat text.
*/

#include "leaderboard/leaderboard-ui.h"

#include <cstdio>
#include <string>
#include <vector>

#include "gui/ui-system.h"
#include "devtools/terminal.h"
#include "gui/gui-main.h"
#include "debug/debug-log.h"
#include "website/api-client.h"
#include "vip/vip-name-render.h"

namespace {

std::vector<LeaderboardEntry> gRows;
bool gRowsLoaded = false;
bool gRowsFailed = false;

void fetchRows()
{
    gRows = getLeaderboard("mmr", 50);
    gRowsLoaded = true;
    gRowsFailed = gRows.empty();
    Debug::warn(Debug::Category::Networking, "[LEADERBOARD] fetched rows=%zu failed=%d\n",
                gRows.size(), (int)gRowsFailed);
}

void drawRow(int rank, const std::string& name,
             const MimitaVip::VipAppearance& appearance,
             const MimitaVip::VipStyleDetail& detail,
             const LeaderboardEntry& entry, float x, float y, float w)
{
    char rankBuf[16];
    std::snprintf(rankBuf, sizeof(rankBuf), "#%d", rank);
    uiDrawText(rankBuf, x, y, 0.26f, {0.6f, 0.75f, 0.9f, 1.0f});
    float cx = x + 60.0f;

    VipNameDrawOptions nameOptions;
    nameOptions.scale = 0.26f;
    nameOptions.alpha = 1.0f;
    nameOptions.phase = 0.0f;
    nameOptions.detail = &detail;
    vipDrawStyledName(name, appearance, cx, y, nameOptions);
    cx += vipMeasureStyledName(name, appearance, nameOptions);

    char mmrBuf[32];
    std::snprintf(mmrBuf, sizeof(mmrBuf), "  MMR %d", entry.stats.currentMmr);
    uiDrawText(mmrBuf, cx, y, 0.24f, {1.0f, 1.0f, 1.0f, 0.9f});
    cx += uiMeasureText(mmrBuf, 0.24f);

    char recBuf[40];
    std::snprintf(recBuf, sizeof(recBuf), "  W/L %d/%d", entry.stats.wins, entry.stats.losses);
    uiDrawText(recBuf, x + w - uiMeasureText(recBuf, 0.24f) - 20.0f, y, 0.24f,
               {0.7f, 0.8f, 0.9f, 0.85f});
}

} // namespace

LeaderboardMenuAction drawLeaderboardMenu(GLFWwindow* win)
{
    if (!gRowsLoaded)
        fetchRows();

    const float sw = uiScreenW();
    const float sh = uiScreenH();
    uiDrawRect({0, 0, sw, sh}, {0.028f, 0.032f, 0.045f, 1.0f}, "leaderboard-bg");

    uiDrawText("LEADERBOARDS", sw * 0.5f - uiMeasureText("LEADERBOARDS", 0.55f) * 0.5f,
               40.0f, 0.55f, {0.9f, 0.95f, 1.0f, 1.0f});

    const float tableX = sw * 0.15f;
    const float tableY = 110.0f;
    const float tableW = sw * 0.70f;
    const float rowH = 30.0f;
    const float tableH = sh - tableY - 110.0f;

    uiDrawRect({tableX, tableY, tableW, tableH}, {0.035f, 0.04f, 0.06f, 0.9f}, "leaderboard-table");
    uiDrawRectOutline({tableX, tableY, tableW, tableH}, {0.15f, 0.2f, 0.3f, 0.4f}, "leaderboard-table-border");

    if (gRowsFailed)
    {
        uiDrawText("Unable to load leaderboard.", tableX + 20.0f, tableY + 20.0f,
                   0.28f, {1.0f, 0.4f, 0.4f, 1.0f});
    }
    else if (gRows.empty())
    {
        uiDrawText("Loading...", tableX + 20.0f, tableY + 20.0f,
                   0.28f, {0.6f, 0.7f, 0.8f, 1.0f});
    }
    else
    {
        static UIScrollState scroll;
        const float contentH = (float)gRows.size() * rowH;
        uiBeginScrollArea(win, {tableX, tableY, tableW, tableH}, contentH, scroll);
        float y = tableY + 8.0f;
        for (size_t i = 0; i < gRows.size(); ++i)
        {
            if (y + rowH > tableY + tableH) break;
            const LeaderboardEntry& entry = gRows[i];
            drawRow(i + 1, entry.username, entry.vipAppearance, entry.vipStyleDetail,
                    entry, tableX + 20.0f, y, tableW - 40.0f);
            y += rowH;
        }
        uiEndScrollArea({tableX, tableY, tableW, tableH}, contentH, scroll);
    }

    if (uiButton(win, "Back", {sw * 0.5f - 80.0f, sh - 70.0f, 160.0f, 40.0f},
                 {0.15f, 0.2f, 0.3f, 0.9f}, "leaderboard-back").clicked)
    {
        gRowsLoaded = false;
        return LeaderboardMenuAction::GoBack;
    }

    return LeaderboardMenuAction::None;
}

void registerLeaderboardCommands()
{
    Terminal::instance().registerCommand({
        "leaderboard", "Open the ranked leaderboard", "leaderboard",
        [](const std::vector<std::string>&) {
            gGuiMenuState = GUI_MENU_LEADERBOARD;
        },
        "08 05 2026",
        CommandCategory::UI
    });
}
