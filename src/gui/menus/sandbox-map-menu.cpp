#include "sandbox-map-menu.h"

#include <algorithm>
#include <cstdio>

#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
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
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/sandbox-map-menu.json");
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    // Draw all static elements from layout
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        if (elem->type == "panel" || elem->type == "text" || elem->type == "label")
        {
            drawGuiElement(win, *elem);
            continue;
        }

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "refreshButton")
        {
            refreshCatalog();
            gLoadMessage.clear();
        }
        else if (id == "prevPage")
        {
            int pageCount = std::max(1, ((int)gCatalog.maps.size() + 6) / 7);
            if (pageCount > 1)
                gPage = std::max(0, gPage - 1);
        }
        else if (id == "nextPage")
        {
            int pageCount = std::max(1, ((int)gCatalog.maps.size() + 6) / 7);
            if (pageCount > 1)
                gPage = std::min(pageCount - 1, gPage + 1);
        }
        else if (id == "backButton")
            result.goBack = true;
    }

    // Dynamic status text (overrides the static empty text element)
    const GuiElement* st = layout.get("statusText");
    if (st)
    {
        glm::vec4 statusColor = gCatalog.maps.empty()
            ? glm::vec4(1.0f, 0.48f, 0.3f, 1.0f)
            : st->getTextColorVec();
        uiDrawText(gCatalog.status.c_str(), cs.designToScreenX(st->x),
                   cs.designToScreenY(st->y), st->fontSize, statusColor);
    }

    // Load message
    if (!gLoadMessage.empty())
    {
        glm::vec4 msgColor = gLoadSucceeded
            ? glm::vec4(0.35f, 1.0f, 0.5f, 1.0f)
            : glm::vec4(1.0f, 0.35f, 0.25f, 1.0f);
        uiDrawText(gLoadMessage.c_str(), cs.designToScreenX(700.0f),
                   cs.designToScreenY(145.0f), 0.30f, msgColor);
    }

    // Map list (dynamic)
    const int rowsPerPage = 7;
    int pageCount = std::max(1, ((int)gCatalog.maps.size() + rowsPerPage - 1) / rowsPerPage);
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

    // Page text
    if (pageCount > 1)
    {
        char pageText[64];
        snprintf(pageText, sizeof(pageText), "Page %d / %d", gPage + 1, pageCount);
        uiDrawText(pageText, cs.designToScreenX(960.0f), cs.designToScreenY(1006.0f),
                   0.28f, {0.72f, 0.78f, 0.88f, 1.0f});
    }

    return result;
}
