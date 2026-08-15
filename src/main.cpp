// C:\important\quiet\n\mimita-priv-v7\src\main.cpp
// feb 10 2026 ultra minimal edit
// purpose:
// main.cpp should only do orchestration.
// No logic. No GLFW. No OpenGL. No physics math. No world iteration.
// Just:
// init
    // loop:
    //   poll input
    //   update audio
    //   update physics
    //   render
// shutdown
// Everything else lives behind headers.
// 6 4 2026
/**
 * like this is my fav idea
 * main.cpp calls renderWorld
 * renderWorld calls renderTextures renderLight renderBlackHoleVisuals etc
 * and THOSE files call like drawLine, visualRelativity, drawLightSpeed etc
 * AND THOOOOOSE files call like super basic boring stuff that is fine to call 1 bilion times per frame
 * and if anything fails, it prints the exact file and place and line etc 
 * with the debug log function BC WE NOT USING PRINTF DONT USE PRINTF JUTS MAKE UR OWN DEBUG LOG ok 
 */

// 6 12 2026 todo plz make main like literal 100 lines
// every file ideally is 100 lines or less, and just exposes 1 function 
//like main just calls functions from other files, and we condense etc
// bc this is so much lines arghhhhhhhh
// although idk i just want it to prefromr good and be simple ish enough  to aedit and code
// etc
// so  it might not be  big deal bc ai is so strong now  Ok Ai Andy 

#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <random>
#include <filesystem>
#include <shellapi.h>
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
#include "debug/structured-log.h"
#include "debug/transform-debug.h"
#include "debug/transform-debug-commands.h"
#include "debug/debug-diag.h"
#include "network/net_mode.h"
#include "network/udp-echo.h"
#include "network/multiplayer-context.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"

#include "devtools/dev-npc-selection.h"
#include "devtools/dev-teleport.h"
#include "devtools/dev-commands.h"
#include "devtools/terminal.h"
#include "devtools/account-config.h"
#include "devtools/npc-spawn-commands.h"
#include "ui/hitmarker.h"
#include "gui/hud/chat-bubble.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "replay/replay-camera.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "shadow/shadow-commands.h"
#include "video/video-settings.h"
#include "video/outro.h"
#include "video/frame-pacer.h"
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
#include "avatar/avatar.h"
#include "avatar/avatar-commands.h"
#include "render/lighting-config.h"
#include "hot-reload/hot-reload-system.h"
#include "profile/local-profile-system.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"

// todo sort 6 7 2026 alphabetical
#include "game/duel.h"
#include "gui/menus/duel-config-menu.h"
#include "terminal/terminal-state.h"
#include "terminal/replay-commands.h"
#include "terminal/debug-commands.h"
#include "auth/auth-system.h"
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
#include "debug/log-manager.h"
#include "camera/camera-commands.h"
#include "devtools/dev-log-commands.h"
#include "devtools/dev-overlay-commands.h"
#include "gui/gui-editor-commands.h"
#include "render/postfx-commands.h"
#include "render/lighting-commands.h"
#include "video/video-commands.h"
#include "audio/music-commands.h"
#include "main-init.h"
#include <windows.h>

// 6 9 2026 sort and be more aweosme
// duelamanger should be  a game manager, with specific modes in it
// not all in main todo 
DuelManager gDuelManager;
BombTagManager gBombTagManager;
FramePacer gFramePacer;

// Global game objects (pointers set by main() for terminal command access)
Player* gpPlayer = nullptr;
Camera* gpCamera = nullptr;
World* gpWorld = nullptr;
NpcSystem* gpNpcSystem = nullptr;
WeaponSystem* gpWeapons = nullptr;
bool* gpFreecamEnabled = nullptr;
glm::vec3* gpDeathPosition = nullptr;
int* gpSelectedEditorObject = nullptr;
bool* gpEditorMode = nullptr;
std::string* gpActiveGameMode = nullptr;
std::string* gpActiveMapPath = nullptr;
bool* gpWorldLoaded = nullptr;

