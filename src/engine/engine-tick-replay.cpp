#include "engine/engine-tick-replay.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include "gui/hud/chat-window.h"
#include <cstdio>
#include <random>
#include <thread>
#include <filesystem>
#include <algorithm>
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
#include "npc/npc-avatar.h"
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
#include "perf/perf-spike.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;

constexpr double SIM_DT = 1.0 / 60.0;

// Maximum fixed simulation ticks that may be caught up in a single rendered
// frame after a stall. Bounding this prevents a long presentation stall
// (e.g. ~1 s under Wine) from queuing dozens of full simulation ticks and
// freezing the game for seconds. Tunable at runtime with `sim.catchup`.
int gSimMaxCatchupTicks = 6;

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
        // Ensure the recorder has the correct map path for the clip.
        // beginRecording() zeroes mWorld, so we re-populate it here.
        {
            ReplayWorldMetadata rw;
            rw.mapPath = ACTIVE_MAP_PATH;
            REPLAY_RECORDER.setWorldMetadata(rw);
        }

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
            if (job.mfWriter && !mfReplayQueueHasRoom(job.mfWriter)) {
                // Encoder inbox full: skip the seek entirely so the same tick is
                // never re-rendered while waiting. The export resumes when the
                // encoder drains a frame.
                if (gReplayExportVerbose)
                    Debug::log(Debug::Category::Replay,
                        "[EXPORT WAIT] encoder busy, skipping seek (tick=%u)\n",
                        (uint32_t)job.exportTick);
            } else {
            replayExportTimingFrameBegin();
            uint32_t seekTick = (uint32_t)job.exportTick;
            uint32_t beforeTick = gReplayPlayer.currentTick();
            if (seekTick < gReplayPlayer.totalTicks()) {
                const double tSeek0 = replayExportNowSec();
                if (gReplayExportVerbose)
                    Debug::log(Debug::Category::Replay, "[EXPORTTRACE] seek tick %u / total %u", seekTick, gReplayPlayer.totalTicks());
                gReplayPlayer.seekToTick(seekTick);
                gExportFrameTimings.seekMs += (replayExportNowSec() - tSeek0) * 1000.0;
                const double tUpd0 = replayExportNowSec();
                gReplayPlayer.update(0.0f);
                gExportFrameTimings.updateMs += (replayExportNowSec() - tUpd0) * 1000.0;
                uint32_t afterTick = gReplayPlayer.currentTick();
                if (gReplayExportVerbose)
                    Debug::log(Debug::Category::Replay, "[EXPORT DEBUG] REPLAY_PLAYER.update: beforeTick=%u afterTick=%u (seekTick=%u)",
                               beforeTick, afterTick, seekTick);
            } else if (gReplayExportVerbose) {
                Debug::log(Debug::Category::Replay, "[EXPORTTRACE] seek tick %u >= total %u (skip)", seekTick, gReplayPlayer.totalTicks());
            }
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

    // Fixed-tick simulation accumulator.
    // Cap the accumulated backlog so a stall can never queue more than
    // gSimMaxCatchupTicks of simulation time; excess real time is dropped so
    // the game snaps back to live rather than spiraling through catch-up.
    simAccumulator = std::min(
        simAccumulator + (double)dt,
        (double)std::max(gSimMaxCatchupTicks, 1) * SIM_DT);

    { Perf::ScopedTimer _t("Simulation");
    int simTicksRun = 0;
    while (simAccumulator >= SIM_DT && simTicksRun < gSimMaxCatchupTicks) {
        InputFrame tickFrame;

        if (!replayPlaybackActive) {
            InputCommandSystem::instance().setKeyboardEnabled(
                !Terminal::instance().isOpen() && !isChatOpen());
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
            { MIMITA_PERF_SCOPE("Replay::RecordFrame");
              gReplayRecorder.recordFrame(tickFrame);
            }
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
            static const std::string kDefaultModelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";

            ReplaySceneFrame& sceneFrame = gReplayRecorder.getWritableFrame();
            sceneFrame.tick = (int)replayTick;
            sceneFrame.time = (float)sceneFrame.tick / 60.0f;
            sceneFrame.actors.clear();
            sceneFrame.effects.clear();

            // Camera
            sceneFrame.camera.position = camera.pos;
            sceneFrame.camera.rotation = glm::vec3(camera.pitch, 0.0f, player.yaw);
            sceneFrame.camera.fov = camera.fov;

            // ── Identity table update helper ──
            auto updateIdentity = [&](uint32_t actorId, ReplayActorType actorType,
                const std::string& idStr, const std::string& nameStr,
                const std::string& modelPath, const std::string& outfit,
                const std::string& charName, const std::string& avatar,
                const std::string& weapName, const std::string& weapModel)
            {
                auto& ident = gReplayRecorder.getOrCreateIdentity(actorId);
                bool changed = (ident.idString != idStr) || (ident.name != nameStr) ||
                               (ident.modelPath != modelPath) || (ident.outfitPath != outfit) ||
                               (ident.characterName != charName) || (ident.avatarName != avatar) ||
                               (ident.weaponName != weapName) || (ident.weaponModelPath != weapModel) ||
                               (ident.type != actorType);
                if (changed) {
                    ident.id = actorId;
                    ident.type = actorType;
                    ident.idString = idStr;
                    ident.name = nameStr;
                    ident.modelPath = modelPath;
                    ident.outfitPath = outfit;
                    ident.characterName = charName;
                    ident.avatarName = avatar;
                    ident.weaponName = weapName;
                    ident.weaponModelPath = weapModel;
                    ident.generation++;
                }
            };

            // ── Build actor from identity table (avoids string copies from game objects) ──
            auto buildActor = [&](ReplayActorState& out, uint32_t actorId)
            {
                const auto* ident = gReplayRecorder.getIdentity(actorId);
                if (ident) {
                    out.id = ident->idString;
                    out.name = ident->name;
                    out.type = kReplayActorTypeNames[(int)ident->type];
                    out.modelPath = ident->modelPath;
                    out.outfitPath = ident->outfitPath;
                    out.characterName = ident->characterName;
                    out.avatarName = ident->avatarName;
                    out.weaponName = ident->weaponName;
                    out.weaponModelPath = ident->weaponModelPath;
                }
            };

            // ── Delta detection: compare with previous state, skip if unchanged ──
            auto tickStateChanged = [&](uint32_t actorId, const ReplayActorState& current) -> bool
            {
                const auto* prev = gReplayRecorder.getPreviousState(actorId);
                if (!prev) return true;
                return (current.position != prev->position) || (current.rotation != prev->rotation) ||
                       (current.velocity != prev->velocity) || (current.health != prev->health) ||
                       (current.maxHealth != prev->maxHealth) || (current.currentAmmo != prev->currentAmmo) ||
                       (current.reserveAmmo != prev->reserveAmmo) || (current.dead != prev->dead) ||
                       (current.shooting != prev->shooting) || (current.reloading != prev->reloading) ||
                       (current.grounded != prev->grounded) || (current.sizeScale != prev->sizeScale);
            };

            auto storeIfDirty = [&](uint32_t actorId, ReplayActorState& actor)
            {
                if (tickStateChanged(actorId, actor)) {
                    sceneFrame.actors.push_back(std::move(actor));
                    // Store compact previous state for next tick comparison
                    ReplayActorTickState compact;
                    compact.actorId = actorId;
                    compact.position = actor.position;
                    compact.rotation = actor.rotation;
                    compact.velocity = actor.velocity;
                    compact.health = (int16_t)actor.health;
                    compact.maxHealth = (int16_t)actor.maxHealth;
                    compact.currentAmmo = (uint16_t)actor.currentAmmo;
                    compact.reserveAmmo = (uint16_t)actor.reserveAmmo;
                    compact.dead = actor.dead;
                    compact.shooting = actor.shooting;
                    compact.reloading = actor.reloading;
                    compact.grounded = actor.grounded;
                    compact.sizeScale = actor.sizeScale;
                    gReplayRecorder.storePreviousState(actorId, compact);
                }
            };

            // ── Player ──
            {
                const uint32_t actorId = 0;
                const std::string& idStr = player.username.empty() ? std::string("admin") : player.username;
                const WeaponDefinition* wdef = weapons.getCurrentDef(player);
                const std::string weapName = wdef ? wdef->id : "none";
                const std::string weapModel = wdef ? wdef->modelPath : "";
                auto wit = player.weaponRuntimes.find(player.equippedWeaponId);
                uint16_t curAmmo = (wit != player.weaponRuntimes.end()) ? wit->second.currentAmmo : 0;
                uint16_t resAmmo = (wit != player.weaponRuntimes.end()) ? wit->second.reserveAmmo : 0;

                updateIdentity(actorId, ReplayActorType::Player, idStr,
                    player.username, kDefaultModelPath, GetPlayerSettings().outfitPath,
                    GetPlayerSettings().characterName, GetPlayerSettings().avatarName,
                    weapName, weapModel);

                ReplayActorState actor;
                buildActor(actor, actorId);
                actor.position = player.pos;
                actor.rotation = glm::vec3(0.0f, 0.0f, player.yaw);
                actor.velocity = player.vel;
                actor.health = player.currentHp;
                actor.maxHealth = player.maxHp;
                actor.currentAmmo = curAmmo;
                actor.reserveAmmo = resAmmo;
                actor.dead = player.dead;
                actor.shooting = weapons.isShooting();
                actor.reloading = weapons.isReloading(player);
                actor.grounded = player.ground.onGround;
                actor.sizeScale = player.sizeScale;
                { auto bp = captureReplayBodyParts(player); actor.bodyParts = bp.parts; actor.bodyPartCount = bp.count; }
                storeIfDirty(actorId, actor);
            }

            // ── NPCs ──
            {
            MIMITA_PERF_SCOPE("Replay::ActorSnapshot");
            for (const Npc& npc : npcSystem.all()) {
                const uint32_t actorId = 1000u + npc.id;
                const std::string idStr = "npc_" + std::to_string(npc.id);
                const WeaponDefinition* wdef = weapons.getDefForSlot(npc.body.equippedSlot);
                const std::string weapName = wdef ? wdef->id : "none";
                const std::string weapModel = wdef ? wdef->modelPath : "";
                auto wit = npc.body.weaponRuntimes.find(npc.body.equippedWeaponId);
                uint16_t curAmmo = (wit != npc.body.weaponRuntimes.end()) ? wit->second.currentAmmo : 0;
                uint16_t resAmmo = (wit != npc.body.weaponRuntimes.end()) ? wit->second.reserveAmmo : 0;

                updateIdentity(actorId,
                    npc.body.dead ? ReplayActorType::Corpse : ReplayActorType::Npc,
                    idStr, npc.body.username, kDefaultModelPath, "",
                    "", npc.avatarName, weapName, weapModel);

                ReplayActorState actor;
                buildActor(actor, actorId);
                actor.position = npc.body.pos;
                actor.rotation = glm::vec3(0.0f, 0.0f, npc.body.yaw);
                actor.velocity = npc.body.vel;
                actor.health = npc.body.currentHp;
                actor.maxHealth = npc.body.maxHp;
                actor.currentAmmo = curAmmo;
                actor.reserveAmmo = resAmmo;
                actor.dead = npc.body.dead;
                actor.grounded = npc.body.ground.onGround;
                actor.sizeScale = npc.body.sizeScale;
                { MIMITA_PERF_SCOPE("Replay::BodyParts");
                  auto bp = captureReplayBodyParts(npc.body);
                  actor.bodyParts = bp.parts; actor.bodyPartCount = bp.count; }
                storeIfDirty(actorId, actor);
            }
            } // ReplayCaptureNPCs

            // ── Remote players ──
            {
            MIMITA_PERF_SCOPE("Replay::ActorSnapshot");
            for (const auto& kv : mpContext.remotePlayers) {
                const Player& p = kv.second;
                const uint32_t actorId = 20000u + kv.first;
                const std::string idStr = "remote_" + std::to_string(kv.first);
                const WeaponDefinition* wdef = weapons.getCurrentDef(p);
                const std::string weapName = wdef ? wdef->id : "none";
                const std::string weapModel = wdef ? wdef->modelPath : "";
                auto wit = p.weaponRuntimes.find(p.equippedWeaponId);
                uint16_t curAmmo = (wit != p.weaponRuntimes.end()) ? wit->second.currentAmmo : 0;
                uint16_t resAmmo = (wit != p.weaponRuntimes.end()) ? wit->second.reserveAmmo : 0;

                updateIdentity(actorId,
                    p.dead ? ReplayActorType::Corpse : ReplayActorType::RemotePlayer,
                    idStr, p.username, kDefaultModelPath, "",
                    p.characterName(), p.avatarName(), weapName, weapModel);

                ReplayActorState actor;
                buildActor(actor, actorId);
                actor.position = p.pos;
                actor.rotation = glm::vec3(0.0f, 0.0f, p.yaw);
                actor.velocity = p.vel;
                actor.health = p.currentHp;
                actor.maxHealth = p.maxHp;
                actor.currentAmmo = curAmmo;
                actor.reserveAmmo = resAmmo;
                actor.dead = p.dead;
                actor.grounded = p.ground.onGround;
                actor.sizeScale = p.sizeScale;
                { MIMITA_PERF_SCOPE("Replay::BodyParts");
                  auto bp = captureReplayBodyParts(p);
                  actor.bodyParts = bp.parts; actor.bodyPartCount = bp.count; }
                storeIfDirty(actorId, actor);
            }
            } // ReplayCaptureRemotePlayers

            // ── Remote NPCs ──
            {
            MIMITA_PERF_SCOPE("Replay::ActorSnapshot");
            for (const auto& kv : mpContext.remoteNpcs) {
                const Player& p = kv.second;
                const uint32_t actorId = 30000u + kv.first;
                const std::string idStr = "rnpc_" + std::to_string(kv.first);

                // Use cached avatar instead of filesystem call
                const std::string& avatarName = [&]() -> const std::string& {
                    const std::string& cached = gReplayRecorder.getCachedNpcAvatar(kv.first, p.networkTransformEpoch);
                    if (!cached.empty()) return cached;
                    static std::string resolved;
                    resolved = npcAvatarNameForLife(kv.first, p.networkTransformEpoch);
                    gReplayRecorder.cacheNpcAvatar(kv.first, p.networkTransformEpoch, resolved);
                    return resolved;
                }();

                const WeaponDefinition* wdef = weapons.getCurrentDef(p);
                const std::string weapName = wdef ? wdef->id : "none";
                const std::string weapModel = wdef ? wdef->modelPath : "";
                auto wit = p.weaponRuntimes.find(p.equippedWeaponId);
                uint16_t curAmmo = (wit != p.weaponRuntimes.end()) ? wit->second.currentAmmo : 0;
                uint16_t resAmmo = (wit != p.weaponRuntimes.end()) ? wit->second.reserveAmmo : 0;

                updateIdentity(actorId,
                    p.dead ? ReplayActorType::Corpse : ReplayActorType::RemoteNpc,
                    idStr, p.username, kDefaultModelPath, "",
                    p.characterName(), avatarName, weapName, weapModel);

                ReplayActorState actor;
                buildActor(actor, actorId);
                actor.position = p.pos;
                actor.rotation = glm::vec3(0.0f, 0.0f, p.yaw);
                actor.velocity = p.vel;
                actor.health = p.currentHp;
                actor.maxHealth = p.maxHp;
                actor.currentAmmo = curAmmo;
                actor.reserveAmmo = resAmmo;
                actor.dead = p.dead;
                actor.grounded = p.ground.onGround;
                actor.sizeScale = p.sizeScale;
                { MIMITA_PERF_SCOPE("Replay::BodyParts");
                  auto bp = captureReplayBodyParts(p);
                  actor.bodyParts = bp.parts; actor.bodyPartCount = bp.count; }
                storeIfDirty(actorId, actor);
            }
            } // ReplayCaptureRemoteNpcs
            // ── Godball ────────────────────────────────────────────────
            if (weapons.godballPhysics().active) {
                MIMITA_PERF_SCOPE("Replay::Effects");
                const auto& gb = weapons.godballPhysics();
                glm::vec3 handPos = WeaponGodball::getHandPosition(player);

                ReplayEffectEvent gbSphere;
                gbSphere.type = "godball";
                gbSphere.position = gb.position;
                gbSphere.scale = glm::vec3(gb.radius);
                gbSphere.color = glm::vec4(0.2f, 0.4f, 0.8f, 0.7f);
                gbSphere.lifetime = 0.1f;
                captureReplayEffect(gbSphere);

                ReplayEffectEvent gbRope;
                gbRope.type = "godball_rope";
                gbRope.from = handPos;
                gbRope.to = gb.position;
                gbRope.lifetime = 0.1f;
                captureReplayEffect(gbRope);
            }

            {
            MIMITA_PERF_SCOPE("Replay::StoreFrame");
            gReplayRecorder.commitFrame();
            }
            gReplayFactory.update();

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
#ifndef NDEBUG
                        // Dev-only replay validation (spawns a python helper).
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
#else
                        Terminal::instance().addLog(
                            "[REPLAY TEST] Replay exported");
#endif
                    }
                    Terminal::instance().execute("replay.record");
                }
            }
        }

        simAccumulator -= SIM_DT;
        simTicksRun++;
    }
    } // Perf::ScopedTimer Simulation

    // Config polling — once per rendered frame, not per simulation tick.
    // These do filesystem stat() calls; running them inside the catch-up loop
    // would multiply their cost by the number of catch-up ticks.
    GuiLayoutManager::instance().pollReload();
    LightingConfig::instance().pollReload();
    ShadowConfig::instance().pollReload();
    pollVoidDeathConfig();
    pollHitmarkerAudioConfig();
    pollReplayExportConfig();
    pollOutroConfig();
    pollReplayHitmarkerConfig();

    ProcessNpcSpawnCommands(npcSystem, camera, world, player);
    ProcessNpcTrainingSpawnCommands(npcSystem, camera, world, player);
}

