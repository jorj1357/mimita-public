// 08 21 2026, 12 00
/* purpose
* Orchestrates menu rendering, menu transitions, and menu-specific 3D previews.
* Draws the avatar creator preview through the shared preview camera owner.
* Preserves menu UI behavior while the avatar preview uses free look.
* DOES NOT own avatar data, preview camera configuration, or terminal commands.
* DOES NOT update gameplay camera movement.
* DOES NOT persist avatar-creator free-look state.
*/

#include "gui-main.h"
#include "menus/main-menu.h"
#include "menus/menu-avatar-preview.h"
#include "menus/play-menu.h"
#include "menus/online-menu.h"
#include "menus/practice-menu.h"
#include "menus/settings-menu.h"
#include "menus/debug-menu.h"
#include "menus/duel-config-menu.h"
#include "menus/server-info-menu.h"
#include "menus/sign-in-menu.h"
#include "menus/login-menu.h"
#include "password-popup.h"
#include "auth/auth-system.h"
#include "auth/auth-controller.h"
#include "auth/auth-popup.h"
#include "duel/duel-queue.h"
#include "gamemode/gamemode-map-pool.h"
#include "gamemode/gamemode.h"
#include "menus/sandbox-map-menu.h"
#include "menus/help-menu.h"
#include "network/community-server-config.h"
#include "game/bomb-tag-config.h"
#include "map/map-catalog.h"
#include "avatar/avatar-editor.h"
#include "avatar/avatar.h"
#include "entities/player.h"
#include "competitive/competitive.h"
#include "competitive/competitive-ui.h"
#include "competitive/competitive-match.h"
#include "leaderboard/leaderboard-ui.h"
#include "ui-system.h"
#include "gui-editor.h"
#include "notifications/notifications.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "render/render-player.h"
#include "render/lighting-config.h"
#include "gui-layout.h"
#include "audio/music-manager.h"
#include "analytics/analytics-manager.h"
#include "input/input-commands.h"
#include "replay/replay-factory.h"
#include "replay/replay.h"
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"
#include "renderer/renderer.h"
#include "network/server.h"
#include "network/server-browser.h"
#include "network/coordinator-client.h"
#include "gui-bindings.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <random>
#include <glad/glad.h>
#include <shellapi.h>
#include <windows.h>

GuiMenuState gGuiMenuState = GUI_MENU_AUTH;

extern Renderer* gRenderer;

// Set up lighting uniforms for player preview rendering
static void setupPlayerPreviewLighting()
{
    if (!gRenderer) return;
    GLuint shader = gRenderer->shaderProgram;
    glUseProgram(shader);
    auto ul = [&](const char* name) { return glGetUniformLocation(shader, name); };
    glUniform1i(ul("uUseColor"), 0);
    glUniform1i(ul("uTex"), 0);
    glUniform1i(ul("uDebugView"), 0);
    glUniform1i(ul("uShadowsEnabled"), 0);
    // Brighter preview lighting (separate from world config)
    glUniform3f(ul("uLightDir"), -0.35f, -0.1f, -1.0f);
    glUniform1f(ul("uAmbientStrength"), 0.65f);
    glUniform1f(ul("uDiffuseStrength"), 0.75f);
    glUniform1f(ul("uEdgeDarkness"), 0.05f);
    glUniform1f(ul("uEdgeWidth"), 0.8f);
    glUniform1f(ul("uAODarkness"), 0.03f);
    glUniform1f(ul("uAOContrast"), 0.6f);
    glUniform1f(ul("uTextureContrast"), 1.10f);
    glUniform1f(ul("uTextureBrightness"), 1.40f);
    glUniform3f(ul("uTint"), 1.0f, 1.0f, 1.0f);
}

static const char* layoutFileForMenu(GuiMenuState state)
{
    switch (state) {
        case GUI_MENU_MAIN:         return "config/gui/main-menu.json";
        case GUI_MENU_PLAY:         return "config/gui/play-menu.json";
        case GUI_MENU_PRACTICE:     return "config/gui/practice-menu.json";
        case GUI_MENU_SANDBOX_MAPS: return "config/gui/sandbox-map-menu.json";
        case GUI_MENU_SETTINGS:     return "config/gui/settings-menu.json";
        case GUI_MENU_SERVERS:      return "config/gui/community-menu.json";
        case GUI_MENU_DUEL_CONFIG:  return "config/gui/duel-config-menu.json";
        case GUI_MENU_SERVER_INFO:  return "config/gui/server-info-menu.json";
        case GUI_MENU_SIGN_IN:      return "config/gui/sign-in-menu.json";
        case GUI_MENU_HELP:         return "config/gui/help-menu.json";
        case GUI_MENU_REPLAY:       return "config/gui/replay-menu.json";
        case GUI_MENU_AVATAR_CREATOR: return "config/gui/avatar-creator.json";
        case GUI_MENU_BOMB_TAG_CONFIG: return "config/gui/bomb-tag-config.json";
        case GUI_MENU_COMPETITIVE:      return "config/gui/competitive-menu.json";
        case GUI_MENU_COMPETITIVE_RESULT: return "config/gui/competitive-result.json";
        case GUI_MENU_AUTH:            return "config/gui/main-menu.json";
        case GUI_MENU_LOGIN:           return "config/gui/login-menu.json";
    }
    return "config/gui/main-menu.json";
}

static DuelConfigResult gPendingDuelConfig{};
BombTagConfigResult gPendingBombTagConfig{};
BombTagConfigResult getPendingBombTagConfig() { return gPendingBombTagConfig; }
void clearPendingBombTagConfig() { gPendingBombTagConfig = BombTagConfigResult{}; }
static bool gServerRunning = false;
static MimitaNet::ListenServerState gListenServer;
static MimitaNet::ServerLaunchSettings gServerLaunchSettings;
static char gServerAddress[64] = "127.0.0.1:1357";
static MultiplayerConnectInfo gPendingConnect{};
static SandboxMapSelection gPendingSandboxMap{};
static PROCESS_INFORMATION gServerProcessInfo{};
static bool gServerProcessLaunched = false;
static uint64_t gServerProcessLaunchMs = 0;
static std::string gPendingServerRoomFilePath;
static uint64_t gPendingServerRoomFileStartMs = 0;
static uint64_t gLastServerRoomFilePollMs = 0;
static bool gWaitingForServerRoomCode = false;
static std::string gPendingLocalJoinFallbackRoomCode;
static uint64_t gPendingLocalJoinFallbackStartMs = 0;

// Resolve the online menu's map selection to the actual map file stem.
// The dropdown stores display names (e.g. "Mimita Memorial"); the engine,
// server process, and connecting clients all need the real file stem
// (e.g. "mimita-memorial"). An empty/unmatched selection falls back to the
// dropdown's displayed default (the first map in the catalog).
static std::string resolveServerMapId(const std::string& selection)
{
    MapCatalogResult catalog = scanMapCatalog();

    auto lower = [](std::string s) {
        for (char& c : s)
            c = (char)std::tolower((unsigned char)c);
        return s;
    };
    auto stem = [](const std::string& path) {
        std::string r = path;
        size_t slash = r.find_last_of("/\\");
        if (slash != std::string::npos)
            r = r.substr(slash + 1);
        if (r.size() > 4 && r.compare(r.size() - 4, 4, ".glb") == 0)
            r = r.substr(0, r.size() - 4);
        return r;
    };

    const std::string target = lower(selection);
    if (!target.empty())
    {
        for (const MapCatalogEntry& m : catalog.maps)
        {
            const std::string s = stem(m.assetPath);
            if (lower(m.displayName) == target || lower(s) == target ||
                lower(m.assetPath) == target)
                return s;
        }
    }

    if (!catalog.maps.empty())
        return stem(catalog.maps[0].assetPath);
    return "funworld3";
}

