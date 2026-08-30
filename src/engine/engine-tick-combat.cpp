// 07 21 2026, 21 30
/* purpose
* Runs frame-level combat, weapon, duel, replay, and gameplay input orchestration.
* Bridges hotkeys and mouse actions into shared terminal/weapon/network command paths.
* Keeps online weapon actions on generic request helpers after local prediction runs.
* Does NOT own server damage authority, network packet dispatch, or transport sockets.
* Does NOT implement weapon JSON parsing, collision solvers, or projectile server simulation.
* Does NOT launch executables, build assets, or manage deployment.
*/

#include "engine/engine-tick-combat.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include "gui/hud/chat-window.h"
#include <cstdio>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "world/world.h"
#include "input/input-commands.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "combat/weapon-system.h"
#include "combat/weapon-types.h"
#include "combat/weapon-rocket-launcher.h"
#include "combat/death-system.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "replay/replay.h"
#include "replay/replay-editor.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/death-ghost.h"
#include "gui/hud/chat-bubble.h"
#include "game/duel.h"
#include "duel/duel-queue.h"
#include "game/bomb-tag.h"
#include "game/game-state.h"
#include "world/world.h"
#include "render/post-fx.h"
#include "physics/ray-utils.h"
#include "pobjects/persistent-physics.h"
#include "gui/menus/pause-menu.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern bool gReplayCinematicMode;

// ── Client fixed-step prediction accumulator ─────────────────────────
// Runs input consumption, projectile prediction, and cooldowns at 60 Hz
// independent of render FPS.
static double g_clientPredictionAccumulator = 0.0;
static uint32_t g_clientSimulationTick = 0;

