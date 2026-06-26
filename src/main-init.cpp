#include "main-init.h"
#include "main-systems.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <memory>
#include "engine/engine.h"
#include "world/world.h"
#include "world/world-loader.h"
#include "world/world-gltf-loader.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "camera.h"
#include "input/input-state.h"
#include "input/input-poll.h"
#include "input/input-frame.h"
#include "input/input-commands.h"
#include "render/render-world.h"
#include "render/post-fx.h"
#include "render/render-player.h"
#include "physics/physics-mini.h"
#include "physics/physics-debug-movement.h"
#include "physics/ray-utils.h"
#include "physics/movement/physics-collision.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "audio/music-manager.h"
#include "analytics/analytics-manager.h"
#include "gui/gui-main.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-editor.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/font-stuff/font-loader.h"
#include "game/game-state.h"
#include "game/game-cli.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/transform-debug.h"
#include "debug/transform-debug-commands.h"
#include "debug/debug-diag.h"
#include "debug/log-manager.h"
#include "debug/crash-handler.h"
#include "utils/pak-file.h"
#include "network/net_mode.h"
#include "network/multiplayer-context.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/dev-teleport.h"
#include "devtools/dev-commands.h"
#include "devtools/terminal.h"
#include "devtools/account-config.h"
#include "devtools/npc-spawn-commands.h"
#include "devtools/dev-log-commands.h"
#include "devtools/dev-overlay-commands.h"
#include "ui/hitmarker.h"
#include "gui/hud/chat-bubble.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "shadow/shadow-commands.h"
#include "video/video-settings.h"
#include "video/outro.h"
#include "video/frame-pacer.h"
#include "video/video-commands.h"
#include "sim/sim-context.h"
#include "engine/engine-tick.h"
#include "combat/weapon-hit.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "void-death/void-death.h"
#include "crosshair/crosshair-commands.h"
#include "crosshair/crosshair-config.h"
#include "crosshair/crosshair-render.h"
#include "config/player-settings.h"
#include "render/outfit-atlas.h"
#include "avatar/avatar.h"
#include "avatar/avatar-commands.h"
#include "avatar/avatar-menu.h"
#include "render/lighting-config.h"
#include "render/lighting-commands.h"
#include "render/postfx-commands.h"
#include "hot-reload/hot-reload-system.h"
#include "profile/local-profile-system.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"
#include "game/duel.h"
#include "gui/menus/duel-config-menu.h"
#include "terminal/terminal-state.h"
#include "terminal/replay-commands.h"
#include "terminal/debug-commands.h"
#include "terminal/player-commands.h"
#include "terminal/weapon-commands.h"
#include "terminal/npc-commands.h"
#include "terminal/duel-commands.h"
#include "terminal/editor-commands.h"
#include "terminal/network-commands.h"
#include "perf/perf.h"
#include "replay/replay-export.h"
#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "game/bomb-tag.h"
#include "gui/gui-editor-commands.h"
#include "camera/camera-commands.h"
#include "audio/music-commands.h"
#include <windows.h>
#include <glad/glad.h>

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern Player* gpPlayer;
extern Camera* gpCamera;
extern World* gpWorld;
extern NpcSystem* gpNpcSystem;
extern WeaponSystem* gpWeapons;
extern bool* gpFreecamEnabled;
extern glm::vec3* gpDeathPosition;
extern int* gpSelectedEditorObject;
extern bool* gpEditorMode;
extern std::string* gpActiveGameMode;
extern std::string* gpActiveMapPath;
extern bool* gpWorldLoaded;
extern ReplayRingBuffer* gpReplayRecorder;
extern ReplayPlayer* gpReplayPlayer;
extern ReplayClipSaver* gpReplayClipSaver;
extern ReplayFactory* gpReplayFactory;
extern ReplayBrowser* gpReplayBrowser;
extern ReplayTimeline* gpReplayTimeline;
extern std::unordered_map<std::string, ActorChatState>* gpReplayChatStates;
extern std::vector<std::string>* gpReplayClipsCache;
extern std::unordered_map<int, std::string>* gpCommandBinds;
extern std::unordered_map<int, bool>* gpBindPrev;
extern DuelConfig* gpDuelConfig;
extern MimitaNet::MultiplayerContext* gpMpContext;
extern GameState* gpGameState;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;
extern bool gMainmenuDebug;

extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;

extern Renderer* gRenderer;

void gameInit(int argc, char** argv, Engine& engine)
{
    printf("[MAIN] start\n");
    installCrashHandler();

    // Extract assets.pak if present (extract to app dir so relative paths work)
    {
        PakFile pak;
        if (pak.open("assets.pak")) {
            // Check if extraction marker exists (faster than checking every file)
            DWORD marker = GetFileAttributesA(".pak-extracted");
            bool extracted = (marker != INVALID_FILE_ATTRIBUTES);
            if (!extracted) {
                printf("[PAK] Extracting %d files to current directory\n", pak.numFiles());
                pak.extractAllTo(".");
                // Write marker file so subsequent launches skip extraction
                FILE* m = fopen(".pak-extracted", "w");
                if (m) { fprintf(m, "1"); fclose(m); }
                printf("[PAK] Extraction complete\n");
            }
        }
    }

    LogManager::instance().init();
    AnalyticsManager::instance().init(LocalProfileSystem::instance().currentUsername());

    printf("[LOG] Console output enabled\n");
    printf("[LOG] File logging enabled\n");
    printf("[LOG] Active log path: %s\n", LogManager::instance().path().c_str());
    printf("[LOG] Mirroring console -> file\n");

    printf("[MAIN] before engine.init\n");
    engine.init(800, 600, "mimita.exe");
    printf("[MAIN] after engine.init\n");

    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    glfwSetCharCallback(engine.window(), [](GLFWwindow*, unsigned int codepoint) {
        signInMenuHandleChar(codepoint);
        avatarMenuHandleChar(codepoint);
        serverInfoMenuHandleChar(codepoint);
        onlineMenuHandleChar(codepoint);
        Terminal::instance().handleChar(codepoint);
        GuiEditor::instance().handleChar(codepoint);
    });
    glfwSetKeyCallback(engine.window(), [](GLFWwindow*, int key, int scancode, int action, int mods) {
        (void)scancode;
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            signInMenuHandleKey(key, action);
            avatarMenuHandleKey(key, action);
            serverInfoMenuHandleKey(key, action);
            onlineMenuHandleKey(key, action);
            Terminal::instance().handleKey(key, mods);
        }
    });
    glfwSetScrollCallback(engine.window(), [](GLFWwindow*, double, double yOffset) {
        Terminal::instance().handleScroll(yOffset);
    });

    printf("[MAIN] after glfwSetInputMode\n");

    fontInit();
    printf("[MAIN] after fontInit()\n");
    uiInit(engine.window());
    printf("[MAIN] after uiInit()\n");
    DebugVis::init(engine.window());
    DebugVis::loadConfig();
    Debug::startupReport();
    printf("[MAIN] after DebugVis::init()\n");

    gameInitSubsystems(engine);
}