static void readServerSettingsFromBindings()
{
    GuiBindings& b = GuiBindings::instance();
    std::string name = b.get("server.name", "MiMITA Server");
    std::string mapName = resolveServerMapId(b.get("server.map"));
    std::string playerLimitStr = b.get("server.player_limit", "999");
    std::string npcsStr = b.get("server.startup_npcs", "true");
    std::string npcCountStr = b.get("server.startup_npc_count", "1");
    std::string privacy = b.get("server.privacy", "Public (no password)");
    std::string mode = b.get("server.mode", "Sandbox");
    std::string weaponSet = b.get("server.weapon_set", "Set 1: Stable weapons");
    std::string rotation = b.get("server.map_rotation_minutes", "15");

    gServerLaunchSettings.serverName = name;
    gServerLaunchSettings.mapName = mapName;
    gServerLaunchSettings.maxPlayers = (uint32_t)std::max(1, std::atoi(playerLimitStr.c_str()));
    gServerLaunchSettings.passwordProtected =
        privacy.find("Private") != std::string::npos;
    gServerLaunchSettings.password = b.get("server.password");
    gServerLaunchSettings.hostPlayerName = AuthSystem::instance().displayName();
    const MimitaNet::CommunityMode* selectedMode =
        MimitaNet::CommunityServerConfig::instance().modeByName(mode);
    gServerLaunchSettings.gameMode = selectedMode ? selectedMode->id : "sandbox";
    if (selectedMode && selectedMode->maxPlayers > 0)
        gServerLaunchSettings.maxPlayers = std::min<uint32_t>(
            gServerLaunchSettings.maxPlayers, (uint32_t)selectedMode->maxPlayers);
    gServerLaunchSettings.weaponSetId = weaponSet.rfind("Set ", 0) == 0
        ? std::max(1, std::atoi(weaponSet.c_str() + 4)) : 1;
    gServerLaunchSettings.autoMapRotation =
        b.get("server.auto_map_rotation", "true") == "true";
    gServerLaunchSettings.mapRotationMinutes =
        (uint32_t)std::clamp(std::atoi(rotation.c_str()), 1, 9999);
    gServerLaunchSettings.discordNotification =
        b.get("server.discord_notification", "true") == "true";
    gServerLaunchSettings.startupNpcsEnabled =
        npcsStr == "true" ||
        npcsStr == "1" ||
        npcsStr == "yes" ||
        npcsStr == "on";
    int npcCount = std::atoi(npcCountStr.c_str());
    if (npcCount < 1) npcCount = 1;
    if (npcCount > 999) npcCount = 999;
    gServerLaunchSettings.startupNpcCount = (uint32_t)npcCount;
    gServerLaunchSettings.port = MimitaNet::DEFAULT_PORT;
    gServerLaunchSettings.resolvedMapPath = "assets/maps/" + mapName + ".glb";

    printf("[COMMUNITY SERVER START] requestedMap=%s serverName=%s maxPlayers=%u npcs=%d count=%u\n",
           mapName.c_str(), gServerLaunchSettings.serverName.c_str(),
           gServerLaunchSettings.maxPlayers,
           (int)gServerLaunchSettings.startupNpcsEnabled, gServerLaunchSettings.startupNpcCount);

    printf(
        "[SERVER SETTINGS AUTHORITATIVE] "
        "startupNpcsBinding=\"%s\" enabled=%d count=%u\n",
        npcsStr.c_str(),
        (int)gServerLaunchSettings.startupNpcsEnabled,
        gServerLaunchSettings.startupNpcCount);
}

static uint16_t selectServerPort(uint16_t preferredPort)
{
    if (!MimitaNet::netStartup())
        return 0;

    auto probe = [](uint16_t requestedPort, uint16_t& selectedPort) {
        SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketHandle == INVALID_SOCKET)
            return false;

        BOOL exclusive = TRUE;
        const bool exclusiveSet = setsockopt(
            socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
            reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(requestedPort);
        bool available = exclusiveSet &&
            bind(socketHandle, reinterpret_cast<sockaddr*>(&address),
                 sizeof(address)) == 0;
        if (available && requestedPort == 0)
        {
            int addressLength = sizeof(address);
            available = getsockname(
                socketHandle, reinterpret_cast<sockaddr*>(&address),
                &addressLength) == 0;
        }
        if (available)
            selectedPort = ntohs(address.sin_port);
        closesocket(socketHandle);
        return available;
    };

    uint16_t selectedPort = 0;
    if (!probe(preferredPort, selectedPort) && !probe(0, selectedPort))
        selectedPort = 0;
    MimitaNet::netShutdown();
    return selectedPort;
}

