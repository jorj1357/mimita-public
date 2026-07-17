#include "online-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-bindings.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include "../../map/map-catalog.h"
#include "../../avatar/avatar-editor-dropdown.h"
#include "../../network/multiplayer-context.h"
#include "../../network/coordinator-client.h"
#include "../../network/server.h"
#include "../../auth/auth-system.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

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
        mapItemsCache = "funworld3";
}

// Build items list for a dropdown element (same logic as in drawGuiElement)
static std::vector<std::string> buildDropdownItems(const GuiElement* elem)
{
    std::vector<std::string> items;
    if (!elem->bindingItems.empty()) {
        std::string itemsStr = GuiBindings::instance().get(elem->bindingItems);
        if (!itemsStr.empty()) {
            size_t pos = 0;
            while ((pos = itemsStr.find(',')) != std::string::npos) {
                items.push_back(itemsStr.substr(0, pos));
                itemsStr.erase(0, pos + 1);
            }
            if (!itemsStr.empty()) items.push_back(itemsStr);
        }
    }
    if (items.empty() && !elem->backgroundImage.empty()) {
        std::string itemsStr = elem->backgroundImage;
        size_t pos = 0;
        while ((pos = itemsStr.find(',')) != std::string::npos) {
            items.push_back(itemsStr.substr(0, pos));
            itemsStr.erase(0, pos + 1);
        }
        if (!itemsStr.empty()) items.push_back(itemsStr);
    }
    if (items.empty()) items.push_back("None");
    return items;
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
    if (serverCodeDisplay != code)
    {
        printf("[ONLINE MENU SET SERVER CODE] old=%s new=%s\n",
               serverCodeDisplay.c_str(),
               code.c_str());
    }
    serverCodeDisplay = code;
}

