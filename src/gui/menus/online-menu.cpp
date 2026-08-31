#include "online-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-bindings.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include "../ui-system-internal.h"
#include "../../map/map-catalog.h"
#include "../../avatar/avatar-editor-dropdown.h"
#include "../../network/multiplayer-context.h"
#include "../../network/coordinator-client.h"
#include "../../network/server-browser.h"
#include "../../network/server.h"
#include "../../network/community-server-config.h"
#include "../../auth/auth-system.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>

namespace {

bool menuActive = false;
bool serverRunning = false;
std::string serverCodeDisplay;
bool mapListScanned = false;
std::string mapItemsCache;
bool browserWsaStarted = false;
bool browserWasActive = false;
UIScrollState gServerListScroll;

// Human-readable uptime: "3h 02m", "12m 05s", "45s".
static std::string formatUptime(uint64_t secs)
{
    uint64_t h = secs / 3600;
    uint64_t m = (secs % 3600) / 60;
    uint64_t s = secs % 60;
    char buf[48];
    if (h > 0) snprintf(buf, sizeof(buf), "%lluh %02llum", (unsigned long long)h, (unsigned long long)m);
    else if (m > 0) snprintf(buf, sizeof(buf), "%llum %02llus", (unsigned long long)m, (unsigned long long)s);
    else snprintf(buf, sizeof(buf), "%llus", (unsigned long long)s);
    return std::string(buf);
}

// ── Server browser sort modes ────────────────────────────────────────
enum class ServerSortMode
{
    NameAz,
    NameZa,
    PingLow,
    PingHigh,
    PlayersMost,
    PlayersLeast,
    UptimeLong,
    UptimeShort
};

static ServerSortMode serverSortModeFromBinding(const std::string& s)
{
    if (s == "Name Z-A") return ServerSortMode::NameZa;
    if (s == "Ping (low)") return ServerSortMode::PingLow;
    if (s == "Ping (high)") return ServerSortMode::PingHigh;
    if (s == "Players (most)") return ServerSortMode::PlayersMost;
    if (s == "Players (least)") return ServerSortMode::PlayersLeast;
    if (s == "Uptime (long)") return ServerSortMode::UptimeLong;
    if (s == "Uptime (short)") return ServerSortMode::UptimeShort;
    return ServerSortMode::NameAz;
}

static std::string lowercaseName(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        c = (char)std::tolower((unsigned char)c);
    return out;
}

// Sort a copy of the browser list by the selected mode. Unreachable servers
// (ping "-") always sink below reachable ones for ping-based sorts.
static void sortServerEntries(std::vector<MimitaNet::ServerBrowserEntry>& entries,
                              ServerSortMode mode)
{
    auto byName = [](const MimitaNet::ServerBrowserEntry& a,
                     const MimitaNet::ServerBrowserEntry& b) {
        int cmp = lowercaseName(a.serverName).compare(lowercaseName(b.serverName));
        if (cmp != 0) return cmp < 0;
        return a.code < b.code;
    };
    auto byPing = [](const MimitaNet::ServerBrowserEntry& a,
                     const MimitaNet::ServerBrowserEntry& b) {
        if (a.ping.reachable != b.ping.reachable)
            return a.ping.reachable;
        if (!a.ping.reachable) return false;
        return a.ping.pingMs < b.ping.pingMs;
    };

    switch (mode)
    {
        case ServerSortMode::NameZa:
            std::sort(entries.begin(), entries.end(),
                [&](const MimitaNet::ServerBrowserEntry& a, const MimitaNet::ServerBrowserEntry& b) {
                    return byName(b, a);
                });
            break;
        case ServerSortMode::PingLow:
            std::sort(entries.begin(), entries.end(), byPing);
            break;
        case ServerSortMode::PingHigh:
            std::sort(entries.begin(), entries.end(),
                [&](const MimitaNet::ServerBrowserEntry& a, const MimitaNet::ServerBrowserEntry& b) {
                    return byPing(b, a);
                });
            break;
        case ServerSortMode::PlayersMost:
            std::sort(entries.begin(), entries.end(),
                [](const MimitaNet::ServerBrowserEntry& a, const MimitaNet::ServerBrowserEntry& b) {
                    return a.players > b.players;
                });
            break;
        case ServerSortMode::PlayersLeast:
            std::sort(entries.begin(), entries.end(),
                [](const MimitaNet::ServerBrowserEntry& a, const MimitaNet::ServerBrowserEntry& b) {
                    return a.players < b.players;
                });
            break;
        case ServerSortMode::UptimeLong:
            std::sort(entries.begin(), entries.end(),
                [](const MimitaNet::ServerBrowserEntry& a, const MimitaNet::ServerBrowserEntry& b) {
                    return a.uptimeSeconds > b.uptimeSeconds;
                });
            break;
        case ServerSortMode::UptimeShort:
            std::sort(entries.begin(), entries.end(),
                [](const MimitaNet::ServerBrowserEntry& a, const MimitaNet::ServerBrowserEntry& b) {
                    return a.uptimeSeconds < b.uptimeSeconds;
                });
            break;
        case ServerSortMode::NameAz:
        default:
            std::sort(entries.begin(), entries.end(), byName);
            break;
    }
}

// Draw the PUBLIC SERVERS browser: fixed header + one scrollable row per server.
static void drawServerBrowser(GLFWwindow* win, OnlineMenuResult& r, const GuiLayout& layout,
                              const std::vector<MimitaNet::ServerBrowserEntry>& entries)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    UIRect listRect = {40.0f, 720.0f, 1840.0f, 310.0f};
    const GuiElement* listElem = layout.get("publicServersList");
    if (listElem)
        listRect = {listElem->x, listElem->y, listElem->w, listElem->h};
    if (listRect.w <= 0.0f || listRect.h <= 0.0f)
        return;

