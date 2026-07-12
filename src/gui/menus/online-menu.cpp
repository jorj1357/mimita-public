#include "online-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-bindings.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include "../../map/map-catalog.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

bool menuActive = false;
bool serverRunning = false;
std::string serverCodeDisplay;
bool mapListScanned = false;
std::string mapItemsCache;

void ensureMapListScanned()
{
    if (mapListScanned)
        return;
    mapListScanned = true;
    MapCatalogResult catalog = scanMapCatalog();
    mapItemsCache.clear();
    for (size_t i = 0; i < catalog.maps.size(); ++i)
    {
        if (i > 0) mapItemsCache += ",";
        mapItemsCache += catalog.maps[i].displayName;
    }
    if (mapItemsCache.empty())
        mapItemsCache = "funworldv3";
}

} // anonymous namespace

void onlineMenuSetActive(bool active) {
    menuActive = active;
    if (!active) {
        GuiBindings::instance().clearFocus();
    }
}

void onlineMenuSetServerRunning(bool running) {
    serverRunning = running;
}

void onlineMenuSetServerCode(const std::string& code) {
    serverCodeDisplay = code;
}

void onlineMenuHandleChar(unsigned int codepoint) {
    (void)codepoint;
}

void onlineMenuHandleKey(int key, int action) {
    (void)key;
    (void)action;
}

OnlineMenuResult drawOnlineMenu(GLFWwindow* win)
{
    OnlineMenuResult r{};

    // Sync bindings with actual state
    GuiBindings& b = GuiBindings::instance();

    // Scan map list once
    ensureMapListScanned();

    // Server state bindings
    b.set("server.code", serverCodeDisplay);
    b.set("server.status", serverRunning ? "Running" : "Stopped");
    b.set("server.running", serverRunning ? "true" : "false");
    b.set("server.not_running", serverRunning ? "false" : "true");
    b.set("server.name_placeholder", "MiMITA Server");
    b.set("server.map_placeholder", "funworldv3");
    b.set("server.player_limit_placeholder", "999");
    b.set("server.map_items", mapItemsCache);
    b.set("join.code_placeholder", "______");
    b.set("join.code", "");

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/community-menu.json");

    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        UIButtonState s = drawGuiElement(win, *elem);

        if (id == "startServerButton" && s.clicked)
        {
            if (!serverRunning)
                r.startServer = true;
        }
        else if (id == "stopServerButton" && s.clicked)
        {
            if (serverRunning)
                r.stopServer = true;
        }
        else if (id == "connectToThisServerButton" && s.clicked)
        {
            // Connect host to local listen server
            r.connectToServer = true;
            r.connectAddress = "127.0.0.1:1357";
        }
        else if (id == "joinServerButton" && s.clicked)
        {
            // Read join code from binding and resolve to IP
            std::string code = b.get("join.code");
            // For now, always connect to localhost
            r.connectToServer = true;
            r.connectAddress = "127.0.0.1:1357";
        }
        else if (id == "backButton" && s.clicked)
        {
            r.goBack = true;
        }
    }

    return r;
}
