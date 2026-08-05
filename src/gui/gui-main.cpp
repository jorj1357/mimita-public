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
#include "auth/auth-system.h"
#include "auth/auth-controller.h"
#include "auth/auth-popup.h"
#include "menus/sandbox-map-menu.h"
#include "menus/help-menu.h"
#include "game/bomb-tag-config.h"
#include "avatar/avatar-menu.h"
#include "avatar/avatar.h"
#include "competitive/competitive.h"
#include "competitive/competitive-ui.h"
#include "competitive/competitive-match.h"
#include "ui-system.h"
#include "gui-editor.h"
#include "notifications/notifications.h"
#include "camera.h"
#include "debug/debug-log.h"
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

static void readServerSettingsFromBindings()
{
    GuiBindings& b = GuiBindings::instance();
    std::string name = b.get("server.name", "MiMITA Server");
    std::string mapName = b.get("server.map", "funworld3");
    std::string playerLimitStr = b.get("server.player_limit", "999");
    std::string npcsStr = b.get("server.startup_npcs", "true");
    std::string npcCountStr = b.get("server.startup_npc_count", "3");
    std::string privacy = b.get("server.privacy", "Public (no password)");

    gServerLaunchSettings.serverName = name;
    gServerLaunchSettings.mapName = mapName;
    gServerLaunchSettings.maxPlayers = (uint32_t)std::max(1, std::atoi(playerLimitStr.c_str()));
    gServerLaunchSettings.passwordProtected =
        privacy.find("Private") != std::string::npos;
    gServerLaunchSettings.password = b.get("server.password");
    gServerLaunchSettings.hostPlayerName = AuthSystem::instance().displayName();
    gServerLaunchSettings.startupNpcsEnabled =
        npcsStr == "true" ||
        npcsStr == "1" ||
        npcsStr == "yes" ||
        npcsStr == "on";
    gServerLaunchSettings.startupNpcCount = (uint32_t)std::max(0, std::atoi(npcCountStr.c_str()));
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

static bool launchServerProcess(const MimitaNet::ServerLaunchSettings& settings)
{
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
        + " --password-protected " + std::string(settings.passwordProtected ? "1" : "0")
        + " --room-file \"" + roomFilePath + "\""
        + (settings.startupNpcsEnabled
            ? " --npcs " + std::to_string(settings.startupNpcCount)
            : " --no-npcs");

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    if (!CreateProcessA(nullptr, &args[0], nullptr, nullptr, FALSE,
                        CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &gServerProcessInfo))
    {
        printf("[SERVER LAUNCH] CreateProcess failed error=%d\n", (int)GetLastError());
        DeleteFileA(tempFile);
        return false;
    }

    printf("[SERVER LAUNCH] launched server process pid=%lu\n",
           (unsigned long)gServerProcessInfo.dwProcessId);

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
    gServerLaunchSettings.serverCode.clear();
}

DuelConfigResult getPendingDuelConfig() { return gPendingDuelConfig; }
void clearPendingDuelConfig() { gPendingDuelConfig = DuelConfigResult{}; }

MultiplayerConnectInfo getPendingMultiplayerConnect() { return gPendingConnect; }
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

            printf("[ROOM DISPLAY] source=headless-server code=%s\n",
                   content.c_str());

            DeleteFileA(gPendingServerRoomFilePath.c_str());
            gPendingServerRoomFilePath.clear();
            gWaitingForServerRoomCode = false;
            return true;
        }
    }

    constexpr uint64_t ROOM_CODE_TIMEOUT_MS = 15000;
    if (now - gPendingServerRoomFileStartMs >= ROOM_CODE_TIMEOUT_MS)
    {
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

void guiMain(GLFWwindow* win, GameState& state)
{
    AuthSystem& auth = AuthSystem::instance();
    AuthController& authCtrl = AuthController::instance();

    if (auth.state() == AuthState::Checking)
        auth.tickValidate();

    // Poll for room code from external server process (non-blocking).
    // Returns true when the room code file was successfully read.
    // 7 22 2026 1230 no localhost 
    if (pollPendingServerRoomCode())
    {
        Debug::log(
            Debug::Category::Networking,
            "[COMMUNITY AUTO CONNECT] room=%s\n",
            gServerLaunchSettings.serverCode.c_str());

        gPendingConnect = {};
        gPendingConnect.shouldConnect = true;
        gPendingConnect.roomCode = gServerLaunchSettings.serverCode;

        onlineMenuSetActive(false);
        state = GAME_PLAYING;
        return;
    }

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
    uiBeginFrame(win, "menu");
    GuiEditor::instance().setActiveLayout(layoutFileForMenu(gGuiMenuState));

    // ── 3D Avatar Preview for Main Menu ──────────────────────────
    if (gGuiMenuState == GUI_MENU_AUTH || gGuiMenuState == GUI_MENU_MAIN)
    {
        MenuAvatarPreview& av = MenuAvatarPreview::instance();
        av.pollHotReload();

        extern Player* gpPlayer;
        if (gpPlayer && gpPlayer->modelLoaded)
        {
            glClear(GL_DEPTH_BUFFER_BIT);
            setupPlayerPreviewLighting();

            GuiLayout& mmLayout = GuiLayoutManager::instance().getLayout("config/gui/main-menu.json");
            const GuiElement* coverEl = mmLayout.get("coverImage");
            const GuiElement* exitEl = mmLayout.get("exitButton");
            int fbW = 0, fbH = 0;
            glfwGetFramebufferSize(win, &fbW, &fbH);
            float scaleX = (float)fbW / 1920.0f;

            float panelLeft = exitEl ? (exitEl->x + exitEl->w + 40.0f) : 650.0f;
            float panelRight = coverEl ? (coverEl->x + coverEl->w) * 0.85f : 1400.0f;

            int prevPX = (int)(panelLeft * scaleX);
            int prevPW = (int)((panelRight - panelLeft) * scaleX);

            glEnable(GL_SCISSOR_TEST);
            glScissor(prevPX, 0, prevPW, fbH);
            glClearColor(0.035f, 0.040f, 0.055f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);

            const auto& cfg = av.config();
            Camera previewCam;
            previewCam.fov = cfg.cameraFOV;

            float yawRad = glm::radians(av.rotationAngle());
            glm::vec3 offset = cfg.cameraPosition;
            float cosA = std::cos(yawRad);
            float sinA = std::sin(yawRad);
            glm::vec3 rotated(
                offset.x * cosA - offset.y * sinA,
                offset.x * sinA + offset.y * cosA,
                offset.z
            );
            previewCam.pos = cfg.cameraTarget + rotated;
            glm::vec3 lookTarget = cfg.cameraTarget;
            previewCam.front = glm::normalize(lookTarget - previewCam.pos);
            previewCam.right = glm::normalize(glm::cross(previewCam.front, glm::vec3(0.0f, 0.0f, 1.0f)));
            previewCam.up = glm::normalize(glm::cross(previewCam.right, previewCam.front));

            renderPlayer(*gpPlayer, previewCam);
        }
    }

    switch (gGuiMenuState)
    {
        case GUI_MENU_AUTH:
        case GUI_MENU_MAIN:
        {
            MainMenuResult r = drawMainMenu(win);

            if (r.goPlay)
                gGuiMenuState = GUI_MENU_PLAY;
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
                        printf("[ONLINE MENU] Use 'Connect Localhost' or run: --client --connect 127.0.0.1:%u --name <name>\n",
                               gServerLaunchSettings.port);
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
                            gPendingConnect.roomCode = gListenServer.serverCode;

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
                    else
                    {
                        gPendingConnect = {};
                        gPendingConnect.shouldConnect = true;
                        gPendingConnect.roomCode = r.roomCode;

                        Debug::log(
                            Debug::Category::Networking,
                            "[ROOM JOIN REQUEST] room=%s transport=room-code\n",
                            r.roomCode.c_str());

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
            // ── 3D Avatar Preview (right panel viewport) ──────────
            MenuAvatarPreview& av = MenuAvatarPreview::instance();
            av.pollHotReload();

            extern Player* gpPlayer;
            if (gpPlayer)
            {
                setupPlayerPreviewLighting();

                GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/avatar-creator.json");
                const GuiElement* rightPanel = layout.get("panelPreview3D");
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(win, &fbW, &fbH);
                float scaleX = (float)fbW / 1920.0f;
                float scaleY = (float)fbH / 1080.0f;

                float vpX = rightPanel ? rightPanel->x * scaleX : 900.0f * scaleX;
                float vpY = rightPanel ? (1080.0f - rightPanel->y - rightPanel->h) * scaleY : 50.0f * scaleY;
                float vpW = rightPanel ? rightPanel->w * scaleX : 1000.0f * scaleX;
                float vpH = rightPanel ? rightPanel->h * scaleY : 950.0f * scaleY;

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
                Camera previewCam;
                previewCam.fov = cfg.cameraFOV;

                float yawRad = glm::radians(av.rotationAngle());
                glm::vec3 offset = cfg.cameraPosition;
                float cosA = std::cos(yawRad);
                float sinA = std::sin(yawRad);
                glm::vec3 rotated(
                    offset.x * cosA - offset.y * sinA,
                    offset.x * sinA + offset.y * cosA,
                    offset.z
                );
                previewCam.pos = cfg.cameraTarget + rotated;
                glm::vec3 lookTarget = cfg.cameraTarget;
                previewCam.front = glm::normalize(lookTarget - previewCam.pos);
                previewCam.right = glm::normalize(glm::cross(previewCam.front, glm::vec3(0.0f, 0.0f, 1.0f)));
                previewCam.up = glm::normalize(glm::cross(previewCam.right, previewCam.front));

                renderPlayer(*gpPlayer, previewCam);

                gRenderer->width = prevRW;
                gRenderer->height = prevRH;
                if (!depthWasEnabled) glDisable(GL_DEPTH_TEST);
                glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            }

            AvatarSystem::instance().autosaveUpdate(1.0f / 60.0f);  // ~60fps dt approximation
            AvatarMenuResult r = drawAvatarMenu(win);
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

    // Draw code login dialog if active (from Enter Sign-In Code button)
    drawAuthCodeDialog(win);

    MusicManager::instance().drawAllOverlay();
    NotificationSystem::instance().render(false);
    InputCommandSystem::instance().drawInputDebug();
    AnalyticsManager::instance().drawFirstLaunchPopup(win);
    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();

    GuiEditor::instance().update(win);
}