    const glm::vec4 headerText = {0.7f, 0.8f, 0.95f, 1.0f};
    const glm::vec4 dimText = {0.6f, 0.65f, 0.75f, 1.0f};
    const glm::vec4 pingGood = {0.3f, 1.0f, 0.4f, 1.0f};
    const glm::vec4 pingBad = {0.5f, 0.5f, 0.55f, 1.0f};
    const float textScale = 0.28f;
    const float rowH = 44.0f;

    const float xName = listRect.x + 10.0f;
    const float xCode = listRect.x + 400.0f;
    const float xPing = listRect.x + 540.0f;
    const float xHost = listRect.x + 640.0f;
    const float xPlayers = listRect.x + 990.0f;
    const float xUptime = listRect.x + 1110.0f;
    const float joinW = 150.0f;
    const float joinX = listRect.x + listRect.w - joinW - 10.0f;

    // Fixed column header above the scroll area
    {
        float hy = listRect.y + 2.0f;
        uiDrawText("SERVER", cs.designToScreenX(xName), cs.designToScreenY(hy), textScale, headerText);
        uiDrawText("ID", cs.designToScreenX(xCode), cs.designToScreenY(hy), textScale, headerText);
        uiDrawText("PING", cs.designToScreenX(xPing), cs.designToScreenY(hy), textScale, headerText);
        uiDrawText("HOST", cs.designToScreenX(xHost), cs.designToScreenY(hy), textScale, headerText);
        uiDrawText("PLAYERS", cs.designToScreenX(xPlayers), cs.designToScreenY(hy), textScale, headerText);
        uiDrawText("UPTIME", cs.designToScreenX(xUptime), cs.designToScreenY(hy), textScale, headerText);
    }

    // Scrollable rows
    UIRect scrollArea = {listRect.x, listRect.y + 26.0f, listRect.w, listRect.h - 26.0f};
    float contentHeight = std::max(listRect.h - 26.0f, (float)entries.size() * rowH + 8.0f);
    uiBeginScrollArea(win, scrollArea, contentHeight, gServerListScroll);

    const float contentTop = listRect.y + 30.0f;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const MimitaNet::ServerBrowserEntry& e = entries[i];
        float y = contentTop + (float)i * rowH;

        glm::vec4 rowBg = (i % 2) == 0
            ? glm::vec4(0.06f, 0.07f, 0.09f, 0.6f)
            : glm::vec4(0.05f, 0.055f, 0.075f, 0.6f);
        uiDrawRect(cs.designToScreen({listRect.x + 4.0f, y, listRect.w - 8.0f, rowH - 4.0f}),
                   rowBg, "server-row");

        float ty = y + 10.0f;
        uiDrawText(e.serverName.c_str(), cs.designToScreenX(xName), cs.designToScreenY(ty), textScale, dimText);
        uiDrawText(e.code.c_str(), cs.designToScreenX(xCode), cs.designToScreenY(ty), textScale, dimText);

