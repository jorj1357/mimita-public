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
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "input/input-frame.h"
#include "input/input-poll.h"
#include "input/input-commands.h"
#include "input/scenario-input.h"
#include "audio/audio.h"
#include "config/weapon-hitfx-config.h"
#include "audio/hitmarker-audio.h"
#include "combat/death-system.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/melee-client.h"
#include "config/networking-config.h"
#include "debug/structured-log.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "ragdoll/ragdoll-mode.h"
#include "physics/config.h"
#include "physics/movement/physics-collision.h"
#include "replay/replay.h"
#include "gui/hud/chat-bubble.h"
#include "network/multiplayer-context.h"
#include "network/community-server-config.h"
#include "network/community-match-client.h"
#include "network/network-weapons.h"
#include "network/weapon-runtime-reconciliation.h"
#include "network/disagreement-visuals.h"
#include "network/reconnect-visuals.h"
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
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    auto& mpContext = MP_CONTEXT;
    auto& gReplayRecorder = REPLAY_RECORDER;
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
        float scheduledSide = 0.0f;
        float scheduledForward = 0.0f;
        if (consumeScheduledTerminalMoveForNetwork(scheduledSide, scheduledForward)) {
            mpInput.wishX = scheduledSide;
            mpInput.wishY = scheduledForward;
        }
        // Roll prediction is owned by the fixed-tick shared roll kernel in
        // physicsMainUpdate (see physics/movement/roll-movement.h). This render
        // path must not integrate the roll body again: mpInput already carries
        // the fixed-tick position/velocity, so a second integration here made
        // the actor move at render FPS and diverge from server authority.

        // Bounded local-player pose evidence for ANY avatar (griever or
        // humanoid): part world positions + orientations must change, proving
        // the mesh is animated/rotating and not stuck to world axes.
        {
            static uint32_t poseLogCounter = 0;
            if ((poseLogCounter++ % 60u) == 0u) {
                nlohmann::json parts = nlohmann::json::array();
                auto addPart = [&](const PhysicalBodyPart& part) {
                    const glm::quat q =
                        glm::normalize(glm::quat_cast(glm::mat3(part.worldTransform)));
                    parts.push_back({
                        {"name", part.name},
                        {"pos", {part.worldTransform[3].x,
                                 part.worldTransform[3].y,
                                 part.worldTransform[3].z}},
                        {"quat", {q.w, q.x, q.y, q.z}}});
                };
                for (size_t i = 0; i < player.physicalBody.parts.size() && i < 6; ++i)
                    addPart(player.physicalBody.parts[i]);
                for (size_t i = 0; i < player.physicalBody.extraParts.size() && i < 6; ++i)
                    addPart(player.physicalBody.extraParts[i]);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Avatar, StructuredLevel::Important,
                    "avatar.pose.live", player.avatarName(),
                    "local player part transforms",
                    (uint32_t)player.movementSimulationTick,
                    {{"avatar", player.avatarName()},
                     {"movement_mode", player.rig.movement.mode},
                     {"model_root_active", player.modelRootRotationActive},
                     {"model_root_quat", {player.modelRootRotation.w,
                                          player.modelRootRotation.x,
                                          player.modelRootRotation.y,
                                          player.modelRootRotation.z}},
                     {"parts", parts}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }

        mpInput.jumpHeld = cmd.isJumpHeld();
        mpInput.dashPressed = cmd.isDashPressed();
        mpInput.downDashPressed = cmd.isDownDashPressed();
        mpInput.freezeHeld = cmd.isFreezeHeld();
        // Melee: held shield intent (Q). The local Melee driver samples the key
        // and publishes it; the server runs the authoritative shield.
        mpInput.shieldHeld = MimitaMelee::shieldHeld();
        mpInput.crouchHeld =
            cmd.isCrouchHeld() || MimitaMelee::debugCrouchHeld();
        mpInput.flyUp = cmd.getState("freeze").held;
        mpInput.flyDown = cmd.getState("down_dash").held;
        mpInput.flyFast = cmd.getState("dash").held;
        mpInput.attackPressed = InputCommandSystem::instance().isKeyboardEnabled() &&
            glfwGetInputMode(engine.window(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED &&
            glfwGetMouseButton(engine.window(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        mpInput.equippedSlot = player.equippedSlot;
        if (mpContext.communityWeaponSetId > 0) {
            auto& cfg = MimitaNet::CommunityServerConfig::instance();
            if (cfg.weaponSets().empty()) cfg.load();
            const WeaponDefinition* current = weapons.getCurrentDef(player);
            if (current) {
                const int logical = cfg.slotForWeapon(mpContext.communityWeaponSetId, current->id);
                if (logical > 0) mpInput.equippedSlot = logical;
            }
        }
        mpInput.weaponState = weapons.networkVisualState(player);
        mpInput.sizeScale = player.sizeScale;
        {
            const auto& gb = weapons.godballPhysics();
        mpInput.godballPosition = gb.position;
        mpInput.godballVelocity = gb.velocity;
        mpInput.godballActive = gb.active;
    }

    // Recorded scenario playback feeds the same authoritative input packet as
    // live play. Only intent fields are replaced; current player state stays
    // live so server validation sees the real predicted position.
    if (ScenarioInput::active())
    {
        if (const InputFrame* recorded = ScenarioInput::frame())
        {
            mpInput.jumpHeld = recorded->jump;
            mpInput.dashPressed = recorded->dashPressed;
            mpInput.downDashPressed = recorded->downDashPressed;
            mpInput.freezeHeld = recorded->freezeHeld;
            mpInput.shieldHeld = recorded->shieldHeld;
            mpInput.crouchHeld = recorded->crouchHeld;
            mpInput.attackPressed = recorded->primaryHeld;
            mpInput.wishX = recorded->wishSide;
            mpInput.wishY = recorded->wishForward;
            mpInput.yaw = recorded->lookYaw;
            mpInput.lookPitch = recorded->lookPitch;
            mpInput.camForward = recorded->camForward;
            mpInput.equippedSlot = player.equippedSlot;
        }
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

            // Network duels use server-controlled respawn; local DuelManager is irrelevant here.
            const bool duelBlocksRespawn = false;

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

        // ── Godball: detect remote player hits and send claims ──────
        if (weapons.godballPhysics().active && mpContext.connected && mpContext.localPlayerId)
        {
            auto remoteHits = weapons.collectRemoteGodballHits(
                player, mpContext.remotePlayers, dt);
            for (const auto& hit : remoteHits)
            {
                if (!hit.targetIsRemotePlayer || hit.targetId == 0) continue;

                MimitaNet::GodballHitClaimPacket claim{};
                claim.header.type = MimitaNet::PACKET_GODBALL_HIT_CLAIM;
                claim.header.tick = mpContext.tick;
                claim.header.playerId = mpContext.localPlayerId;
                claim.attackerId = mpContext.localPlayerId;
                claim.targetId = hit.targetId;
                claim.contactSerial = 0;
                claim.simulationTick = mpContext.tick;
                claim.damage = hit.damage;
                claim.ballSpeed = glm::length(weapons.godballPhysics().velocity);
                claim.hitX = hit.end.x;
                claim.hitY = hit.end.y;
                claim.hitZ = hit.end.z;
                claim.normalX = hit.direction.x;
                claim.normalY = hit.direction.y;
                claim.normalZ = hit.direction.z;
                claim.spawnGeneration = mpContext.lastKnownSpawnGeneration;
                MimitaNet::mpSendPacket(mpContext, &claim, sizeof(claim));
            }
        }

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
                "reload-result",
                /*applyAmmo=*/false,
                /*allowReloadStateOverride=*/false,
                rr.header.tick);

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
                ar.accepted ? "attack-result-accepted" : "attack-result-rejected",
                /*applyAmmo=*/false,
                /*allowReloadStateOverride=*/true,
                ar.serverTick);
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
            if (glm::length(player.vel) > 0.001f || glm::length(player.externalImpulse) > 0.001f ||
                !mpContext.pendingKnockbacks.empty())
                Debug::warn(Debug::Category::Networking,
                    "[SpawnVelocityReset] player=%u spawnGen=%u epoch=%u pending=%zu -> zero\n",
                    mpContext.localPlayerId, spawn.spawnGeneration, spawn.transformEpoch,
                    mpContext.pendingKnockbacks.size());
            mpContext.pendingKnockbacks.clear();
            // Install the complete authoritative transform now, before the next
            // fixed simulation tick, so gameplay and the aim body can never
            // start from the temporary local fallback position.
            MimitaNet::mpApplyAuthoritativeTransform(mpContext, player);
            mpContext.lastAppliedEpoch = mpContext.localServerEpoch;
            mpContext.localPlayerReconciled = true;

            // The authoritative spawn packet is the single client-side new-life
            // boundary. Clear every local death/presentation flag here so a
            // fast server respawn cannot leave the render gate dead, and so a
            // delayed damage event from the previous life cannot re-kill it.
            {
                const bool wasDead = player.dead;
                const bool hadDeathState = player.dead || player.netPredictedDead ||
                    player.networkDeathPresented || player.currentHp <= 0;
                mpContext.pendingVictimHealth.clear();
                player.currentHp = spawn.health;
                if (spawn.health > player.maxHp)
                    player.maxHp = spawn.health;
                player.dead = false;
                player.netPredictedDead = false;
                player.networkDeathPresented = false;
                player.proceduralFrozen = false;
                player.deathAnim = Player::DeathAnimState{};
                player.respawnTimer = 0.0f;
                player.voidDeathTriggered = false;
                player.killedBy.clear();
                player.spawnGeneration = spawn.spawnGeneration;
                if (mpContext.awaitingExplodeDeath &&
                    mpContext.lastKnownSpawnGeneration !=
                        mpContext.explodeRequestStartSpawnGeneration)
                {
                    mpContext.awaitingExplodeDeath = false;
                    mpContext.explodeRequestLastSendMs = 0;
                }
                if (hadDeathState)
                {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Network, StructuredLevel::Important,
                        "client.local.spawn_applied", std::to_string(mpContext.localPlayerId),
                        "authoritative spawn packet cleared local death state",
                        mpContext.tick,
                        nlohmann::json{{"actor_id", mpContext.localPlayerId},
                                       {"spawn_generation", spawn.spawnGeneration},
                                       {"transform_epoch", spawn.transformEpoch},
                                       {"health", player.currentHp},
                                       {"dead_before", wasDead},
                                       {"dead_after", player.dead}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
            }

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

            if (spawn.equippedWeaponDefNetworkId != 0) {
                const std::string* weaponId = MimitaNet::weaponIdForDefNetworkId(
                    spawn.equippedWeaponDefNetworkId);
                const WeaponDefinition* def = weaponId
                    ? WeaponRegistry::instance().get(*weaponId) : nullptr;
                if (def && player.weaponRuntimes.find(def->id) != player.weaponRuntimes.end() &&
                    (mpContext.communityWeaponSetId <= 0 ||
                     MimitaNet::CommunityServerConfig::instance().weaponAllowed(
                         mpContext.communityWeaponSetId, def->id))) {
                    weapons.equip(player, def->slot);
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Weapons, StructuredLevel::Important,
                        "weapon.spawn.equipped_applied",
                        std::to_string(mpContext.localPlayerId), "authoritative-spawn",
                        mpContext.tick,
                        nlohmann::json{{"weapon", def->id},
                                       {"native_slot", def->slot},
                                       {"weapon_set", mpContext.communityWeaponSetId},
                                       {"weapon_def_network_id", spawn.equippedWeaponDefNetworkId},
                                       {"spawn_generation", spawn.spawnGeneration}});
                } else {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Weapons, StructuredLevel::Errors,
                        "weapon.spawn.equipped_apply_failed",
                        std::to_string(mpContext.localPlayerId),
                        "unknown-or-not-owned-authoritative-equipped-weapon",
                        mpContext.tick,
                        nlohmann::json{{"weapon_def_network_id", spawn.equippedWeaponDefNetworkId},
                                       {"weapon_set", mpContext.communityWeaponSetId},
                                       {"spawn_generation", spawn.spawnGeneration}});
                }
            }

            const auto& match = MimitaNet::CommunityMatchClient::instance();
            if (match.mode() == "juggernaut" &&
                match.fighterWeaponChoiceCommitted(mpContext.localPlayerId)) {
                const int choice = match.fighterWeaponChoice();
                const WeaponDefinition* selected =
                    WeaponRegistry::instance().get(MimitaNet::CommunityMatchClient::fighterWeaponId(choice));
                if (selected && player.weaponRuntimes.find(selected->id) != player.weaponRuntimes.end()) {
                    weapons.equip(player, selected->slot);
                    Debug::log(Debug::Category::Weapons,
                        "[JUGGERNAUT CHOICE APPLY] player=%u choice=%d weapon=%s nativeSlot=%d spawnGen=%u\n",
                        mpContext.localPlayerId, choice, selected->id.c_str(), selected->slot,
                        spawn.spawnGeneration);
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Weapons, StructuredLevel::Important,
                        "weapon.selection.applied", std::to_string(mpContext.localPlayerId),
                        "authoritative-spawn", mpContext.tick,
                        nlohmann::json{{"choice", choice}, {"weapon", selected->id},
                                       {"native_slot", selected->slot},
                                       {"spawn_generation", spawn.spawnGeneration}});
                } else {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Weapons, StructuredLevel::Errors,
                        "weapon.selection.apply_failed", std::to_string(mpContext.localPlayerId),
                        "missing-runtime-or-definition", mpContext.tick,
                        nlohmann::json{{"choice", choice},
                                       {"weapon", MimitaNet::CommunityMatchClient::fighterWeaponId(choice)},
                                       {"spawn_generation", spawn.spawnGeneration}});
                }
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
                // Arm the reliable retry: keep re-sending this ack until the
                // server's SpawnActivated confirms receipt (see mpTick).
                mpContext.pendingSpawnAckGeneration = spawn.spawnGeneration;
                mpContext.pendingSpawnAckEpoch = spawn.transformEpoch;
                mpContext.pendingSpawnAckLastSendMs = MimitaNet::nowMs();
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
            const uint64_t now = MimitaNet::nowMs();
            const bool mapLoadRetryDue = mpContext.lastMapLoadAttemptMs == 0 ||
                now - mpContext.lastMapLoadAttemptMs >= 500;
            const bool needsLoad = !mpContext.requiredMapId.empty() && worldLoaded &&
                !mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId) &&
                !mpContext.waitingForMapLoad && mapLoadRetryDue;

            if (needsLoad)
            {
                mpContext.waitingForMapLoad = true;
                mpContext.lastMapLoadAttemptMs = now;
                ++mpContext.mapLoadAttempts;
                std::string requiredPath = "assets/maps/" + mpContext.requiredMapId + ".glb";
                printf("[NET MAP REQUIRED] mapId=%s path=%s loadingMap=1\n",
                       mpContext.requiredMapId.c_str(), requiredPath.c_str());

                GAME_STATE = GAME_LOADING_MAP;
                bool loadOk = loadWorldFromGLB(world, requiredPath.c_str());
                if (loadOk)
                {
                    const std::string oldMap = ACTIVE_MAP_PATH;
                    ACTIVE_MAP_PATH = requiredPath;
                    WORLD_LOADED = true;
                    mpContext.mapLoadAttempts = 0;
                    printf("[CLIENT MAP SWITCH] old=%s new=%s attempts=%u\n",
                           oldMap.c_str(), requiredPath.c_str(), mpContext.mapLoadAttempts);
                    player.reset();
                    RagdollModeSystem::instance().clearCorpses();  // old map positions invalid
                }
                else
                {
                    printf("[CLIENT MAP ERROR] failed to load map=%s attempt=%u retryMs=500\n",
                           requiredPath.c_str(), mpContext.mapLoadAttempts);
                    // Fail visibly instead of silently continuing on the stale
                    // world: surface the missing asset to the player and stop
                    // retrying once the missing-asset condition is certain.
                    if (mpContext.mapLoadAttempts == 1 ||
                        mpContext.mapLoadAttempts == 10 ||
                        mpContext.mapLoadAttempts == 30)
                    {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::Network, StructuredLevel::Errors,
                            "network.client-map-load-failed", "",
                            "unable to load server-required map; not reporting ready",
                            mpContext.tick,
                            {{"required_map_id", mpContext.requiredMapId},
                             {"required_path", requiredPath},
                             {"load_attempts", mpContext.mapLoadAttempts}},
                            __FILE__, __LINE__, __FUNCTION__);
                    }
                    if (mpContext.mapLoadAttempts >= 30)
                    {
                        mpContext.connectionStatus =
                            "Map asset missing: " + mpContext.requiredMapId;
                        mpContext.lastMapLoadAttemptMs = now + 30000;  // stop retrying
                    }
                }
                GAME_STATE = GAME_PLAYING;
                mpContext.waitingForMapLoad = false;
            }

            // Send ClientMapReady only when the world actually loaded for the
            // server-required map. A failed map load must not be reported as
            // ready while the stale previous world is still active; otherwise
            // the server admits a client whose collision world does not match
            // and authoritative spawn positions become out-of-world.
            const bool mapReady = !mpContext.requiredMapId.empty() && worldLoaded &&
                mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId);
            if (!mpContext.requiredMapId.empty() && worldLoaded && !mapReady) {
                static uint64_t lastMapMismatchLogMs = 0;
                if (now - lastMapMismatchLogMs >= 1000) {
                    lastMapMismatchLogMs = now;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Network, StructuredLevel::Errors,
                        "network.client-map-not-ready", "",
                        "client world does not match server-required map",
                        mpContext.tick,
                        {{"required_map_id", mpContext.requiredMapId},
                         {"active_map_path", ACTIVE_MAP_PATH},
                         {"load_attempts", mpContext.mapLoadAttempts}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
            }
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

            bool staleKnockback = staleDamage;
            if (!localTarget && event.targetTransformEpoch != 0)
            {
                auto remoteIt = mpContext.remotePlayerInterpolation.find(event.targetPlayerId);
                if (remoteIt != mpContext.remotePlayerInterpolation.end() &&
                    remoteIt->second.lastSnapshotTransformEpoch != 0)
                    staleKnockback = event.targetTransformEpoch !=
                        remoteIt->second.lastSnapshotTransformEpoch;
            }
            if (staleKnockback)
                Debug::warn(Debug::Category::Networking,
                    "[Knockback] ignored old-life shot target=%u eventEpoch=%u\n",
                    event.targetPlayerId, event.targetTransformEpoch);

            const bool confirmedMeleeImpact =
                (event.effectFlags & MimitaNet::SHOT_EFFECT_MELEE_CONFIRM) != 0;
            if ((!localShooter ||
                 (confirmedMeleeImpact && localTarget)) &&
                !staleKnockback &&
                glm::length(event.knockback) > 0.001f)
            {
                if (localTarget) {
                    // The accepted Melee result is applied once for the local
                    // victim. Remote victims are corrected by snapshots; never
                    // add the same impulse to their local replica here.
                    if (!confirmedMeleeImpact ||
                        event.shotSerial > mpContext.lastAppliedMeleeImpulseEventId) {
                        player.externalImpulse += event.knockback;
                        if (confirmedMeleeImpact) {
                            mpContext.lastAppliedMeleeImpulseEventId = event.shotSerial;
                            StructuredLogger::instance().writeEvent(
                                StructuredCategory::Duel, StructuredLevel::Important,
                                "melee.client.impulse_applied",
                                std::to_string(event.shotSerial),
                                "accepted Melee impulse applied to local victim once",
                                event.eventServerTick,
                                nlohmann::json{{"event_id", event.shotSerial},
                                               {"attacker", event.shooterPlayerId},
                                               {"target", event.targetPlayerId},
                                               {"impulse", {event.knockback.x,
                                                            event.knockback.y,
                                                            event.knockback.z}}});
                        }
                    }
                } else if (!confirmedMeleeImpact) {
                    auto remote = mpContext.remotePlayers.find(
                        event.targetPlayerId);
                    if (remote != mpContext.remotePlayers.end())
                        remote->second.externalImpulse += event.knockback;
                }
            }
            if (!localShooter || confirmedMeleeImpact)
            {
                // The bullet always comes out of the shooter's gun ON THE
                // VIEWER'S SCREEN: compute the muzzle from the rendered body
                // (never the lagged authoritative snapshot).
                const glm::vec3 visualDelta =
                    MimitaNet::mpRemoteShooterRenderDelta(
                        mpContext, event.shooterPlayerId);
                const glm::vec3 muzzle =
                    MimitaNet::mpRemoteShooterMuzzle(
                        mpContext, event.shooterPlayerId,
                        event.origin + visualDelta);
                const bool continueAfterHit = NetworkingConfig::instance().data()
                    .combat.beamContinueAfterHit;
                // Beam endpoint: keep the server's fired direction but re-anchor
                // the start to the rendered muzzle; the beam extends to the
                // weapon's max range past the first hit.
                glm::vec3 beamEnd = event.hit;
                if (continueAfterHit && glm::length(event.beamEnd) > 0.001f)
                    beamEnd = muzzle + (event.beamEnd - event.origin);

                if (event.effectFlags & MimitaNet::SHOT_EFFECT_WEAPON_TRIGGER)
                {
                    auto remoteShooter = mpContext.remotePlayers.find(
                        event.shooterPlayerId);
                    if (remoteShooter != mpContext.remotePlayers.end())
                    {
                        remoteShooter->second.networkShootEffectTimer = 0.1f;
                        remoteShooter->second.networkWeaponState |= 1u;
                    }
                    else
                    {
                        auto remoteNpc = mpContext.remoteNpcs.find(
                            event.shooterPlayerId);
                        if (remoteNpc != mpContext.remoteNpcs.end())
                        {
                            remoteNpc->second.networkShootEffectTimer = 0.1f;
                            remoteNpc->second.networkWeaponState |= 1u;
                        }
                    }
                }

                if (!localShooter && !confirmedMeleeImpact) {
                    ReplayEffectEvent gunshotEvent;
                    gunshotEvent.type = "gunshot";
                    gunshotEvent.position = muzzle;
                    gunshotEvent.direction = event.direction;
                    gunshotEvent.from = muzzle;
                    gunshotEvent.to = event.hit; // exact server-confirmed hit
                    gunshotEvent.normal = event.normal;
                    gunshotEvent.sourceActorId = shooterName;
                    gunshotEvent.assetId = MimitaNet::networkWeaponTypeName(event.weapon);
                    captureReplayEffect(gunshotEvent);
                }

                if (event.effectFlags & MimitaNet::SHOT_EFFECT_MUZZLE)
                    EffectPartSystem::instance().spawnMuzzleFlash(muzzle, shooterName, 1.0f, MimitaNet::networkWeaponTypeName(event.weapon));
                if (event.effectFlags & MimitaNet::SHOT_EFFECT_TRACER)
                {
                    const char* weaponId = MimitaNet::networkWeaponTypeName(event.weapon);
                    EffectPart* tracer = EffectPartSystem::instance().spawnTracer(
                        muzzle, beamEnd, shooterName, 1.0f, weaponId);
                    auto& logger = StructuredLogger::instance();
                    if (logger.shouldLog(StructuredCategory::Rendering,
                                         StructuredLevel::Important))
                    {
                        logger.writeEvent(
                            StructuredCategory::Rendering, StructuredLevel::Important,
                            "presentation.tracer_spawn", std::to_string(event.shotSerial),
                            tracer ? "spawned" : "rejected_by_spawn_policy",
                            mpContext.tick,
                            nlohmann::json{
                                {"shooter", event.shooterPlayerId},
                                {"target", event.targetPlayerId},
                                {"local_target", localTarget},
                                {"weapon", weaponId},
                                {"spawned", tracer != nullptr},
                                {"camera_distance_to_origin",
                                 glm::length(muzzle - gpCamera->pos)},
                                {"camera_distance_to_hit",
                                 glm::length(event.hit - gpCamera->pos)},
                                {"beam_length", glm::length(beamEnd - muzzle)},
                                {"effect_flags", event.effectFlags}},
                            __FILE__, __LINE__, __FUNCTION__);
                    }
                }
                if (event.effectFlags &
                    MimitaNet::SHOT_EFFECT_SHOOT_SOUND)
                {
                    const char* shootSound = "revolvershoot";
                    if (event.weapon == MimitaNet::NETWORK_WEAPON_SHOTGUN ||
                        event.weapon == MimitaNet::NETWORK_WEAPON_AA12)
                        shootSound = "shotgunshoot";
                    else if (event.weapon == MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER)
                        shootSound = "rocketlauncher/rocketshoot";
                    else if (event.weapon == MimitaNet::NETWORK_WEAPON_GRENADE_LAUNCHER)
                        shootSound = "grenadelauncher/grenadelaunchershoot";
                    else if (event.weapon == MimitaNet::NETWORK_WEAPON_LARGE_MACHINE_GUN)
                        shootSound = "weapon/machinegun/machinegunshoot";
                    else if (event.weapon == MimitaNet::NETWORK_WEAPON_HEAVY_SHOTGUN)
                        shootSound = "weapon/heavyshotgun/heavyshotgunfire";
                    const float shootPitch =
                        event.weapon == MimitaNet::NETWORK_WEAPON_SHOTGUN
                            ? 0.8f
                            : event.weapon == MimitaNet::NETWORK_WEAPON_LARGE_MACHINE_GUN
                                ? 0.72f
                            : event.weapon == MimitaNet::NETWORK_WEAPON_HEAVY_SHOTGUN
                                ? 0.72f
                            : event.weapon == MimitaNet::NETWORK_WEAPON_REVOLVER
                                ? 0.9f
                                : 1.0f;
                    playWorldSoundOwned(
                        shootSound, event.origin + visualDelta,
                        1.0f, shootPitch, 80.0f,
                        audioOwnerKey(event.shooterPlayerId, shootSound));
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
                    EffectPartSystem::instance().spawnImpactSphereTickCfg(event.hit);
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
                    ev.spawnDamageNumber = !confirmedMeleeImpact ||
                        event.power > 0.5f;
                    if (confirmedMeleeImpact) {
                        ev.percentDisplay = true;
                        ev.percentAmount = event.power;
                    }
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
                    ev.spawnDamageNumber = !confirmedMeleeImpact ||
                        event.power > 0.5f;
                    if (confirmedMeleeImpact) {
                        ev.percentDisplay = true;
                        ev.percentAmount = event.power;
                    }
                    HitEffects::onHit(ev);
                }
                if (event.effectFlags &
                    MimitaNet::SHOT_EFFECT_HIT_SOUND)
                {
                    const auto& meleeSound =
                        WeaponHitFxConfig::instance().soundFor("captain_falcon_moveset");
                    const auto& meleePresentation =
                        WeaponHitFxConfig::instance().presentationFor("captain_falcon_moveset");
                    if (!confirmedMeleeImpact ||
                        (meleeSound.enabled && meleePresentation.enabled &&
                         meleePresentation.hitSound)) {
                        float dist = glm::length(event.hit - audioListenerPosition());
                        float vol, pit;
                        const float baseVolume = confirmedMeleeImpact
                            ? meleeSound.baseVolume * 0.55f : 1.2f;
                        computeImpactAudio(baseVolume, dist, 0.5f, vol, pit);
                        const char* hitEvent = event.impactType == MimitaNet::SHOT_IMPACT_WORLD
                            ? "hitworld" : "player_hurt";
                        playWorldSound(hitEvent, event.hit, vol, pit, 60.0f);
                        Debug::log(Debug::Category::Audio, "[%s AUDIO] dist=%.1f pitch=%.2f volume=%.2f\n",
                                   event.impactType == MimitaNet::SHOT_IMPACT_WORLD ? "WORLD IMPACT" : "HIT",
                                   dist, pit, vol);
                    }
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
                    // The reliable kill can arrive after the victim already
                    // respawned (instant respawn or delayed/lossy delivery).
                    // Re-running the death animation on the new life would
                    // freeze a corpse at the respawn position and hide the
                    // live body. Skip the kill when the victim's replica has
                    // advanced past the life this kill describes.
                    bool victimRespawnedSinceKill = false;
                    if (event.targetTransformEpoch != 0)
                    {
                        auto interpIt =
                            mpContext.remotePlayerInterpolation.find(
                                event.targetPlayerId);
                        if (interpIt !=
                                mpContext.remotePlayerInterpolation.end() &&
                            interpIt->second.lastSnapshotTransformEpoch >
                                (uint32_t)event.targetTransformEpoch)
                            victimRespawnedSinceKill = true;
                    }
                    if (!victimRespawnedSinceKill)
                    {
                        remote->second.netPredictedDead = false;
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
                    else
                    {
                        uint32_t currentEpoch = 0;
                        auto interpIt =
                            mpContext.remotePlayerInterpolation.find(
                                event.targetPlayerId);
                        if (interpIt !=
                                mpContext.remotePlayerInterpolation.end())
                            currentEpoch =
                                interpIt->second.lastSnapshotTransformEpoch;
                        printf("[NET KILL SKIP] target=%u killEpoch=%u "
                               "currentEpoch=%u reason=victim-already-respawned\n",
                               event.targetPlayerId,
                               (unsigned)event.targetTransformEpoch,
                               currentEpoch);
                    }
                }
            }
        }
        mpContext.shotEvents.clear();

        // Apply server-confirmed victim health in step with the shot visual.
        MimitaNet::mpDrainPendingVictimHealth(mpContext, player);

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
        for (const auto& pending : mpContext.pendingKnockbacks)
        {
            if (pending.targetSpawnGeneration != 0 &&
                pending.targetSpawnGeneration != mpContext.lastKnownSpawnGeneration)
            {
                Debug::warn(Debug::Category::Networking,
                    "[Knockback] ignored old-life queued target=%u eventGen=%u currentGen=%u source=%s\n",
                    mpContext.localPlayerId, pending.targetSpawnGeneration,
                    mpContext.lastKnownSpawnGeneration, pending.source.c_str());
                continue;
            }
            player.externalImpulse += pending.impulse;
            Debug::log(Debug::Category::Networking,
                "[NET KNOCKBACK APPLY] player=%u impulse=(%.2f,%.2f,%.2f) source=%s\n",
                mpContext.localPlayerId, pending.impulse.x, pending.impulse.y,
                pending.impulse.z, pending.source.c_str());
        }
        mpContext.pendingKnockbacks.clear();

        // Input is sent inside mpTick() above — do NOT send a second packet here.
        // The engine-tick-net.cpp duplicate InputPacket has been removed.
        // Position, velocity, yaw, look, and wish are all captured together inside mpTick's MpInput.

        if (!InputCommandSystem::instance().isKeyboardEnabled()) {
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

        // Observer-facing disconnect/reconnect effects for remote players:
        // keeps red beams and the ticking "connection lost" label alive above
        // any frozen body, and cleans up when the entity is removed.
        MimitaNet::mpUpdateReconnectVisuals(mpContext, dt);

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
