#include "engine/engine-tick-net.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <string>
#include <unordered_set>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "input/input-frame.h"
#include "input/input-poll.h"
#include "input/input-commands.h"
#include "audio/audio.h"
#include "audio/hitmarker-audio.h"
#include "combat/death-system.h"
#include "combat/weapon-system.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "replay/replay.h"
#include "gui/hud/chat-bubble.h"
#include "network/multiplayer-context.h"
#include "network/disagreement-visuals.h"
#include "render/render-player.h"
#include "perf/perf.h"
#include "debug/debug-log.h"
#include "game/game-state.h"
#include "world/world-gltf-loader.h"
#include "terminal/terminal-state.h"

extern DuelManager gDuelManager;

void engineTickNet(Engine& engine, float dt)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    auto& mpContext = MP_CONTEXT;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayClipSaver = REPLAY_CLIP_SAVER;
    auto& gReplayFactory = REPLAY_FACTORY;

    { Perf::ScopedTimer _net("Networking");
    if (mpContext.active) {
        // ── Map synchronization ──────────────────────────────────────────
        // If server requires a different map, load it before processing gameplay
        if (!mpContext.requiredMapId.empty() && worldLoaded &&
            mpContext.requiredMapId != "funworldv3" &&
            ACTIVE_MAP_PATH.find(mpContext.requiredMapId) == std::string::npos)
        {
            std::string requiredPath = "assets/maps/" + mpContext.requiredMapId + ".glb";
            printf("[NET MAP REQUIRED] mapId=%s path=%s\n",
                   mpContext.requiredMapId.c_str(), requiredPath.c_str());

            if (loadWorldFromGLB(world, requiredPath.c_str()))
            {
                ACTIVE_MAP_PATH = requiredPath;
                WORLD_LOADED = true;
                printf("[CLIENT MAP SWITCH] old=%s new=%s\n",
                       ACTIVE_MAP_PATH.c_str(), requiredPath.c_str());
                player.reset();
            }
            else
            {
                printf("[CLIENT MAP ERROR] failed to load map=%s\n", requiredPath.c_str());
            }
        }

        MimitaNet::MpInput mpInput;
        mpInput.position = player.pos;
        mpInput.velocity = player.vel;
        mpInput.yaw = camera.yaw;
        mpInput.camForward = camera.front;
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
        mpInput.sizeScale = player.sizeScale;
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
                    player.externalImpulse += event.knockback;
                else
                {
                    auto remote = mpContext.remotePlayers.find(
                        event.targetPlayerId);
                    if (remote != mpContext.remotePlayers.end())
                        remote->second.externalImpulse += event.knockback;
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
                    float dist = glm::length(event.hit - audioListenerPosition());
                    float vol, pit;
                    computeImpactAudio(1.2f, dist, 0.5f, vol, pit);
                    const char* hitEvent = event.impactType == MimitaNet::SHOT_IMPACT_WORLD
                        ? "hitworld" : "player_hurt";
                    playWorldSound(hitEvent, event.hit, vol, pit, 60.0f);
                    Debug::log(Debug::Category::Audio, "[%s AUDIO] dist=%.1f pitch=%.2f volume=%.2f\n",
                               event.impactType == MimitaNet::SHOT_IMPACT_WORLD ? "WORLD IMPACT" : "HIT",
                               dist, pit, vol);
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
            for (auto it = spawnedNpcIds.begin(); it != spawnedNpcIds.end(); ) {
                if (mpContext.remoteNpcs.find(*it) == mpContext.remoteNpcs.end()) {
                    npcSystem.destroySelected({*it});
                    it = spawnedNpcIds.erase(it);
                } else {
                    ++it;
                }
            }
        }

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

        mpContext.showPlayerList = glfwGetKey(engine.window(), GLFW_KEY_TAB) == GLFW_PRESS;
        static bool f3Prev = false;
        bool f3Down = glfwGetKey(engine.window(), GLFW_KEY_F3) == GLFW_PRESS;
        if (f3Down && !f3Prev)
            mpContext.showDebugOverlay = !mpContext.showDebugOverlay;
        f3Prev = f3Down;

        // Process server disagreement events (spawn visual effects)
        // Events are consumed once and cleared to prevent re-spawning every frame.
        if (!mpContext.disagreementEvents.empty())
        {
            for (const auto& event : mpContext.disagreementEvents)
            {
                MimitaNet::spawnDisagreementEffect(event);
                MimitaNet::logDisagreement(event);
            }
            mpContext.disagreementEvents.clear();
        }

    }
    } // Perf::ScopedTimer Networking
}

// ── Ghost rendering (called from render stage, outside networking scope) ───
// The ghost state is stored in mpContext and rendered here.
void engineRenderGhost(const Player& localPlayer, const Camera& camera)
{
    auto& mpContext = MP_CONTEXT;
    if (!mpContext.active || !mpContext.showServerGhost || !mpContext.hasLocalServerPosition)
        return;

    static Player serverGhost;
    static bool ghostInitialized = false;

    if (!ghostInitialized)
    {
        serverGhost = localPlayer;
        serverGhost.username = "SERVER POSITION";
        ghostInitialized = true;
        printf("[SERVER GHOST] enabled=1\n");
    }

    serverGhost.pos = mpContext.localServerPosition;
    serverGhost.vel = mpContext.localServerVelocity;
    serverGhost.yaw = mpContext.localServerYaw;
    serverGhost.renderGhost = true;
    serverGhost.dead = false;

    // Render the ghost — this is a separate render pass
    renderPlayer(serverGhost, camera);

    printf("[SERVER GHOST RENDER] pos=(%.1f,%.1f,%.1f) tick=%u\n",
           serverGhost.pos.x, serverGhost.pos.y, serverGhost.pos.z,
           mpContext.latestServerTick);
}