static bool launchServerProcess(MimitaNet::ServerLaunchSettings& settings)
{
    gPendingLocalJoinFallbackRoomCode.clear();
    gPendingLocalJoinFallbackStartMs = 0;
    const uint16_t requestedPort = settings.port;
    settings.port = selectServerPort(requestedPort);
    if (settings.port == 0)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "network.gui-server.port-selection-failed", "",
            "could not select an exclusive UDP port for the new server", 0,
            {{"requested_port", requestedPort}}, __FILE__, __LINE__, __func__);
        return false;
    }

    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    // Generate a temp file path for the server to write its room code
    char tempPath[MAX_PATH];
    char tempFile[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tempPath) ||
        !GetTempFileNameA(tempPath, "mimita_room_", 0, tempFile))
    {
        printf("[SERVER LAUNCH] GetTempFileName failed error=%d\n", (int)GetLastError());
        return false;
    }
    std::string roomFilePath(tempFile);
    // Ensure file is empty at start
    FILE* clearFile = fopen(roomFilePath.c_str(), "w");
    if (clearFile) fclose(clearFile);

    std::string args = "\"" + std::string(exePath) + "\""
        + " --server"
        + " --bind 0.0.0.0:" + std::to_string(settings.port)
        + " --name \"" + settings.serverName + "\""
        + " --map \"" + settings.mapName + "\""
        + " --host-player \"" + settings.hostPlayerName + "\""
        + " --max-players " + std::to_string(settings.maxPlayers)
        + " --mode \"" + settings.gameMode + "\""
        + " --weapon-set " + std::to_string(settings.weaponSetId)
        + (settings.autoMapRotation ? "" : " --no-map-rotation")
        + " --map-rotation-minutes " + std::to_string(settings.mapRotationMinutes)
        + (settings.discordNotification ? "" : " --no-discord-notification")
        + " --password-protected " + std::string(settings.passwordProtected ? "1" : "0")
        + " --password \"" + settings.password + "\""
        + " --room-file \"" + roomFilePath + "\""
        + (settings.startupNpcsEnabled
            ? " --npcs " + std::to_string(settings.startupNpcCount)
            : " --no-npcs")
        + (settings.duelMode
            ? " --duel --gamemode \"" + settings.gamemodeId + "\""
            : "");

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    // The client and its dedicated server are one diagnostic session. Create
    // the canonical events path before spawning the child so the server cannot
    // race ahead and create its own timestamped events.jsonl.
    StructuredLogger::instance().init();
    const std::string sharedEventsPath =
        StructuredLogger::instance().eventsPath();
    if (!sharedEventsPath.empty())
    {
        std::error_code pathError;
        const std::string absoluteEventsPath =
            std::filesystem::absolute(sharedEventsPath, pathError).string();
        if (!pathError && !SetEnvironmentVariableA(
                "MIMITA_EVENTS_FILE", absoluteEventsPath.c_str()))
        {
            printf("[SERVER LAUNCH] failed to export shared events file error=%lu\n",
                   (unsigned long)GetLastError());
        }
        else
        {
            printf("[SERVER LAUNCH] shared events file: %s\n",
                   pathError ? sharedEventsPath.c_str()
                             : absoluteEventsPath.c_str());
        }
    }

    if (!CreateProcessA(nullptr, &args[0], nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &gServerProcessInfo))
    {
        printf("[SERVER LAUNCH] CreateProcess failed error=%d\n", (int)GetLastError());
        DeleteFileA(tempFile);
        return false;
    }

    printf("[SERVER LAUNCH] launched server process pid=%lu\n",
           (unsigned long)gServerProcessInfo.dwProcessId);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "network.gui-server.launch", std::to_string(gServerProcessInfo.dwProcessId),
        "launched local authoritative server on an exclusive UDP port", 0,
        {{"server_pid", gServerProcessInfo.dwProcessId},
         {"requested_port", requestedPort}, {"selected_port", settings.port},
         {"map", settings.mapName}, {"mode", settings.gameMode}},
        __FILE__, __LINE__, __func__);

    // The client created the shared events path and exported it; the child
    // inherits the parent environment (CreateProcess lpEnvironment=nullptr) and
    // must append to this exact file rather than making its own.
    {
        char eventsEnv[MAX_PATH] = {};
        const DWORD envLen = GetEnvironmentVariableA(
            "MIMITA_EVENTS_FILE", eventsEnv, MAX_PATH);
        printf("[SERVER LAUNCH] shared events file: %s\n",
               envLen > 0 ? eventsEnv : "(unset; server will create its own)");
    }

    printf("[SERVER LAUNCH ARGS] npcFlag=%s\n",
           settings.startupNpcsEnabled
               ? ("--npcs " + std::to_string(settings.startupNpcCount)).c_str()
               : "--no-npcs");
    gServerProcessLaunched = true;
    gServerProcessLaunchMs = MimitaNet::nowMs();

    // Store the pending room file path for async polling
    gPendingServerRoomFilePath = roomFilePath;
    gPendingServerRoomFileStartMs = MimitaNet::nowMs();
    gLastServerRoomFilePollMs = 0;
    gWaitingForServerRoomCode = true;

    printf("[ROOM CODE WAIT START] path=%s processId=%lu\n",
           gPendingServerRoomFilePath.c_str(),
           (unsigned long)gServerProcessInfo.dwProcessId);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "network.room-code.wait.started",
        std::to_string(gServerProcessInfo.dwProcessId),
        "waiting for dedicated server room registration", 0,
        nlohmann::json{{"server_pid", gServerProcessInfo.dwProcessId},
                       {"timeout_ms", 60000},
                       {"map", settings.mapName},
                       {"mode", settings.gameMode}},
        __FILE__, __LINE__, __func__);

    return true;
}

bool launchServerPresetAndJoin(const MimitaNet::ServerLaunchSettings& settings)
{
    if (MP_CONTEXT.active)
        return false;
    gPendingLocalJoinFallbackRoomCode.clear();
    gPendingLocalJoinFallbackStartMs = 0;

    auto queueLocalJoin = [](const MimitaNet::ServerLaunchSettings& launchSettings) {
        gPendingConnect = {};
        gPendingConnect.shouldConnect = true;
        gPendingConnect.directAddress =
            "127.0.0.1:" + std::to_string(launchSettings.port);
        gPendingConnect.mapName = launchSettings.mapName;
        gPendingConnect.serverName = launchSettings.serverName;
        THE_PLAYER.username = launchSettings.hostPlayerName.empty()
            ? AuthSystem::instance().displayName()
            : launchSettings.hostPlayerName;
        THE_PLAYER.reset();
        GAME_STATE = GAME_PLAYING;
        const std::string directAddress = gPendingConnect.directAddress;
        const bool directConnectStarted = MimitaNet::mpInit(
            MP_CONTEXT, directAddress, THE_PLAYER.username);
        if (directConnectStarted)
            gPendingConnect = {};
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network,
            directConnectStarted ? StructuredLevel::Important
                                 : StructuredLevel::Errors,
            "network.gui-server.local-join-queued", "",
            directConnectStarted
                ? "started direct local UDP join independently of room registration"
                : "direct local UDP join failed to initialize; queued retry",
            MP_CONTEXT.tick,
            {{"address", directAddress},
             {"map", launchSettings.mapName},
             {"connect_started", directConnectStarted},
             {"server_pid", gServerProcessLaunched &&
                                gServerProcessInfo.hProcess
                            ? gServerProcessInfo.dwProcessId : 0}},
            __FILE__, __LINE__, __func__);
    };

    // Join the local authoritative UDP server as soon as it is available; its
    // public coordinator room is registered independently in the background.
    if (gListenServer.active)
    {
        queueLocalJoin(gServerLaunchSettings);
        return true;
    }

    if (gServerProcessLaunched && gServerProcessInfo.hProcess)
    {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(gServerProcessInfo.hProcess, &exitCode) &&
            exitCode == STILL_ACTIVE)
        {
            const bool samePreset =
                gServerLaunchSettings.mapName == settings.mapName &&
                gServerLaunchSettings.gameMode == settings.gameMode &&
                gServerLaunchSettings.serverName == settings.serverName &&
                gServerLaunchSettings.port == settings.port;
            if (!samePreset)
                return false;
            queueLocalJoin(gServerLaunchSettings);
            return true;
        }

        CloseHandle(gServerProcessInfo.hProcess);
        CloseHandle(gServerProcessInfo.hThread);
        gServerProcessInfo = {};
        gServerProcessLaunched = false;
        gServerLaunchSettings.serverCode.clear();
    }

    gServerLaunchSettings = settings;
    if (!launchServerProcess(gServerLaunchSettings))
    {
        // Keep the GUI's listen-server fallback and connect through its local
        // authoritative socket.
        if (!MimitaNet::startListenServer(
                gListenServer, gServerLaunchSettings.port, "", "",
                &gServerLaunchSettings))
            return false;
        queueLocalJoin(gServerLaunchSettings);
        return true;
    }

    gServerLaunchSettings.externalProcessLaunched = true;
    // Start the local client immediately. The room-file handshake only updates
    // the online listing; it is not a prerequisite for local gameplay.
    queueLocalJoin(gServerLaunchSettings);
    return true;
}