const std::string& onlineMenuGetServerCode() {
    return serverCodeDisplay;
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

    ensureMapListScanned();

    b.set("server.code", serverCodeDisplay);
    b.set("server.status", serverRunning ? "Running" : "Stopped");
    b.set("server.running", serverRunning ? "true" : "false");
    b.set("server.not_running", serverRunning ? "false" : "true");
    b.set("server.name_placeholder", "MiMITA Server");
    b.set("server.map_placeholder", "funworld3");
    b.set("server.player_limit_placeholder", "999");
    b.set("server.map_items", mapItemsCache);
    // Log current map selection
    {
        static std::string lastMapSelection;
        std::string currentMap = b.get("server.map");
        if (currentMap != lastMapSelection && !currentMap.empty()) {
            lastMapSelection = currentMap;
            printf("[COMMUNITY MAP SELECT] mapId=%s\n", currentMap.c_str());
        }
    }
    // Initialize one-time defaults (only set if not already present)
    static bool defaultsInitialized = false;
    if (!defaultsInitialized) {
        b.set("server.startup_npcs", "true");
        b.set("server.startup_npc_count", "3");
        b.set("join.code_placeholder", "______");
        defaultsInitialized = true;
        printf("[ONLINE MENU] defaults initialized\n");
    }

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/community-menu.json");

    // Phase 1: draw all elements sorted by layer
    std::vector<std::pair<int, std::string>> sorted;
    for (const std::string& id : layout.elementIds()) {
        const GuiElement* e = layout.get(id);
        if (e) sorted.push_back({e->layer, id});
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
            return a.first < b.first;
        });

    for (const auto& pair : sorted)
    {
        const std::string& id = pair.second;
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
            // Use coordinator join flow to get a valid join token
            std::string code = serverCodeDisplay;
            if (!code.empty())
            {
                printf("[ROOM JOIN LOOKUP] api=coordinatorJoin code=%s source=connectToThisServer\n", code.c_str());
                r.roomCode = code;
                MimitaNet::CoordinatorJoinResult joinResult =
                    MimitaNet::coordinatorJoin(code,
                        AuthSystem::instance().displayName());
                if (joinResult.ok)
                {
                    r.connectToServer = true;
                    r.connectAddress = joinResult.serverIp;
                    r.connectPort = joinResult.serverPort;
                    r.joinToken = joinResult.joinToken;
                    printf("[ONLINE MENU] connectToThisServer code=%s token=%s\n",
                           code.c_str(), joinResult.joinToken.substr(0, 12).c_str());
                }
                else
                {
                    printf("[ONLINE MENU] connectToThisServer join failed, falling back to direct\n");
                    r.connectToServer = true;
                    r.connectAddress = "127.0.0.1";
                    r.connectPort = MimitaNet::DEFAULT_PORT;
                }
            }
            else
            {
                r.connectToServer = true;
                r.connectAddress = "127.0.0.1";
                r.connectPort = MimitaNet::DEFAULT_PORT;
            }
        }
        else if (id == "connectLocalhostButton" && s.clicked)
        {
            // Use coordinator join flow for consistent validation
            std::string code = serverCodeDisplay;
            if (!code.empty())
            {
                printf("[ROOM JOIN LOOKUP] api=coordinatorJoin code=%s source=connectLocalhost\n", code.c_str());
                r.roomCode = code;
                MimitaNet::CoordinatorJoinResult joinResult =
                    MimitaNet::coordinatorJoin(code,
                        AuthSystem::instance().displayName());
                if (joinResult.ok)
                {
                    r.connectToServer = true;
                    r.connectAddress = joinResult.serverIp;
                    r.connectPort = joinResult.serverPort;
                    r.joinToken = joinResult.joinToken;
                    printf("[ONLINE MENU] connectLocalhost code=%s token=%s\n",
                           code.c_str(), joinResult.joinToken.substr(0, 12).c_str());
                }
                else
                {
                    printf("[ONLINE MENU] connectLocalhost join failed, falling back to direct\n");
                    r.roomCode = code;
                    r.connectLocalhost = true;
                    r.connectAddress = "127.0.0.1";
                    r.connectPort = MimitaNet::DEFAULT_PORT;
                }
            }
            else
            {
                r.roomCode = serverCodeDisplay;
                r.connectLocalhost = true;
                r.connectAddress = "127.0.0.1";
                r.connectPort = MimitaNet::DEFAULT_PORT;
            }
        }
        else if (id == "joinServerButton" && s.clicked)
        {
            std::string code = b.get("join.code");
            if (code.find("______") != std::string::npos)
                code.clear();

            if (!code.empty())
            {
                // Non-mutating lookup: check if room exists and its type
                printf("[ROOM JOIN] looking up code=%s\n", code.c_str());
                MimitaNet::CoordinatorLookupResult lookup =
                    MimitaNet::coordinatorIceLookup(code);
                if (lookup.reachable && lookup.isIce && lookup.exists)
                {
                    // ICE room — full ICE negotiation happens in mpIceConnect.
                    r.roomCode = code;
                    r.connectToServer = true;
                    r.connectAddress = "ice:" + code;  // ICE marker address
                    r.connectPort = 0;
                    printf("[ONLINE MENU] ICE room code=%s\n", code.c_str());
                }
                else if (lookup.reachable && lookup.exists && lookup.status == "online")
                {
                    // Normal (non-ICE) room
                    MimitaNet::CoordinatorJoinResult joinResult =
                        MimitaNet::coordinatorJoin(code,
                            AuthSystem::instance().displayName());
                    if (joinResult.ok)
                    {
                        r.roomCode = code;
                        r.connectToServer = true;
                        r.connectAddress = joinResult.serverIp;
                        r.connectPort = joinResult.serverPort;
                        r.joinToken = joinResult.joinToken;
                        printf("[ONLINE MENU] normal join code=%s server=%s:%u\n",
                               code.c_str(), joinResult.serverIp.c_str(),
                               joinResult.serverPort);
                    }
                    else
                    {
                        b.set("join.code", "Join failed");
                        printf("[ONLINE MENU] join FAILED for code=%s\n", code.c_str());
                    }
                }
                else if (lookup.exists)
                {
                    b.set("join.code", "Server offline");
                }
                else if (!lookup.reachable)
                {
                    b.set("join.code", "Coordinator unreachable");
                }
                else
                {
                    b.set("join.code", "Room not found");
                }
            }
        }
        else if (id == "backButton" && s.clicked)
        {
            r.goBack = true;
        }
    }

    // Phase 2: draw open dropdown overlays on top of everything
    auto& allDropdowns = getDropdownStates();
    for (auto& kv : allDropdowns)
    {
        DropdownState& ds = kv.second;
        if (!ds.open) continue;

        const GuiElement* elem = layout.get(kv.first);
        if (!elem || elem->type != "dropdown") continue;

        std::vector<std::string> items = buildDropdownItems(elem);
        if (items.empty()) continue;

        int sel = drawDropdownOverlay(win, ds, elem->x, elem->y, elem->w, elem->h, items);
        if (sel >= 0 && !elem->binding.empty() && sel < (int)items.size()) {
            b.set(elem->binding, items[sel]);
        }
    }

    return r;
}
