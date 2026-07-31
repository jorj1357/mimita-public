// 07 21 2026, 17 10
/* purpose
* Owns per-frame multiplayer integration between engine state, local player state, and network tick.
* Builds client movement input packets from shared physics results and presentation serials.
* Applies network reconciliation, weapon result processing, and remote network updates.
* Does NOT define packet schemas, server movement validation, or authoritative damage rules.
* Does NOT run the main physics simulation or own renderer/world loading code.
* Does NOT invent movement availability separate from Player shared movement state.
*/

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
#include "combat/weapon-registry.h"
#include "debug/structured-log.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "replay/replay.h"
#include "gui/hud/chat-bubble.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "network/weapon-runtime-reconciliation.h"
#include "network/disagreement-visuals.h"
#include "render/render-player.h"
#include "perf/perf.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
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
        // Reconcile before building outgoing input so the packet carries the
        // latest authoritative position.  This also applies pending epoch
        // changes, respawns, and teleports before we snapshot local state.
        MimitaNet::mpReconcileLocalPlayer(mpContext, player, dt);

        // Network tick must run every frame regardless of map loading state,
        // so heartbeats, keepalives, and packet receive continue uninterrupted.
        MimitaNet::MpInput mpInput;
        mpInput.position = player.pos;
        mpInput.velocity = player.vel;
        mpInput.externalImpulse = player.externalImpulse;
        mpInput.yaw = camera.yaw;
        mpInput.lookPitch = camera.pitch;
        mpInput.camForward = camera.front;
        mpInput.movementSimulationTick = player.movementSimulationTick;
        mpInput.onGround = player.ground.onGround;
        mpInput.stableOnGround = player.ground.stableOnGround;
        mpInput.hasWorldContact = player.ground.hasWorldContact;
        mpInput.realWorldContactThisFrame = player.ground.realWorldContactThisFrame;
        mpInput.airJumpArmed = player.jump.airJumpArmed;
        mpInput.airJumpLocked = player.jump.airJumpLocked;
        mpInput.dashAvailable = player.dash.dashAvailable;
        mpInput.dashMomentumProtectionActive = player.dash.momentumProtectionActive;
        mpInput.downDashAvailable = player.dash.downDashAvailable;
        mpInput.freezeActive = player.freeze.freezeActive;
        mpInput.freezeAvailable = player.freeze.freezeAvailable;
        mpInput.groundReturnAvailable = player.groundReturn.available;
        const auto& cmd = InputCommandSystem::instance();
        mpInput.wishX = 0.0f;
        mpInput.wishY = 0.0f;
        if (cmd.getState("walkforward").held) mpInput.wishY += 1.0f;
        if (cmd.getState("walkback").held)    mpInput.wishY -= 1.0f;
        if (cmd.getState("walkleft").held)    mpInput.wishX -= 1.0f;
        if (cmd.getState("walkright").held)   mpInput.wishX += 1.0f;
        mpInput.jumpHeld = cmd.isJumpHeld();
        mpInput.dashPressed = cmd.isDashPressed();
        mpInput.downDashPressed = cmd.isDownDashPressed();
        mpInput.freezeHeld = cmd.isFreezeHeld();
        mpInput.attackPressed = !Terminal::instance().isOpen() &&
            glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        mpInput.equippedSlot = player.equippedSlot;
        mpInput.weaponState = weapons.networkVisualState(player);
        mpInput.sizeScale = player.sizeScale;
        {
            const auto& gb = weapons.godballPhysics();
            mpInput.godballPosition = gb.position;
            mpInput.godballActive = gb.active;
        }

        // Detect canonical gameplay events and increment serials.
        // These flags are set by physicsMainUpdate() -> physicsMainUpdate_Internal()
        // in simulateTick (called from engineTickReplay, which runs before this tick).
        // They remain true until player.updateAudio() consumes them in engineTickCombat.
        // Using actual gameplay flags ensures we only replicate *accepted* events,
        // not rejected input attempts (e.g. dash on cooldown).
        {
            static int prevEquipSlot = 0;
            static glm::vec2 prevWish{0.0f, 0.0f};
            if (player.dash.didDash)
                mpContext.nextLocalDashSerial++;
            if (player.jump.didGroundJump)
                mpContext.nextLocalGroundJumpSerial++;
            if (player.jump.didAirJump)
                mpContext.nextLocalAirJumpSerial++;
            if (player.dash.didDownDash)
                mpContext.nextLocalDownDashSerial++;
            if (player.freeze.didFreeze)
                mpContext.nextLocalFreezeSerial++;
            // Detect movement direction change: net wish direction change
            // while moving, or first movement press while idle.
            const glm::vec2 curWish(mpInput.wishX, mpInput.wishY);
            const float curWishLen = glm::length(curWish);
            const float prevWishLen = glm::length(prevWish);
            if (curWishLen > 0.001f)
            {
                // Movement key(s) held. Detect direction change.
                if (prevWishLen < 0.001f || glm::dot(glm::normalize(curWish), glm::normalize(prevWish)) < 0.85f)
                    mpContext.nextLocalMovementDirectionSerial++;
            }
            prevWish = curWish;
            if (mpInput.equippedSlot != prevEquipSlot)
                mpContext.nextLocalEquipSerial++;
            prevEquipSlot = mpInput.equippedSlot;
        }

        // ── Instant respawn request ──────────────────────────────────────
        // Detect Space input separately from gameplay events.  Supports:
        //   * Pressing Space after death
        //   * Holding Space when death occurs
        //   * Buffered Space within 250 ms before death becomes authoritative
        {
            static bool prevSpaceHeld = false;
            static bool wasOnlineDead = false;
            static uint64_t lastSpacePressMs = 0;

            const bool spaceHeld = mpInput.jumpHeld;  // Space key
            const bool spacePressed = spaceHeld && !prevSpaceHeld;

            // Determine online death state using both local presentation and server health
            const bool onlineDead = player.dead || player.currentHp <= 0 ||
                (mpContext.connected && mpContext.localServerHealth <= 0);
            const bool justBecameOnlineDead = onlineDead && !wasOnlineDead;

            constexpr uint64_t RESPAWN_INPUT_BUFFER_MS = 250;
            const bool spaceBuffered = !spaceHeld && !spacePressed &&
                (MimitaNet::nowMs() - lastSpacePressMs < RESPAWN_INPUT_BUFFER_MS) &&
                justBecameOnlineDead;

            const bool duelBlocksRespawn = false; // simplified; can check gDuelManager

            const bool shouldRequestRespawn = onlineDead && !duelBlocksRespawn &&
                (spacePressed || (justBecameOnlineDead && spaceHeld) || spaceBuffered);

            if (shouldRequestRespawn && mpContext.pendingRespawnSerial == 0)
            {
                mpContext.pendingRespawnSerial = mpContext.nextLocalRespawnSerial++;
                if (mpContext.nextLocalRespawnSerial == 0)
                    mpContext.nextLocalRespawnSerial = 1;
                mpContext.pendingRespawnStartEpoch = mpContext.localServerEpoch;
                mpContext.pendingRespawnStartedMs = MimitaNet::nowMs();

                const char* reason = spacePressed ? "space-press" :
                    (spaceHeld ? "held-on-death" : "buffered-press");
                printf("[CLIENT RESPAWN CREATE] playerId=%u serial=%u startEpoch=%u "
                       "localSnapshotTick=%u reason=%s\n",
                       mpContext.localPlayerId, mpContext.pendingRespawnSerial,
                       mpContext.localServerEpoch, mpContext.latestLocalSnapshotTick, reason);
            }

            if (spacePressed)
                lastSpacePressMs = MimitaNet::nowMs();

            prevSpaceHeld = spaceHeld;
            wasOnlineDead = onlineDead;
        }

        MimitaNet::mpTick(mpContext, player.username, dt, &mpInput, world);
        if (!mpContext.approvedLocalName.empty())
            player.username = mpContext.approvedLocalName;

        // ── Ammo refund for rejected projectile fire requests ───────────
        // Refund goes to the weapon that sent the request, not the currently equipped weapon.
        for (const auto& rej : mpContext.fireRejections)
        {
            const std::string weaponId = MimitaNet::networkWeaponTypeName(rej.weapon);
            if (weaponId == "unknown" || weaponId == "none") continue;
            const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
            if (!def) continue;
            auto rtIt = player.weaponRuntimes.find(weaponId);
            if (rtIt != player.weaponRuntimes.end())
            {
                float ammoBefore = (float)rtIt->second.currentAmmo;
                float cdBefore = rtIt->second.fireCooldown;

                rtIt->second.currentAmmo = std::min(rtIt->second.currentAmmo + 1, def->magazineSize);

                // For cooldown rejection: preserve server cooldown, never zero it
                if (rej.reason == MimitaNet::PROJECTILE_FIRE_COOLDOWN && rej.cooldownRemaining > 0.0f)
                {
                    rtIt->second.fireCooldown = std::max(rtIt->second.fireCooldown, rej.cooldownRemaining);
                }
                else if (rej.reason != 255)
                {
                    // Non-timeout, non-cooldown rejection: cooldown stays as-is
                }
                // Timeout (reason==255): refund once, don't force cooldown to zero

                {
                    auto& _lg = ::StructuredLogger::instance();
                    if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
                        ::StructuredLogger::Entry e;
                        e.category = ::StructuredCategory::GrenadeLauncher;
                        e.level = ::StructuredLevel::Important;
                        e.eventId = "GRENADE_AMMO_REFUND";
                        e.correlationId = "GRENADE_P" + std::to_string(mpContext.localPlayerId)
                            + "_F" + std::to_string(rej.fireSerial) + "_J0";
                        e.reason = "refund";
                        char b[256]; std::snprintf(b, sizeof(b),
                            "fireSerial=%u weapon=%s reason=%u ammoBefore=%.0f ammoAfter=%d "
                            "cdBefore=%.3f cdAfter=%.3f serverCd=%.2f",
                            rej.fireSerial, weaponId.c_str(), rej.reason,
                            ammoBefore, rtIt->second.currentAmmo,
                            cdBefore, rtIt->second.fireCooldown,
                            rej.cooldownRemaining);
                        e.message = b;
                        _lg.write(e);
                    }
                }
            }
        }
        mpContext.fireRejections.clear();

        // ── Process pending reload results ────────────────────────────────
        for (const auto& rr : mpContext.pendingReloadResults)
        {
            Debug::log(Debug::Category::Weapons, "[RELOAD PROCESS] playerId=%u requestId=%u accepted=%d reason=%d ammo=%d/%d stateRev=%u\n",
                       mpContext.localPlayerId, rr.requestId, (int)rr.accepted, (int)rr.reason,
                       rr.magazineAmmo, rr.reserveAmmo, rr.stateRevision);

            MimitaNet::reconcileAuthoritativeWeaponRuntime(
                mpContext, player, rr.weaponDefNetworkId,
                rr.magazineAmmo, rr.reserveAmmo,
                rr.nextAllowedFireTick,
                rr.reloading != 0,
                rr.reloadCompleteTick,
                rr.stateRevision,
                rr.spawnGeneration,
                "reload-result");

            auto pendingIt = mpContext.pendingReloadRequests.find(rr.requestId);
            if (pendingIt != mpContext.pendingReloadRequests.end())
            {
                mpContext.pendingReloadRequests.erase(pendingIt);
                Debug::log(Debug::Category::Weapons, "[RELOAD PENDING CLEAR] requestId=%u\n", rr.requestId);
            }
        }
        mpContext.pendingReloadResults.clear();

        // ── Process pending attack results ────────────────────────────────
        for (const auto& ar : mpContext.pendingAttackResults)
        {
            const std::string* weaponId = MimitaNet::weaponIdForDefNetworkId(ar.weaponDefNetworkId);
            bool keepReloading = false;
            if (weaponId)
            {
                auto rtIt = player.weaponRuntimes.find(*weaponId);
                if (rtIt != player.weaponRuntimes.end())
                    keepReloading = rtIt->second.isReloading;
            }
            MimitaNet::reconcileAuthoritativeWeaponRuntime(
                mpContext, player, ar.weaponDefNetworkId,
                ar.magazineAmmo, ar.reserveAmmo,
                ar.nextAllowedFireTick,
                keepReloading,
                0,
                ar.stateRevision,
                ar.spawnGeneration,
                ar.accepted ? "attack-result-accepted" : "attack-result-rejected");
        }
        mpContext.pendingAttackResults.clear();

        // ── Process pending authoritative spawn ───────────────────────────
        if (mpContext.pendingAuthoritativeSpawn.has_value())
        {
            const auto& spawn = *mpContext.pendingAuthoritativeSpawn;
            Debug::log(Debug::Category::Weapons, "[SPAWN AUTHORITATIVE APPLY] playerId=%u spawnGen=%u epoch=%u health=%d weapons=%u\n",
                       mpContext.localPlayerId, spawn.spawnGeneration, spawn.transformEpoch,
                       spawn.health, spawn.weaponCount);

            // Reset temporary local weapon-runtime state before applying authoritative entries
            player.weaponRuntimes.clear();

            // Process every valid authoritative weapon entry via the canonical reconciler
            for (uint8_t i = 0; i < spawn.weaponCount; ++i)
            {
                const auto& slot = spawn.weapons[i];
                Debug::log(Debug::Category::Weapons, "[SPAWN WEAPON ENTRY] idx=%u defId=%u mag=%d res=%d nextFire=%llu reload=%d rev=%u\n",
                           i, slot.weaponDefNetworkId, slot.magazineAmmo, slot.reserveAmmo,
                           (unsigned long long)slot.nextAllowedFireTick, (int)slot.reloading, slot.stateRevision);

                MimitaNet::reconcileAuthoritativeWeaponRuntime(
                    mpContext, player,
                    slot.weaponDefNetworkId,
                    slot.magazineAmmo,
                    slot.reserveAmmo,
                    slot.nextAllowedFireTick,
                    slot.reloading != 0,
                    0, // reloadCompleteTick not in spawn packet; timer from 0 if reloading
                    slot.stateRevision,
                    spawn.spawnGeneration,
                    "spawn-packet");
            }

            // Send SpawnAck only after authoritative state is installed
            if (mpContext.active && mpContext.localPlayerId)
            {
                MimitaNet::SpawnAckPacket ack{};
                ack.header.type = MimitaNet::PACKET_SPAWN_ACK;
                ack.header.tick = mpContext.tick;
                ack.header.playerId = mpContext.localPlayerId;
                ack.spawnGeneration = spawn.spawnGeneration;
                ack.transformEpoch = spawn.transformEpoch;
                MimitaNet::mpSendPacket(mpContext, &ack, sizeof(ack));
                Debug::log(Debug::Category::Weapons, "[SPAWN ACK SEND] playerId=%u spawnGen=%u epoch=%u (after weapon reconciliation)\n",
                           mpContext.localPlayerId, ack.spawnGeneration, ack.transformEpoch);
            }

            mpContext.pendingAuthoritativeSpawn.reset();
        }

        // ── Map synchronization ──────────────────────────────────────────
        // Load server-required map without blocking networking.
        // mpTick() already ran above — heartbeats and packets keep flowing.
        {
            using namespace MimitaNet;
            const bool needsLoad = !mpContext.requiredMapId.empty() && worldLoaded &&
                !mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId) &&
                !mpContext.waitingForMapLoad;

            if (needsLoad)
            {
                mpContext.waitingForMapLoad = true;
                std::string requiredPath = "assets/maps/" + mpContext.requiredMapId + ".glb";
                printf("[NET MAP REQUIRED] mapId=%s path=%s loadingMap=1\n",
                       mpContext.requiredMapId.c_str(), requiredPath.c_str());

                GAME_STATE = GAME_LOADING_MAP;
                bool loadOk = loadWorldFromGLB(world, requiredPath.c_str());
                if (loadOk)
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
                GAME_STATE = GAME_PLAYING;
                mpContext.waitingForMapLoad = false;
            }

            // Send ClientMapReady whenever the required map is ready,
            // regardless of whether it was already loaded or just switched.
            const bool mapReady = !mpContext.requiredMapId.empty() && worldLoaded;
            const bool readyAlreadySent =
                mpContext.clientMapReadySent &&
                mpContext.clientMapReadySentForPlayerId == mpContext.localPlayerId &&
                mapIdsReferToSameMap(mpContext.clientMapReadySentForMap, mpContext.requiredMapId);

            if (mpContext.connected && mpContext.localPlayerId != 0 &&
                mapReady && !readyAlreadySent)
            {
                std::string normalizedRequired = normalizeMapId(mpContext.requiredMapId);
                ClientMapReadyPacket ready;
                ready.header.type = PACKET_CLIENT_MAP_READY;
                ready.header.tick = mpContext.tick;
                ready.header.playerId = mpContext.localPlayerId;
                ready.assignedPlayerId = mpContext.localPlayerId;
                size_t mapIdLen = normalizedRequired.copy(ready.mapId, sizeof(ready.mapId) - 1);
                ready.mapId[mapIdLen] = '\0';
                mpSendPacket(mpContext, &ready, sizeof(ready));

                mpContext.clientMapReadySent = true;
                mpContext.clientMapReadySentForPlayerId = mpContext.localPlayerId;
                mpContext.clientMapReadySentForMap = mpContext.requiredMapId;

                printf("[CLIENT MAP READY] sent playerId=%u mapId=%s reason=%s\n",
                       mpContext.localPlayerId, ready.mapId,
                       mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId)
                           ? "already-loaded" : "load-completed");
            }
        }

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

            // ── Stale damage rejection by epoch ─────────────────────────
            // Damage events from a previous life are discarded to prevent
            // re-killing a newly respawned player.
            const bool staleDamage =
                event.damageConfirmed &&
                localTarget &&
                event.targetTransformEpoch != 0 &&
                mpContext.localServerEpoch != 0 &&
                event.targetTransformEpoch != mpContext.localServerEpoch;

            if (staleDamage)
            {
                printf("[CLIENT DAMAGE DROP] target=%u eventEpoch=%u currentEpoch=%u "
                       "eventSerial=%u reason=old-life\n",
                       event.targetPlayerId, event.targetTransformEpoch,
                       mpContext.localServerEpoch, event.shotSerial);
            }

            printf("[NET SHOT APPLY] shooter=%u serial=%u target=%u "
                   "damage=%d weapon=%u impact=%u damageConfirmed=%d "
                   "targetEpoch=%u staleEpoch=%d\n",
                   event.shooterPlayerId, event.shotSerial,
                   event.targetPlayerId, event.damage, event.weapon,
                   event.impactType, (int)event.damageConfirmed,
                   event.targetTransformEpoch, (int)staleDamage);

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
                    const char* shootSound = "revolvershoot";
                    if (event.weapon == MimitaNet::NETWORK_WEAPON_SHOTGUN ||
                        event.weapon == MimitaNet::NETWORK_WEAPON_AA12)
                        shootSound = "shotgunshoot";
                    playWorldSound(
                        shootSound, event.origin,
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
                    EffectPartSystem::instance().spawnImpactSphereTick(event.hit, {0.1f, 0.5f, 1.0f});
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

        // Consume pending projectile knockback for local player
        if (glm::length(mpContext.pendingKnockback) > 0.001f)
        {
            player.externalImpulse += mpContext.pendingKnockback;
            printf("[NET KNOCKBACK APPLY] player=%u impulse=(%.2f,%.2f,%.2f) "
                   "source=%s pos=(%.2f,%.2f,%.2f)\n",
                   mpContext.localPlayerId,
                   mpContext.pendingKnockback.x, mpContext.pendingKnockback.y,
                   mpContext.pendingKnockback.z,
                   mpContext.pendingKnockbackSource.c_str(),
                   player.pos.x, player.pos.y, player.pos.z);
            mpContext.pendingKnockback = glm::vec3(0.0f);
            mpContext.pendingKnockbackSource.clear();
        }

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

        // Input is sent inside mpTick() above — do NOT send a second packet here.
        // The engine-tick-net.cpp duplicate InputPacket has been removed.
        // Position, velocity, yaw, look, and wish are all captured together inside mpTick's MpInput.

        if (Terminal::instance().isOpen()) {
            mpContext.showPlayerList = false;
        } else {
            mpContext.showPlayerList = glfwGetKey(engine.window(), GLFW_KEY_TAB) == GLFW_PRESS;
            static bool f3Prev = false;
            bool f3Down = glfwGetKey(engine.window(), GLFW_KEY_F3) == GLFW_PRESS;
            if (f3Down && !f3Prev)
                mpContext.showDebugOverlay = !mpContext.showDebugOverlay;
            f3Prev = f3Down;
        }

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
