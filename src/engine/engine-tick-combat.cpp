#include "engine/engine-tick-combat.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <unordered_map>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "input/input-commands.h"
#include "perf/perf.h"
#include "combat/weapon-system.h"
#include "combat/weapon-types.h"
#include "combat/death-system.h"
#include "network/multiplayer-context.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "replay/replay.h"
#include "replay/replay-editor.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "gui/hud/chat-bubble.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "game/game-state.h"
#include "world/world.h"
#include "render/post-fx.h"
#include "physics/ray-utils.h"
#include "pobjects/persistent-physics.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern bool gReplayCinematicMode;

void engineTickCombat(Engine& engine, float dt)
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
    int& selectedEditorObject = SELECTED_EDITOR_OBJ;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& replayWeaponModels = REPLAY_WEAPON_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& G_COMMAND_BINDS = CMD_BINDS;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& gReplayClipSaver = REPLAY_CLIP_SAVER;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();

    { Perf::ScopedTimer _wp("Weapons");
    if (!replayPlaybackActive)
        weapons.update(camera, player, npcSystem, world, dt);
    }
    if (!replayPlaybackActive) {
        PersistentPhysicsSystem::instance().update(dt, world, player, npcSystem, &camera);
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
}
