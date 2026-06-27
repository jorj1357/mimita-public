#include "gui-main.h"
#include "menus/main-menu.h"
#include "menus/play-menu.h"
#include "menus/online-menu.h"
#include "menus/practice-menu.h"
#include "menus/settings-menu.h"
#include "menus/debug-menu.h"
#include "menus/duel-config-menu.h"
#include "menus/server-info-menu.h"
#include "menus/sign-in-menu.h"
#include "auth/auth-system.h"
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
#include "camera.h"
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
#include <cstdio>
#include <glad/glad.h>
#include <shellapi.h>

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
    }
    return "config/gui/main-menu.json";
}

static DuelConfigResult gPendingDuelConfig{};
BombTagConfigResult gPendingBombTagConfig{};
BombTagConfigResult getPendingBombTagConfig() { return gPendingBombTagConfig; }
void clearPendingBombTagConfig() { gPendingBombTagConfig = BombTagConfigResult{}; }
static bool gServerRunning = false;
static char gServerAddress[64] = "127.0.0.1:1357";
static MultiplayerConnectInfo gPendingConnect{};
static SandboxMapSelection gPendingSandboxMap{};

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

void guiMain(GLFWwindow* win, GameState& state)
{
    AuthSystem& auth = AuthSystem::instance();

    if (auth.state() == AuthState::Checking)
        auth.tickValidate();

    if (gGuiMenuState == GUI_MENU_AUTH)
    {
        if (auth.state() == AuthState::Authenticated ||
            auth.state() == AuthState::NeedsLogin ||
            auth.state() == AuthState::Offline)
            gGuiMenuState = GUI_MENU_MAIN;
    }

    GuiLayoutManager::instance().pollReload();
    uiBeginFrame(win, "menu");
    GuiEditor::instance().setActiveLayout(layoutFileForMenu(gGuiMenuState));

    // ── 3D Avatar Preview for Main Menu ──────────────────────────
    if (gGuiMenuState == GUI_MENU_AUTH || gGuiMenuState == GUI_MENU_MAIN)
    {
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

            Camera previewCam;
            static float menuPreviewAngle = 0.0f;
            menuPreviewAngle += 0.005f;
            if (menuPreviewAngle > 360.0f) menuPreviewAngle -= 360.0f;

            float rad = glm::radians(menuPreviewAngle);
            previewCam.pos = glm::vec3(
                gpPlayer->pos.x + std::cos(rad) * 10.0f,
                gpPlayer->pos.y + std::sin(rad) * 10.0f,
                gpPlayer->pos.z + 3.5f);
            previewCam.front = glm::normalize(gpPlayer->pos + glm::vec3(0, 0, 1.5f) - previewCam.pos);
            previewCam.right = glm::normalize(glm::cross(previewCam.front, glm::vec3(0, 0, 1)));
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
            OnlineMenuResult r = drawOnlineMenu(win);
            if (r.connectToServer)
            {
                gPendingConnect.shouldConnect = true;
                gPendingConnect.address = r.connectAddress;
                onlineMenuSetActive(false);
                state = GAME_PLAYING;
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
                gServerRunning = true;
            if (r.connect)
            {
                serverInfoMenuSetActive(false);
                gPendingConnect.shouldConnect = true;
                gPendingConnect.address = gServerAddress;
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

                Camera previewCam;
                static float previewAngle = 0.0f;
                previewAngle += 0.008f;
                if (previewAngle > 360.0f) previewAngle -= 360.0f;

                float previewDist = 5.5f;
                float previewHeight = 2.0f;
                float rad = glm::radians(previewAngle);
                previewCam.pos = glm::vec3(
                    gpPlayer->pos.x + std::cos(rad) * previewDist,
                    gpPlayer->pos.y + std::sin(rad) * previewDist,
                    gpPlayer->pos.z + previewHeight
                );
                previewCam.front = glm::normalize(gpPlayer->pos + glm::vec3(0, 0, 1.5f) - previewCam.pos);
                previewCam.right = glm::normalize(glm::cross(previewCam.front, glm::vec3(0, 0, 1)));
                previewCam.up = glm::normalize(glm::cross(previewCam.right, previewCam.front));

                renderPlayer(*gpPlayer, previewCam);

                gRenderer->width = prevRW;
                gRenderer->height = prevRH;
                if (!depthWasEnabled) glDisable(GL_DEPTH_TEST);
                glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            }

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
    InputCommandSystem::instance().drawInputDebug();
    AnalyticsManager::instance().drawFirstLaunchPopup(win);
    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();

    GuiEditor::instance().update(win);
}
