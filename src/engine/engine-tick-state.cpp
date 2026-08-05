#include "engine/engine-tick-state.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <random>
#include <GLFW/glfw3.h>
#include "world/world-gltf-loader.h"
#include "audio/music-manager.h"
#include "gui/gui-main.h"
#include "gui/hud/chat-window.h"
#include "gui/menus/duel-config-menu.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "game/spawn-utils.h"
#include "game/spawn-override.h"
#include "network/multiplayer-context.h"
#include "network/net_mode.h"
#include "network/server.h"
#include "input/mouse-lock.h"
#include "profile/local-profile-system.h"
#include "auth/auth-system.h"
#include "devtools/terminal.h"
#include "devtools/dev-overlay.h"
#include "devtools/dev-npc-selection.h"
#include "replay/replay.h"
#include "debug/debug-log.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;

void engineTickState(Engine& engine, float dt)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    bool& editorMode = EDITOR_MODE;
    std::string& activeGameMode = ACTIVE_GAME_MODE;
    std::string& activeMapPath = ACTIVE_MAP_PATH;
    int& selectedEditorObject = SELECTED_EDITOR_OBJ;
    bool& freecamEnabled = FREECAM_ENABLED;
    glm::vec3& deathPosition = DEATH_POSITION;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& replayWeaponModels = REPLAY_WEAPON_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& G_COMMAND_BINDS = CMD_BINDS;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& gReplayClipSaver = REPLAY_CLIP_SAVER;
    auto& gReplayFactory = REPLAY_FACTORY;
    auto& gReplayBrowser = REPLAY_BROWSER;
    auto& gReplayTimeline = REPLAY_TIMELINE;

    static GameState prevState = GAME_MENU;
    static bool npcsSpawned = false;
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, 0);
    static const std::string defaultMapPath =
        "assets/maps/mimita-aabb-only-interior-small-v4.glb";

    if (gameState != prevState)
    {
        printf("[MAIN] gameState changed %d -> %d\n", (int)prevState, (int)gameState);
        if (gameState == GAME_PLAYING) {
            MusicManager::instance().enterGameMode();
        } else {
            MusicManager::instance().enterMenuMode();
        }
        if (gameState == GAME_PLAYING)
        {
            SandboxMapSelection sandboxSelection =
                getPendingSandboxMapSelection();
            if (sandboxSelection.shouldStart)
            {
                const std::string selectedPath = sandboxSelection.mapPath;
                clearPendingSandboxMapSelection();
                printf("[SANDBOX MAP] selected path=%s\n", selectedPath.c_str());

                if (!loadWorldFromGLB(world, selectedPath.c_str()))
                {
                    const std::string message =
                        "Failed to load: " + selectedPath;
                    printf("[SANDBOX MAP ERROR] %s\n", message.c_str());
                    reportSandboxMapLoadResult(message, false);
                    gameState = GAME_MENU;
                }
                else
                {
                    activeMapPath = selectedPath;
                    worldLoaded = true;
                    npcSystem.destroyAll();
                    npcsSpawned = false;
                    player.reset();
                    player.username = AuthSystem::instance().displayName();

                    glm::vec3 overridePos;
                    if (tryGetSpawnOverride(overridePos)) {
                        player.pos = overridePos;
                        player.respawnPosition = overridePos;
                        Debug::log(Debug::Category::General, "[SANDBOX SPAWN] override active at (%.1f %.1f %.1f)\n",
                                   overridePos.x, overridePos.y, overridePos.z);
                    } else if (!world.spawnPoints.empty())
                    {
                        std::uniform_int_distribution<size_t> dist(0, world.spawnPoints.size() - 1);
                        world.selectedSpawnIndex = (int)dist(rng);
                        const SpawnPoint& spawn = world.spawnPoints[world.selectedSpawnIndex];
                        player.pos = spawn.position;
                        player.respawnPosition = spawn.position;
                        printf("[SANDBOX SPAWN] selected index=%d name=%s world=(%.3f %.3f %.3f)\n",
                               world.selectedSpawnIndex, spawn.tag.c_str(), spawn.position.x,
                               spawn.position.y, spawn.position.z);
                    }
                    else
                    {
                        world.selectedSpawnIndex = -1;
                        const glm::vec3 fallback = FALLBACK_SPAWN_POS;
                        player.pos = fallback;
                        player.respawnPosition = fallback;
                        printf("[SANDBOX SPAWN WARNING] no GLB spawns; fallback=(%.1f %.1f %.1f)\n",
                               fallback.x, fallback.y, fallback.z);
                    }

                    reportSandboxMapLoadResult(
                        "Loaded: " + selectedPath, true);
                    printf("[SANDBOX MAP] load success path=%s spawns=%zu\n",
                           activeMapPath.c_str(), world.spawnPoints.size());

                    // Start a local-only server for sandbox mode
                    MimitaNet::ServerLaunchSettings s;
                    s.startLocalServer = true;
                    s.mapName = activeMapPath;
                    s.startupNpcsEnabled = true;
                    s.startupNpcCount = 3;
                    MimitaNet::ListenServerState* ls = getListenServerState();
                    if (ls && !ls->active) {
                        if (MimitaNet::startListenServer(*ls, MimitaNet::DEFAULT_PORT, "", "", &s)) {
                            printf("[SANDBOX] local server started code=%s\n", ls->serverCode.c_str());
                            // Connect client to local server
                            if (MimitaNet::mpInit(mpContext, "127.0.0.1:" + std::to_string(MimitaNet::DEFAULT_PORT),
                                                  AuthSystem::instance().displayName())) {
                                printf("[SANDBOX] connected to local server\n");
                            } else {
                                printf("[SANDBOX] WARNING: failed to connect to local server\n");
                            }
                        } else {
                            printf("[SANDBOX] WARNING: failed to start local server\n");
                        }
                    }
                }
            }

            if (gameState == GAME_PLAYING && !worldLoaded && !mpContext.active)
            {
                printf("[MAIN] PLAY requested without sandbox selection; loading default world\n");
                if (loadWorldFromGLB(world, defaultMapPath.c_str()))
                {
                    worldLoaded = true;
                    activeMapPath = defaultMapPath;
                }
                else
                {
                    printf("[MAIN ERROR] default world failed to load: %s\n",
                           defaultMapPath.c_str());
                    gameState = GAME_MENU;
                }
            }

            {
                DuelConfigResult dcr = getPendingDuelConfig();
                if (dcr.startDuel) {
                    DuelConfig cfg;
                    cfg.numNpcs = dcr.numNpcs;
                    cfg.killsToWin = dcr.killsToWin;
                    cfg.duelLengthSeconds = dcr.duelLengthSeconds;
                    cfg.npcDifficulty = dcr.npcDifficulty;
                    cfg.enabled = true;
                    gDuelManager.start(cfg, player, npcSystem, world);
                    activeMapPath = cfg.mapPath;
                    worldLoaded = !world.mesh.verts.empty();
                    clearPendingDuelConfig();
                    gBombTagManager.stop();
                }
            }
            {
                BombTagConfigResult bcr = getPendingBombTagConfig();
                if (bcr.start) {
                    gDuelManager.stopDuel();
                    BombTagConfig cfg;
                    cfg.numNpcs = bcr.numNpcs;
                    cfg.lives = bcr.lives;
                    cfg.timeLimitSeconds = bcr.timeLimitSeconds;
                    cfg.npcDifficulty = bcr.npcDifficulty;
                    cfg.enabled = true;
                    gBombTagManager.setCamera(camera);
                    gBombTagManager.start(cfg, player, npcSystem, world);
                    activeMapPath = cfg.mapPath;
                    worldLoaded = !world.mesh.verts.empty();
                    clearPendingBombTagConfig();
                }
            }
            {
                MultiplayerConnectInfo mci = getPendingMultiplayerConnect();
                if (mci.shouldConnect) {
                    // Don't force-load default map. If a sandbox map was loaded, keep it.
                    // The server's map identity will arrive via Welcome/JoinAccept.
                    // If no world at all, load a minimal default so the client can start.
                    if (!worldLoaded)
                    {
                        std::string connectMap = defaultMapPath;
                        if (!mci.directAddress.empty() && !mci.mapName.empty())
                            connectMap = "assets/maps/" + mci.mapName + ".glb";
                        if (loadWorldFromGLB(world, connectMap.c_str()))
                        {
                            activeMapPath = connectMap;
                            worldLoaded = true;
                            Debug::log(Debug::Category::Networking,
                                       "[MAIN NET] loaded map for connecting path=%s\n",
                                       connectMap.c_str());
                        }
                    }
                    else
                    {
                        Debug::log(Debug::Category::Networking,
                                   "[MAIN NET] keeping current world=%s for connecting\n",
                                   activeMapPath.c_str());
                    }

                    player.username = AuthSystem::instance().displayName();
                    player.reset();
                    // 7 22 2026 1227 attempting fix 
                    // no more local host 

                    if (!mci.directAddress.empty())
                    {
                        Debug::log(
                            Debug::Category::Networking,
                            "[DIRECT JOIN START] address=%s\n",
                            mci.directAddress.c_str());

                        const bool connectionStarted = MimitaNet::mpInit(
                            mpContext,
                            mci.directAddress,
                            player.username);
                        if (connectionStarted)
                        {
                            Debug::log(
                                Debug::Category::Networking,
                                "[CONNECT STARTED] address=%s room=%s\n",
                                mci.directAddress.c_str(),
                                mci.roomCode.c_str());
                        }
                        else
                        {
                            Debug::warn(
                                Debug::Category::Networking,
                                "[CONNECT FAILED] address=%s room=%s reason=start-failed\n",
                                mci.directAddress.c_str(),
                                mci.roomCode.c_str());
                        }
                    }
                    else if (mci.roomCode.empty())
                    {
                        Debug::warn(
                            Debug::Category::Networking,
                            "[ROOM JOIN FAILED] reason=empty-room-code\n");
                    }
                    else
                    {
                        mpContext.currentRoomCode = mci.roomCode;

                        Debug::log(
                            Debug::Category::Networking,
                            "[ROOM JOIN START] room=%s\n",
                            mci.roomCode.c_str());

                        // Async ICE connect: returns immediately, runs on a
                        // background thread, and is polled in mpTick. The game
                        // never blocks during ICE negotiation.
                        MimitaNet::mpIceConnectStart(
                            mpContext,
                            mci.roomCode,
                            player.username);
                    }

                    clearPendingMultiplayerConnect();
                    }
            }
            if (gameState == GAME_PLAYING && worldLoaded &&
                !gReplayRecorder.isRecording()) {
                Terminal::instance().execute("replay.record");
                Terminal::instance().addLog(
                    "[REPLAY] 60 second ring buffer active");
            }
            bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !isChatOpen() && !duelMatchOver && MouseLock::locked()
                    ? GLFW_CURSOR_DISABLED
                    : GLFW_CURSOR_NORMAL);
        }
        else
        {
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        prevState = gameState;
    }

    {
        static DuelPhase prevDuelPhase = DuelPhase::Off;
        if (gDuelManager.phase() != prevDuelPhase) {
            bool enteredMatchEnd = gDuelManager.phase() == DuelPhase::MatchEnd;
            if (enteredMatchEnd && gDuelManager.endState() == DuelEndState::None) {
                gDuelManager.matchEndTick = gReplayRecorder.currentTick();
                if (!gReplayRecorder.isRecording())
                    Terminal::instance().execute("replay.record");
                setReplayCaptureEnabled(true);
                Debug::log(Debug::Category::Duel, "[DUEL FLOW] VictoryScreen start (3s)");
                Debug::log(Debug::Category::Duel, "[FINAL KILL] detected tick=%u winner=%s",
                    gDuelManager.matchEndTick,
                    gDuelManager.matchWinner() == DuelTeam::Player ? "PLAYER" : "NPC");
                Debug::log(Debug::Category::Duel, "[FINAL KILL] recording aftermath for 5s (endTick=%u)", gDuelManager.matchEndTick);
            }
            prevDuelPhase = gDuelManager.phase();
            bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !isChatOpen() && !duelMatchOver && MouseLock::locked()
                    ? GLFW_CURSOR_DISABLED
                    : GLFW_CURSOR_NORMAL);
        }
    }

    DevOverlay::instance().update(dt);
    NpcSelectionManager::instance().update();

    static bool gravePrev = false;
    bool graveDown = glfwGetKey(engine.window(), GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
    if (graveDown && !gravePrev) {
        Terminal::instance().toggle();
        bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
        glfwSetInputMode(engine.window(), GLFW_CURSOR,
            Terminal::instance().isOpen() || isChatOpen() ? GLFW_CURSOR_NORMAL :
            (gameState == GAME_PLAYING && !duelMatchOver && MouseLock::locked() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL));
    }
    gravePrev = graveDown;
}