        char pingBuf[32];
        if (e.ping.reachable) snprintf(pingBuf, sizeof(pingBuf), "%ums", e.ping.pingMs);
        else snprintf(pingBuf, sizeof(pingBuf), "-");
        uiDrawText(pingBuf, cs.designToScreenX(xPing), cs.designToScreenY(ty), textScale,
                   e.ping.reachable ? pingGood : pingBad);

        uiDrawText(e.hostPlayerName.empty() ? "-" : e.hostPlayerName.c_str(),
                   cs.designToScreenX(xHost), cs.designToScreenY(ty), textScale, dimText);

        char playersBuf[64];
        snprintf(playersBuf, sizeof(playersBuf), "%d/%d", e.players, e.maxPlayers);
        uiDrawText(playersBuf, cs.designToScreenX(xPlayers), cs.designToScreenY(ty), textScale, dimText);

        uiDrawText(formatUptime(e.uptimeSeconds).c_str(), cs.designToScreenX(xUptime),
                   cs.designToScreenY(ty), textScale, dimText);

        std::string btnId = "serverbrowser_join_" + e.code;
        UIButtonState bs = uiButton(win, "JOIN",
            {joinX, y + 3.0f, joinW, rowH - 10.0f},
            {0.15f, 0.55f, 0.25f, 1.0f}, btnId.c_str());
        if (bs.clicked)
        {
            printf("[SERVER BROWSER] join code=%s private=%d\n", e.code.c_str(), (int)e.passwordProtected);
            r.roomCode = e.code;
            r.connectToServer = true;
            r.passwordProtected = e.passwordProtected;
            r.serverName = e.serverName;
        }
    }

    uiEndScrollArea(scrollArea, contentHeight, gServerListScroll);
}

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
static void buildDropdownItems(const GuiElement* elem, std::vector<std::string>& items)
{
    items.clear();
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

    // Server browser: start once, refresh when the menu opens, tick each frame.
    if (!browserWsaStarted)
    {
        MimitaNet::serverBrowserInit();
        browserWsaStarted = true;
    }
    if (menuActive && !browserWasActive)
        MimitaNet::serverBrowserRequestRefresh();
    browserWasActive = menuActive;
    MimitaNet::serverBrowserTick();

    ensureMapListScanned();

    b.set("server.code", serverCodeDisplay);
    b.set("server.status", serverRunning ? "Running" : "Stopped");
    b.set("server.running", serverRunning ? "true" : "false");
    b.set("server.not_running", serverRunning ? "false" : "true");
    b.set("server.name_placeholder", "MiMITA Server");
    b.set("server.map_placeholder", "funworld3");
    b.set("server.player_limit_placeholder", "999");
    b.set("server.map_items", mapItemsCache);
    MimitaNet::CommunityServerConfig& communityConfig = MimitaNet::CommunityServerConfig::instance();
    if (communityConfig.modes().empty()) communityConfig.load();
    communityConfig.pollReload();
    b.set("server.mode_items", communityConfig.modeItems());
    b.set("server.weapon_set_items", communityConfig.weaponSetItems());
    // Log current map selection
    {
        static std::string lastMapSelection;
        std::string currentMap = b.get("server.map");
        if (currentMap != lastMapSelection && !currentMap.empty()) {
            lastMapSelection = currentMap;
            printf("[COMMUNITY MAP SELECT] mapId=%s\n", currentMap.c_str());
        }
    }
    // Initialize one-time defaults (only set if binding has no value yet)
    static bool defaultsInitialized = false;
    if (!defaultsInitialized)
    {
        if (b.get("server.startup_npcs").empty())
            b.set("server.startup_npcs", "true");

        if (b.get("server.startup_npc_count").empty())
            b.set("server.startup_npc_count", "1");
        if (b.get("server.mode").empty())
            b.set("server.mode", "Sandbox");
        if (b.get("server.weapon_set").empty())
            b.set("server.weapon_set", "Set 1: Stable weapons");
        if (b.get("server.weapon_set_description").empty())
            b.set("server.weapon_set_description", communityConfig.weaponSetDescription(1));
        if (b.get("server.auto_map_rotation").empty())
            b.set("server.auto_map_rotation", "true");
        if (b.get("server.map_rotation_minutes").empty())
            b.set("server.map_rotation_minutes", "15");
        if (b.get("server.discord_notification").empty())
            b.set("server.discord_notification", "true");

        if (b.get("join.code_placeholder").empty())
            b.set("join.code_placeholder", "______");

        if (b.get("server.sort").empty())
            b.set("server.sort", "Name A-Z");

        defaultsInitialized = true;

        printf(
            "[ONLINE MENU] defaults initialized startupNpcs=%s npcCount=%s\n",
            b.get("server.startup_npcs").c_str(),
            b.get("server.startup_npc_count").c_str());
    }

    static std::string lastWeaponSet;
    const std::string selectedWeaponSet = b.get("server.weapon_set");
    if (selectedWeaponSet != lastWeaponSet && !selectedWeaponSet.empty()) {
        lastWeaponSet = selectedWeaponSet;
        int setId = selectedWeaponSet.rfind("Set ", 0) == 0
            ? std::max(1, std::atoi(selectedWeaponSet.c_str() + 4)) : 1;
        b.set("server.weapon_set_description", communityConfig.weaponSetDescription(setId));
    }

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/community-menu.json");

    // Derive dropdown modal state from actual open dropdowns (blocks Phase 1 clicks)
    UISys::gDropdownModalActive = false;
    for (auto& kv : getDropdownStates()) {
        if (kv.second.open && !kv.second.openThisFrame) {
            UISys::gDropdownModalActive = true;
            break;
        }
    }

    // Phase 1: draw all elements sorted by layer
    static thread_local std::vector<std::pair<int, std::string>> sorted;
    sorted.clear();
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
            std::string code = serverCodeDisplay;
            if (!code.empty())
            {
                printf("[ONLINE MENU] connectToThisServer code=%s\n", code.c_str());
                r.roomCode = code;
                r.connectToServer = true;
            }
        }
        else if (id == "joinServerButton" && s.clicked)
        {
            std::string code = b.get("join.code");
            if (code.find("______") != std::string::npos)
                code.clear();

            if (!code.empty())
            {
                printf("[ROOM JOIN] looking up code=%s\n", code.c_str());
                MimitaNet::CoordinatorLookupResult lookup =
                    MimitaNet::coordinatorIceLookup(code);
                if (lookup.reachable && lookup.exists)
                {
                    r.roomCode = code;
                    r.connectToServer = true;
                    r.passwordProtected = lookup.passwordProtected;
                    r.serverName = lookup.serverName;
                    printf("[ONLINE MENU] found room code=%s private=%d\n", code.c_str(), (int)lookup.passwordProtected);
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

    // Populate the PUBLIC SERVERS panel: clear fallback text when rows exist,
    // sort by the selected mode, then draw the server browser rows over the panel.
    {
        static thread_local std::vector<MimitaNet::ServerBrowserEntry> entries;
        MimitaNet::serverBrowserEntries(entries);
        sortServerEntries(entries, serverSortModeFromBinding(b.get("server.sort")));
        b.set("server.list", entries.empty() ? "No public servers found. Start one above!" : " ");
        drawServerBrowser(win, r, layout, entries);
    }

    // Enter-on-submit check: if Enter was pressed in join code input, trigger join
    if (gSubmittedBinding == "join.code")
    {
        gSubmittedBinding.clear();
        std::string code = b.get("join.code");
        if (code.find("______") != std::string::npos)
            code.clear();
        if (!code.empty())
        {
            printf("[ROOM JOIN] enter-submit code=%s\n", code.c_str());
            MimitaNet::CoordinatorLookupResult lookup =
                MimitaNet::coordinatorIceLookup(code);
            if (lookup.reachable && lookup.exists)
            {
                r.roomCode = code;
                r.connectToServer = true;
                r.passwordProtected = lookup.passwordProtected;
                r.serverName = lookup.serverName;
                printf("[ONLINE MENU] enter found room code=%s private=%d\n", code.c_str(), (int)lookup.passwordProtected);
            }
            else if (!lookup.reachable)
                b.set("join.code", "Coordinator unreachable");
            else
                b.set("join.code", "Room not found");
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

        static thread_local std::vector<std::string> items;
        buildDropdownItems(elem, items);
        if (items.empty()) continue;

        int sel = drawDropdownOverlay(win, ds, elem->x, elem->y, elem->w, elem->h, items);
        if (sel >= 0 && !elem->binding.empty() && sel < (int)items.size()) {
            b.set(elem->binding, items[sel]);
        }
    }

    return r;
}
