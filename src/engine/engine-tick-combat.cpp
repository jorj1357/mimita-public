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
#include "mapeditor/map-editor.h"
#include "gui/hud/chat-window.h"
#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "world/world.h"
#include "input/input-commands.h"
#include "input/input-frame.h"
#include "input/scenario-input.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-rocket-launcher.h"
#include "combat/melee-client.h"
#include "combat/death-system.h"
#include "network/multiplayer-context.h"
#include "network/community-server-config.h"
#include "network/community-match-client.h"
#include "network/network-weapons.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-factory.h"
#include "replay/replay-editor.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "physics/physical-entity.h"
#include "procedural/procedural-world-client.h"
#include "ragdoll/ragdoll-mode.h"
#include "gui/hud/chat-bubble.h"
#include "game/duel.h"
#include "duel/duel-queue.h"
#include "game/gamemode-manager.h"
#include "game/game-state.h"
#include "world/world.h"
#include "render/post-fx.h"
#include "physics/ray-utils.h"
#include "pobjects/persistent-physics.h"
#include "gui/menus/pause-menu.h"

extern DuelManager gDuelManager;
extern GamemodeManager gGamemodeManager;
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
    auto& gReplayFactory = REPLAY_FACTORY;

    if (ScenarioInput::active())
    {
        if (const InputFrame* recorded = ScenarioInput::frame())
        {
            camera.yaw = recorded->lookYaw;
            camera.pitch = recorded->lookPitch;
            if (glm::length(recorded->camForward) > 0.001f)
            {
                camera.front = glm::normalize(recorded->camForward);
                camera.right = glm::normalize(
                    glm::cross(camera.front, glm::vec3(0.0f, 0.0f, 1.0f)));
                camera.up = glm::normalize(glm::cross(camera.right, camera.front));
            }
        }
    }

    player.weaponAimDownSights = !MapEditor::instance().active() &&
        ScenarioInput::mouseButton(engine.window(), GLFW_MOUSE_BUTTON_RIGHT);

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();

    { MIMITA_PERF_SCOPE("Combat::HitResolve");
    if (!replayPlaybackActive)
        weapons.update(camera, player, npcSystem, world,
                       &mpContext.remotePlayers,
                       &mpContext.remoteNpcs, dt);
    }
    if (!replayPlaybackActive) {
        MIMITA_PERF_SCOPE("Combat::EffectSpawn");
    }
    if (!replayPlaybackActive) {
        // Mirror the server-authoritative procedural exit barrier as a client
        // physical entity before the fixed-step actor/physics update runs.
        clientProceduralWorldTick(world);
        for (int i = 0; i < clientSteps; ++i)
            clientProceduralTeleportShieldTick();
        MIMITA_PERF_SCOPE("Combat::PhysicsEntities");
        PersistentPhysicsSystem::instance().update(dt, world, player, npcSystem, &camera);
        {
            // Feed the entity/destruction physics cost into PerfTimes so the
            // overlay/report shows the real physics number (it was only a
            // spike-scope before and never appeared in the breakdown).
            Perf::ScopedTimer entityPhysics("PhysicsEntities");
            PhysicalEntitySystem::instance().advanceKinematics(dt, world);
        }
    }
    if (!replayPlaybackActive) {
        // Local/offline duel only. Network duels are controlled by DuelQueue + server DuelStatePacket.
        if (gDuelManager.enabled()) {
            gDuelManager.update(dt, player, npcSystem, world, camera);
        }
        // Bomb Tag: client-side rendering only (server owns all gameplay)
        if (gGamemodeManager.enabled()) {
            gGamemodeManager.update(dt, player);
            gGamemodeManager.renderWorldElements(camera, player);
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
                    ReplayClip saveCopy = clip;
                    const bool saveQueued = gReplayFactory.enqueueClipSave(std::move(saveCopy), savePath);
                    gDuelManager.finalKillReplayPath = savePath;
                    if (gReplayPlayer.loadClip(std::move(clip))) {
                        gDuelManager.setReplayReady();
                        Debug::log(Debug::Category::Replay,
                            "[REPLAY] final kill save %s path=%s",
                            saveQueued ? "queued" : "rejected", savePath.c_str());
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
        gReplayPlayer.totalTicks() > 0 &&
        !isReplayExportActive())
    {
        Debug::log(Debug::Category::Duel, "[DUEL] Replay Looping (tick=%u/%u)",
                   gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
        gReplayPlayer.seekToTick(0);
        gReplayPlayer.resume();
    }

    // Effects and hit bursts are presentation on the fixed 60 Hz gameplay
    // clock. Advance them in bounded fixed steps rather than once per render
    // frame, so a 240 FPS renderer does not pay four times the update cost.
    {
        static double effectAccumulator = 0.0;
        constexpr double kEffectDt = 1.0 / 60.0;
        effectAccumulator = std::min(effectAccumulator + (double)dt, 0.2);
        int steps = 0;
        { MIMITA_PERF_SCOPE("Combat::EffectSpawn");
          while (effectAccumulator >= kEffectDt && steps < 4) {
              EffectPartSystem::instance().update((float)kEffectDt);
              HitEffects::updateHitBursts((float)kEffectDt);
              effectAccumulator -= kEffectDt;
              ++steps;
          }
        }
    }

    updateChatBubbles(player.chatState);
    for (auto& kv : mpContext.remotePlayers)
        updateChatBubbles(kv.second.chatState);
    for (auto& kv : gReplayChatStates)
        updateChatBubbles(kv.second);

    static bool mousePrev = false;
    const bool scenarioInputActive = ScenarioInput::active();
    const bool gameplayInputAllowed = scenarioInputActive ||
        (!PauseMenu::isOpen() && !MapEditor::instance().active());
    const bool inputEnabled = scenarioInputActive ||
        InputCommandSystem::instance().isKeyboardEnabled();
    const bool cursorCaptured = scenarioInputActive ||
        glfwGetInputMode(engine.window(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    bool mouseDown = ScenarioInput::mouseButton(
        engine.window(), GLFW_MOUSE_BUTTON_LEFT);
    // Local/offline duel only for DuelManager checks; also check DuelQueue for network duels.
    bool duelEndVisible = gDuelManager.phase() == DuelPhase::MatchEnd ||
        DuelQueue::instance().matchOver();
    bool duelCountdown = gDuelManager.isCountdownActive() ||
        DuelQueue::instance().countdownActive();
    bool bombTagEndVisible = gGamemodeManager.isMatchEnd();
    bool bombTagCountdown = gGamemodeManager.isCountdownActive();
    if ((duelEndVisible || bombTagEndVisible) && mouseDown && !mousePrev) {
        Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] mouseClick=1 owner=game_end_ui consumed=1");
        Debug::log(Debug::Category::Duel, "[INPUT OWNERSHIP] weaponInputBlocked=1 reason=end_ui_visible");
    }
    // When a Captain Falcon move set is equipped, LMB/RMB/E drive Melee actions
    // instead of ordinary weapon fire.
    const bool meleeEquipped = MimitaMelee::hasEquippedMoveSet(player, weapons);
    bool meleePrimaryPressed = false;
    bool meleeSecondaryPressed = false;
    if (meleeEquipped) {
        if (!replayPlaybackActive && gameplayInputAllowed &&
            inputEnabled && mouseDown && !mousePrev &&
            cursorCaptured)
            meleePrimaryPressed = true;
    } else if (!replayPlaybackActive && !duelEndVisible && !duelCountdown &&
        !bombTagEndVisible && !bombTagCountdown &&
        gameplayInputAllowed && inputEnabled && mouseDown &&
        cursorCaptured) {
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
    bool rightMouseDown = ScenarioInput::mouseButton(
        engine.window(), GLFW_MOUSE_BUTTON_RIGHT);
    if (meleeEquipped) {
        if (!replayPlaybackActive && gameplayInputAllowed &&
            inputEnabled &&
            rightMouseDown && !rightMousePrev &&
            cursorCaptured)
            meleeSecondaryPressed = true;
    } else if (!replayPlaybackActive && !duelCountdown &&
        gameplayInputAllowed && inputEnabled && rightMouseDown && !rightMousePrev &&
        cursorCaptured) {
        if (!editorMode) {
            RevolverShotResult altResult = weapons.fireAlt(camera, player, npcSystem, world);
            // For Swordsword lunge in online mode, send attack-start
            if (altResult.fired && mpContext.active && mpContext.gameplayActive) {
                const WeaponDefinition* curDef = weapons.getCurrentDef(player);
                if (curDef && curDef->behaviorType == WeaponBehaviorType::Swordsword) {
                    uint16_t netId = MimitaNet::weaponDefNetworkIdFor(curDef->id);
                    int requestSlot = curDef->slot;
                    if (mpContext.communityWeaponSetId > 0) {
                        auto& cfg = MimitaNet::CommunityServerConfig::instance();
                        if (cfg.weaponSets().empty()) cfg.load();
                        const int logical = cfg.slotForWeapon(
                            mpContext.communityWeaponSetId, curDef->id);
                        if (logical > 0) requestSlot = logical;
                    }
                    if (netId != 0)
                        MimitaNet::mpSendAttackRequest(
                            mpContext, netId, requestSlot,
                            player.pos, camera.front, player.pos, 2);
                }
            }
        }
    }
    rightMousePrev = rightMouseDown;

    // Publish crouch intent for Melee actors for the authored `crouch` state
    // clip, independent of the driver's move-set equip gate.
    if (player.usesMeleeAuthoredPose()) {
        const InputFrame* recorded = ScenarioInput::frame();
        player.crouching = scenarioInputActive && recorded
            ? recorded->crouchHeld
            : (InputCommandSystem::instance().isCrouchHeld() ||
               MimitaMelee::debugCrouchHeld());
    }

    // Captain Falcon Melee: sample the Melee controls and drive the local
    // action (animation + predicted self-impulses). The server is authoritative
    // for the action result; the client predicts presentation immediately.
    // The driver runs whenever the move set is equipped (even without a captured
    // cursor) so agent.melee live tests can force actions.
    if (meleeEquipped && !replayPlaybackActive && gameplayInputAllowed) {
        MimitaMelee::ClientInput meleeInput;
        if (cursorCaptured) {
            static bool grabPrev = false;
            const bool grabDown =
                ScenarioInput::keyDown(engine.window(), GLFW_KEY_E);
            meleeInput.primaryPressed = meleePrimaryPressed;
            meleeInput.secondaryPressed = meleeSecondaryPressed;
            meleeInput.secondaryHeld = rightMouseDown;
            meleeInput.grabPressed = grabDown && !grabPrev;
            meleeInput.shieldHeld =
                ScenarioInput::keyDown(engine.window(), GLFW_KEY_Q);
            meleeInput.jumpHeld =
                ScenarioInput::keyDown(engine.window(), GLFW_KEY_SPACE);
            const InputFrame* recorded = ScenarioInput::frame();
            meleeInput.crouchHeld = scenarioInputActive && recorded
                ? recorded->crouchHeld
                : (InputCommandSystem::instance().isCrouchHeld() ||
                   MimitaMelee::debugCrouchHeld());
            grabPrev = grabDown;
        }
        const InputFrame* movementFrame = ScenarioInput::frame();
        meleeInput.movementPressed = scenarioInputActive && movementFrame
            ? movementFrame->movementPressed
            : glm::length(InputCommandSystem::instance().getMoveVector()) > 0.01f;
        MimitaMelee::clientTick(player, camera, weapons, mpContext, meleeInput,
                                g_clientSimulationTick);
        // Publish crouch intent for the authored `crouch` state clip, independent
        // of the driver's move-set check and the cursor gate.
        const InputFrame* recordedCrouch = ScenarioInput::frame();
        player.crouching = scenarioInputActive && recordedCrouch
            ? recordedCrouch->crouchHeld
            : (InputCommandSystem::instance().isCrouchHeld() ||
               MimitaMelee::debugCrouchHeld());
    } else {
        MimitaMelee::reset();
    }
    static bool slotPrev[10] = {};
    const auto& matchClient = MimitaNet::CommunityMatchClient::instance();
    const bool isJuggernautFighter =
        mpContext.active && matchClient.mode() == "juggernaut" &&
        matchClient.localTeam(mpContext.localPlayerId) == 0;
    if (isJuggernautFighter && !replayPlaybackActive && gameplayInputAllowed &&
        inputEnabled) {
        for (int choice = 0; choice < 4; ++choice) {
            const bool down = ScenarioInput::keyDown(
                engine.window(), GLFW_KEY_1 + choice);
            if (down && !slotPrev[choice + 1] &&
                matchClient.fighterWeaponChoiceVisible(mpContext.localPlayerId)) {
                Terminal::instance().execute("equipslot" + std::to_string(choice + 1));
                MimitaNet::CommunityMatchClient::instance().selectFighterWeapon(choice);
            }
        }
    }
    for (int keySlot = 0; keySlot <= 9; ++keySlot) {
        int key = keySlot == 0 ? GLFW_KEY_0 : GLFW_KEY_0 + keySlot;
        bool down = ScenarioInput::keyDown(engine.window(), key);
        const bool pickerLocked = isJuggernautFighter &&
            matchClient.fighterWeaponChoiceVisible(mpContext.localPlayerId) &&
            keySlot >= 1 && keySlot <= 4;
        if (!pickerLocked && !replayPlaybackActive && !duelCountdown &&
            gameplayInputAllowed && inputEnabled && down && !slotPrev[keySlot]) {
            Terminal::instance().execute("equipslot" + std::to_string(keySlot));
        }
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
