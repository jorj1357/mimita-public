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
#include "menus/sandbox-map-menu.h"
#include "menus/help-menu.h"
#include "game/bomb-tag-config.h"
#include "avatar/avatar-menu.h"
#include "avatar/avatar.h"
#include "ui-system.h"
#include "gui-editor.h"
#include "camera.h"
#include "render/render-player.h"
#include "gui-layout.h"
#include "audio/music-manager.h"
#include "analytics/analytics-manager.h"
#include "input/input-commands.h"
#include "replay/replay-factory.h"
#include "replay/replay.h"
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"
#include <cstdio>
#include <shellapi.h>

GuiMenuState gGuiMenuState = GUI_MENU_AUTH;

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
    if (auth.state() == AuthState::Linking)
        auth.pollLinkFlow();

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

            break;
        }

        case GUI_MENU_PLAY:
        {
            PlayMenuResult r = drawPlayMenu(win);

            if (r.goDuels)
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
            // ── 3D Avatar Preview ─────────────────────────────────
            // Render the player model into the center of the screen
            // behind the menu UI for a live character preview.
            extern Player* gpPlayer;
            if (gpPlayer)
            {
                // Save & clear depth for clean preview rendering behind UI
                glClear(GL_DEPTH_BUFFER_BIT);

                // Setup orbiting preview camera
                Camera previewCam;
                static float previewAngle = 0.0f;
                previewAngle += 0.008f; // auto-rotate
                if (previewAngle > 360.0f) previewAngle -= 360.0f;

                float previewDist = 9.0f;
                float previewHeight = 3.0f;
                float rad = glm::radians(previewAngle);
                previewCam.pos = glm::vec3(
                    gpPlayer->pos.x + std::cos(rad) * previewDist,
                    gpPlayer->pos.y + std::sin(rad) * previewDist,
                    gpPlayer->pos.z + previewHeight
                );
                // Look toward player center
                previewCam.front = glm::normalize(gpPlayer->pos + glm::vec3(0, 0, 1.5f) - previewCam.pos);
                previewCam.right = glm::normalize(glm::cross(previewCam.front, glm::vec3(0, 0, 1)));
                previewCam.up = glm::normalize(glm::cross(previewCam.right, previewCam.front));

                // Render with an unobtrusive background for the preview area
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(win, &fbW, &fbH);
                // Use a scissor rect to only render in the center preview area
                // (between left panel ~300px and right panel ~1450px in design coords)
                float scaleX = (float)fbW / 1920.0f;
                int previewPX = (int)(300.0f * scaleX);
                int previewPW = (int)((1450.0f - 300.0f) * scaleX);
                glEnable(GL_SCISSOR_TEST);
                glScissor(previewPX, 0, previewPW, fbH);
                glClearColor(0.035f, 0.040f, 0.055f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);

                // Render the player model
                renderPlayer(*gpPlayer, previewCam);
            }

            printf("[AVATAR UI] Opening Avatar Creator\n");
            AvatarMenuResult r = drawAvatarMenu(win);
            if (r.goBack) {
                printf("[AVATAR UI] Closing Avatar Creator\n");
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

    MusicManager::instance().drawAllOverlay();
    InputCommandSystem::instance().drawInputDebug();
    AnalyticsManager::instance().drawFirstLaunchPopup(win);
    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();

    GuiEditor::instance().update(win);
}
