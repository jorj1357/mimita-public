#include "engine/engine-tick-camera.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <GLFW/glfw3.h>
#include "camera.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "physics/physics-debug-movement.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "replay/replay.h"
#include "gui/hud/chat-bubble.h"
#include "ui/hitmarker.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "game/duel.h"
#include "devtools/terminal.h"
#include "game/game-state.h"

extern DuelManager gDuelManager;

void engineTickCamera(Engine& engine, float dt)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& freecamEnabled = FREECAM_ENABLED;
    GameState& gameState = GAME_STATE;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& gReplayChatStates = REPLAY_CHAT_STATES;
    auto& mpContext = MP_CONTEXT;

    if (!Terminal::instance().isOpen())
        applyDebugMovement(player, engine.window(), camera, dt);

    camera.decayPunch(dt);
    camera.updateVectors();

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();
    const bool replayFreecam =
        replayPlaybackActive &&
        gReplayPlayer.cameraController().mode() ==
            ReplayCameraMode::Freecam;
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
}
