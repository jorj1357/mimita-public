#include "engine/engine-tick.h"
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
#include "physics/movement/physics-collision.h"
#include "physics/ray-utils.h"
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
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "debug/transform-debug.h"
#include "debug/debug-diag.h"
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
#include "sim/sim-context.h"
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
#include "game/game-cli.h"
#include "debug/log-manager.h"
#include <windows.h>

namespace fs = std::filesystem;

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;
extern bool gMainmenuDebug;




constexpr double SIM_DT = 1.0 / 60.0;

void engineTick(Engine& engine)
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

    struct ReplayTestState {
        bool active = false;
        uint32_t tick = 0;
        uint32_t npcId = 0;
    };

    static SimContext simContext;
    static bool simContextInitialized = false;
    if (!simContextInitialized) {
        simContext.player = &player;
        simContext.world = &world;
        simContext.npcSystem = &npcSystem;
        simContext.randomSeed = 0.0f;
        simContextInitialized = true;
    }

    static double simAccumulator = 0.0;
    static GameState prevState = GAME_MENU;
    static bool npcsSpawned = false;
    static ReplayTestState replayTest;
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, 0);
    static const std::string defaultMapPath =
        "assets/maps/mimita-aabb-only-interior-small-v4.glb";


    {
        HotReloadSystem::instance().reloadGameDLLIfChanged();
        gFramePacer.beginFrame();
        Perf::beginFrame();
        float dt = engine.beginFrame();
        AnalyticsManager::instance().update(dt);
        updatePlayerProceduralHotReload(dt);
        CrosshairConfig::instance().pollReload();
        AvatarSystem::instance().pollHotReload();
        bool worldPassRan = false;

        // Re-register avatar UI after hot reload (EXE code, always safe)
        {
            static uint32_t lastReloadCount = 0;
            uint32_t currentCount = HotReloadSystem::instance().gameMemory().reloadCount;
            if (currentCount != lastReloadCount) {
                printf("[AVATAR UI] Hot reload re-register complete (count=%u)\n", currentCount);
                Terminal::instance().addLog("[AVATAR UI] Hot reload re-register complete");
                lastReloadCount = currentCount;
            }
        }

        { Perf::ScopedTimer _aud("Audio");
        audioUpdate(dt);
        MusicManager::instance().update(dt);
        }
        DebugVis::update();
        uiSetDebug(DebugVis::ui());

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

                        if (!world.spawnPoints.empty())
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
                            const glm::vec3 fallback{1.0f, 5.0f, 60.0f};
                            player.pos = fallback;
                            player.respawnPosition = fallback;
                            printf("[SANDBOX SPAWN WARNING] no GLB spawns; fallback=(%.1f %.1f %.1f)\n",
                                   fallback.x, fallback.y, fallback.z);
                        }

                        reportSandboxMapLoadResult(
                            "Loaded: " + selectedPath, true);
                        printf("[SANDBOX MAP] load success path=%s spawns=%zu\n",
                               activeMapPath.c_str(), world.spawnPoints.size());
                    }
                }

                if (gameState == GAME_PLAYING && !worldLoaded)
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

                // Handle duel config from menu
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
                // Handle bomb tag config from menu
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
                // Handle multiplayer connect from menu
                {
                    MultiplayerConnectInfo mci = getPendingMultiplayerConnect();
                    if (mci.shouldConnect) {
                        if (activeMapPath != defaultMapPath &&
                            loadWorldFromGLB(world, defaultMapPath.c_str()))
                        {
                            activeMapPath = defaultMapPath;
                            worldLoaded = true;
                            printf("[MAIN NET] restored server-compatible map=%s\n",
                                   activeMapPath.c_str());
                        }
                        player.username = LocalProfileSystem::instance().currentUsername();
                        if (MimitaNet::mpInit(mpContext, mci.address, player.username)) {
                            printf("[MAIN] multiplayer connected to %s\n", mci.address.c_str());
                            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
                    gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !duelMatchOver
                        ? GLFW_CURSOR_DISABLED
                        : GLFW_CURSOR_NORMAL);
            }
            else
            {
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            prevState = gameState;
        }

        // Update cursor mode when duel phase changes (e.g. Active → MatchEnd)
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
                    gameState == GAME_PLAYING && !Terminal::instance().isOpen() && !duelMatchOver
                        ? GLFW_CURSOR_DISABLED
                        : GLFW_CURSOR_NORMAL);
            }
        }

        // Dev tools update
        DevOverlay::instance().update(dt);
        NpcSelectionManager::instance().update();

        // Terminal toggle on grave accent (`/~)
        static bool gravePrev = false;
        bool graveDown = glfwGetKey(engine.window(), GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
        if (graveDown && !gravePrev) {
            Terminal::instance().toggle();
            bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                Terminal::instance().isOpen() ? GLFW_CURSOR_NORMAL :
                (gameState == GAME_PLAYING && !duelMatchOver ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL));
        }
        gravePrev = graveDown;

        // Final kill clip creation (immediately, no 5-second wait)
        if (gDuelManager.phase() == DuelPhase::MatchEnd &&
            gDuelManager.matchEndTick > 0 &&
            gReplayRecorder.isRecording() &&
            !gDuelManager.isReplayReady())
        {
            uint32_t killTick = gDuelManager.matchEndTick;
            uint32_t nowTick = gReplayRecorder.currentTick();
            uint32_t start = killTick > 480 ? killTick - 480 : 0;
            uint32_t end = killTick + 5u * ReplayRingBuffer::TickRate;
            if (end > nowTick) end = nowTick;
            Debug::log(Debug::Category::Duel, "[DUEL] creating final kill clip start=%u end=%u (kill=%u)", start, end, killTick);
            ReplayClip clip = gReplayRecorder.makeClip(start, end, killTick, "", "");
            bool clipCreated = false;
            if (clip.sceneFrames.empty()) {
                Debug::log(Debug::Category::Replay, "[REPLAY] final kill marker missing, using last 10 seconds");
                start = nowTick > 600 ? nowTick - 600 : 0;
                end = nowTick;
                clip = gReplayRecorder.makeClip(start, end, 0, "", "");
            } else {
                Debug::log(Debug::Category::Replay, "[REPLAY] using final kill marker at tick %u", killTick);
            }
            if (!clip.sceneFrames.empty()) {
                Debug::log(Debug::Category::Replay, "[REPLAY] clip has %zu sceneFrames, %zu frames", clip.sceneFrames.size(), clip.frames.size());
                std::string savePath = generateReplayClipPath();
                Debug::log(Debug::Category::Replay, "[REPLAY] saving clip to %s", savePath.c_str());
                if (clip.save(savePath)) {
                    gDuelManager.finalKillReplayPath = savePath;
                    gDuelManager.finalKillSavedOnce = true;
                    Debug::log(Debug::Category::Replay, "[REPLAY] final kill auto-saved: %s frames=%zu", savePath.c_str(), clip.sceneFrames.size());
                } else {
                    Debug::log(Debug::Category::Replay, "[REPLAY] clip.save FAILED for %s", savePath.c_str());
                }
                std::string tmpPath = "replays/_final_kill_temp.json";
                if (clip.save(tmpPath)) {
                    Debug::log(Debug::Category::Replay, "[REPLAY] temp clip saved OK");
                } else {
                    Debug::log(Debug::Category::Replay, "[REPLAY] temp clip save FAILED");
                }
                Debug::log(Debug::Category::Replay, "[REPLAY] loading clip from %s", tmpPath.c_str());
                if (gReplayPlayer.loadFromJSON(tmpPath)) {
                    gDuelManager.setReplayReady();
                    Debug::log(Debug::Category::Replay, "[REPLAY] replayReady=1 clip loaded OK totalTicks=%u currentTick=%u",
                               gReplayPlayer.totalTicks(), gReplayPlayer.currentTick());
                    clipCreated = true;
                } else {
                    Debug::log(Debug::Category::Replay, "[REPLAY] loadFromJSON FAILED");
                }
            } else {
                Debug::log(Debug::Category::Replay, "[REPLAY] clip.sceneFrames EMPTY after makeClip - NO REPLAY DATA");
            }
            if (clipCreated)
                gDuelManager.matchEndTick = 0;
            else
                DevOverlay::instance().showNotification("Replay unavailable", 5.0f);
        }

        // Force GAME_PLAYING rendering path during replay export so the
        // replay scene is rendered into the framebuffer for glReadPixels.
        // Without this, export from main menu (GAME_MENU) would capture the
        // menu UI instead of the replay.
        const bool replayExportForceRender =
            (getReplayExportJob().state == ReplayExportJob::Capturing ||
             getReplayExportJob().state == ReplayExportJob::Encoding) && worldLoaded;

        if (gameState == GAME_PLAYING || replayExportForceRender)
        {
            DebugVis::beginCollisionFrame();
            gReplayPlayer.update(dt);

            // Replay export mode: seek and rebuild interpolated frame for capture
            // [D] [E] Step 1: replay update (seek + zero-dt update)
            {
                const ReplayExportJob& job = getReplayExportJob();
                if (job.state == ReplayExportJob::Capturing) {
                    uint32_t seekTick = job.capturedTicks;
                    uint32_t beforeTick = gReplayPlayer.currentTick();
                    if (seekTick < gReplayPlayer.totalTicks()) {
                        Debug::log(Debug::Category::Replay, "[EXPORTTRACE] seek tick %u / total %u", seekTick, gReplayPlayer.totalTicks());
                        gReplayPlayer.seekToTick(seekTick);
                        gReplayPlayer.update(0.0f);
                        uint32_t afterTick = gReplayPlayer.currentTick();
                        Debug::log(Debug::Category::Replay, "[EXPORT DEBUG] REPLAY_PLAYER.update: beforeTick=%u afterTick=%u (seekTick=%u)",
                                   beforeTick, afterTick, seekTick);
                    } else {
                        Debug::log(Debug::Category::Replay, "[EXPORTTRACE] seek tick %u >= total %u (skip)", seekTick, gReplayPlayer.totalTicks());
                    }
                }
            }

            const bool replayPlaybackActive = gReplayPlayer.isPlaying();
            Debug::log(Debug::Category::Replay, "[EXPORTTRACE] SET replayPlaybackActive=%d file=main.cpp line=%d",
                       (int)replayPlaybackActive, 2689);
            bool replayRenderActive = replayPlaybackActive ||
                (getReplayExportJob().state == ReplayExportJob::Capturing && gReplayPlayer.totalTicks() > 0);
            // Force replay rendering during export even if the player stopped
            if (getReplayExportJob().state == ReplayExportJob::Capturing) {
                if (!replayRenderActive) {
                    Debug::log(Debug::Category::Replay, "[EXPORTTRACE] FORCE replayRenderActive=1 during export (was 0)");
                }
                replayRenderActive = true;
            }
            Debug::log(Debug::Category::Replay, "[EXPORTTRACE] SET replayRenderActive=%d file=main.cpp line=%d",
                       (int)replayRenderActive, 2690);
            Debug::log(Debug::Category::Replay, "[EXPORTTRACE] replayPlaybackActive=%d replayRenderActive=%d totalTicks=%u exportState=%d",
                   (int)replayPlaybackActive, (int)replayRenderActive, gReplayPlayer.totalTicks(),
                   (int)getReplayExportJob().state);
            setReplayCaptureEnabled(!replayPlaybackActive);

            // Fixed-tick simulation accumulator
            // Accumulate real dt and step simulation at SIM_DT rate
            simAccumulator += (double)dt;

            { Perf::ScopedTimer _t("Simulation");
            while (simAccumulator >= SIM_DT) {
                InputFrame tickFrame;

                if (!replayPlaybackActive) {
                    // Live input: build InputFrame from keyboard + terminal override
                    InputCommandSystem::instance().setKeyboardEnabled(!Terminal::instance().isOpen());
                    // tickFrame = buildInputFrame(engine.window(), camera);

                    // lock mvoemnet if countdown in duels 6 7 2026 
                    tickFrame = buildInputFrame(engine.window(), camera);

                    if (gDuelManager.phase() == DuelPhase::Countdown ||
                        gDuelManager.phase() == DuelPhase::MatchEnd ||
                        gBombTagManager.isCountdownActive() ||
                        gBombTagManager.phase() == BombTagPhase::MatchEnd)
                    {
                        tickFrame.moveX = 0.0f;
                        tickFrame.moveY = 0.0f;

                        tickFrame.jump = false;
                        tickFrame.jumpPressed = false;

                        tickFrame.dashPressed = false;
                        tickFrame.freezeHeld = false;

                        tickFrame.reloadPressed = false;
                    }
                    if (tickFrame.reloadPressed) {
                        if (DebugConfig::DEBUG_INPUT)
                            Debug::log(Debug::Category::General, "[INPUT] key -> action=reload -> command=reload\n");
                        Terminal::instance().execute("reload");
                    }

                    if (replayTest.active) {
                        tickFrame = {};
                        if (replayTest.tick < 45) {
                            tickFrame.moveY = 1.0f;
                            tickFrame.movementPressed = true;
                        }
                        if (replayTest.tick >= 20 &&
                            replayTest.tick < 24) {
                            tickFrame.jump = true;
                            tickFrame.jumpPressed =
                                replayTest.tick == 20;
                        }
                        if (replayTest.tick == 55) {
                            tickFrame.moveY = 1.0f;
                            tickFrame.movementPressed = true;
                            tickFrame.dashPressed = true;
                        }

                        if (replayTest.tick == 90) {
                            weapons.equip(player, 1);
                            Terminal::instance().execute("shoot");
                        } else if (replayTest.tick == 150) {
                            weapons.equip(player, 3);
                            Terminal::instance().execute("shoot");
                        } else if (replayTest.tick == 180) {
                            tickFrame.reloadPressed = true;
                            Terminal::instance().execute("reload");
                        } else if (replayTest.tick == 220) {
                            for (Npc& npc : npcSystem.all()) {
                                if (npc.id != replayTest.npcId ||
                                    npc.body.dead)
                                    continue;
                                DeathSystem::instance().kill(
                                    npc.body,
                                    "npc_" + std::to_string(npc.id),
                                    "npc",
                                    player.username,
                                    camera.front,
                                    24.0f);
                                break;
                            }
                        }
                    }
                }

                const bool recordingReplayTick =
                    gReplayRecorder.isRecording() && !replayPlaybackActive;
                uint32_t replayTick = 0;
                if (recordingReplayTick) {
                    replayTick = gReplayRecorder.currentTick();
                    gReplayRecorder.recordFrame(tickFrame);
                }

                // Run simulation for this tick
                if (!freecamEnabled && !replayPlaybackActive)
                    simulateTick(simContext, tickFrame);

                // Capture death position for camera orbit (player.pos stays at death location)
                if (player.dead && glm::length(deathPosition) < 0.1f)
                    deathPosition = player.pos;
                // Reset death position on respawn
                if (!player.dead)
                    deathPosition = glm::vec3(0.0f);

                if (recordingReplayTick) {
                    ReplaySceneFrame sceneFrame;
                    sceneFrame.tick = (int)replayTick;
                    sceneFrame.time = (float)sceneFrame.tick / 60.0f;

                    // Camera
                    sceneFrame.camera.position = camera.pos;
                    sceneFrame.camera.rotation = glm::vec3(camera.pitch, 0.0f, camera.yaw);
                    sceneFrame.camera.fov = camera.fov;

                    // Player
                    ReplayActorState playerActor;
                    playerActor.id = player.username.empty() ? "admin" : player.username;
                    playerActor.name = player.username;
                    playerActor.type = "player";
                    playerActor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
                    playerActor.position = player.pos;
                    playerActor.rotation = glm::vec3(0.0f, 0.0f, player.yaw);
                    playerActor.velocity = player.vel;
                    playerActor.health = player.currentHp;
                    playerActor.maxHealth = player.maxHp;
                    playerActor.dead = player.dead;
                    playerActor.grounded = player.onGround;
                    playerActor.collidable = !player.dead;
                    playerActor.fade = 0.0f;
                    playerActor.outfitPath = GetPlayerSettings().outfitPath;
                    {
                        const WeaponDefinition* wdef = weapons.getCurrentDef(player);
                        if (wdef) {
                            playerActor.weaponName = wdef->id;
                            playerActor.weaponModelPath = wdef->modelPath;
                        } else {
                            playerActor.weaponName = "none";
                            playerActor.weaponModelPath = "";
                        }
                        auto wit = player.weaponRuntimes.find(player.equippedWeaponId);
                        if (wit != player.weaponRuntimes.end()) {
                            playerActor.currentAmmo = wit->second.currentAmmo;
                            playerActor.reserveAmmo = wit->second.reserveAmmo;
                        }
                    }
                    playerActor.reloading = weapons.isReloading(player);
                    playerActor.shooting = weapons.isShooting();
                    playerActor.animationState = player.onGround
                        ? (glm::length(glm::vec2(player.vel.x, player.vel.y)) > 0.5f ? "move" : "idle")
                        : "air";
                    playerActor.bodyParts = captureReplayBodyParts(player);
                    sceneFrame.actors.push_back(playerActor);

                    // NPCs
                    for (const Npc& npc : npcSystem.all()) {
                        ReplayActorState npcActor;
                        npcActor.id = "npc_" + std::to_string(npc.id);
                        npcActor.name = npc.body.username;
                        npcActor.type = npc.body.dead ? "corpse" : "npc";
                        npcActor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
                        npcActor.position = npc.body.pos;
                        npcActor.rotation = glm::vec3(0.0f, 0.0f, npc.body.yaw);
                        npcActor.velocity = npc.body.vel;
                        npcActor.health = npc.body.currentHp;
                        npcActor.maxHealth = npc.body.maxHp;
                        npcActor.dead = npc.body.dead;
                        npcActor.grounded = npc.body.onGround;
                        npcActor.collidable = !npc.body.dead;
                        npcActor.fade = 0.0f;
                        npcActor.outfitPath = "";
                        {
                            const WeaponDefinition* wdef = weapons.getDefForSlot(npc.body.equippedSlot);
                            if (wdef) {
                                npcActor.weaponName = wdef->id;
                                npcActor.weaponModelPath = wdef->modelPath;
                            } else {
                                npcActor.weaponName = "none";
                                npcActor.weaponModelPath = "";
                            }
                        }
                        {
                            auto wit = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
                            if (wit != npc.body.weaponRuntimes.end()) {
                                npcActor.currentAmmo = wit->second.currentAmmo;
                                npcActor.reserveAmmo = wit->second.reserveAmmo;
                            }
                        }
                        npcActor.animationState = npcStateName(npc.stateMachine.currentState);
                        npcActor.bodyParts = captureReplayBodyParts(npc.body);
                        sceneFrame.actors.push_back(npcActor);
                    }
                    DeathSystem::instance().appendReplayActors(sceneFrame.actors);

                    gReplayRecorder.recordSceneFrame(sceneFrame);
                    gReplayClipSaver.update();
                    gReplayFactory.update();
                    GuiLayoutManager::instance().pollReload();
                    LightingConfig::instance().pollReload();
                    ShadowConfig::instance().pollReload();
                    pollVoidDeathConfig();
                    pollHitmarkerAudioConfig();
                    pollReplayExportConfig();
                    pollOutroConfig();
                    pollReplayHitmarkerConfig();

                    if (replayTest.active) {
                        ++replayTest.tick;
                        if (replayTest.tick >= 300) {
                            gReplayRecorder.stopRecording();
                            const std::string path =
                                generateReplayExportPath();
                            const bool exported =
                                gReplayRecorder.exportToJSON(path);
                            replayTest.active = false;
                            if (!exported) {
                                Terminal::instance().addLog(
                                    "[REPLAY TEST] Replay export failed");
                            } else {
                                const std::string absolutePath =
                                    std::filesystem::absolute(path).string();
                                const std::string command =
                                    "python devscripts/replay-validation-runner.py "
                                    "--replay \"" + absolutePath + "\"";
                                std::thread([command, absolutePath]() {
                                    printf(
                                        "[REPLAY TEST] Starting validation for %s\n",
                                        absolutePath.c_str());
                                    const int result =
                                        std::system(command.c_str());
                                    printf(
                                        "[REPLAY TEST] Validation process exit=%d\n",
                                        result);
                                }).detach();
                                Terminal::instance().addLog(
                                    "[REPLAY TEST] Replay exported; "
                                    "headless validation started");
                            }
                            Terminal::instance().execute("replay.record");
                        }
                    }
                }

                simAccumulator -= SIM_DT;
            }
            } // Perf::ScopedTimer Simulation

            // Process local NPC commands only outside authoritative multiplayer.
            if (!mpContext.active) {
                ProcessNpcSpawnCommands(npcSystem, camera, world, player);
                ProcessNpcTrainingSpawnCommands(npcSystem, camera, world, player);
            }

            // Multiplayer tick - receive snapshots
            { Perf::ScopedTimer _net("Networking");
            if (mpContext.active) {
                MimitaNet::MpInput mpInput;
                mpInput.position = player.pos;
                mpInput.velocity = player.vel;
                mpInput.yaw = camera.yaw;
                mpInput.camForward = camera.front;
                // Read live input keys directly for network input packet
                mpInput.wishX = 0.0f;
                mpInput.wishY = 0.0f;
                if (glfwGetKey(engine.window(), GLFW_KEY_W) == GLFW_PRESS) mpInput.wishY += 1.0f;
                if (glfwGetKey(engine.window(), GLFW_KEY_S) == GLFW_PRESS) mpInput.wishY -= 1.0f;
                if (glfwGetKey(engine.window(), GLFW_KEY_A) == GLFW_PRESS) mpInput.wishX -= 1.0f;
                if (glfwGetKey(engine.window(), GLFW_KEY_D) == GLFW_PRESS) mpInput.wishX += 1.0f;
                mpInput.jumpHeld = glfwGetKey(engine.window(), GLFW_KEY_SPACE) == GLFW_PRESS;
                mpInput.dashPressed = glfwGetKey(engine.window(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
                mpInput.freezeHeld = InputCommandSystem::instance().isFreezeHeld();
                mpInput.attackPressed = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                mpInput.equippedSlot = player.equippedSlot;
                MimitaNet::mpTick(mpContext, player.username, dt, &mpInput);
                if (!mpContext.approvedLocalName.empty())
                    player.username = mpContext.approvedLocalName;

                for (const MimitaNet::NetworkShotEvent& event : mpContext.shotEvents)
                {
                    const bool localShooter =
                        event.shooterPlayerId == mpContext.localPlayerId;
                    const bool localTarget =
                        event.targetPlayerId == mpContext.localPlayerId;
                    const auto shooterInfo =
                        mpContext.playerRegistry.find(event.shooterPlayerId);
                    const auto targetInfo =
                        mpContext.playerRegistry.find(event.targetPlayerId);
                    const std::string shooterName =
                        shooterInfo != mpContext.playerRegistry.end()
                        ? shooterInfo->second.name
                        : "player_" + std::to_string(event.shooterPlayerId);
                    const std::string targetName =
                        targetInfo != mpContext.playerRegistry.end()
                        ? targetInfo->second.name
                        : "player_" + std::to_string(event.targetPlayerId);

                    printf("[NET SHOT APPLY] shooter=%u serial=%u target=%u "
                           "damage=%d weapon=%u impact=%u damageConfirmed=%d\n",
                           event.shooterPlayerId, event.shotSerial,
                           event.targetPlayerId, event.damage, event.weapon,
                           event.impactType, (int)event.damageConfirmed);

                    if (event.damageConfirmed && localTarget)
                    {
                        player.currentHp = event.targetHealth;
                        mpContext.localServerHealth = event.targetHealth;
                    }
                    else if (event.damageConfirmed)
                    {
                        auto remote = mpContext.remotePlayers.find(
                            event.targetPlayerId);
                        if (remote != mpContext.remotePlayers.end())
                            remote->second.currentHp = event.targetHealth;
                        auto interpolation =
                            mpContext.remotePlayerInterpolation.find(
                                event.targetPlayerId);
                        if (interpolation !=
                            mpContext.remotePlayerInterpolation.end())
                            interpolation->second.target.health =
                                event.targetHealth;
                    }

                    if (!localShooter && glm::length(event.knockback) > 0.001f)
                    {
                        if (localTarget)
                            player.vel += event.knockback;
                        else
                        {
                            auto remote = mpContext.remotePlayers.find(
                                event.targetPlayerId);
                            if (remote != mpContext.remotePlayers.end())
                                remote->second.vel += event.knockback;
                        }
                    }

                    if (!localShooter)
                    {
                        auto remoteShooter = mpContext.remotePlayers.find(
                            event.shooterPlayerId);
                        if (remoteShooter != mpContext.remotePlayers.end() &&
                            (event.effectFlags &
                             MimitaNet::SHOT_EFFECT_WEAPON_TRIGGER))
                        {
                            remoteShooter->second.networkShootEffectTimer = 0.1f;
                            remoteShooter->second.networkWeaponState |= 1u;
                        }

                        if (event.weapon ==
                            MimitaNet::NETWORK_WEAPON_REVOLVER)
                        {
                            ReplayEffectEvent gunshotEvent;
                            gunshotEvent.type = "gunshot";
                            gunshotEvent.position = event.origin;
                            gunshotEvent.direction = event.direction;
                            gunshotEvent.from = event.origin;
                            gunshotEvent.to = event.hit;
                            gunshotEvent.normal = event.normal;
                            gunshotEvent.sourceActorId = shooterName;
                            captureReplayEffect(gunshotEvent);
                        }

                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_MUZZLE)
                            EffectPartSystem::instance().spawnMuzzleFlash(
                                event.origin, shooterName);
                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_TRACER)
                            EffectPartSystem::instance().spawnTracer(
                                event.origin, event.hit, shooterName);
                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_SHOOT_SOUND)
                        {
                            playWorldSound(
                                "revolvershoot", event.origin,
                                1.0f, 1.0f, 80.0f);
                        }

                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_WORLD_IMPACT)
                        {
                            HitEvent ev;
                            ev.position = event.hit;
                            ev.normal = event.normal;
                            ev.direction = event.direction;
                            ev.hitWorld = true;
                            ev.damage = 0;
                            ev.attacker = shooterName;
                            ev.weaponSource = "net_shot";
                            HitEffects::onHit(ev);
                        }
                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_DEBRIS) {
                            float debrisForce = std::clamp(event.power / 40.0f, 0.1f, 5.0f);
                            EffectPartSystem::instance().spawnWorldDebris(
                                event.hit, event.normal, debrisForce);
                        }
                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_ENTITY_IMPACT)
                        {
                            HitEvent ev;
                            ev.position = event.hit;
                            ev.normal = event.normal;
                            ev.direction = event.direction;
                            ev.hitEntity = true;
                            ev.damage = event.damage;
                            ev.attacker = shooterName;
                            ev.victim = targetName;
                            ev.weaponSource = "net_shot";
                            HitEffects::onHit(ev);
                        }
                        if (event.effectFlags & MimitaNet::SHOT_EFFECT_BLOOD)
                        {
                            HitEvent ev;
                            ev.position = event.hit;
                            ev.normal = event.normal;
                            ev.direction = event.direction;
                            ev.hitEntity = true;
                            ev.damage = event.damage;
                            ev.attacker = shooterName;
                            ev.victim = targetName;
                            ev.weaponSource = "net_shot";
                            HitEffects::onHit(ev);
                        }
                        if (event.effectFlags &
                            MimitaNet::SHOT_EFFECT_HIT_SOUND)
                        {
                            playWorldSound(
                                event.impactType == MimitaNet::SHOT_IMPACT_WORLD
                                    ? "hitworld" : "player_hurt",
                                event.hit, 0.9f, 1.0f, 40.0f);
                        }
                        printf("[NET SHOT RECONSTRUCT] shooter=%u serial=%u "
                               "localShooter=0 impact=%u origin=(%.2f %.2f %.2f) "
                               "hit=(%.2f %.2f %.2f)\n",
                               event.shooterPlayerId, event.shotSerial,
                               event.impactType,
                               event.origin.x, event.origin.y, event.origin.z,
                               event.hit.x, event.hit.y, event.hit.z);
                    }
                    else
                    {
                        printf("[NET SHOT OWNERSHIP] shooter=%u serial=%u "
                               "visualsSkipped=1 reason=local-prediction\n",
                               event.shooterPlayerId, event.shotSerial);
                    }

                    if (event.damageConfirmed && event.killed && !localTarget)
                    {
                        auto remote = mpContext.remotePlayers.find(
                            event.targetPlayerId);
                        if (remote != mpContext.remotePlayers.end())
                        {
                            remote->second.dead = false;
                            DeathSystem::instance().kill(
                                remote->second,
                                "net_player_" + std::to_string(event.targetPlayerId),
                                "player",
                                shooterName,
                                event.direction,
                                event.weapon == MimitaNet::NETWORK_WEAPON_GODBALL
                                    ? 18.0f : 12.0f);
                        }
                    }
                }
                mpContext.shotEvents.clear();

                for (const auto& chatMsg : mpContext.incomingChatMessages)
                {
                    printf("[CHAT] %s: %s\n", chatMsg.senderName.c_str(), chatMsg.text.c_str());
                    for (auto& kv : mpContext.remotePlayers)
                    {
                        if (kv.second.username == chatMsg.senderName)
                        {
                            addChatMessage(kv.second.chatState, chatMsg.text, chatMsg.senderName);
                            break;
                        }
                    }
                    playChatSound((int)chatMsg.text.size());

                    ReplayEffectEvent chatEvent;
                    chatEvent.type = "chat";
                    chatEvent.sourceActorId = chatMsg.senderName;
                    chatEvent.assetId = chatMsg.text;
                    chatEvent.lifetime = computeChatDuration((int)chatMsg.text.size());
                    captureReplayEffect(chatEvent);
                }
                mpContext.incomingChatMessages.clear();

                MimitaNet::mpReconcileLocalPlayer(mpContext, player, dt);

                // Sync server NPCs to local NpcSystem for AI
                {
                    static std::unordered_set<uint32_t> spawnedNpcIds;
                    for (const auto& kv : mpContext.remoteNpcs) {
                        const uint32_t entityId = kv.first;
                        if (spawnedNpcIds.find(entityId) == spawnedNpcIds.end()) {
                            spawnedNpcIds.insert(entityId);
                            float diff = 1.0f;
                            npcSystem.spawnNpc(entityId, diff, kv.second.pos);
                        }
                    }
                    // Remove local NPCs whose server entity no longer exists
                    for (auto it = spawnedNpcIds.begin(); it != spawnedNpcIds.end(); ) {
                        if (mpContext.remoteNpcs.find(*it) == mpContext.remoteNpcs.end()) {
                            npcSystem.destroySelected({*it});
                            it = spawnedNpcIds.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                // Send input to server if we have an assigned ID
                if (mpContext.localPlayerId != 0) {
                    InputFrame mpInput = buildInputFrame(engine.window(), camera);
                    MimitaNet::InputPacket in{};
                    in.header.type = MimitaNet::PACKET_INPUT;
                    in.header.tick = mpContext.tick;
                    in.header.playerId = mpContext.localPlayerId;
                    in.wishX = mpInput.moveX;
                    in.wishY = mpInput.moveY;
                    in.camForwardX = camera.front.x;
                    in.camForwardY = camera.front.y;
                    in.camForwardZ = camera.front.z;
                    in.yaw = camera.yaw;
                    in.clientPx = player.pos.x;
                    in.clientPy = player.pos.y;
                    in.clientPz = player.pos.z;
                    in.clientVx = player.vel.x;
                    in.clientVy = player.vel.y;
                    in.clientVz = player.vel.z;
                    in.equippedSlot = (int16_t)player.equippedSlot;
                    in.weaponState =
                        (weapons.isShooting() ? 1u : 0u) |
                        (weapons.isReloading(player) ? 2u : 0u);
                    in.clientPingMs = mpContext.localPingMs;
                    in.jumpHeld = mpInput.jump ? 1 : 0;
                    in.dashPressed = mpInput.dashPressed ? 1 : 0;
                    in.attackPressed = 0;
                    in.freezeHeld = mpInput.freezeHeld ? 1 : 0;
                    MimitaNet::mpSendPacket(mpContext, &in, sizeof(in));
                }

                // TAB player list
                mpContext.showPlayerList = glfwGetKey(engine.window(), GLFW_KEY_TAB) == GLFW_PRESS;
                // F3 debug overlay
                static bool f3Prev = false;
                bool f3Down = glfwGetKey(engine.window(), GLFW_KEY_F3) == GLFW_PRESS;
                if (f3Down && !f3Prev)
                    mpContext.showDebugOverlay = !mpContext.showDebugOverlay;
                f3Prev = f3Down;
            }
            } // Perf::ScopedTimer Networking

            if (!Terminal::instance().isOpen())
                applyDebugMovement(player, engine.window(), camera, dt);

            camera.decayPunch(dt);
            camera.updateVectors();
            const bool replayFreecam =
                replayPlaybackActive &&
                gReplayPlayer.cameraController().mode() ==
                    ReplayCameraMode::Freecam;
            // Camera ownership: only ONE system may modify camera per frame.
            // Freecam (normal or replay) takes priority; replay controller runs only
            // when freecam is inactive.
            const bool anyFreecam = (freecamEnabled || replayFreecam) &&
                                    !Terminal::instance().isOpen();
            if (replayPlaybackActive && !anyFreecam) {
                if (const ReplaySceneFrame* replayFrame =
                        gReplayPlayer.currentSceneFrame()) {
                    gReplayPlayer.cameraController().update(
                        camera, *replayFrame,
                        gReplayPlayer.killerId(),
                        gReplayPlayer.victimId(), dt);
                }
            }
            if (anyFreecam) {
                glm::vec3 flatForward = camera.front;
                flatForward.z = 0.0f;
                if (glm::length(flatForward) > 0.001f) flatForward = glm::normalize(flatForward);
                glm::vec3 flatRight = glm::normalize(glm::cross(flatForward, glm::vec3(0,0,1)));
                glm::vec3 move(0.0f);
                if (glfwGetKey(engine.window(), GLFW_KEY_W) == GLFW_PRESS) move += flatForward;
                if (glfwGetKey(engine.window(), GLFW_KEY_S) == GLFW_PRESS) move -= flatForward;
                if (glfwGetKey(engine.window(), GLFW_KEY_D) == GLFW_PRESS) move += flatRight;
                if (glfwGetKey(engine.window(), GLFW_KEY_A) == GLFW_PRESS) move -= flatRight;
                if (glfwGetKey(engine.window(), GLFW_KEY_E) == GLFW_PRESS) move.z += 1.0f;
                if (glfwGetKey(engine.window(), GLFW_KEY_Q) == GLFW_PRESS) move.z -= 1.0f;
                if (glm::length(move) > 0.001f)
                    camera.pos += glm::normalize(move) * GetPlayerSettings().freecamSpeed * dt;
            } else if (replayPlaybackActive) {
                // ReplayCameraController owns the camera (already applied above).
            } else if (gDuelManager.phase() == DuelPhase::MatchEnd) {
                camera.follow(gDuelManager.winnerCameraTarget());
                camera.smoothCollision(gDuelManager.winnerCameraTarget(), world.collisionMesh.triangles, dt);
            } else {
                camera.follow(player.pos);
                camera.smoothCollision(player.pos, world.collisionMesh.triangles, dt);
            }
            setAudioListener(camera.pos, camera.front);
            EffectPartSystem::instance().setWorld(world);
            if (replayPlaybackActive) {
                for (const ReplayEffectEvent& effect :
                     gReplayPlayer.takeTriggeredEffects()) {
                    if (effect.type == "chat") {
                        ActorChatState& chatState = gReplayChatStates[effect.sourceActorId];
                        addChatMessage(chatState, effect.assetId, effect.sourceActorId);
                        playChatSound((int)effect.assetId.size());
                    } else if (effect.type == "gunshot") {
                        EffectPartSystem::instance().spawnMuzzleFlash(
                            effect.from, effect.sourceActorId);
                        EffectPartSystem::instance().spawnTracer(
                            effect.from, effect.to, effect.sourceActorId);
                    } else if (effect.type == "blood_spurt_emitter") {
                        EffectPartSystem::instance().spawnBloodEffect(
                            effect.position, effect.direction, 50.0f,
                            effect.sourceActorId, effect.targetActorId);
                        // Only show hitmarker when the viewed actor (killer) lands a hit
                        if (effect.sourceActorId == gReplayPlayer.killerId())
                            hitmarker();
                    } else if (effect.type == "dash") {
                        EffectPartSystem::instance().spawnDash(effect.position);
                    } else if (effect.type == "footstep") {
                        EffectPartSystem::instance().spawnFootstep(effect.position);
                    } else if (effect.type == "impact_world") {
                        HitEvent ev;
                        ev.position = effect.position;
                        ev.normal = effect.normal;
                        ev.direction = effect.direction;
                        ev.hitWorld = true;
                        ev.damage = 0;
                        ev.attacker = effect.sourceActorId;
                        ev.victim = effect.targetActorId;
                        ev.weaponSource = "replay";
                        HitEffects::onHit(ev);
                        EffectPartSystem::instance().spawnWorldDebris(
                            effect.position, effect.normal, 1.0f);
                    } else if (effect.type == "debris_block") {
                        EffectPartSystem::instance().spawnWorldDebris(
                            effect.position, effect.normal, 1.5f);
                    } else if (effect.type == "hit_burst") {
                        HitEffects::spawnHitEffects(effect.position, effect.normal, effect.normal, 0, "replay", "replay");
                    } else if (effect.type == "impact_entity") {
                        HitEvent ev;
                        ev.position = effect.position;
                        ev.normal = effect.normal;
                        ev.direction = effect.direction;
                        ev.hitEntity = true;
                        ev.damage = 0;
                        ev.attacker = effect.sourceActorId;
                        ev.victim = effect.targetActorId;
                        ev.weaponSource = "replay";
                        HitEffects::onHit(ev);
                    } else if (effect.type == "muzzle_flash") {
                        EffectPartSystem::instance().spawnMuzzleFlash(
                            effect.position, effect.sourceActorId);
                    } else if (effect.type == "tracer") {
                        EffectPartSystem::instance().spawnTracer(
                            effect.from, effect.to, effect.sourceActorId);
                    } else if (effect.type == "death_ellipsoid") {
                        glm::vec3 dir = glm::length(effect.to - effect.from) > 0.001f
                            ? glm::normalize(effect.to - effect.from)
                            : glm::vec3(1.0f, 0.0f, 0.0f);
                        float len = glm::length(effect.to - effect.from);
                        const auto& deCfg = HitEffects::config().deathEllipsoid;
                        float rad = effect.scale.x > 0.0f ? effect.scale.x : deCfg.radius;
                        EffectPartSystem::instance().spawnDeathEllipsoid(
                            effect.from, dir, len, rad, std::max(effect.lifetime, 0.1f));
                    } else if (!effect.type.empty() &&
                               effect.type != "corpse_spawn") {
                        EffectPartSystem::instance().spawnCustom(
                            effect.position, glm::vec3(effect.color),
                            std::max(effect.lifetime, 0.1f),
                            effect.type.c_str());
                    }
                }
                {
                    const ReplayCameraMode camMode =
                        gReplayPlayer.cameraController().mode();
                    std::string viewedEntity;
                    switch (camMode) {
                        case ReplayCameraMode::Freecam:
                            break;
                        case ReplayCameraMode::FirstPerson:
                        case ReplayCameraMode::Orbit:
                            viewedEntity = gReplayPlayer.killerId();
                            break;
                        case ReplayCameraMode::Victim:
                            viewedEntity = gReplayPlayer.victimId();
                            break;
                    }
                    for (const ReplaySoundEvent& sound :
                         gReplayPlayer.takeTriggeredSounds()) {
                        bool play = true;
                        if (sound.soundPath == "hitmarker1") {
                            play = ReplayShouldPlayHitmarkerAudio(
                                gReplayPlayer.killerId(), camMode, viewedEntity);
                            Debug::log(Debug::Category::Replay,
                                "[REPLAY HITMARKER]\n"
                                "  attacker=%s\n"
                                "  viewedEntity=%s\n"
                                "  camera=%s\n"
                                "  play=%d\n",
                                gReplayPlayer.killerId().c_str(),
                                viewedEntity.c_str(),
                                gReplayPlayer.cameraController().modeName(),
                                (int)play);
                        }
                        if (!play)
                            continue;
                        playWorldSound(
                            sound.soundPath, sound.position,
                            sound.volume, sound.pitch,
                            sound.maxDistance > 0.0f ? sound.maxDistance : 40.0f);
                    }
                }
            }
            { Perf::ScopedTimer _wp("Weapons");
            if (!replayPlaybackActive)
                weapons.update(camera, player, npcSystem, world, dt);
            }
            if (mpContext.active)
            {
                const std::vector<RevolverShotResult> godballHits =
                    weapons.collectRemoteGodballHits(
                        player, mpContext.remotePlayers, dt);
                for (const RevolverShotResult& hit : godballHits)
                {
                    const glm::vec3 direction =
                        glm::length(hit.end - hit.start) > 0.001f
                        ? glm::normalize(hit.end - hit.start)
                        : glm::vec3(0.0f, 1.0f, 0.0f);
                    MimitaNet::mpSendShotEvent(
                        mpContext, hit.targetId, (int)hit.damage, hit.damage,
                        MimitaNet::SHOT_EFFECT_ENTITY_IMPACT |
                            MimitaNet::SHOT_EFFECT_BLOOD |
                            MimitaNet::SHOT_EFFECT_HIT_SOUND,
                        MimitaNet::NETWORK_WEAPON_GODBALL,
                        MimitaNet::SHOT_IMPACT_ENTITY,
                        hit.start, hit.end, direction, -direction,
                        hit.knockbackImpulse);
                }
            }

            if (!replayPlaybackActive) {
                if (gDuelManager.enabled()) {
                    gDuelManager.update(dt, player, npcSystem, world, camera);
                }
                if (gBombTagManager.enabled()) {
                    gBombTagManager.update(dt, player, npcSystem, world);
                }
                player.updateAudio(dt);

                // Auto-start replay when state becomes FinalKillReplay
                if (gDuelManager.endState() == DuelEndState::FinalKillReplay &&
                    !gReplayPlayer.isPlaying())
                {
                    if (gDuelManager.isReplayReady()) {
                        Debug::log(Debug::Category::Duel, "[DUEL] Starting Final Kill Replay");
                        Debug::log(Debug::Category::Duel, "[REPLAY] Replay Loaded totalTicks=%u",
                                   gReplayPlayer.totalTicks());
                        gReplayPlayer.beginPlayback();
                        Debug::log(Debug::Category::Duel, "[REPLAY] Replay Playing isPlaying=%d currentTick=%u",
                                   (int)gReplayPlayer.isPlaying(), gReplayPlayer.currentTick());
                    } else if (gReplayRecorder.isRecording() && gDuelManager.matchEndTick > 0) {
                        // Clip wasn't created yet; do it now synchronously
                        Debug::log(Debug::Category::Duel, "[REPLAY] Replay not ready, creating clip now");
                        uint32_t killTick = gDuelManager.matchEndTick;
                        uint32_t nowTick = gReplayRecorder.currentTick();
                        uint32_t start = killTick > 480 ? killTick - 480 : 0;
                        uint32_t end = killTick + 5u * ReplayRingBuffer::TickRate;
                        if (end > nowTick) end = nowTick;
                        ReplayClip clip = gReplayRecorder.makeClip(start, end, killTick, "", "");
                        if (clip.sceneFrames.empty()) {
                            start = nowTick > 600 ? nowTick - 600 : 0;
                            end = nowTick;
                            clip = gReplayRecorder.makeClip(start, end, 0, "", "");
                        }
                        if (!clip.sceneFrames.empty()) {
                            std::string savePath = generateReplayClipPath();
                            clip.save(savePath);
                            gDuelManager.finalKillReplayPath = savePath;
                            std::string tmpPath = "replays/_final_kill_temp.json";
                            clip.save(tmpPath);
                            if (gReplayPlayer.loadFromJSON(tmpPath)) {
                                gDuelManager.setReplayReady();
                                Debug::log(Debug::Category::Duel, "[REPLAY] Replay Loaded totalTicks=%u",
                                           gReplayPlayer.totalTicks());
                                gReplayPlayer.beginPlayback();
                                Debug::log(Debug::Category::Duel, "[REPLAY] Replay Playing isPlaying=%d currentTick=%u",
                                           (int)gReplayPlayer.isPlaying(), gReplayPlayer.currentTick());
                            }
                        }
                        if (!gDuelManager.isReplayReady()) {
                            Debug::log(Debug::Category::Duel, "[REPLAY] Clip creation failed, transitioning to menu");
                            gDuelManager.setEndState(DuelEndState::ReplayMenu);
                        }
                    } else {
                        Debug::log(Debug::Category::Duel, "[REPLAY] No recording data available, showing menu");
                        gDuelManager.setEndState(DuelEndState::ReplayMenu);
                    }
                }

                // Transition to ReplayMenu only when replay has actually finished playing
                if (gDuelManager.endState() == DuelEndState::FinalKillReplay &&
                    gDuelManager.isReplayReady() &&
                    gReplayPlayer.currentTick() > 0 &&
                    !gReplayPlayer.isPlaying())
                {
                    Debug::log(Debug::Category::Duel, "[DUEL] Replay Finished -> ReplayMenu");
                    gDuelManager.setEndState(DuelEndState::ReplayMenu);
                }
            }

            // Loop replay: when it reaches the end, seek back to 0 and continue
            if (gDuelManager.endState() == DuelEndState::FinalKillReplay &&
                gReplayPlayer.isPlaying() &&
                gReplayPlayer.currentTick() >= gReplayPlayer.totalTicks() &&
                gReplayPlayer.totalTicks() > 0)
            {
                Debug::log(Debug::Category::Duel, "[DUEL] Replay Looping (tick=%u/%u)",
                           gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
                gReplayPlayer.seekToTick(0);
                gReplayPlayer.resume();
            }

            // Update effect parts
            EffectPartSystem::instance().update(dt);
            HitEffects::updateHitBursts(dt);

            updateChatBubbles(player.chatState, dt);
            for (auto& kv : mpContext.remotePlayers)
                updateChatBubbles(kv.second.chatState, dt);
            for (auto& kv : gReplayChatStates)
                updateChatBubbles(kv.second, dt);

            static bool mousePrev = false;
            bool mouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            bool duelEndVisible = gDuelManager.phase() == DuelPhase::MatchEnd;
            bool duelCountdown = gDuelManager.isCountdownActive();
            bool bombTagEndVisible = gBombTagManager.phase() == BombTagPhase::MatchEnd;
            bool bombTagCountdown = gBombTagManager.isCountdownActive();
            if ((duelEndVisible || bombTagEndVisible) && mouseDown && !mousePrev) {
                Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] mouseClick=1 owner=game_end_ui consumed=1");
                Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] weaponInputBlocked=1 reason=end_ui_visible");
            }
            if (!replayPlaybackActive && !duelEndVisible && !duelCountdown &&
                !bombTagEndVisible && !bombTagCountdown &&
                !Terminal::instance().isOpen() && mouseDown) {
                const WeaponDefinition* curDef = weapons.getCurrentDef(player);
                bool isAuto = curDef && curDef->fireMode == WeaponFireMode::Automatic;
                bool shouldFire = isAuto || (!isAuto && mouseDown && !mousePrev);
                if (shouldFire) {
                    if (editorMode) {
                        selectedEditorObject = selectWorldTriangle(world, camera.pos, camera.front);
                        Terminal::instance().addLog(selectedEditorObject >= 0
                            ? "[EDITOR] selected triangle id " + std::to_string(selectedEditorObject)
                            : "[EDITOR] no object selected");
                    } else {
                        Terminal::instance().execute("shoot");
                    }
                }
            }
            mousePrev = mouseDown;

            static bool rightMousePrev = false;
            bool rightMouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (!replayPlaybackActive && !duelCountdown &&
                !Terminal::instance().isOpen() && rightMouseDown && !rightMousePrev) {
                if (!editorMode) {
                    weapons.fireAlt(camera, player, npcSystem, world);
                }
            }
            rightMousePrev = rightMouseDown;

            static bool slotPrev[10] = {};
            for (int keySlot = 0; keySlot <= 9; ++keySlot) {
                int key = keySlot == 0 ? GLFW_KEY_0 : GLFW_KEY_0 + keySlot;
                bool down = glfwGetKey(engine.window(), key) == GLFW_PRESS;
                if (!replayPlaybackActive && !duelCountdown &&
                    !Terminal::instance().isOpen() && down && !slotPrev[keySlot])
                    Terminal::instance().execute("equipslot" + std::to_string(keySlot));
                slotPrev[keySlot] = down;
            }
            // Process command keybinds (rising edge)
            {
                static std::unordered_map<int, bool> bindPrev;
                for (const auto& pair : G_COMMAND_BINDS) {
                    bool down = glfwGetKey(engine.window(), pair.first) == GLFW_PRESS;
                    if (down && !bindPrev[pair.first] && !Terminal::instance().isOpen())
                        Terminal::instance().execute(pair.second);
                    bindPrev[pair.first] = down;
                }
            }
            // Replay playback keyboard shortcuts (only while replay is active)
            if (replayPlaybackActive && !Terminal::instance().isOpen()) {
                static bool spacePrev = false;
                bool spaceDown = glfwGetKey(engine.window(), GLFW_KEY_SPACE) == GLFW_PRESS;
                if (spaceDown && !spacePrev) {
                    if (gReplayPlayer.isPaused()) gReplayPlayer.resume();
                    else gReplayPlayer.pause();
                }
                spacePrev = spaceDown;

                static bool leftPrev = false;
                bool leftDown = glfwGetKey(engine.window(), GLFW_KEY_LEFT) == GLFW_PRESS;
                if (leftDown && !leftPrev) {
                    uint32_t t = gReplayPlayer.currentTick();
                    uint32_t seekTo = t > 300 ? t - 300 : 0;
                    gReplayPlayer.seekToTick(seekTo);
                }
                leftPrev = leftDown;

                static bool rightPrev = false;
                bool rightDown = glfwGetKey(engine.window(), GLFW_KEY_RIGHT) == GLFW_PRESS;
                if (rightDown && !rightPrev) {
                    uint32_t t = gReplayPlayer.currentTick();
                    uint32_t total = gReplayPlayer.totalTicks();
                    gReplayPlayer.seekToTick(std::min(t + 300, total));
                }
                rightPrev = rightDown;

                static bool lPrev = false;
                bool lDown = glfwGetKey(engine.window(), GLFW_KEY_L) == GLFW_PRESS;
                if (lDown && !lPrev) {
                    gReplayCinematicMode = !gReplayCinematicMode;
                    printf("[CINEMATIC] %s\n", gReplayCinematicMode ? "Enabled" : "Disabled");
                    Terminal::instance().addLog(std::string("[CINEMATIC] ") + (gReplayCinematicMode ? "Enabled" : "Disabled"));
                }
                lPrev = lDown;
            }
            { Perf::ScopedTimer _ren("Rendering");
            diagRenderFrameBegin(dt);
            renderShadowMap(world, camera.pos);
            glViewport(0, 0, engine.renderer->width, engine.renderer->height);
            PostFX::instance().bindFBO();
            diagRenderStage(1);
            renderSky(world, camera);
            renderWorld(world, camera);
            PostFX::instance().consumeMagentaTest();
            diagRenderStage(2);
            {
                static uint64_t renderLogFrame = 0;
                if (renderLogFrame++ % 60 == 0 || getReplayExportJob().state == ReplayExportJob::Capturing) {
                    Debug::log(Debug::Category::Replay, "[EXPORTTRACE] RENDER: replayRenderActive=%d hasSceneFrame=%d exportState=%d",
                           (int)replayRenderActive,
                           gReplayPlayer.currentSceneFrame() ? 1 : 0,
                           (int)getReplayExportJob().state);
                }
            }
            // Update export render mode flag: hide replay/export/debug UI
            // during capture so the exported video looks like gameplay footage.
            gReplayExportRenderMode = isReplayExportActive();

            // [E] Step 2: replay render (into PostFX FBO)
            if (getReplayExportJob().state == ReplayExportJob::Capturing) {
                Debug::log(Debug::Category::Replay, "[EXPORT DEBUG] render order: 1.replay update, 2.replay render, 3.glReadPixels");
            }
            if (replayRenderActive) {
                if (const ReplaySceneFrame* replayFrame =
                        gReplayPlayer.currentSceneFrame()) {
                    const glm::mat4 replayView = camera.getView();
                    const glm::mat4 replayProj = camera.getProj(
                        (float)engine.renderer->width,
                        (float)engine.renderer->height);
                    for (const ReplayActorState& actorState :
                         replayFrame->actors) {
                        std::unique_ptr<Player>& actor =
                            replayActorModels[actorState.id];
                        if (!actor) {
                            actor = std::make_unique<Player>();
                            Debug::log(Debug::Category::Replay, "[HEALTHBAR] created owner=%s", actorState.id.c_str());
                            // Restore per-actor outfit
                            const std::string& outfitToUse =
                                !actorState.outfitPath.empty()
                                    ? actorState.outfitPath
                                    : gReplayPlayer.outfitPath();
                            if (!outfitToUse.empty())
                                OutfitAtlas::instance().apply(*actor, outfitToUse);
                        }
                        actor->username = actorState.name;
                        actor->currentHp = actorState.health;
                        actor->maxHp = actorState.maxHealth;
                        actor->dead = actorState.dead;
                        actor->vel = actorState.velocity;
                        actor->onGround = actorState.grounded;
                        actor->equippedWeaponId = actorState.weaponName;
                        actor->applyReplayPose(
                            actorState.position,
                            actorState.rotation.z,
                            actorState.bodyParts);

                        const bool hideFirstPersonActor =
                            gReplayPlayer.cameraController().mode() ==
                                ReplayCameraMode::FirstPerson &&
                            actorState.id == gReplayPlayer.killerId();
                        if (!hideFirstPersonActor) {
                            actor->renderCurrentPose(
                                engine.renderer->shaderProgram,
                                replayView, replayProj);
                        }

                        const WeaponDefinition* definition =
                            WeaponRegistry::instance().get(
                                actorState.weaponName);
                        if (definition && !definition->modelPath.empty()) {
                            actor->equippedSlot = definition->slot;
                            const std::string weaponKey =
                                actorState.id + ":" + definition->id;
                            WeaponViewModel& viewModel =
                                replayWeaponModels[weaponKey];
                            viewModel.update(
                                camera, *actor, dt, definition, false);
                            viewModel.render(
                                camera, *actor, definition->slot);
                        }
                    }
                }
            } else {
                // Spawn flash: black out world behind the player
                if (player.spawnFlashTimer > 0.0f) {
                    static GLuint spawnFlashVao = 0, spawnFlashVbo = 0;
                    if (!spawnFlashVao) {
                        float verts[] = { -1,-1,0, 3,-1,0, -1,3,0 };
                        glGenVertexArrays(1, &spawnFlashVao);
                        glGenBuffers(1, &spawnFlashVbo);
                        glBindVertexArray(spawnFlashVao);
                        glBindBuffer(GL_ARRAY_BUFFER, spawnFlashVbo);
                        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
                        glEnableVertexAttribArray(0);
                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
                    }
                    GLuint shader = engine.renderer->shaderProgram;
                    glDisable(GL_DEPTH_TEST);
                    glUseProgram(shader);
                    glm::mat4 id(1.0f);
                    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, &id[0][0]);
                    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, &id[0][0]);
                    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, &id[0][0]);
                    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 1);
                    glUniform4f(glGetUniformLocation(shader, "uColor"), 0.0f, 0.0f, 0.0f, 1.0f);
                    glBindVertexArray(spawnFlashVao);
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                    glEnable(GL_DEPTH_TEST);
                }
                renderPlayer(player, camera);
                if (mpContext.active) {
                    for (auto& kv : mpContext.remotePlayers) {
                        renderNetworkPlayer(kv.second, camera, kv.first, false);
                        weapons.renderRemoteWeapon(kv.second, camera);
                    }
                    for (auto& kv : mpContext.remoteNpcs) {
                        renderNetworkPlayer(kv.second, camera, kv.first, false);
                    }
                }
                npcSystem.render(camera);
            }
            diagRenderStage(3);
            {   static float rlogTimer = 0.0f; rlogTimer -= dt;
                if (rlogTimer <= 0.0f && replayPlaybackActive) {
                    rlogTimer = 1.0f;
                    const auto* rframe = gReplayPlayer.currentSceneFrame();
                    printf("[REPLAY] cameraMode=%s freecam=%d paused=%d "
                           "viewingActor=%s tick=%u actorCount=%zu effectCount=%zu\n",
                           gReplayPlayer.cameraController().modeName(),
                           (int)(gReplayPlayer.cameraController().mode() == ReplayCameraMode::Freecam),
                           (int)gReplayPlayer.isPaused(),
                           rframe && !rframe->actors.empty() ? rframe->actors[0].name.c_str() : "none",
                           gReplayPlayer.currentTick(),
                           rframe ? rframe->actors.size() : 0,
                           gReplayPlayer.totalEffectCount());
                }
            }
            if (mpContext.active && !replayPlaybackActive)
            {
                static uint64_t lastReplicaRenderLogMs = 0;
                const uint64_t renderLogNow = MimitaNet::nowMs();
                if (renderLogNow - lastReplicaRenderLogMs >= 1000)
                {
                    for (const auto& kv : mpContext.remotePlayers)
                        printf("[CLIENT RENDER ENTITY] entityId=%u type=Player visible=%d mesh=%s "
                               "position=(%.2f,%.2f,%.2f)\n",
                               kv.first, (int)!kv.second.dead,
                               kv.second.modelLoaded ? "player-glb" : "fallback-capsule",
                               kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
                    for (const auto& kv : mpContext.remoteNpcs)
                        printf("[CLIENT RENDER ENTITY] entityId=%u type=NPC visible=%d mesh=%s "
                               "position=(%.2f,%.2f,%.2f)\n",
                               kv.first, (int)!kv.second.dead,
                               kv.second.modelLoaded ? "player-glb" : "fallback-capsule",
                               kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
                    lastReplicaRenderLogMs = renderLogNow;
                }
            }
            if (!replayPlaybackActive) {
                DeathSystem::instance().render(camera);
                weapons.render(camera, player);
            }
            diagRenderStage(4);

            // Remote player presentation debug overlay
            if (gNetPresentationDebug && mpContext.active)
            {
                float debugY = 120.0f;
                for (const auto& kv : mpContext.remotePlayers)
                {
                    const Player& rp = kv.second;
                    auto it = mpContext.remotePlayerInterpolation.find(kv.first);
                    const MimitaNet::EntityInterpolationState* interp =
                        it != mpContext.remotePlayerInterpolation.end() ? &it->second : nullptr;

                    char buf[256];
                    snprintf(buf, sizeof(buf),
                        "REMOTE id=%u  weapon=%s  hp=%d  dead=%d  ground=%d  "
                        "dashSer=%u  aim=(%.2f,%.2f)",
                        kv.first, rp.equippedWeaponId.c_str(),
                        rp.currentHp, (int)rp.dead, (int)rp.onGround,
                        (unsigned)rp.networkLastDashSerial,
                        rp.aimDirection.x, rp.aimDirection.y);
                    uiDrawText(buf, 10.0f, debugY, 0.32f,
                        rp.dead ? glm::vec4(1,0,0,1) : glm::vec4(0.3f,1,0.5f,1));
                    debugY += 22.0f;

                    if (interp)
                    {
                        snprintf(buf, sizeof(buf),
                            "  pos=(%.1f,%.1f,%.1f)  vel=(%.1f,%.1f,%.1f)  yaw=%.1f",
                            rp.pos.x, rp.pos.y, rp.pos.z,
                            rp.vel.x, rp.vel.y, rp.vel.z, rp.yaw);
                        uiDrawText(buf, 10.0f, debugY, 0.28f, {0.6f,0.7f,0.9f,1});
                        debugY += 20.0f;
                    }
                }
            }
            
            // Entity replication debug overlay
            if (gNetDebugEntities && mpContext.active)
            {
                float debugY = 120.0f;
                auto drawEntityLine = [&](const char* label, uint32_t id,
                    const Player& entity, const glm::vec4& color) {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                        "%s id=%u  hp=%d  dead=%d  weapon=%s  "
                        "pos=(%.1f,%.1f,%.1f)  yaw=%.1f",
                        label, id, entity.currentHp, (int)entity.dead,
                        entity.equippedWeaponId.c_str(),
                        entity.pos.x, entity.pos.y, entity.pos.z, entity.yaw);
                    uiDrawText(buf, 10.0f, debugY, 0.28f, color);
                    debugY += 18.0f;
                };

                for (const auto& kv : mpContext.remotePlayers)
                    drawEntityLine("PLAYER", kv.first, kv.second,
                        kv.second.dead ? glm::vec4(1,0,0,1) : glm::vec4(0.3f,1,0.5f,1));

                for (const auto& kv : mpContext.remoteNpcs)
                    drawEntityLine("NPC", kv.first, kv.second,
                        kv.second.dead ? glm::vec4(1,0.3f,0,1) : glm::vec4(0.2f,0.8f,1,1));
            }

            // Render effect parts (world-space visualizations)
            EffectPartSystem::instance().render(camera);
            HitEffects::renderHitBursts(camera);
            DebugVis::flushTris(camera);
            diagRenderStage(5);
            } // Perf::ScopedTimer Rendering

            // Post-process pass: unbind FBO and apply full-screen effects
            PostFX::instance().unbindFBO();
            diagRenderStage(6);
            PostFX::instance().advanceTime(dt);
            PostFX::instance().pollReload();
            PostFX::instance().render();
            renderShadowMapOverlay(engine.renderer->width, engine.renderer->height);
            diagRenderStage(7);

            worldPassRan = true;

            npcSystem.drawDebug(camera);
            drawDebugStuff(player, camera, world);

            if (mpContext.active && mpContext.showDebugOverlay)
            {
                const glm::vec4 predictedColor{0.1f, 1.0f, 0.25f, 1.0f};
                const glm::vec4 serverColor{1.0f, 0.15f, 0.1f, 1.0f};
                const glm::vec4 remoteColor{0.15f, 0.55f, 1.0f, 1.0f};
                const glm::vec4 npcColor{0.1f, 1.0f, 0.9f, 1.0f};
                const auto drawReplicaCapsule =
                    [&camera](const Player& replica, const glm::vec4& color)
                    {
                        const Capsule capsule = replica.getCapsule();
                        const float giantRadius = capsule.r * 1.8f;
                        DebugVis::drawDiagnosticWireSphere(
                            camera, capsule.a, giantRadius, color);
                        DebugVis::drawDiagnosticWireSphere(
                            camera, capsule.b, giantRadius, color);
                        DebugVis::drawDiagnosticLine(
                            camera, capsule.a, capsule.b, color);
                    };

                DebugVis::drawWireSphere(camera, player.pos, 0.72f, predictedColor);
                DebugVis::drawWorldLabel(
                    player.pos + glm::vec3(0.0f, 0.0f, 2.0f),
                    "LOCAL PREDICTED", predictedColor);
                if (mpContext.hasLocalServerPosition)
                {
                    DebugVis::drawWireSphere(
                        camera, mpContext.localServerPosition, 0.78f, serverColor);
                    DebugVis::drawLine(
                        camera, player.pos, mpContext.localServerPosition, serverColor);
                    char serverLabel[128];
                    snprintf(serverLabel, sizeof(serverLabel),
                             "SERVER id=%u error=%.2fm",
                             mpContext.localPlayerId,
                             glm::length(player.pos - mpContext.localServerPosition));
                    DebugVis::drawWorldLabel(
                        mpContext.localServerPosition + glm::vec3(0.0f, 0.0f, 2.3f),
                        serverLabel, serverColor);
                }

                for (const auto& kv : mpContext.remotePlayers)
                {
                    drawReplicaCapsule(kv.second, remoteColor);
                    bool usedHeadTransform = false;
                    const glm::vec3 healthbarAnchor =
                        playerHealthbarAnchor(
                            kv.second, &usedHeadTransform);
                    DebugVis::drawDiagnosticWireSphere(
                        camera, healthbarAnchor, 0.16f,
                        usedHeadTransform
                            ? glm::vec4(1.0f, 0.75f, 0.1f, 1.0f)
                            : glm::vec4(1.0f, 0.2f, 0.8f, 1.0f));
                    const auto interpolation = mpContext.remotePlayerInterpolation.find(kv.first);
                    if (interpolation != mpContext.remotePlayerInterpolation.end() &&
                        interpolation->second.hasTarget)
                    {
                        DebugVis::drawDiagnosticWireSphere(
                            camera, interpolation->second.target.position, 0.36f, serverColor);
                        DebugVis::drawDiagnosticLine(
                            camera, kv.second.pos, interpolation->second.target.position, serverColor);
                    }
                    char label[128];
                    snprintf(
                        label, sizeof(label),
                        "REMOTE PLAYER id=%u HP=%d anchor=%s interp=100ms",
                        kv.first, kv.second.currentHp,
                        usedHeadTransform ? "head" : "fallback");
                    DebugVis::drawDiagnosticWorldLabel(
                        healthbarAnchor + glm::vec3(0.0f, 0.0f, 0.25f),
                        label, remoteColor);
                }
                for (const auto& kv : mpContext.remoteNpcs)
                {
                    drawReplicaCapsule(kv.second, npcColor);
                    const auto interpolation = mpContext.remoteNpcInterpolation.find(kv.first);
                    if (interpolation != mpContext.remoteNpcInterpolation.end() &&
                        interpolation->second.hasTarget)
                    {
                        DebugVis::drawDiagnosticWireSphere(
                            camera, interpolation->second.target.position, 0.36f, serverColor);
                        DebugVis::drawDiagnosticLine(
                            camera, kv.second.pos, interpolation->second.target.position, serverColor);
                    }
                    char label[128];
                    snprintf(label, sizeof(label), "NPC id=%u HP=%d interp=100ms",
                             kv.first, kv.second.currentHp);
                    DebugVis::drawDiagnosticWorldLabel(
                        kv.second.pos + glm::vec3(0.0f, 0.0f, 2.0f),
                        label, npcColor);
                }
            }

            // Draw NPC selection debug visuals
            if (DebugVis::enabled()) {
                NpcSelectionManager::instance().drawSelection(npcSystem, camera);
            }

            { Perf::ScopedTimer _ui("UI");
            uiBeginFrame(engine.window(), "game-debug-overlay");

            // HUD layout helper: draw dynamic text using JSON positions/colors
            GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/hud.json");
            auto hudText = [&](const std::string& id, const std::string& text) {
                const GuiElement* el = hudLayout.get(id);
                if (!el) return;
                float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
                glm::vec4 color = el->getTextColorVec();
                uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), scale, color);
            };

            drawHitmarker(dt);
            if (mpContext.active && !mpContext.connected)
            {
                const float boxW = 360.0f;
                const float boxX = (uiScreenW() - boxW) * 0.5f;
                hudText("connectionText", mpContext.connectionStatus);
            }
            if (gReplayRecorder.isRecording() && !gReplayExportRenderMode) {
                const float overlayX = uiScreenW() - 230.0f;
                uiDrawRect({overlayX - 18.0f, 20.0f, 12.0f, 12.0f},
                           {1.0f, 0.05f, 0.05f, 1.0f}, "replay-record-dot");
                uiDrawText("[REPLAY REC]", overlayX, 30.0f, 0.34f,
                           {1.0f, 0.12f, 0.12f, 1.0f});
                char replayTickText[64];
                snprintf(replayTickText, sizeof(replayTickText), "tick: %u",
                         gReplayRecorder.currentTick());
                uiDrawText(replayTickText, overlayX, 58.0f, 0.30f,
                           {1.0f, 0.12f, 0.12f, 1.0f});
            }
            if (replayPlaybackActive && !gReplayCinematicMode) {
                const float rOverlayX = uiScreenW() - 280.0f;
                const float rOverlayY = 20.0f;
                const auto* rFrame = gReplayPlayer.currentSceneFrame();
                const uint32_t totalTicks = gReplayPlayer.totalTicks();
                const float currentTime = gReplayPlayer.currentTick() / 60.0f;

                // REPLAY EXPORT UI FILTER: hide replay-only UI overlay during export
                if (!gReplayExportRenderMode) {
                const float totalTime = (float)totalTicks / 60.0f;
                const char* camMode = gReplayPlayer.cameraController().modeName();
                const bool paused = gReplayPlayer.isPaused();

                const float labelX = rOverlayX + 12.0f;
                const float valueX = rOverlayX + 100.0f;
                const float lineH = 20.0f;
                const float bgW = 250.0f;
                float bgH = lineH * 5.0f + 16.0f;

                uiDrawRect({rOverlayX, rOverlayY, bgW, bgH},
                           {0.0f, 0.0f, 0.0f, 0.70f}, "replay-hud-bg");

                float y = rOverlayY + 8.0f;
                uiDrawText("Viewing:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                if (rFrame && !rFrame->actors.empty())
                    uiDrawText(rFrame->actors[0].name.c_str(), valueX, y, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});
                y += lineH;

                uiDrawText("Camera:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                {
                    char camBuf[64];
                    snprintf(camBuf, sizeof(camBuf), "%s%s", camMode,
                             paused ? " [PAUSED]" : "");
                    uiDrawText(camBuf, valueX, y, 0.28f,
                               paused ? glm::vec4{1.0f, 0.9f, 0.3f, 1.0f}
                                      : glm::vec4{0.7f, 0.85f, 1.0f, 1.0f});
                }
                y += lineH;

                uiDrawText("Tick:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                {
                    char tickBuf[64];
                    snprintf(tickBuf, sizeof(tickBuf), "%u / %u",
                             (unsigned)gReplayPlayer.currentTick(), (unsigned)totalTicks);
                    uiDrawText(tickBuf, valueX, y, 0.28f, {0.9f, 0.9f, 0.3f, 1.0f});
                }
                y += lineH;

                uiDrawText("Time:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
                {
                    auto formatTime = [](float seconds) -> std::string {
                        int totalSec = (int)seconds;
                        int mins = totalSec / 60;
                        int secs = totalSec % 60;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                        return std::string(buf);
                    };
                    std::string curStr = formatTime(currentTime);
                    std::string totStr = formatTime(totalTime);
                    char timeBuf[64];
                    snprintf(timeBuf, sizeof(timeBuf), "%s / %s", curStr.c_str(), totStr.c_str());
                    uiDrawText(timeBuf, valueX, y, 0.28f, {0.9f, 0.9f, 0.3f, 1.0f});
                }
                y += lineH;

                if (totalTicks > 0) {
                    const float barX = rOverlayX + 8.0f;
                    const float barY = y + 4.0f;
                    const float barW = bgW - 16.0f;
                    const float barH = 6.0f;
                    uiDrawRect({barX, barY, barW, barH}, {0.3f, 0.3f, 0.3f, 0.7f}, "seek-bg");
                    float progress = std::clamp((float)gReplayPlayer.currentTick() / (float)std::max(totalTicks, 1u), 0.0f, 1.0f);
                    uiDrawRect({barX, barY, barW * progress, barH}, {0.9f, 0.9f, 0.3f, 0.9f}, "seek-fill");
                }
                } // end REPLAY EXPORT UI FILTER (hide replay-only overlay)

                // Crosshair for the first armed actor
                if (rFrame && !rFrame->actors.empty()) {
                    const ReplayActorState& primary = rFrame->actors[0];
                    if (!primary.weaponName.empty() && primary.weaponName != "none") {
                        updateCrosshairDynamic(
                            dt, glm::length(glm::vec2(primary.velocity)),
                            primary.grounded, false, primary.shooting);
                        drawCrosshair(uiScreenW() * 0.5f, uiScreenH() * 0.5f);
                        char ammoLine[48];
                        snprintf(ammoLine, sizeof(ammoLine), "%d / %d",
                                 primary.currentAmmo, primary.reserveAmmo);
                        const float ammoW = uiMeasureText(ammoLine, 0.34f);
                        uiDrawText(ammoLine, uiScreenW() * 0.5f - ammoW * 0.5f,
                                   uiScreenH() * 0.5f - 42.0f,
                                   0.34f, {1.0f, 0.82f, 0.3f, 1.0f});
                    }
                }
                // Healthbars for replay actors in the current scene frame only.
                // Iterating the full replayActorModels map would render healthbars
                // for dead actors whose entries were never cleaned up.
                if (const ReplaySceneFrame* hbFrame = gReplayPlayer.currentSceneFrame()) {
                    for (const ReplayActorState& actorState : hbFrame->actors) {
                        auto mit = replayActorModels.find(actorState.id);
                        if (mit == replayActorModels.end() || !mit->second || mit->second->dead) {
                            Debug::log(Debug::Category::Replay, "[HEALTHBAR] skipped render owner=%s dead=%d",
                                       actorState.id.c_str(), mit != replayActorModels.end() && mit->second ? mit->second->dead : 0);
                            continue;
                        }
                        drawPlayerHealthbar(*mit->second, camera, "replay-hp");
                    }
                }

                // REPLAY EXPORT UI FILTER: hide controls help during export
                if (!gReplayExportRenderMode) {
                // Replay controls help panel (bottom right)
                const float helpX = uiScreenW() - 220.0f;
                const float helpY = uiScreenH() - 140.0f;
                const float helpW = 200.0f;
                const float helpH = 110.0f;
                uiDrawRect({helpX, helpY, helpW, helpH},
                           {0.0f, 0.0f, 0.0f, 0.65f}, "replay-help-bg");
                float hy = helpY + 6.0f;
                uiDrawText("REPLAY CONTROLS", helpX + 8.0f, hy, 0.28f,
                           {0.9f, 0.9f, 0.3f, 1.0f}); hy += 18.0f;
                uiDrawText("SPACE    Pause/Resume", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("<-       Back 5s", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("->       Forward 5s", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("L        Cinematic", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
                uiDrawText("freecam  Free Camera", helpX + 8.0f, hy, 0.24f,
                           {0.8f, 0.8f, 1.0f, 1.0f});
                } // end REPLAY EXPORT UI FILTER (controls help)
            } else {
                if (weapons.getCurrentDef(player)) {
                    updateCrosshairDynamic(
                        dt, glm::length(glm::vec2(player.vel)), player.onGround,
                        player.didDash, weapons.isShooting());
                    drawCrosshair(uiScreenW() * 0.5f, uiScreenH() * 0.5f);
                }
            }
            // Spawn flash: hide GUI
            if (player.spawnFlashTimer > 0.0f)
            {
                Debug::logThrottled(Debug::Category::Audio, "spawnflash", 1.0f,
                    "[SPAWN FX] hiding GUI for spawn flash (timer=%.0f)\n", player.spawnFlashTimer);
            }
            else
            {
            hudText("playerName", player.username);
            char hpText[64];
            snprintf(hpText, sizeof(hpText), "HP: %d/%d", player.currentHp, player.maxHp);
            hudText("hpText", hpText);
            if (player.dead && gDuelManager.phase() != DuelPhase::MatchEnd) {
                if (!gReplayExportRenderMode || ReplayExportUI::showDeathScreen)
                {
                const float centerX = uiScreenW() * 0.5f;
                const float centerY = uiScreenH() * 0.5f;
                std::string deathText = "you died to " +
                    (player.killedBy.empty() ? std::string("unknown") : player.killedBy);
                char respawnText[128];
                snprintf(respawnText, sizeof(respawnText),
                         "respawning automatically in %.3f...", player.respawnTimer);
                uiDrawRect(
                    {centerX - 270.0f, centerY - 80.0f, 540.0f, 160.0f},
                    {0.0f, 0.0f, 0.0f, 0.75f},
                    "death-overlay");
                hudText("deathText", deathText);
                hudText("respawnText", respawnText);
                hudText("respawnHint", "press space to respawn instantly");
                }
            }
            if (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay)
            {
                glm::vec3 totalVel = player.vel;
                float speed = glm::length(totalVel);
                char spBuf[64];
                snprintf(spBuf, sizeof(spBuf), "Speed: %.2f m/s", speed);
                hudText("speedText", spBuf);
            }
            if (DebugConfig::DEBUG_PHYSICS && (!gReplayExportRenderMode || ReplayExportUI::showSpeedDisplay))
            {
                char dbg[512];
                int y = 200;
                snprintf(dbg, sizeof(dbg),
                    "gnd:%d stb:%d raw:%d realC:%d wc:%d pos:(%.1f %.1f %.1f) vel.z:%.1f",
                    (int)player.onGround, (int)player.stableOnGround,                     (int)player.wasOnGround,
                    (int)player.realWorldContactThisFrame, (int)player.hasWorldContact,
                    player.pos.x, player.pos.y, player.pos.z,
                    player.vel.z);
                uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f}); y += 16;
                snprintf(dbg, sizeof(dbg),
                    "jmpI:%.2f aj:%d da:%d grAv:%d ddAv:%d jc:%d",
                    player.jumpIntentTimer, player.airJumpsLeft,
                    (int)player.dashAvailable, (int)player.groundReturnAvailable,
                    (int)player.downDashAvailable, (int)player.jumpConsumed);
                uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f}); y += 16;
                float feetZ = player.pos.z - PLAYER_HEIGHT * 0.5f;
                snprintf(dbg, sizeof(dbg),
                    "feetZ:%.2f landCD:%.2f fzAv:%d",
                    feetZ, player.landingCooldown, (int)player.freezeAvailable);
                uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f}); y += 16;
                snprintf(dbg, sizeof(dbg),
                    "didLand:%d wasGnd:%d stbGnd:%d gLost:%.2f airT:%.2f",
                    (int)player.didLand, (int)player.wasOnGround,
                    (int)player.stableOnGround, player.groundLostTimer, player.airborneTimer);
                uiDrawText(dbg, 24.0f, (float)y, 0.28f, {0.3f, 1.0f, 0.6f, 1.0f});
            }
            if (!gReplayExportRenderMode || ReplayExportUI::showModeText)
            {
                char modeText[128];
                snprintf(modeText, sizeof(modeText), "%s | %s | slot %d",
                         editorMode ? "EDITOR" : "PLAYING", activeGameMode.c_str(), player.equippedSlot);
                hudText("modeText", modeText);
                // Multiplayer HUD
                if (mpContext.active) {
                    char mpText[128];
                    snprintf(mpText, sizeof(mpText), "MP id=%u players=%zu server=%s",
                             mpContext.localPlayerId,
                             mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1 : 0),
                             mpContext.serverAddress.c_str());
                    uiDrawText(mpText, 24, 232, 0.32f, {0.7f, 0.9f, 1.0f, 1.0f});
                }
                {
                    const WeaponDefinition* curDef = nullptr;
                    for (const auto& pair : WeaponRegistry::instance().all()) {
                        if (pair.second.slot == player.equippedSlot) {
                            curDef = &pair.second;
                            break;
                        }
                    }
                    if (curDef) {
                        auto it = player.weaponRuntimes.find(curDef->id);
                        if (it != player.weaponRuntimes.end()) {
                            const WeaponRuntime& rt = it->second;
                            char ammoText[96];
                            int displayReserve = std::max(0, rt.reserveAmmo);
                            snprintf(ammoText, sizeof(ammoText), "%s: %d / %d",
                                     curDef->displayName.c_str(),
                                     rt.currentAmmo, displayReserve);
                            hudText("ammoText", ammoText);

                            if (rt.isReloading) {
                                char reloadText[96];
                                snprintf(reloadText, sizeof(reloadText),
                                         "no bullets! reloading... %.2f",
                                         std::max(0.0f, rt.reloadTimer));
                                hudText("reloadText", reloadText);
                            }
                        }
                    }
                }
                if (player.inventoryOpen)
                    uiDrawText("INVENTORY: [1] Revolver [2-10] Empty", 24, 260, 0.36f, {0.9f,0.9f,1.0f,1.0f});
            }
            {
                const float normalSize = 44.0f;
                const float gap = 7.0f;
                const float totalWidth = normalSize * 10.0f + gap * 9.0f;
                float x = uiScreenW() * 0.5f - totalWidth * 0.5f;
                float y = uiScreenH() - 70.0f;
                for (int slot = 1; slot <= 10; ++slot) {
                    bool equipped = player.equippedSlot == slot;
                    float size = equipped ? normalSize * 1.2f : normalSize;
                    float offset = (size - normalSize) * 0.5f;
                    UIRect rect{x - offset, y - offset, size, size};
                    uiDrawRect(rect, slot == 1 ? glm::vec4(0.32f,0.32f,0.36f,0.95f)
                                               : glm::vec4(0.12f,0.12f,0.14f,0.92f), "hotbar-slot");
                    uiDrawRectOutline(rect, equipped ? glm::vec4(1,1,1,1)
                                                     : glm::vec4(0.45f,0.45f,0.48f,1), "hotbar-border");
                    std::string label = slot == 10 ? "0" : std::to_string(slot);
                    uiDrawText(label.c_str(), rect.x + 5, rect.y + 16, 0.30f, {1,1,1,1});
                    // Show weapon name for this slot
                    const WeaponDefinition* slotDef = nullptr;
                    for (const auto& pair : WeaponRegistry::instance().all()) {
                        if (pair.second.slot == slot) {
                            slotDef = &pair.second;
                            break;
                        }
                    }
                    if (slotDef) {
                        std::string shortName = slotDef->id.substr(0, 3);
                        std::transform(shortName.begin(), shortName.end(), shortName.begin(), ::toupper);
                        uiDrawText(shortName.c_str(), rect.x + 13, rect.y + 34, 0.20f,
                                   equipped ? glm::vec4(1,0.85f,0.35f,1) : glm::vec4(0.55f,0.55f,0.58f,1));
                    } else {
                        uiDrawText("-", rect.x + 13, rect.y + 34, 0.20f, glm::vec4(0.55f,0.55f,0.58f,1));
                    }
                    x += normalSize + gap;
                }
            }
            // Live entity healthbars: only during live gameplay, NOT during
            // replay/export. During replay/export, the replay actor healthbars
            // are rendered separately (see above). The live player and NPCs
            // exist in the world but their models are not rendered; rendering
            // their healthbars would produce orphaned floating bars.
            if (!replayPlaybackActive) {
            {
                float nameX = 0.0f, nameY = 0.0f;
                if (DebugVis::projectToScreen(camera, player.pos + glm::vec3(0,0,PLAYER_HEIGHT * 0.7f),
                                              nameX, nameY)) {
                    float ratio = player.maxHp > 0 ? (float)player.currentHp / player.maxHp : 0.0f;
                    uiDrawRect({nameX - 70, nameY - 8, 140, 12}, {0.55f,0.05f,0.05f,0.95f}, "self-hp-bg");
                    uiDrawRect({nameX - 70, nameY - 8, 140 * ratio, 12}, {0.05f,0.8f,0.15f,0.95f}, "self-hp-current");
                    uiDrawText(player.username.c_str(), nameX - 35, nameY - 32, 0.32f, {1,1,1,1});
                    uiDrawText(hpText, nameX - 35, nameY + 8, 0.28f, {1,1,1,1});
                }
            }
                for (const Npc& npc : npcSystem.all()) {
                    if (npc.body.dead) {
                        Debug::log(Debug::Category::Replay, "[HEALTHBAR] skipped render owner=%s dead=1", npc.body.username.c_str());
                        continue;
                    }
                    drawPlayerHealthbar(npc.body, camera, "npc-hp");
                }
            }

            renderChatBubbles(player.chatState, player, camera);
            if (!replayPlaybackActive)
            {
                for (auto& kv : mpContext.remotePlayers)
                    renderChatBubbles(kv.second.chatState, kv.second, camera);
            }
            else
            {
                for (const auto& kv : gReplayChatStates)
                {
                    auto actorIt = replayActorModels.find(kv.first);
                    if (actorIt != replayActorModels.end() && actorIt->second)
                        renderChatBubbles(kv.second, *actorIt->second, camera);
                }
            }

            if (mpContext.active)
            {
                static uint64_t lastHealthbarLogMs = 0;
                const uint64_t healthbarNowMs = MimitaNet::nowMs();
                const bool logHealthbars =
                    mpContext.showDebugOverlay &&
                    healthbarNowMs - lastHealthbarLogMs >= 1000;

                for (const auto& kv : mpContext.remotePlayers)
                {
                    const HealthbarRenderResult result =
                        drawPlayerHealthbar(
                            kv.second, camera, "network-player-hp");
                    if (logHealthbars)
                    {
                        printf(
                            "[NET HEALTHBAR] entityId=%u owner=remote "
                            "health=%d/%d anchor=%s "
                            "world=(%.2f %.2f %.2f) screen=(%.1f %.1f) "
                            "distance=%.1f rendered=%d cull=%s\n",
                            kv.first,
                            kv.second.currentHp, kv.second.maxHp,
                            result.usedHeadTransform ? "head" : "fallback",
                            result.anchor.x, result.anchor.y, result.anchor.z,
                            result.screen.x, result.screen.y,
                            result.distance, (int)result.rendered,
                            healthbarCullReasonName(result.cullReason));
                    }
                }

                if (logHealthbars)
                    lastHealthbarLogMs = healthbarNowMs;
            }
            {
                static float hpLogTimer = 0.0f; hpLogTimer -= 0.016f;
                if (hpLogTimer <= 0.0f && !npcSystem.all().empty()) {
                    hpLogTimer = 1.0f;
                    printf("[PLAYER HP FRAME] hp=%d/%d pos=(%.1f %.1f %.1f)\n",
                           player.currentHp, player.maxHp,
                           player.pos.x, player.pos.y, player.pos.z);
                }
            }
            if (!gReplayExportRenderMode || ReplayExportUI::showNpcDebug)
            {
                char npcText[96];
                snprintf(npcText, sizeof(npcText), "NPCs: %zu", npcSystem.all().size());
                uiDrawText(npcText, 24, 168, 0.32f, {1.0f, 0.82f, 0.38f, 1.0f});
                if (!npcSystem.all().empty()) {
                    const Npc& first = npcSystem.all().front();
                    char tuneText[256];
                    snprintf(tuneText, sizeof(tuneText),
                             "  Diff=%.0f aimErr=%.1fdeg reaction=%.2fs moveVar=%.2f",
                             first.difficulty,
                             NpcCombat::aimErrorDegrees(first.difficulty),
                             first.tuning.reactionDelay,
                             first.tuning.movementPrecision);
                    uiDrawText(tuneText, 24, 184, 0.28f, {0.8f, 0.9f, 1.0f, 1.0f});
                }
            }
            if (DebugVis::render() && (!gReplayExportRenderMode || ReplayExportUI::showDebugVis))
            {
                char dbg[256];
                snprintf(dbg, sizeof(dbg), "dt %.3f grounded %d vel %.2f %.2f %.2f cam %.1f %.1f %.1f",
                         dt, (int)player.onGround, player.vel.x, player.vel.y, player.vel.z,
                         camera.pos.x, camera.pos.y, camera.pos.z);
                uiDrawText(dbg, 24, 184, 0.30f, {1.0f, 0.9f, 0.45f, 1.0f});
            }

            // Duel state onscreen debug overlay (always visible during MatchEnd)
            if (gDuelManager.phase() == DuelPhase::MatchEnd &&
                (!gReplayExportRenderMode || ReplayExportUI::showDuelDebug))
            {
                const char* stateName = "None";
                switch (gDuelManager.endState()) {
                case DuelEndState::None:          stateName = "None"; break;
                case DuelEndState::VictoryScreen: stateName = "VictoryScreen"; break;
                case DuelEndState::Countdown:     stateName = "Countdown"; break;
                case DuelEndState::FinalKillReplay: stateName = "FinalKillReplay"; break;
                case DuelEndState::ReplayMenu:    stateName = "ReplayMenu"; break;
                }
                float y = 20.0f;
                char buf[128];
                snprintf(buf, sizeof(buf), "DUEL STATE: %s", stateName);
                uiDrawText(buf, 24, y, 0.35f, {0.3f, 1.0f, 0.3f, 1.0f}); y += 20.0f;
                snprintf(buf, sizeof(buf), "ReplayReady: %d", (int)gDuelManager.isReplayReady());
                uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
                snprintf(buf, sizeof(buf), "ReplayLoaded: %s", gReplayPlayer.totalTicks() > 0 ? "YES" : "NO");
                uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
                snprintf(buf, sizeof(buf), "ReplayPlaying: %d", (int)gReplayPlayer.isPlaying());
                uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
                snprintf(buf, sizeof(buf), "CurrentReplayTick: %u/%u", gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
                uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
                const ReplayExportJob& job = getReplayExportJob();
                const char* exportState = "Idle";
                switch (job.state) {
                case ReplayExportJob::Idle:      exportState = "Idle"; break;
                case ReplayExportJob::Capturing: exportState = "Capturing"; break;
                case ReplayExportJob::Encoding:  exportState = "Encoding"; break;
                case ReplayExportJob::Done:      exportState = "Done"; break;
                case ReplayExportJob::Failed:    exportState = "Failed"; break;
                }
                snprintf(buf, sizeof(buf), "ExportInProgress: %s", exportState);
                uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
                snprintf(buf, sizeof(buf), "ReplayPath: %s", gDuelManager.finalKillReplayPath.c_str());
                uiDrawText(buf, 24, y, 0.30f, {0.8f, 0.8f, 0.8f, 1.0f});
            }
            // Apply slow-motion during final kill replay
            if (gDuelManager.endState() == DuelEndState::FinalKillReplay) {
                float elapsed = gReplayPlayer.totalTicks() > 0
                    ? (float)gReplayPlayer.currentTick() / 60.0f
                    : 0.0f;
                float factor = 1.0f;
                if (elapsed < 2.0f) {
                    factor = 0.15f;
                } else if (elapsed < 3.5f) {
                    float p = (elapsed - 2.0f) / 1.5f;
                    factor = 0.15f + p * 0.85f;
                }
                gReplayPlayer.setTimescale(factor);
            }

            if (gDuelManager.phase() == DuelPhase::MatchEnd) {
                DuelMenuAction action = gDuelManager.renderMatchOverScreen(engine.window());
                if (action == DuelMenuAction::PlayAgain) {
                    Debug::log(Debug::Category::Duel, "[DUEL] restarting same settings");
                    gDuelManager.restartDuel(player, npcSystem, world);
                } else if (action == DuelMenuAction::ExitToMenu) {
                    Debug::log(Debug::Category::Duel, "[DUEL] Exit To Main Menu clicked");
                    Debug::log(Debug::Category::Duel, "[DUEL] Requesting menu transition");
                    Debug::log(Debug::Category::Duel, "[DUEL] Leaving duel");
                    Debug::log(Debug::Category::Duel, "[DUEL] old state=GAME_PLAYING new state=GAME_MENU");
                    Debug::log(Debug::Category::Duel, "[DUEL] stopping replay playback");
                    gReplayPlayer.stopPlayback();
                    Debug::log(Debug::Category::Duel, "[DUEL] stopping replay recording");
                    if (gReplayRecorder.isRecording())
                        gReplayRecorder.stopRecording();
                    Debug::log(Debug::Category::Duel, "[DUEL] clearing duel state");
                    gDuelManager.stopDuel();
                    gBombTagManager.stop();
                    Debug::log(Debug::Category::Duel, "[DUEL] destroying NPCs");
                    npcSystem.destroyAll();
                    Debug::log(Debug::Category::Duel, "[DUEL] forcing cursor normal");
                    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    Debug::log(Debug::Category::Duel, "[DUEL] Loading main menu");
                    gameState = GAME_MENU;
                    Debug::log(Debug::Category::Duel, "[DUEL EXIT] transition complete");
                } else if (action == DuelMenuAction::SaveReplay) {
                    Debug::log(Debug::Category::Duel, "[EXPORT] button pressed");
                    Debug::log(Debug::Category::Duel, "[EXPORT] finalKillReplayPath=%s replayLoaded=%s replayPlaying=%s",
                               gDuelManager.finalKillReplayPath.c_str(),
                               gReplayPlayer.totalTicks() > 0 ? "YES" : "NO",
                               gReplayPlayer.isPlaying() ? "YES" : "NO");
                    if (!gDuelManager.finalKillReplayPath.empty()) {
                        std::string jsonPath = gDuelManager.finalKillReplayPath;
                        Debug::log(Debug::Category::Duel, "[EXPORT] source replay path=%s", jsonPath.c_str());
                        int rw = engine.renderer ? engine.renderer->width : 1280;
                        int rh = engine.renderer ? engine.renderer->height : 720;
                        Debug::log(Debug::Category::Duel, "[EXPORT] output mp4 path will be generated by startReplayExport");
                        if (startReplayExport(jsonPath, rw, rh)) {
                            DevOverlay::instance().showNotification("Exporting replay...", 2.0f);
                        } else {
                            Debug::log(Debug::Category::Duel, "[EXPORT] startReplayExport returned FALSE");
                            const ReplayExportJob& job = getReplayExportJob();
                            Debug::log(Debug::Category::Duel, "[EXPORT] job state=%d error=%s",
                                       (int)job.state, job.errorMsg.c_str());
                        }
                    } else {
                        DevOverlay::instance().showNotification("Replay not ready yet. Wait for replay to load.", 5.0f);
                        Debug::log(Debug::Category::Duel, "[EXPORT] finalKillReplayPath EMPTY - replay clip not saved yet");
                    }
                }
            } else if (gBombTagManager.phase() == BombTagPhase::MatchEnd) {
                BombTagMenuAction btAction = gBombTagManager.renderMatchOverScreen(engine.window());
                if (btAction == BombTagMenuAction::PlayAgain) {
                    BombTagConfig cfg;
                    cfg.numNpcs = 3;
                    cfg.npcDifficulty = 5.0f;
                    cfg.lives = 0;
                    cfg.timeLimitSeconds = 180;
                    cfg.enabled = true;
                    cfg.mapPath = activeMapPath;
                    npcSystem.destroyAll();
                    gBombTagManager.setCamera(camera);
                    gBombTagManager.start(cfg, player, npcSystem, world);
                } else if (btAction == BombTagMenuAction::ExitToMenu) {
                    gReplayPlayer.stopPlayback();
                    if (gReplayRecorder.isRecording())
                        gReplayRecorder.stopRecording();
                    gBombTagManager.stop();
                    gDuelManager.stopDuel();
                    npcSystem.destroyAll();
                    glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    gameState = GAME_MENU;
                }
            } else {
                if (gDuelManager.enabled())
                    gDuelManager.renderHud();
                if (gBombTagManager.enabled())
                    gBombTagManager.renderHud();
            }

            // TAB Player List overlay
            if (mpContext.active && mpContext.showPlayerList)
            {
                float listX = uiScreenW() * 0.5f - 160.0f;
                float listY = uiScreenH() * 0.25f;
                float listW = 320.0f;
                float lineH = 24.0f;
                float headerH = 30.0f;

                // Count all players (local + remote)
                size_t totalPlayers = mpContext.playerRegistry.size();
                float listH = headerH + (totalPlayers + 1) * lineH + 10.0f;

                uiDrawRect({listX, listY, listW, listH}, {0.0f, 0.0f, 0.0f, 0.85f}, "player-list-bg");
                uiDrawRectOutline({listX, listY, listW, listH}, {0.5f, 0.6f, 0.8f, 1.0f}, "player-list-border");

                float y = listY + 8.0f;
                uiDrawText("PLAYERS", listX + 10.0f, y, 0.36f, {0.8f, 0.9f, 1.0f, 1.0f});
                y += headerH;
                uiDrawText("ID   NAME                         PING",
                           listX + 10.0f, y, 0.28f, {0.65f, 0.75f, 0.9f, 1.0f});
                y += lineH;

                // Local player
                if (mpContext.localPlayerId)
                {
                    const char* localName = player.username.empty() ? "you" : player.username.c_str();
                    char localLine[128];
                    snprintf(localLine, sizeof(localLine), "%u   %s   %dms (you)",
                             mpContext.localPlayerId, localName, mpContext.localPingMs);
                    uiDrawText(localLine, listX + 10.0f, y, 0.32f, {0.3f, 1.0f, 0.4f, 1.0f});
                    y += lineH;
                }

                // Remote players
                for (const auto& kv : mpContext.playerRegistry)
                {
                    if (kv.first == mpContext.localPlayerId)
                        continue;
                    const char* pname = kv.second.name.c_str();
                    char remoteLine[128];
                    snprintf(remoteLine, sizeof(remoteLine), "%u  %s  %dms",
                             kv.first, pname, kv.second.pingMs);
                    uiDrawText(remoteLine, listX + 10.0f, y, 0.32f, {0.9f, 0.95f, 1.0f, 1.0f});
                    y += lineH;
                }
            }

            // F3 Networking Debug Overlay
            if (mpContext.active && mpContext.showDebugOverlay)
            {
                float dbgX = uiScreenW() - 360.0f;
                float dbgY = 20.0f;
                float lineH = 18.0f;
                float dbgW = 340.0f;
                float dbgH = (13.0f + (float)mpContext.remotePlayers.size()) * lineH + 10.0f;

                uiDrawRect({dbgX, dbgY, dbgW, dbgH}, {0.0f, 0.0f, 0.0f, 0.8f}, "net-debug-bg");

                float y = dbgY + 6.0f;
                char buf[256];

                snprintf(buf, sizeof(buf), "STATUS: %s", mpContext.connectionStatus.c_str());
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                           mpContext.connected ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f)
                                               : glm::vec4(1.0f, 0.55f, 0.2f, 1.0f));
                y += lineH;

                snprintf(buf, sizeof(buf), "LOCAL PLAYER ID: %u", mpContext.localPlayerId);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.3f, 1.0f, 0.4f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "PING: %dms  FAKELAG: mode %d delay %dms queue %zu",
                         mpContext.localPingMs,
                         mpContext.fakeLagMode,
                         mpContext.fakeLagMode == 1
                            ? mpContext.fakeLagCurrentMs
                            : mpContext.fakeLagStaticMs,
                         mpContext.outgoingQueue.size());
                uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.75f, 0.85f, 1.0f, 1.0f});
                y += lineH;

                snprintf(buf, sizeof(buf), "ENTITIES: %zu (PLAYERS %zu / NPCS %zu)",
                         mpContext.remotePlayers.size() + mpContext.remoteNpcs.size() +
                             (mpContext.localPlayerId ? 1u : 0u),
                         mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1u : 0u),
                         mpContext.remoteNpcs.size());
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "TICK CLIENT %u / SERVER %llu",
                         mpContext.tick, (unsigned long long)mpContext.lastSnapshotTick);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

                const uint64_t snapshotAge = mpContext.lastSnapshotReceivedMs
                    ? MimitaNet::nowMs() - mpContext.lastSnapshotReceivedMs
                    : 0;
                snprintf(buf, sizeof(buf), "SNAPSHOT AGE: %llums  INTERP: 100ms",
                         (unsigned long long)snapshotAge);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

                const uint64_t snapshotTotal =
                    mpContext.snapshotsReceived + mpContext.snapshotsMissed;
                const float lossPercent = snapshotTotal
                    ? 100.0f * (float)mpContext.snapshotsMissed / (float)snapshotTotal
                    : 0.0f;
                snprintf(buf, sizeof(buf), "SNAPSHOT LOSS: %.1f%% (%llu missed)",
                         lossPercent, (unsigned long long)mpContext.snapshotsMissed);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "PACKETS TX/RX: %llu / %llu",
                         (unsigned long long)mpContext.packetsSent,
                         (unsigned long long)mpContext.packetsReceived);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "SERVER: %s", mpContext.serverAddress.c_str());
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.75f, 0.85f, 1.0f}); y += lineH;

                snprintf(buf, sizeof(buf), "LOCAL POS: %.1f %.1f %.1f HP=%d",
                         player.pos.x, player.pos.y, player.pos.z,
                         mpContext.localServerHealth);
                uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                           {0.35f, 1.0f, 0.45f, 1.0f});
                y += lineH;

                if (mpContext.hasLocalServerPosition)
                {
                    snprintf(buf, sizeof(buf), "SERVER POS ERROR: %.2fm",
                             glm::length(player.pos - mpContext.localServerPosition));
                    uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                               {1.0f, 0.35f, 0.25f, 1.0f});
                }
                y += lineH;

                for (const auto& kv : mpContext.remotePlayers)
                {
                    const Player& rp = kv.second;
                    auto nameIt = mpContext.playerRegistry.find(kv.first);
                    const char* rname = (nameIt != mpContext.playerRegistry.end()) ? nameIt->second.name.c_str() : "?";
                    snprintf(buf, sizeof(buf), "  %s id=%u pos=(%.1f,%.1f,%.1f)",
                             rname, kv.first, rp.pos.x, rp.pos.y, rp.pos.z);
                    uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.6f, 0.85f, 1.0f, 1.0f});
                    y += lineH;
                }
            }

            // REPLAY EXPORT UI FILTER: hide browser, timeline, export overlay
            if (!gReplayExportRenderMode) {
            // Replay Browser overlay (rendered on top of everything)
            gReplayBrowser.draw();

            // Replay Timeline during playback
            if (replayPlaybackActive) {
                if (const ReplaySceneFrame* rFrame = gReplayPlayer.currentSceneFrame()) {
                    gReplayTimeline.draw(gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
                }
            }

            // Replay export overlay
            if (isReplayExportActive())
            {
                float ex = uiScreenW() * 0.5f - 200.0f;
                float ey = uiScreenH() * 0.7f;
                float ew = 400.0f;
                float eh = 80.0f;
                uiDrawRect({ex, ey, ew, eh}, {0.0f, 0.0f, 0.0f, 0.8f}, "export-bg");
                std::string status = getReplayExportStatusText();
                uiDrawText(status.c_str(), ex + 10.0f, ey + 8.0f, 0.32f, {0.3f, 1.0f, 0.5f, 1.0f});
                // Progress bar
                float p = getReplayExportProgress();
                uiDrawRect({ex + 10.0f, ey + eh - 16.0f, (ew - 20.0f) * p, 10.0f},
                           {0.3f, 1.0f, 0.3f, 1.0f}, "export-progress");
            }
            // Replay export completed popup
            {
                static bool exportPopupShown = false;
                const ReplayExportJob& job = getReplayExportJob();
                if (job.state == ReplayExportJob::Done && !exportPopupShown) {
                    exportPopupShown = true;
                    std::string result = getReplayExportStatusText();
                    DevOverlay::instance().showNotification(result, 8.0f);
                    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Export success popup shown");
                }
                if (job.state == ReplayExportJob::Failed && !exportPopupShown) {
                    exportPopupShown = true;
                    std::string result = getReplayExportStatusText();
                    DevOverlay::instance().showNotification(result, 8.0f);
                    Debug::log(Debug::Category::Replay, "[REPLAY SAVE] Export failed popup shown");
                }
                if (job.state == ReplayExportJob::Idle)
                    exportPopupShown = false;
            }
            } // end REPLAY EXPORT UI FILTER (browser, timeline, export overlay)
            MusicManager::instance().drawAllOverlay();
            if (gFramePacer.showFPS() && (!gReplayExportRenderMode || ReplayExportUI::showFps))
            {
                uiDrawText(gFramePacer.fpsText(), 12.0f, 12.0f, 0.36f,
                           {0.3f, 1.0f, 0.5f, 1.0f});
                if (gFramePacer.frameDebug())
                {
                    uiDrawText(gFramePacer.debugText(), 12.0f, 38.0f, 0.30f,
                               {0.5f, 0.8f, 1.0f, 1.0f});
                }
            }
            if (PostFX::instance().debugEnabled && (!gReplayExportRenderMode || ReplayExportUI::showPostFxDebug))
            {
                const char* txt = PostFX::instance().debugText();
                if (txt && txt[0])
                    uiDrawText(txt, uiScreenW() - 380.0f, 12.0f, 0.28f,
                               {1.0f, 0.8f, 0.2f, 1.0f});
            }
            if (ShadowConfig::instance().data().debugDrawShadowFrustum && (!gReplayExportRenderMode || ReplayExportUI::showShadowDebug))
            {
                const auto& sd = ShadowConfig::instance().data();
                char buf[512];
                glm::vec3 dir = LightingConfig::instance().lightDir();
                snprintf(buf, sizeof(buf),
                    "SHADOWS\nenabled: %s\nmapSize: %d\ndistance: %.0f\nbias: %.4f\ndarkness: %.2f\nsoftness: %.1f\n\nsunDir:\n%.2f\n%.2f\n%.2f",
                    sd.enabled ? "yes" : "no",
                    sd.shadowMapSize,
                    sd.shadowDistance,
                    sd.shadowBias,
                    sd.shadowDarkness,
                    sd.shadowSoftness,
                    dir.x, dir.y, dir.z);
                uiDrawText(buf, uiScreenW() - 280.0f, 120.0f, 0.26f,
                           {1.0f, 0.9f, 0.4f, 1.0f});
            }
            } // end spawn flash GUI hide else

            // Cleanup pass: remove replay actor models not present in the current
            // scene frame. These are stale entries (dead entities removed from the
            // actor list) that would otherwise render orphaned healthbars.
            if (gReplayPlayer.totalTicks() > 0) {
                const ReplaySceneFrame* cleanupFrame = gReplayPlayer.currentSceneFrame();
                std::vector<std::string> toRemove;
                for (const auto& kv : replayActorModels) {
                    if (!cleanupFrame ||
                        std::none_of(cleanupFrame->actors.begin(),
                                     cleanupFrame->actors.end(),
                                     [&](const ReplayActorState& a) { return a.id == kv.first; })) {
                        Debug::log(Debug::Category::Replay, "[HEALTHBAR] destroyed owner=%s reason=stale",
                                   kv.first.c_str());
                        toRemove.push_back(kv.first);
                    }
                }
                for (const std::string& id : toRemove)
                    replayActorModels.erase(id);
            }

            // Update perf state counters
            Perf::state().npcCount = (int)npcSystem.all().size();
            if (Perf::state().npcCount > Perf::state().peakNpcCount)
                Perf::state().peakNpcCount = (float)Perf::state().npcCount;
            Perf::state().playerCount = 1;
            Perf::state().bloodCount = EffectPartSystem::instance().activeCount();
            Perf::state().particleCount = EffectPartSystem::instance().activeCount();
            Perf::state().effectCount = (int)EffectPartSystem::instance().activeCount();
            Perf::state().corpseCount = (int)DeathSystem::instance().corpses().size();
            if (gReplayPlayer.isPlaying())
                Perf::state().replayMemoryMb = (double)gReplayPlayer.totalTicks() * sizeof(ReplaySceneFrame) / (1024.0 * 1024.0);
            if (!gReplayExportRenderMode || ReplayExportUI::showPerfOverlay)
                Perf::renderOverlay();
            if (!gReplayExportRenderMode || ReplayExportUI::showPerfOverlay)
                uiRenderFrameDebugOverlay(engine.window(), "PLAYING", worldPassRan);
            uiEndFrame();

            // GUI editor for HUD elements
            if (GuiEditor::instance().isEnabled()) {
                GuiEditor::instance().setActiveLayout("config/gui/hud.json");
                GuiEditor::instance().update(engine.window());
            }
            } // Perf::ScopedTimer UI

            // Dev overlay notifications (temporary)
            if (!gReplayExportRenderMode || ReplayExportUI::showDevOverlay)
                DevOverlay::instance().render();
        }

        // Advance GUI media animations (GIF frames, future video)
        uiUpdateMedia(dt);

        // Skip menu rendering during replay export so glReadPixels captures
        // the replay scene (rendered in GAME_PLAYING block above) instead of
        // the menu UI.
        if (gameState == GAME_MENU && !isReplayExportActive())
        {
            guiMain(engine.window(), gameState);
        }

        static bool escapePrev = false;
        bool escapeDown = glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escapeDown && !escapePrev)
        {
            if (Terminal::instance().isOpen()) {
                Terminal::instance().toggle();
                bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
                glfwSetInputMode(engine.window(), GLFW_CURSOR,
                    gameState == GAME_PLAYING && !duelMatchOver ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            } else if (gReplayPlayer.isPlaying()) {
                // Watching replay -> ReplayMenu
                printf("[MAINMENU] cleaning replay\n");
                gReplayPlayer.stopPlayback();
                replayActorModels.clear();
                replayWeaponModels.clear();
                gReplayChatStates.clear();
                printf("[MAINMENU] switching to replay menu\n");
                gGuiMenuState = GUI_MENU_REPLAY;
                gameState = GAME_MENU;
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else if (gameState == GAME_MENU && gGuiMenuState == GUI_MENU_REPLAY) {
                // ReplayMenu -> MainMenu
                printf("[MAINMENU] switching to main menu\n");
                gGuiMenuState = GUI_MENU_MAIN;
            } else {
                // Clean up duel state if match was in progress
                if (gDuelManager.phase() != DuelPhase::Off) {
                    Debug::log(Debug::Category::Duel, "[DUEL] escape pressed during duel (phase=%d) — cleaning up", (int)gDuelManager.phase());
                    gReplayPlayer.stopPlayback();
                    if (gReplayRecorder.isRecording())
                        gReplayRecorder.stopRecording();
                    gDuelManager.stopDuel();
                    npcSystem.destroyAll();
                }
                // Disconnect from multiplayer if active
                if (mpContext.active) {
                    MimitaNet::mpShutdown(mpContext);
                }
                Debug::log(Debug::Category::Duel, "[DUEL] escape: transitioning to main menu");
                gameState = GAME_MENU;
                glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        escapePrev = escapeDown;

        // F10: Open replays folder
        static bool f10Prev = false;
        bool f10Down = glfwGetKey(engine.window(), GLFW_KEY_F10) == GLFW_PRESS;
        if (f10Down && !f10Prev) {
            ShellExecuteA(NULL, "open", "replays", NULL, NULL, SW_SHOWNORMAL);
            Debug::log(Debug::Category::General, "[MAIN] opened replays folder");
        }
        f10Prev = f10Down;

        // F12: Emergency main menu shortcut
        static bool f12Prev = false;
        bool f12Down = glfwGetKey(engine.window(), GLFW_KEY_F12) == GLFW_PRESS;
        if (f12Down && !f12Prev && !Terminal::instance().isOpen()) {
            Debug::log(Debug::Category::General, "[MAINMENU] F12 pressed — forcing main menu");
            forceMainMenu();
            DevOverlay::instance().showNotification("Returned to Main Menu (F12)", 3.0f);
            Terminal::instance().addLog("[MAINMENU] triggered via F12 key");
        }
        f12Prev = f12Down;

        // Failsafe: if in menu but duel still active, force cleanup
        if (gameState == GAME_MENU && gDuelManager.phase() != DuelPhase::Off) {
            Debug::log(Debug::Category::Duel, "[DUEL FAILSAFE] gameState=MENU but duel phase=%d — forcing cleanup", (int)gDuelManager.phase());
            forceMainMenu();
        }

        // [E] Step 3: glReadPixels capture (after ALL rendering, before terminal overlay)
        if (isReplayExportActive()) {
            static bool loggedOnce = false;
            if (!loggedOnce) {
                Debug::log(Debug::Category::Replay, "[EXPORT DEBUG] render order: 3.glReadPixels (capture)");
                loggedOnce = true;
            }
            updateReplayExport();
        }

        // Terminal rendering (on top of everything)
        // 6 14 2026 yes absolutely render terminal on top of everything
        // terminal is the task manager escape if we have crashes etc 
        Terminal::instance().render();
        diagRenderStage(8);

        engine.endFrame();
        diagRenderStage(9);
        diagRenderFrameEnd();
        Perf::endFrame();
        gFramePacer.endFrame();

    }



}