ReplayRingBuffer* gpReplayRecorder = nullptr;
ReplayPlayer* gpReplayPlayer = nullptr;
std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels = nullptr;
std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels = nullptr;
ReplayFactory* gpReplayFactory = nullptr;
ReplayBrowser* gpReplayBrowser = nullptr;
ReplayTimeline* gpReplayTimeline = nullptr;
ReplayCameraMgr* gpReplayCameraMgr = nullptr;
std::unordered_map<std::string, ActorChatState>* gpReplayChatStates = nullptr;
std::vector<std::string>* gpReplayClipsCache = nullptr;

// REPLAY EXPORT UI FILTER
// Set to false if you want replay controls/debug overlays
// visible in exported videos.
bool gReplayExportRenderMode = false;
std::unordered_map<int, std::string>* gpCommandBinds = nullptr;
std::unordered_map<int, bool>* gpBindPrev = nullptr;

DuelConfig* gpDuelConfig = nullptr;
MimitaNet::MultiplayerContext* gpMpContext = nullptr;

GameState* gpGameState = nullptr;
ChatHistory* gpChatHistory = nullptr;

// mainmenu debug flag (toggle with mainmenu_debug command)
bool gMainmenuDebug = false;
// replay cinematic mode (toggle with L key during playback)
bool gReplayCinematicMode = false;
// network debug flags (toggled by net_debug_* commands)
bool gNetPresentationDebug = false;
bool gNetDebugEntities = false;
// room code HUD display toggle (toggled by roomcodeshow command)
bool gRoomCodeShow = true;

int main(int argc, char** argv)
{
    if (handleGameCLI(argc, argv)) return 0;

    printf("[BUILD] compiled on %s at %s\n", __DATE__, __TIME__);
    printf("[BUILD] commit=64b085c\n");

    LocalProfileSystem::instance().init();
    MimitaNet::LaunchOptions launchOptions = MimitaNet::parseLaunchOptions(argc, argv);
    AuthSystem::instance().init(launchOptions.sessionToken);
    if (launchOptions.name.empty())
        launchOptions.name = AuthSystem::instance().displayName();
    if (launchOptions.server && launchOptions.client)
    {
        printf("[MAIN] choose only one mode: --server or --client\n");
        MimitaNet::printLaunchUsage();
        return 1;
    }
    if (launchOptions.udpEcho)
    {
        printf("[BOOT MODE] mode=udp-echo-server graphicsInitialized=0 uiInitialized=0\n");
        return MimitaNet::runUdpEchoServer(launchOptions);
    }
    if (launchOptions.server)
    {
        printf("[BOOT MODE] mode=headless-server graphicsInitialized=0 uiInitialized=0\n");
        LogManager::instance().setLogType("Server");
        LogManager::instance().init();
        int ret = MimitaNet::runServer(launchOptions);
        LogManager::instance().shutdown();
        return ret;
    }
    if (launchOptions.client)
    {
        printf("[BOOT MODE] mode=standalone-client graphicsInitialized=0 uiInitialized=0\n");
        LogManager::instance().setLogType("Client");
        LogManager::instance().init();
        int ret = MimitaNet::runClient(launchOptions);
        LogManager::instance().shutdown();
        return ret;
    }

    printf("[BOOT MODE] mode=full-client graphicsInitialized=1 uiInitialized=1\n");
    LogManager::instance().setLogType("Gameterminal");
    LogManager::instance().init();

    Engine engine;
    gameInit(argc, argv, engine);

    // --connect <ip:port> on a normal GUI launch: boot straight into
    // gameplay and auto-join the given server via direct UDP. This is the
    // launcher path (mimita-launcher.py) for fast local playtests.
    if (launchOptions.connectExplicit)
    {
        Debug::log(Debug::Category::Networking,
                   "[BOOT MODE] auto-connect full-client to %s\n",
                   launchOptions.connect.c_str());
        GAME_STATE = GAME_PLAYING;
        MultiplayerConnectInfo info;
        info.shouldConnect = true;
        info.directAddress = launchOptions.connect;
        info.mapName = launchOptions.mapName;
        setPendingMultiplayerConnect(info);
    }

    while (engine.running())
    {
        engineTick(engine);
    }
    AnalyticsManager::instance().shutdown();
    StructuredLogger::instance().shutdown();
    HotReloadSystem::instance().unloadGameDLL();
    engine.shutdown();
    LogManager::instance().shutdown();
    
    return 0;
}

