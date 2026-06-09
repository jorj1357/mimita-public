#include "sandbox-map-menu.h"

#include <algorithm>
#include <cstdio>

#include "../gui-back.h"
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

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(win, &width, &height);
    const float centerX = width * 0.5f;

    uiDrawRect({0, 0, (float)width, (float)height},
               {0.035f, 0.04f, 0.052f, 1.0f}, "sandbox-map-menu-bg");
    uiDrawText("SANDBOX MAPS", centerX - 125.0f, 60.0f, 0.72f,
               {0.55f, 0.78f, 1.0f, 1.0f});
    uiDrawText(gCatalog.status.c_str(), centerX - 210.0f, 112.0f, 0.34f,
               gCatalog.maps.empty()
                   ? glm::vec4(1.0f, 0.48f, 0.3f, 1.0f)
                   : glm::vec4(0.65f, 0.85f, 1.0f, 1.0f));

    if (!gLoadMessage.empty())
    {
        uiDrawText(gLoadMessage.c_str(), centerX - 260.0f, 145.0f, 0.30f,
                   gLoadSucceeded
                       ? glm::vec4(0.35f, 1.0f, 0.5f, 1.0f)
                       : glm::vec4(1.0f, 0.35f, 0.25f, 1.0f));
    }

    const int rowsPerPage = std::max(1, std::min(7, (height - 270) / 58));
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
                     {centerX - 280.0f, y, 560.0f, 48.0f},
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

    const float controlsY = (float)height - 82.0f;
    if (uiButton(win, "REFRESH", {centerX - 90.0f, controlsY, 180.0f, 46.0f},
                 {0.24f, 0.62f, 0.48f, 1.0f}).clicked)
    {
        refreshCatalog();
        gLoadMessage.clear();
    }

    if (pageCount > 1)
    {
        if (uiButton(win, "<", {centerX - 220.0f, controlsY, 62.0f, 46.0f},
                     {0.24f, 0.35f, 0.5f, 1.0f}).clicked)
            gPage = std::max(0, gPage - 1);
        if (uiButton(win, ">", {centerX + 158.0f, controlsY, 62.0f, 46.0f},
                     {0.24f, 0.35f, 0.5f, 1.0f}).clicked)
            gPage = std::min(pageCount - 1, gPage + 1);

        char pageText[64];
        snprintf(pageText, sizeof(pageText), "Page %d / %d", gPage + 1, pageCount);
        uiDrawText(pageText, centerX - 54.0f, controlsY - 28.0f, 0.28f,
                   {0.72f, 0.78f, 0.88f, 1.0f});
    }

    if (guiBackButton(win))
        result.goBack = true;
    return result;
}