static void stopServerProcess()
{
    if (gServerProcessLaunched &&
        gServerProcessInfo.hProcess != nullptr)
    {
        TerminateProcess(gServerProcessInfo.hProcess, 0);
        CloseHandle(gServerProcessInfo.hProcess);
        CloseHandle(gServerProcessInfo.hThread);
        gServerProcessInfo = {};
        gServerProcessLaunched = false;
        printf("[SERVER LAUNCH] server process terminated\n");
    }

    // Clean up any pending room file state
    if (!gPendingServerRoomFilePath.empty())
    {
        DeleteFileA(gPendingServerRoomFilePath.c_str());
        printf("[ROOM CODE CLEANUP] deleted path=%s\n", gPendingServerRoomFilePath.c_str());
    }
    gPendingServerRoomFilePath.clear();
    gWaitingForServerRoomCode = false;
    gLastServerRoomFilePollMs = 0;
    gPendingServerRoomFileStartMs = 0;
    gPendingLocalJoinFallbackRoomCode.clear();
    gPendingLocalJoinFallbackStartMs = 0;
    gServerLaunchSettings.serverCode.clear();
}

// ── Duel host server helpers (duels queue module) ─────────────────────

bool launchDuelHostServer(const std::string& mapName)
{
    if (gServerProcessLaunched || gListenServer.active)
    {
        Debug::warn(Debug::Category::Duel,
            "[DUEL HOST] already running a server; not launching a duel server\n");
        return false;
    }
    if (gServerProcessInfo.hProcess != nullptr)
    {
        TerminateProcess(gServerProcessInfo.hProcess, 0);
        CloseHandle(gServerProcessInfo.hProcess);
        CloseHandle(gServerProcessInfo.hThread);
        gServerProcessInfo = {};
    }

    MimitaNet::ServerLaunchSettings s;
    s.serverName = "Duel";
    s.mapName = mapName;
    s.maxPlayers = 2;
    s.startupNpcsEnabled = true;
    s.startupNpcCount = 1;
    s.hostPlayerName = AuthSystem::instance().displayName();
    // Random high port so multiple duel servers can run on one machine (you
    // can test with several mimita.exe instances at once). Joining is via ICE
    // room code, so the port is transport metadata only.
    {
        static std::mt19937 rng((unsigned)MimitaNet::nowMs());
        std::uniform_int_distribution<int> portDist(20000, 60000);
        s.port = (uint16_t)portDist(rng);
    }
    s.duelMode = true;
    s.gamemodeId = "duel";
    gServerLaunchSettings = s;
    return launchServerProcess(s);
}

bool pollDuelServerRoomCode(std::string& outCode)
{
    if (!gServerProcessLaunched || !gWaitingForServerRoomCode ||
        gPendingServerRoomFilePath.empty())
        return false;

    FILE* f = fopen(gPendingServerRoomFilePath.c_str(), "r");
    if (!f) return false;

    char line[256] = {};
    std::string content;
    if (fgets(line, sizeof(line), f))
    {
        content = line;
        while (!content.empty() &&
               (content.back() == '\n' || content.back() == '\r'))
            content.pop_back();
    }
    fclose(f);

    if (content.empty()) return false;

    Debug::log(Debug::Category::Duel, "[DUEL HOST] room code ready: %s\n", content.c_str());
    gServerLaunchSettings.serverCode = content;
    DeleteFileA(gPendingServerRoomFilePath.c_str());
    gPendingServerRoomFilePath.clear();
    gWaitingForServerRoomCode = false;
    gLastServerRoomFilePollMs = 0;
    outCode = content;
    return true;
}

void stopExternalServerProcess()
{
    stopServerProcess();
}

DuelConfigResult getPendingDuelConfig() { return gPendingDuelConfig; }
void clearPendingDuelConfig() { gPendingDuelConfig = DuelConfigResult{}; }

MultiplayerConnectInfo getPendingMultiplayerConnect() { return gPendingConnect; }
void setPendingMultiplayerConnect(const MultiplayerConnectInfo& info) { gPendingConnect = info; }
void clearPendingMultiplayerConnect() { gPendingConnect = {}; }
SandboxMapSelection getPendingSandboxMapSelection() { return gPendingSandboxMap; }
void clearPendingSandboxMapSelection() { gPendingSandboxMap = {}; }
void reportSandboxMapLoadResult(const std::string& message, bool success)
{
    sandboxMapMenuSetLoadResult(message, success);
}

ExternalServerProcessStatus getExternalServerProcessStatus()
{
    ExternalServerProcessStatus status;

    if (!gServerProcessLaunched)
        return status;

    status.launched = true;
    status.processId = (uint32_t)gServerProcessInfo.dwProcessId;
    status.port = gServerLaunchSettings.port;
    status.roomCode = gServerLaunchSettings.serverCode;
    status.serverName = gServerLaunchSettings.serverName;
    status.mapName = gServerLaunchSettings.mapName;
    status.uptimeMs = MimitaNet::nowMs() - gServerProcessLaunchMs;

    if (!gServerProcessInfo.hProcess)
        return status;

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(gServerProcessInfo.hProcess, &exitCode))
        return status;

    if (exitCode == STILL_ACTIVE)
    {
        status.running = true;
    }
    else
    {
        static uint64_t lastExitLogMs = 0;
        uint64_t now = MimitaNet::nowMs();
        if (now - lastExitLogMs >= 5000)
        {
            printf("[SERVER PROCESS STATUS] exited pid=%lu exitCode=%lu\n",
                   (unsigned long)gServerProcessInfo.dwProcessId, (unsigned long)exitCode);
            lastExitLogMs = now;
        }
        status.running = false;
    }

    return status;
}

MimitaNet::ListenServerState* getListenServerState()
{
    return &gListenServer;
}

