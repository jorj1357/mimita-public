#include "engine/engine-tick-replay.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <random>
#include <thread>
#include <filesystem>
#include <GLFW/glfw3.h>
#include "world/world.h"
#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "camera.h"
#include "input/input-frame.h"
#include "input/input-poll.h"
#include "input/input-commands.h"
#include "sim/sim-context.h"
#include "combat/weapon-system.h"
#include "combat/death-system.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "devtools/dev-overlay.h"
#include "devtools/npc-spawn-commands.h"
#include "replay/replay.h"
#include "replay/replay-editor.h"
#include "replay/replay-export.h"
#include "replay/replay-factory.h"
#include "gui/gui-layout.h"
#include "render/lighting-config.h"
#include "shadow/shadow-config.h"
#include "void-death/void-death.h"
#include "audio/hitmarker-audio.h"
#include "video/outro.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "config/player-settings.h"
#include "npc/npc-state-machine.h"
#include "config.h"
#include "perf/perf.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;

constexpr double SIM_DT = 1.0 / 60.0;

void engineTickReplay(Engine& engine, float dt)
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
    static ReplayTestState replayTest;

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

    DebugVis::beginCollisionFrame();
    // During export capture, skip the normal update and let the seek block below
    // handle frame advancement + effect collection, otherwise effects collect twice.
    if (getReplayExportJob().state != ReplayExportJob::Capturing) {
        gReplayPlayer.update(dt);
        gReplayEditor.update(dt);
    }

    // Replay export mode: seek and rebuild interpolated frame for capture
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
    bool replayRenderActive = replayPlaybackActive ||
        (getReplayExportJob().state == ReplayExportJob::Capturing && gReplayPlayer.totalTicks() > 0);
    if (getReplayExportJob().state == ReplayExportJob::Capturing) {
        if (!replayRenderActive) {
            Debug::log(Debug::Category::Replay, "[EXPORTTRACE] FORCE replayRenderActive=1 during export (was 0)");
        }
        replayRenderActive = true;
    }
    setReplayCaptureEnabled(!replayPlaybackActive);

    // Fixed-tick simulation accumulator
    simAccumulator += (double)dt;

    { Perf::ScopedTimer _t("Simulation");
    while (simAccumulator >= SIM_DT) {
        InputFrame tickFrame;

        if (!replayPlaybackActive) {
            InputCommandSystem::instance().setKeyboardEnabled(!Terminal::instance().isOpen());
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

        // Capture death position for camera orbit
        if (player.dead && glm::length(deathPosition) < 0.1f)
            deathPosition = player.pos;
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
            playerActor.grounded = player.ground.onGround;
            playerActor.collidable = !player.dead;
            playerActor.fade = 0.0f;
            playerActor.outfitPath = GetPlayerSettings().outfitPath;
            playerActor.characterName = GetPlayerSettings().characterName;
            playerActor.avatarName = GetPlayerSettings().avatarName;
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
            playerActor.animationState = player.ground.onGround
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
                npcActor.grounded = npc.body.ground.onGround;
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
            // ── Godball ────────────────────────────────────────────────
            if (weapons.godballPhysics().active) {
                const auto& gb = weapons.godballPhysics();
                glm::vec3 handPos = WeaponGodball::getHandPosition(player);

                ReplayEffectEvent gbSphere;
                gbSphere.type = "godball";
                gbSphere.position = gb.position;
                gbSphere.scale = glm::vec3(gb.radius);
                gbSphere.color = glm::vec4(0.2f, 0.4f, 0.8f, 0.7f);
                gbSphere.lifetime = 0.1f;
                captureReplayEffect(gbSphere);
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] recorded type=godball tick=%d pos=(%.2f %.2f %.2f) radius=%.2f color=(%.2f %.2f %.2f %.2f)\n",
                    replayTick, gb.position.x, gb.position.y, gb.position.z,
                    gb.radius, 0.2f, 0.4f, 0.8f, 0.7f);

                ReplayEffectEvent gbRope;
                gbRope.type = "godball_rope";
                gbRope.from = handPos;
                gbRope.to = gb.position;
                gbRope.lifetime = 0.1f;
                captureReplayEffect(gbRope);
                Debug::log(Debug::Category::Replay,
                    "[REPLAY EFFECT] recorded type=godball_rope tick=%d from=(%.2f %.2f %.2f) to=(%.2f %.2f %.2f)\n",
                    replayTick, handPos.x, handPos.y, handPos.z,
                    gb.position.x, gb.position.y, gb.position.z);
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

    ProcessNpcSpawnCommands(npcSystem, camera, world, player);
    ProcessNpcTrainingSpawnCommands(npcSystem, camera, world, player);
}