void engineTickCombat(Engine& engine, float dt)
{
    // Fixed-step client prediction loop
    g_clientPredictionAccumulator += (double)dt;
    constexpr double kClientFixedDt = 1.0 / 60.0;
    constexpr int kMaxClientSteps = 5;
    int clientSteps = 0;
    while (g_clientPredictionAccumulator >= kClientFixedDt && clientSteps < kMaxClientSteps)
    {
        g_clientPredictionAccumulator -= kClientFixedDt;
        ++g_clientSimulationTick;
        ++clientSteps;

        // TODO: consume input buffer, advance predicted projectiles,
        // advance client-side cooldowns and reload timers.
    }

    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    bool& editorMode = EDITOR_MODE;
    std::string& activeGameMode = ACTIVE_GAME_MODE;
    int& selectedEditorObject = SELECTED_EDITOR_OBJ;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& replayWeaponModels = REPLAY_WEAPON_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& G_COMMAND_BINDS = CMD_BINDS;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();

    { MIMITA_PERF_SCOPE("Combat::HitResolve");
    if (!replayPlaybackActive)
        weapons.update(camera, player, npcSystem, world,
                       &mpContext.remoteNpcs, dt);
    }
    if (!replayPlaybackActive) {
        MIMITA_PERF_SCOPE("Combat::EffectSpawn");
        NpcCombat::updateNpcProjectiles(world, npcSystem, camera, player, dt);
    }
    if (!replayPlaybackActive) {
        PersistentPhysicsSystem::instance().update(dt, world, player, npcSystem, &camera);
    }
    if (!replayPlaybackActive) {
        // Local/offline duel only. Network duels are controlled by DuelQueue + server DuelStatePacket.
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
    { MIMITA_PERF_SCOPE("Combat::EffectSpawn");
    EffectPartSystem::instance().update(dt);
    }
    DeathGhostSystem::instance().update(dt);
    { MIMITA_PERF_SCOPE("Combat::DecalTrace");
    HitEffects::updateHitBursts(dt);
    }

    updateChatBubbles(player.chatState, dt);
    for (auto& kv : mpContext.remotePlayers)
        updateChatBubbles(kv.second.chatState, dt);
    for (auto& kv : gReplayChatStates)
        updateChatBubbles(kv.second, dt);

    static bool mousePrev = false;
    const bool gameplayInputAllowed = !PauseMenu::isOpen();
    bool mouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    // Local/offline duel only for DuelManager checks; also check DuelQueue for network duels.
    bool duelEndVisible = gDuelManager.phase() == DuelPhase::MatchEnd ||
        DuelQueue::instance().matchOver();
    bool duelCountdown = gDuelManager.isCountdownActive() ||
        DuelQueue::instance().countdownActive();
    bool bombTagEndVisible = gBombTagManager.phase() == BombTagPhase::MatchEnd;
    bool bombTagCountdown = gBombTagManager.isCountdownActive();
    if ((duelEndVisible || bombTagEndVisible) && mouseDown && !mousePrev) {
        Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] mouseClick=1 owner=game_end_ui consumed=1");
        Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] weaponInputBlocked=1 reason=end_ui_visible");
    }
    if (!replayPlaybackActive && !duelEndVisible && !duelCountdown &&
        !bombTagEndVisible && !bombTagCountdown &&
        gameplayInputAllowed && InputCommandSystem::instance().isKeyboardEnabled() && mouseDown &&
        glfwGetInputMode(engine.window(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
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
    if (!mouseDown && mousePrev && mpContext.active)
        MimitaNet::mpFlushOpHitscanBatch(mpContext,
                                         mpContext.lastKnownSpawnGeneration);
    mousePrev = mouseDown;

    static bool rightMousePrev = false;
    bool rightMouseDown = glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (!replayPlaybackActive && !duelCountdown &&
        gameplayInputAllowed && InputCommandSystem::instance().isKeyboardEnabled() && rightMouseDown && !rightMousePrev &&
        glfwGetInputMode(engine.window(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        if (!editorMode) {
            RevolverShotResult altResult = weapons.fireAlt(camera, player, npcSystem, world);
            // For Swordsword lunge in online mode, send attack-start
            if (altResult.fired && mpContext.active && mpContext.gameplayActive) {
                const WeaponDefinition* curDef = weapons.getCurrentDef(player);
                if (curDef && curDef->behaviorType == WeaponBehaviorType::Swordsword) {
                    uint16_t netId = MimitaNet::weaponDefNetworkIdFor(curDef->id);
                    if (netId != 0)
                        MimitaNet::mpSendAttackRequest(
                            mpContext, netId, curDef->slot,
                            player.pos, camera.front, player.pos, 2);
                }
            }
        }
    }
    rightMousePrev = rightMouseDown;

    static bool slotPrev[10] = {};
    for (int keySlot = 0; keySlot <= 9; ++keySlot) {
        int key = keySlot == 0 ? GLFW_KEY_0 : GLFW_KEY_0 + keySlot;
        bool down = glfwGetKey(engine.window(), key) == GLFW_PRESS;
        if (!replayPlaybackActive && !duelCountdown &&
            gameplayInputAllowed && InputCommandSystem::instance().isKeyboardEnabled() && down && !slotPrev[keySlot])
            Terminal::instance().execute("equipslot" + std::to_string(keySlot));
        slotPrev[keySlot] = down;
    }

    {
        static std::unordered_map<int, bool> bindPrev;
        for (const auto& pair : G_COMMAND_BINDS) {
            bool down = glfwGetKey(engine.window(), pair.first) == GLFW_PRESS;
            if (down && !bindPrev[pair.first] && gameplayInputAllowed && InputCommandSystem::instance().isKeyboardEnabled())
                Terminal::instance().execute(pair.second);
            bindPrev[pair.first] = down;
        }
    }

    // Replay playback keyboard shortcuts (only while replay is active)
    if (replayPlaybackActive && InputCommandSystem::instance().isKeyboardEnabled()) {
        static bool spacePrev = false;
        bool spaceDown = glfwGetKey(engine.window(), GLFW_KEY_SPACE) == GLFW_PRESS;
        if (spaceDown && !spacePrev) {
            if (gReplayPlayer.isPaused()) {
                gReplayPlayer.resume();
                if (gReplayEditor.isLoaded()) gReplayEditor.playing = true;
                Debug::log(Debug::Category::Replay, "[ReplayControls] Space: resumed\n");
            } else {
                gReplayPlayer.pause();
                if (gReplayEditor.isLoaded()) gReplayEditor.playing = false;
                Debug::log(Debug::Category::Replay, "[ReplayControls] Space: paused\n");
            }
        }
        spacePrev = spaceDown;

        bool ctrlHeld = glfwGetKey(engine.window(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(engine.window(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

        // ── Ctrl+Left/Right: tick-by-tick with hold-to-repeat ──────
        // Initial delay 250ms, then repeat every 100ms.
        // Never auto-starts playback — preserves pause state.
        {
            static float leftHoldTimer = 0.0f;
            static bool leftRepeating = false;
            bool leftDown = glfwGetKey(engine.window(), GLFW_KEY_LEFT) == GLFW_PRESS;
            if (ctrlHeld && leftDown) {
                bool doSeek = false;
                if (leftHoldTimer == 0.0f) {
                    // First press edge — seek immediately
                    doSeek = true;
                    leftHoldTimer = 0.001f;  // non-zero signals "held"
                } else {
                    leftHoldTimer += dt;
                    if (!leftRepeating && leftHoldTimer >= 0.25f) {
                        leftRepeating = true;
                        leftHoldTimer = 0.0f;
                        doSeek = true;
                    } else if (leftRepeating && leftHoldTimer >= 0.1f) {
                        leftHoldTimer -= 0.1f;
                        doSeek = true;
                    }
                }
                if (doSeek) {
                    bool wasPaused = gReplayPlayer.isPaused();
                    uint32_t t = gReplayPlayer.currentTick();
                    uint32_t seekTo = t > 1 ? t - 1 : 0;
                    gReplayPlayer.seekToTick(seekTo);
                    if (gReplayEditor.isLoaded())
                        gReplayEditor.seekToTick((int)seekTo);
                    if (wasPaused) {
                        gReplayPlayer.update(0.0f);
                        gReplayPlayer.pause();
                    }
                    Debug::log(Debug::Category::Replay, "[ReplayControls] Ctrl+Left: tick %u (wasPaused=%d)\n", seekTo, (int)wasPaused);
                }
            } else {
                leftHoldTimer = 0.0f;
                leftRepeating = false;
            }

            static float rightHoldTimer = 0.0f;
            static bool rightRepeating = false;
            bool rightDown = glfwGetKey(engine.window(), GLFW_KEY_RIGHT) == GLFW_PRESS;
            if (ctrlHeld && rightDown) {
                bool doSeek = false;
                if (rightHoldTimer == 0.0f) {
                    doSeek = true;
                    rightHoldTimer = 0.001f;
                } else {
                    rightHoldTimer += dt;
                    if (!rightRepeating && rightHoldTimer >= 0.25f) {
                        rightRepeating = true;
                        rightHoldTimer = 0.0f;
                        doSeek = true;
                    } else if (rightRepeating && rightHoldTimer >= 0.1f) {
                        rightHoldTimer -= 0.1f;
                        doSeek = true;
                    }
                }
                if (doSeek) {
                    bool wasPaused = gReplayPlayer.isPaused();
                    uint32_t t = gReplayPlayer.currentTick();
                    uint32_t total = gReplayPlayer.totalTicks();
                    uint32_t seekTo = std::min(t + 1, total);
                    gReplayPlayer.seekToTick(seekTo);
                    if (gReplayEditor.isLoaded())
                        gReplayEditor.seekToTick((int)seekTo);
                    if (wasPaused) {
                        gReplayPlayer.update(0.0f);
                        gReplayPlayer.pause();
                    }
                    Debug::log(Debug::Category::Replay, "[ReplayControls] Ctrl+Right: tick %u (wasPaused=%d)\n", seekTo, (int)wasPaused);
                }
            } else {
                rightHoldTimer = 0.0f;
                rightRepeating = false;
            }
        }

        // ── Left/Right (no Ctrl): seek 5 seconds ───────────────────
        {
            static bool leftPrev = false;
            bool leftDown = glfwGetKey(engine.window(), GLFW_KEY_LEFT) == GLFW_PRESS;
            if (leftDown && !leftPrev && !ctrlHeld) {
                bool wasPaused = gReplayPlayer.isPaused();
                uint32_t t = gReplayPlayer.currentTick();
                uint32_t seekTo = t > 300 ? t - 300 : 0;
                gReplayPlayer.seekToTick(seekTo);
                if (gReplayEditor.isLoaded())
                    gReplayEditor.seekToTick((int)seekTo);
                if (wasPaused) {
                    gReplayPlayer.update(0.0f);
                    gReplayPlayer.pause();
                }
            }
            leftPrev = leftDown;
        }

        {
            static bool rightPrev = false;
            bool rightDown = glfwGetKey(engine.window(), GLFW_KEY_RIGHT) == GLFW_PRESS;
            if (rightDown && !rightPrev && !ctrlHeld) {
                bool wasPaused = gReplayPlayer.isPaused();
                uint32_t t = gReplayPlayer.currentTick();
                uint32_t total = gReplayPlayer.totalTicks();
                uint32_t seekTo = std::min(t + 300, total);
                gReplayPlayer.seekToTick(seekTo);
                if (gReplayEditor.isLoaded())
                    gReplayEditor.seekToTick((int)seekTo);
                if (wasPaused) {
                    gReplayPlayer.update(0.0f);
                    gReplayPlayer.pause();
                }
            }
            rightPrev = rightDown;
        }

        static bool lPrev = false;
        bool lDown = glfwGetKey(engine.window(), GLFW_KEY_L) == GLFW_PRESS;
        if (lDown && !lPrev) {
            gReplayCinematicMode = !gReplayCinematicMode;
            printf("[CINEMATIC] %s\n", gReplayCinematicMode ? "Enabled" : "Disabled");
            Terminal::instance().addLog(std::string("[CINEMATIC] ") + (gReplayCinematicMode ? "Enabled" : "Disabled"));
        }
        lPrev = lDown;
    }
}