static bool pollPendingServerRoomCode()
{
    if (!gWaitingForServerRoomCode)
        return false;

    if (!gServerProcessLaunched)
    {
        printf("[ROOM CODE WAIT CANCEL] reason=server-process-not-running path=%s\n",
               gPendingServerRoomFilePath.c_str());
        if (!gPendingServerRoomFilePath.empty())
            DeleteFileA(gPendingServerRoomFilePath.c_str());
        gPendingServerRoomFilePath.clear();
        gWaitingForServerRoomCode = false;
        return false;
    }

    {
        DWORD exitCode = 0;
        if (gServerProcessInfo.hProcess &&
            GetExitCodeProcess(gServerProcessInfo.hProcess, &exitCode) &&
            exitCode != STILL_ACTIVE)
        {
            printf("[ROOM CODE WAIT FAILED] reason=server-exited exitCode=%lu path=%s\n",
                   exitCode, gPendingServerRoomFilePath.c_str());
            onlineMenuSetServerCode("Server failed to create room");
            if (!gPendingServerRoomFilePath.empty())
                DeleteFileA(gPendingServerRoomFilePath.c_str());
            gPendingServerRoomFilePath.clear();
            gWaitingForServerRoomCode = false;
            gLastServerRoomFilePollMs = 0;
            return false;
        }
    }

    const uint64_t now = MimitaNet::nowMs();

    if (gLastServerRoomFilePollMs != 0 &&
        now - gLastServerRoomFilePollMs < 100)
        return false;

    gLastServerRoomFilePollMs = now;

    FILE* f = fopen(gPendingServerRoomFilePath.c_str(), "r");
    if (f)
    {
        char line[256] = {};
        std::string content;

        if (fgets(line, sizeof(line), f))
        {
            content = line;
            while (!content.empty() &&
                   (content.back() == '\n' || content.back() == '\r'))
                content.pop_back();
        }

        fclose(f);

        if (!content.empty())
        {
            printf("[ROOM CODE SYNC] path=%s code=%s previous=%s\n",
                   gPendingServerRoomFilePath.c_str(),
                   content.c_str(),
                   gServerLaunchSettings.serverCode.c_str());

            gServerLaunchSettings.serverCode = content;
            onlineMenuSetServerCode(content);
            if (MP_CONTEXT.currentRoomCode != content)
            {
                MP_CONTEXT.currentRoomCode = content;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Network, StructuredLevel::Important,
                    "network.room-code.hud-source-updated", content,
                    "newly registered local server code was applied to the gameplay HUD",
                    MP_CONTEXT.tick,
                    {{"room_code", content},
                     {"source", "gui-server-room-file"}},
                    __FILE__, __LINE__, __func__);
            }

            printf("[ROOM DISPLAY] source=headless-server code=%s\n",
                   content.c_str());

            const uint64_t waitElapsedMs = now - gPendingServerRoomFileStartMs;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "network.room-code.wait.completed",
                std::to_string(gServerProcessInfo.dwProcessId),
                "dedicated server room code received", 0,
                nlohmann::json{{"server_pid", gServerProcessInfo.dwProcessId},
                               {"wait_elapsed_ms", waitElapsedMs},
                               {"timeout_ms", 60000},
                               {"map", gServerLaunchSettings.mapName},
                               {"mode", gServerLaunchSettings.gameMode}},
                __FILE__, __LINE__, __func__);

            DeleteFileA(gPendingServerRoomFilePath.c_str());
            gPendingServerRoomFilePath.clear();
            gWaitingForServerRoomCode = false;
            return true;
        }
    }

    constexpr uint64_t ROOM_CODE_TIMEOUT_MS = 60000;
    if (now - gPendingServerRoomFileStartMs >= ROOM_CODE_TIMEOUT_MS)
    {
        const uint64_t waitElapsedMs = now - gPendingServerRoomFileStartMs;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "network.room-code.wait.timed-out",
            std::to_string(gServerProcessInfo.dwProcessId),
            "dedicated server room was not registered before the wait deadline", 0,
            nlohmann::json{{"server_pid", gServerProcessInfo.dwProcessId},
                           {"wait_elapsed_ms", waitElapsedMs},
                           {"timeout_ms", ROOM_CODE_TIMEOUT_MS},
                           {"map", gServerLaunchSettings.mapName},
                           {"mode", gServerLaunchSettings.gameMode}},
            __FILE__, __LINE__, __func__);
        printf("[ROOM CODE WAIT TIMEOUT] path=%s elapsedMs=%llu\n",
               gPendingServerRoomFilePath.c_str(),
               (unsigned long long)(now - gPendingServerRoomFileStartMs));

        onlineMenuSetServerCode("Room creation timed out");

        DeleteFileA(gPendingServerRoomFilePath.c_str());
        gPendingServerRoomFilePath.clear();
        gWaitingForServerRoomCode = false;
    }
    return false;
}

void guiTickPendingServerRoomCode(GameState& state)
{
    if (pollPendingServerRoomCode())
    {
        const bool localDirectJoinPending =
            MP_CONTEXT.active && !MP_CONTEXT.connected &&
            MP_CONTEXT.serverAddress.rfind("127.0.0.1:", 0) == 0;
        if (localDirectJoinPending)
        {
            constexpr uint64_t LOCAL_JOIN_GRACE_MS = 1500;
            gPendingLocalJoinFallbackRoomCode = gServerLaunchSettings.serverCode;
            gPendingLocalJoinFallbackStartMs = MimitaNet::nowMs();
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "network.gui-server.local-join-grace-started",
                gServerLaunchSettings.serverCode,
                "room registration completed; allowing local UDP handshake retries before coordinator fallback",
                MP_CONTEXT.tick,
                {{"server_address", MP_CONTEXT.serverAddress},
                 {"room_code", gServerLaunchSettings.serverCode},
                 {"wait_elapsed_ms", MimitaNet::nowMs() - gPendingServerRoomFileStartMs},
                 {"local_join_grace_ms", LOCAL_JOIN_GRACE_MS}},
                __FILE__, __LINE__, __func__);
            return;
        }

        if (!MP_CONTEXT.active)
        {
            Debug::log(
                Debug::Category::Networking,
                "[COMMUNITY AUTO CONNECT] room=%s source=coordinator-fallback\n",
                gServerLaunchSettings.serverCode.c_str());

            gPendingConnect = {};
            gPendingConnect.shouldConnect = true;
            gPendingConnect.roomCode = gServerLaunchSettings.serverCode;

            onlineMenuSetActive(false);
            state = GAME_PLAYING;
            return;
        }
    }

    if (gPendingLocalJoinFallbackRoomCode.empty())
        return;

    if (MP_CONTEXT.connected ||
        (MP_CONTEXT.active &&
         MP_CONTEXT.serverAddress.rfind("127.0.0.1:", 0) != 0))
    {
        gPendingLocalJoinFallbackRoomCode.clear();
        gPendingLocalJoinFallbackStartMs = 0;
        return;
    }

    constexpr uint64_t LOCAL_JOIN_GRACE_MS = 1500;
    if (MimitaNet::nowMs() - gPendingLocalJoinFallbackStartMs <
        LOCAL_JOIN_GRACE_MS)
        return;

    if (MP_CONTEXT.active)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "network.gui-server.local-join-fallback",
            gPendingLocalJoinFallbackRoomCode,
            "local UDP handshake did not connect during its retry window; joining through coordinator",
            MP_CONTEXT.tick,
            {{"server_address", MP_CONTEXT.serverAddress},
             {"room_code", gPendingLocalJoinFallbackRoomCode},
             {"grace_elapsed_ms", MimitaNet::nowMs() - gPendingLocalJoinFallbackStartMs}},
            __FILE__, __LINE__, __func__);
        MimitaNet::mpShutdown(MP_CONTEXT);
    }

    if (!MP_CONTEXT.active)
    {
        gPendingConnect = {};
        gPendingConnect.shouldConnect = true;
        gPendingConnect.roomCode = gPendingLocalJoinFallbackRoomCode;
        onlineMenuSetActive(false);
        state = GAME_PLAYING;
    }
    gPendingLocalJoinFallbackRoomCode.clear();
    gPendingLocalJoinFallbackStartMs = 0;
}

