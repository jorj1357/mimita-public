#include "sandbox-map-menu.h"

#include <algorithm>
#include <cstdio>

#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"
#include "map/map-catalog.h"

namespace {

MapCatalogResult gCatalog;
bool gActive = false;
int gPage = 0;
std::string gLoadMessage;
bool gLoadSucceeded = false;

void refreshCatalog()
{
    gCatalog = scanMapCatalog();
    gPage = 0;
}

} // namespace

void sandboxMapMenuSetActive(bool active)
{
    if (active && !gActive)
    {
        refreshCatalog();
        gLoadMessage.clear();
    }
    gActive = active;
}

void sandboxMapMenuSetLoadResult(const std::string& message, bool success)
{
    gLoadMessage = message;
    gLoadSucceeded = success;
}

SandboxMapMenuResult drawSandboxMapMenu(GLFWwindow* win)
{
    SandboxMapMenuResult result;

    float fbW = uiScreenW(), fbH = uiScreenH();

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/sandbox-map-menu.json");

    uiDrawRect({0, 0, fbW, fbH},
               {0.035f, 0.04f, 0.052f, 1.0f}, "sandbox-map-menu-bg");

    // Header text
    const GuiElement* headerEl = layout.get("header");
    if (headerEl) drawGuiElement(win, *headerEl);

    // Status text (dynamic)
    uiDrawText(gCatalog.status.c_str(), uiScaleX(750.0f), uiScaleY(112.0f), 0.34f,
               gCatalog.maps.empty()
                   ? glm::vec4(1.0f, 0.48f, 0.3f, 1.0f)
                   : glm::vec4(0.65f, 0.85f, 1.0f, 1.0f));

    if (!gLoadMessage.empty())
    {
        uiDrawText(gLoadMessage.c_str(), uiScaleX(700.0f), uiScaleY(145.0f), 0.30f,
                   gLoadSucceeded
                       ? glm::vec4(0.35f, 1.0f, 0.5f, 1.0f)
                       : glm::vec4(1.0f, 0.35f, 0.25f, 1.0f));
    }

    // In design space (1920x1080), rowsPerPage is always capped at 7
    const int rowsPerPage = 7;
    const int pageCount = std::max(
        1, ((int)gCatalog.maps.size() + rowsPerPage - 1) / rowsPerPage);
    gPage = std::clamp(gPage, 0, pageCount - 1);
    const int first = gPage * rowsPerPage;
    const int last = std::min(first + rowsPerPage, (int)gCatalog.maps.size());

    float y = 190.0f;
    for (int i = first; i < last; ++i)
    {
        const MapCatalogEntry& map = gCatalog.maps[i];
        if (uiButton(win, map.displayName.c_str(),
                     {680.0f, y, 560.0f, 48.0f},
                     {0.18f, 0.42f, 0.68f, 1.0f}).clicked)
        {
            printf("[SANDBOX MAP MENU] selected index=%d path=%s\n",
                   i, map.assetPath.c_str());
            result.startSandbox = true;
            result.mapPath = map.assetPath;
            return result;
        }
        y += 58.0f;
    }

    const float controlsY = 998.0f;

    // Render REFRESH, prev, next, back from layout
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;
        if (id == "header") continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "REFRESH")
        {
            refreshCatalog();
            gLoadMessage.clear();
        }
        else if (id == "<")
        {
            if (pageCount > 1)
                gPage = std::max(0, gPage - 1);
        }
        else if (id == ">")
        {
            if (pageCount > 1)
                gPage = std::min(pageCount - 1, gPage + 1);
        }
        else if (id == "backButton")
        {
            result.goBack = true;
        }
    }

    // Page text
    if (pageCount > 1)
    {
        char pageText[64];
        snprintf(pageText, sizeof(pageText), "Page %d / %d", gPage + 1, pageCount);
        uiDrawText(pageText, uiScaleX(906.0f), uiScaleY(controlsY - 28.0f), 0.28f,
                   {0.72f, 0.78f, 0.88f, 1.0f});
    }

    return result;
}