void guiMain(GLFWwindow* win, GameState& state)
{
    AuthSystem& auth = AuthSystem::instance();
    AuthController& authCtrl = AuthController::instance();

    if (auth.state() == AuthState::Checking)
        auth.tickValidate();

    if (gGuiMenuState == GUI_MENU_AUTH)
    {
        bool sessionFound = (auth.state() == AuthState::Authenticated);
        bool sessionOffline = (auth.state() == AuthState::Offline);

        if (sessionFound || sessionOffline)
        {
            gGuiMenuState = GUI_MENU_MAIN;
        }
        else
        {
            if (authCtrl.runtime().state == AuthState::SignedOut ||
                authCtrl.runtime().state == AuthState::CheckingStoredSession)
            {
                authCtrl.checkStoredSession();
            }
            if (authCtrl.runtime().state == AuthState::SignedIn)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            else
            {
                gGuiMenuState = GUI_MENU_LOGIN;
                loginMenuSetActive(true);
            }
        }
    }

    GuiLayoutManager::instance().pollReload();
    uiBeginFrame(win, "menu", true);
    GuiEditor::instance().setActiveLayout(layoutFileForMenu(gGuiMenuState));

    switch (gGuiMenuState)
    {
        case GUI_MENU_AUTH:
        case GUI_MENU_MAIN:
        {
            MainMenuResult r = drawMainMenu(win);

            if (r.goPlay)
            {
                // Play now opens the community server browser/create flow.
                // Duel queue remains disabled until it is intentionally restored.
                onlineMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SERVERS;
            }
            else if (r.goSettings)
            {
                AnalyticsManager::instance().trackUi("settings_opened");
                gGuiMenuState = GUI_MENU_SETTINGS;
            }
            else if (r.goReplays)
            {
                printf("[MAIN MENU] switching to replay menu\n");
                AnalyticsManager::instance().trackUi("replay_viewed");
                gGuiMenuState = GUI_MENU_REPLAY;
            }
            else if (r.goAvatarCreator)
            {
                printf("[MAIN MENU] switching to avatar creator\n");
                AnalyticsManager::instance().trackUi("outfit_editor_opened");
                gGuiMenuState = GUI_MENU_AVATAR_CREATOR;
            }
            else if (r.goExit)
                glfwSetWindowShouldClose(win, GLFW_TRUE);
            else if (r.switchAccount)
            {
                printf("[MAIN MENU] switching account\n");
                auth.clearSession();
                authPopupReset();
                ShellExecuteA(nullptr, "open",
                    "https://mimita.fun/clientsignin",
                    nullptr, nullptr, SW_SHOWNORMAL);
            }
            else if (r.logOut)
            {
                printf("[MAIN MENU] logging out\n");
                auth.logout();
            }
            else if (r.enterSignInCode)
            {
                printf("[MAIN MENU] Enter Sign-In Code clicked\n");
                authPopupStartCodeInput();
            }
            else if (r.goLogin)
            {
                printf("[MAIN MENU] switching to login screen\n");
                loginMenuSetActive(true);
                gGuiMenuState = GUI_MENU_LOGIN;
            }
            else if (r.goHelp)
            {
                gGuiMenuState = GUI_MENU_HELP;
            }

            break;
        }

        case GUI_MENU_PLAY:
        {
            PlayMenuResult r = drawPlayMenu(win);

            if (r.goCompetitive)
            {
                LoadCompetitiveProfile("default");
                RefreshCompetitiveProfileFromApi();
                printf("[COMP] Opening competitive menu, MMR=%d\n",
                       GetCompetitiveProfile().mmr);
                gGuiMenuState = GUI_MENU_COMPETITIVE;
            }
            else if (r.goCompetitiveSignIn)
            {
                gGuiMenuState = GUI_MENU_SIGN_IN;
            }
            else if (r.goDuels)
            {
                gGuiMenuState = GUI_MENU_DUEL_CONFIG;
            }
            else if (r.goQueueDuels)
            {
                // Enter the duels queue: sandbox practice + matchmaking.
                DuelQueue::instance().startQueue(
                    DuelQueue::defaultProfileId(),
                    AuthSystem::instance().displayName(),
                    "", GamemodeMapPool::instance().list());
                extern GameState* gpGameState;
                if (gpGameState) *gpGameState = GAME_PLAYING;
            }
            else if (r.goBombTag)
            {
                gGuiMenuState = GUI_MENU_BOMB_TAG_CONFIG;
            }
            else if (r.goOnline)
            {
                onlineMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SERVERS;
            }
            else if (r.goPractice)
            {
                gGuiMenuState = GUI_MENU_PRACTICE;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_COMPETITIVE:
        {
            // Refresh MMR animation each frame
            updateMmrAnimation(0.016f);

            auto action = drawCompetitiveMenu(win);
            if (action == CompetitiveMenuAction::FindMatch)
            {
                // Mark as competitive match before starting
                setCompetitiveMatchActive(true, 5000);
                // Start a competitive duel vs NPCs (first to 5 kills)
                DuelConfig cfg;
                cfg.numNpcs = 1;
                cfg.killsToWin = 5;
                cfg.duelLengthSeconds = 300;
                cfg.npcDifficulty = 5.0f;
                cfg.npcNames = {"Competitive Bot"};
                cfg.enabled = true;
                gPendingDuelConfig.startDuel = true;
                gPendingDuelConfig.numNpcs = cfg.numNpcs;
                gPendingDuelConfig.killsToWin = cfg.killsToWin;
                gPendingDuelConfig.duelLengthSeconds = cfg.duelLengthSeconds;
                gPendingDuelConfig.npcDifficulty = cfg.npcDifficulty;

                extern GameState* gpGameState;
                if (gpGameState) *gpGameState = GAME_PLAYING;
            }
            else if (action == CompetitiveMenuAction::GoBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_LEADERBOARD:
        {
            if (drawLeaderboardMenu(win) == LeaderboardMenuAction::GoBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_COMPETITIVE_RESULT:
        {
            updateMmrAnimation(0.016f);
            auto action = drawCompetitiveResultScreen(win);
            if (action == CompetitiveResultAction::PlayAgain)
            {
                DuelConfig cfg;
                cfg.numNpcs = 1;
                cfg.killsToWin = 5;
                cfg.duelLengthSeconds = 300;
                cfg.npcDifficulty = 5.0f;
                cfg.npcNames = {"Competitive Bot"};
                cfg.enabled = true;
                gPendingDuelConfig.startDuel = true;
                gPendingDuelConfig.numNpcs = cfg.numNpcs;
                gPendingDuelConfig.killsToWin = cfg.killsToWin;
                gPendingDuelConfig.duelLengthSeconds = cfg.duelLengthSeconds;
                gPendingDuelConfig.npcDifficulty = cfg.npcDifficulty;

                extern GameState* gpGameState;
                if (gpGameState) *gpGameState = GAME_PLAYING;
                gGuiMenuState = GUI_MENU_MAIN;
            }
            else if (action == CompetitiveResultAction::ExitToMenu)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_PRACTICE:
        {
            PracticeMenuResult r = drawPracticeMenu(win);

            if (r.goSandbox)
            {
                sandboxMapMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SANDBOX_MAPS;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_SANDBOX_MAPS:
        {
            SandboxMapMenuResult r = drawSandboxMapMenu(win);
            if (r.startSandbox)
            {
                gPendingSandboxMap.shouldStart = true;
                gPendingSandboxMap.mapPath = r.mapPath;
                sandboxMapMenuSetActive(false);
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                sandboxMapMenuSetActive(false);
                gGuiMenuState = GUI_MENU_PRACTICE;
            }
            break;
        }

        case GUI_MENU_SETTINGS:
        {
            SettingsMenuResult r = drawSettingsMenu(win);

            if (r.signOut)
            {
                AuthSystem::instance().logout();
                gGuiMenuState = GUI_MENU_AUTH;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_SERVERS:
        {
            // Sync display with server state (external process or listen server)
            bool serverActive = gListenServer.active || gServerProcessLaunched;
            onlineMenuSetServerRunning(serverActive);
            if (gListenServer.active)
                onlineMenuSetServerCode(gListenServer.serverCode);
            else if (gServerProcessLaunched && !gServerLaunchSettings.serverCode.empty())
                onlineMenuSetServerCode(gServerLaunchSettings.serverCode);

            // Keep the browser aware of the locally hosted room so it pings
            // the host's own server via 127.0.0.1 instead of the external IP.
            {
                const std::string ownCode = gListenServer.active
                    ? gListenServer.serverCode
                    : gServerLaunchSettings.serverCode;
                MimitaNet::serverBrowserSetOwnRoomCode(
                    serverActive && !ownCode.empty() ? ownCode : std::string());
            }

            OnlineMenuResult r = drawOnlineMenu(win);
            if (r.startServer)
            {
                onlineMenuSetServerCode("");
                if (!gListenServer.active && !gServerProcessLaunched)
                {
                    // Read settings from UI bindings before starting
                    readServerSettingsFromBindings();

                    // Coordinator is the single source of truth — clear so we wait for it
                    gServerLaunchSettings.serverCode.clear();
                    onlineMenuSetServerCode("Creating room...");

                    // Launch server as separate process with a visible CMD window
                    const bool processLaunched = launchServerProcess(gServerLaunchSettings);

                    if (processLaunched)
                    {
                        gServerLaunchSettings.externalProcessLaunched = true;
                        printf("[ONLINE MENU] External server process launched port=%u code=%s map=%s\n",
                               gServerLaunchSettings.port, gServerLaunchSettings.serverCode.c_str(),
                               gServerLaunchSettings.mapName.c_str());
                        printf("[ONLINE MENU] joining local server now; coordinator room registration continues in background\n");
                        gPendingConnect = {};
                        gPendingConnect.shouldConnect = true;
                        gPendingConnect.directAddress =
                            "127.0.0.1:" + std::to_string(gServerLaunchSettings.port);
                        gPendingConnect.mapName = gServerLaunchSettings.mapName;
                        gPendingConnect.serverName = gServerLaunchSettings.serverName;
                        onlineMenuSetActive(false);
                        state = GAME_PLAYING;
                    }
                    else
                    {
                        // Fallback: listen server only (in-process)
                        if (MimitaNet::startListenServer(gListenServer, gServerLaunchSettings.port,
                            "", "", &gServerLaunchSettings))
                        {
                            printf("[ONLINE MENU] Listen server started (fallback) port=%u code=%s map=%s\n",
                                   gServerLaunchSettings.port, gListenServer.serverCode.c_str(),
                                   gServerLaunchSettings.mapName.c_str());

                            Debug::log(
                                Debug::Category::Networking,
                                "[COMMUNITY AUTO CONNECT] room=%s source=listen-server\n",
                                gListenServer.serverCode.c_str());

                            gPendingConnect = {};
                            gPendingConnect.shouldConnect = true;
                            gPendingConnect.directAddress =
                                "127.0.0.1:" + std::to_string(gServerLaunchSettings.port);
                            gPendingConnect.mapName = gServerLaunchSettings.mapName;
                            gPendingConnect.serverName = gServerLaunchSettings.serverName;

                            onlineMenuSetActive(false);
                            state = GAME_PLAYING;
                        }
                    }
                }
            }
            else if (r.stopServer)
            {
                if (gListenServer.active || gServerProcessLaunched)
                {
                    if (MP_CONTEXT.active)
                        MimitaNet::mpShutdown(MP_CONTEXT);

                    stopServerProcess();
                    if (gListenServer.active)
                        MimitaNet::stopListenServer(gListenServer);
                    onlineMenuSetServerRunning(false);
                    onlineMenuSetServerCode("");
                }
            }
            else if (r.connectToServer)
                {
                    if (r.roomCode.empty())
                    {
                        Debug::warn(
                            Debug::Category::Networking,
                            "[ROOM JOIN FAILED] reason=empty-room-code\n");

                        onlineMenuSetServerCode("Enter a room code");
                    }
                    else if (r.passwordProtected)
                    {
                        PasswordPopup::open(r.roomCode, r.serverName);
                    }
                    else
                    {
                        gPendingConnect = {};
                        gPendingConnect.shouldConnect = true;
                        DWORD ownServerExitCode = 0;
                        const bool ownExternalServerAlive =
                            gServerProcessLaunched && gServerProcessInfo.hProcess &&
                            GetExitCodeProcess(gServerProcessInfo.hProcess,
                                               &ownServerExitCode) &&
                            ownServerExitCode == STILL_ACTIVE;
                        const bool joiningOwnExternalServer =
                            ownExternalServerAlive &&
                            r.roomCode == gServerLaunchSettings.serverCode &&
                            !gServerLaunchSettings.passwordProtected;
                        const bool joiningOwnListenServer =
                            gListenServer.active &&
                            r.roomCode == gListenServer.serverCode &&
                            !gListenServer.passwordProtected;
                        if (joiningOwnExternalServer || joiningOwnListenServer)
                        {
                            const uint16_t localPort = joiningOwnExternalServer
                                ? gServerLaunchSettings.port : gListenServer.port;
                            const std::string& localMap = joiningOwnExternalServer
                                ? gServerLaunchSettings.mapName : gListenServer.mapName;
                            const std::string& localName = joiningOwnExternalServer
                                ? gServerLaunchSettings.serverName : gListenServer.serverName;
                            gPendingConnect.directAddress =
                                "127.0.0.1:" + std::to_string(localPort);
                            gPendingConnect.mapName = localMap;
                            gPendingConnect.serverName = localName;
                        }
                        else
                            gPendingConnect.roomCode = r.roomCode;

                        Debug::log(
                            Debug::Category::Networking,
                            "[ROOM JOIN REQUEST] room=%s transport=%s\n",
                            r.roomCode.c_str(),
                            gPendingConnect.directAddress.empty()
                                ? "room-code" : gPendingConnect.directAddress.c_str());

                        onlineMenuSetActive(false);

                        // Temporary until there is a dedicated GAME_CONNECTING state.
                        state = GAME_PLAYING;     
                        }
                }
            else if (r.goBack)
            {
                onlineMenuSetActive(false);
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_DUEL_CONFIG:
        {
            DuelConfigResult r = drawDuelConfigMenu(win);
            if (r.startDuel) {
                gPendingDuelConfig = r;
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_BOMB_TAG_CONFIG:
        {
            BombTagConfigResult r = drawBombTagConfigMenu(win);
            if (r.start) {
                gPendingBombTagConfig = r;
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_SERVER_INFO:
        {
            ServerInfoResult r = drawServerInfoMenu(win, gServerAddress, gServerRunning);
            if (r.startServer)
            {
                if (!gListenServer.active)
                {
                    MimitaNet::startListenServer(gListenServer, MimitaNet::DEFAULT_PORT);
                    gServerRunning = gListenServer.active;
                }
            }
            if (r.connect)
            {
                serverInfoMenuSetActive(false);
                gPendingConnect.shouldConnect = true;
                gPendingConnect.roomCode = gListenServer.active ? gListenServer.serverCode : onlineMenuGetServerCode();
                state = GAME_PLAYING;
            }
            else if (r.goBack)
                gGuiMenuState = GUI_MENU_SERVERS;
            break;
        }

        case GUI_MENU_SIGN_IN:
        {
            SignInMenuResult r = drawSignInMenu(win);
            if (r.signedIn || r.goBack)
            {
                signInMenuSetActive(false);
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_LOGIN:
        {
            LoginMenuResult r = drawLoginMenu(win);
            if (r.signedIn)
            {
                loginMenuSetActive(false);
                gGuiMenuState = GUI_MENU_MAIN;
            }
            else if (r.goBack)
            {
                loginMenuSetActive(false);
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_HELP:
        {
            HelpMenuResult r = drawHelpMenu(win);
            if (r.goBack)
                gGuiMenuState = GUI_MENU_MAIN;
            break;
        }

        case GUI_MENU_REPLAY:
        {
            ReplayBrowser& browser = REPLAY_BROWSER;
            browser.setOpen(true);
            browser.draw();

            // Back button using layout
            GuiLayout& rl = GuiLayoutManager::instance().getLayout("config/gui/replay-menu.json");
            const GuiElement* bb = rl.get("backButton");
            if (bb && uiButton(win, "BACK", {bb->x, bb->y, bb->w, bb->h}, {0.7f, 0.2f, 0.2f, 1.0f}).clicked)
            {
                browser.setOpen(false);
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_AVATAR_CREATOR:
        {
            // ── 3D Avatar Preview (center panel viewport) ──────────
            // Uses the MenuAvatarPreview player so the character is always
            // centered, animated and fully visible regardless of gameplay.
            MenuAvatarPreview& av = MenuAvatarPreview::instance();
            av.pollHotReload();
            av.update(1.0f / 60.0f, glm::vec3(0.0f, 1.0f, 0.0f));
            av.updateAvatarFreecamInput(win, 1.0f / 60.0f,
                                        !Terminal::instance().isOpen());

            Player* previewPlayer = av.ensurePlayer();
            if (previewPlayer)
            {
                setupPlayerPreviewLighting();

                GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
                const GuiElement* prevPanel = layout.get("panelPreview");
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(win, &fbW, &fbH);
                float scaleX = (float)fbW / 1920.0f;
                float scaleY = (float)fbH / 1080.0f;

                float vpX = prevPanel ? prevPanel->x * scaleX : 900.0f * scaleX;
                float vpY = prevPanel ? (1080.0f - prevPanel->y - prevPanel->h) * scaleY : 50.0f * scaleY;
                float vpW = prevPanel ? prevPanel->w * scaleX : 600.0f * scaleX;
                float vpH = prevPanel ? prevPanel->h * scaleY : 900.0f * scaleY;

                // A transient zero-sized framebuffer can occur while the window
                // is being resized. Keep projection and OpenGL viewport inputs valid.
                vpW = std::max(1.0f, vpW);
                vpH = std::max(1.0f, vpH);

                GLint prevViewport[4];
                glGetIntegerv(GL_VIEWPORT, prevViewport);
                GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);

                glViewport((int)vpX, (int)vpY, (int)vpW, (int)vpH);
                glEnable(GL_DEPTH_TEST);

                int prevRW = gRenderer->width;
                int prevRH = gRenderer->height;
                gRenderer->width = (int)vpW;
                gRenderer->height = (int)vpH;

                glClearColor(0.035f, 0.040f, 0.055f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                const auto& cfg = av.config();

                // Position + rotate the preview character (idle animation runs
                // in av.update above).
                glm::vec3 basePos = cfg.characterPosition + cfg.modelOffset;
                float yaw = cfg.characterRotationDeg.z;
                if (cfg.rotationAxis == "z") yaw += av.rotationAngle();
                if (cfg.lookAtCamera) yaw = 180.0f + (cfg.rotationAxis == "z" ? av.rotationAngle() : 0.0f);
                previewPlayer->pos = basePos;
                previewPlayer->yaw = yaw;
                previewPlayer->meshScale = cfg.characterScale;

                Camera previewCam;
                av.setupCamera(previewCam, previewPlayer->pos, (int)vpW, (int)vpH, true);

                renderPlayer(*previewPlayer, previewCam);

                gRenderer->width = prevRW;
                gRenderer->height = prevRH;
                if (!depthWasEnabled) glDisable(GL_DEPTH_TEST);
                glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            }

            AvatarSystem::instance().autosaveUpdate(1.0f / 60.0f);  // ~60fps dt approximation
            AvatarEditorResult r = drawAvatarEditor(win);
            if (r.goBack) {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            if (r.goApply) {
                extern Player* gpPlayer;
                if (gpPlayer) {
                    AvatarSystem::instance().applyToPlayer(*gpPlayer, true);
                    Terminal::instance().addLog("[AVATAR] Applied to player");
                }
            }
            if (r.goSave) {
                extern Player* gpPlayer;
                if (gpPlayer) {
                    AvatarSystem::instance().applyToPlayer(*gpPlayer, true);
                    Terminal::instance().addLog("[AVATAR] Saved and applied to player");
                }
            }
            break;
        }
    }

    // Handle password popup result: when user submits password, start the connect
    if (!PasswordPopup::isOpen() && !PasswordPopup::getPassword().empty())
    {
        gPendingConnect = {};
        gPendingConnect.shouldConnect = true;
        gPendingConnect.roomCode = PasswordPopup::getRoomCode();
        gPendingConnect.serverName = PasswordPopup::getServerName();
        gPendingConnect.passwordProtected = true;
        gPendingConnect.password = PasswordPopup::getPassword();
        PasswordPopup::clearResult();
        onlineMenuSetActive(false);
        state = GAME_PLAYING;
        printf("[PASSWORD POPUP] connect room=%s\n", gPendingConnect.roomCode.c_str());
    }

    // Draw code login dialog if active (from Enter Sign-In Code button)
    drawAuthCodeDialog(win);

    // Draw password popup on top of everything
    if (PasswordPopup::isOpen())
        PasswordPopup::render(win);

    MusicManager::instance().drawAllOverlay();
    NotificationSystem::instance().render(false);
    InputCommandSystem::instance().drawInputDebug();
    AnalyticsManager::instance().drawFirstLaunchPopup(win);
    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();

    GuiEditor::instance().update(win);
}
