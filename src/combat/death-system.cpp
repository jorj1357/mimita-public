#include "combat/death-system.h"
#include "combat/weapon-runtime.h"

#include <algorithm>
#include <cmath>

#include <glad/glad.h>
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/transform-debug.h"
#include "debug/gl-debug.h"
#include "camera.h"
#include "audio/audio.h"
#include "input/input-state.h"
#include "npc/npc.h"
#include "physics/config.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "render/render-player.h"
#include "renderer/renderer.h"
#include "replay/replay.h"
#include "world/world.h"
#include "game/duel.h"
#include "game/spawn-utils.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "killfeed/killfeed.h"

extern DuelManager gDuelManager;

extern Renderer* gRenderer;

namespace {
constexpr float RESPAWN_SECONDS = 3.0f;
constexpr float CORPSE_STAGE1_SECONDS = 5.0f;
constexpr float CORPSE_STAGE2_SECONDS = 1.0f;
constexpr float CORPSE_TOTAL_SECONDS = 6.0f;
constexpr float DEAD_WORLD_FLOOR = -500.0f;

#define DEAD_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[DEAD] " fmt "\n", ##__VA_ARGS__); } while(0)

void emitLifecycleEvent(const char* type,
                        const Player& actor,
                        const std::string& actorId,
                        const std::string& otherActorId)
{
    ReplayEffectEvent event;
    event.type = type;
    event.position = actor.pos;
    event.direction = actor.vel;
    event.sourceActorId = otherActorId;
    event.targetActorId = actorId;
    captureReplayEffect(event);
}
}

DeathSystem& DeathSystem::instance()
{
    static DeathSystem system;
    return system;
}

bool DeathSystem::kill(
    Player& victim,
    const std::string& actorId,
    const std::string& actorType,
    const std::string& killer,
    const glm::vec3& shotDirection,
    float lethalForce)
{
    if (victim.dead)
        return false;

    glm::vec3 direction = glm::length(shotDirection) > 0.001f
        ? glm::normalize(shotDirection)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    // Step 1: capture pre-death momentum BEFORE freezing
    glm::vec3 linearVel = victim.vel;
    glm::vec3 externalVel = victim.externalImpulse;
    glm::vec3 victimPos = victim.pos;
    glm::quat victimRotation = glm::angleAxis(glm::radians(victim.yaw), glm::vec3(0.0f, 0.0f, 1.0f));

    // Step 2: freeze the victim — stop all animation/control
    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.inputWishMove = glm::vec2(0.0f);
    victim.currentHp = 0;
    victim.dead = true;
    victim.proceduralFrozen = true;
    victim.syncLegacyStateToLayers();

    // Step 3: disable weapon/aim/procedural pose before capturing final state.
    for (PhysicalBodyPart& part : victim.physicalBody.parts) {
        if (part.name == "leftArm" || part.name == "rightArm") {
            part.pose = ProceduralPose{};
            part.perfectPose = ProceduralPose{};
            part.translationSpring = SpringState{};
            part.rotationSpring = SpringState{};
        }
    }
    victim.syncLegacyStateToLayers();

    // Capture final body state (neutral pose, no weapon offsets)
    victim.updateModelWorldTransforms();

    printf("[DEATH] victim=%s hitDir=(%.2f %.2f %.2f) damage=%.0f\n",
           victim.username.c_str(), direction.x, direction.y, direction.z, lethalForce);

    DeadBody body;
    body.id = actorId + "_corpse_" + std::to_string(++mCorpseSerial);
    body.name = victim.username + " corpse";
    body.spawnPosition = victimPos;
    body.transferredVelocity = linearVel;
    body.deathImpulse = direction * lethalForce;

    body.debugId = mCorpseSerial;
    body.debugDeathDir = direction;

    if (DebugConfig::DEBUG_NPC_DEATH) {
        Debug::log(Debug::Category::Ragdoll, "[RAGDOLL SPAWN]\n");
        Debug::log(Debug::Category::Ragdoll, "  id=%s\n", body.id.c_str());
        Debug::log(Debug::Category::Ragdoll, "  deathPos=(%.2f %.2f %.2f)\n", victimPos.x, victimPos.y, victimPos.z);
        Debug::log(Debug::Category::Ragdoll, "  deathVel=(%.2f %.2f %.2f)\n", linearVel.x, linearVel.y, linearVel.z);
        Debug::log(Debug::Category::Ragdoll, "  deathSpeed=%.2f\n", glm::length(linearVel));
        Debug::log(Debug::Category::Ragdoll, "  deathImpulse=(%.2f %.2f %.2f)\n",
               body.deathImpulse.x, body.deathImpulse.y, body.deathImpulse.z);
        Debug::log(Debug::Category::Ragdoll, "  deathDir=(%.2f %.2f %.2f)\n", direction.x, direction.y, direction.z);
        Debug::log(Debug::Category::Ragdoll, "  lethalForce=%.1f\n", lethalForce);
        Debug::log(Debug::Category::Ragdoll, "  corpseInitialVel=(%.2f %.2f %.2f)\n",
               body.velocity.x, body.velocity.y, body.velocity.z);
        Debug::log(Debug::Category::Ragdoll, "  corpseAngVel=(%.2f %.2f %.2f)\n",
               body.angularVelocity.x, body.angularVelocity.y, body.angularVelocity.z);
        Debug::log(Debug::Category::Ragdoll, "  velocityInherited=(%.2f %.2f %.2f)\n",
               linearVel.x, linearVel.y, linearVel.z);
        Debug::log(Debug::Category::Ragdoll, "  diffBeforeAfter=(%.2f %.2f %.2f)\n",
               linearVel.x - body.velocity.x,
               linearVel.y - body.velocity.y,
               linearVel.z - body.velocity.z);
        Debug::log(Debug::Category::Ragdoll, "[/RAGDOLL SPAWN]\n\n");
    }

    // Single physics body slightly above death position to avoid floor embedding
    body.position = victimPos + glm::vec3(0.0f, 0.0f, 0.1f);
    body.rotation = victimRotation;

    // Capsule dimensions matching the player
    body.capsuleRadius = PLAYER_RADIUS;
    body.capsuleHeight = PLAYER_HEIGHT;

    // Inherit velocity — clamped to prevent launch, preserve momentum feel
    float velMag = glm::length(linearVel + externalVel);
    float maxInherit = 8.0f;
    if (velMag > maxInherit) {
        body.velocity = (linearVel + externalVel) * (maxInherit / velMag);
    } else {
        body.velocity = linearVel + externalVel;
    }

    // Death impulse scales with hit force — flings body backward on strong hits
    body.velocity += direction * (lethalForce * 0.5f);

    // Angular velocity from shot direction — brief tumble, not helicopter spin.
    // Cross product with Z-up gives rotation axis perpendicular to shot.
    // Reduced multiplier so body gets one natural roll, not infinite spinning.
    float angForce = (lethalForce * 0.06f + 0.3f);
    body.angularVelocity = glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f)) * angForce;
    float angSpeed = glm::length(body.angularVelocity);
    if (angSpeed > 4.0f) {
        body.angularVelocity = (body.angularVelocity / angSpeed) * 4.0f;
    }

    // Freeze skeleton pose from current physical body transforms
    body.frozenParts.reserve(victim.physicalBody.parts.size());
    body.partMeshes = victim.physicalBody.partMeshes;
    for (size_t i = 0; i < victim.physicalBody.parts.size(); ++i) {
        DeadBody::FrozenPart fp;
        fp.name = victim.physicalBody.parts[i].name;
        fp.nodeIndex = victim.physicalBody.parts[i].nodeIndex;
        fp.worldTransform = victim.physicalBody.parts[i].worldTransform;
        body.frozenParts.push_back(std::move(fp));
    }

    emitLifecycleEvent("death", victim, actorId, killer);

    // Step 3 (continued from above): duel tracking and respawn timer
    const DuelPhase duelPhaseBeforeDeath = gDuelManager.phase();
    if (actorType == "player")
    {
        gDuelManager.onEntityDeath(DuelTeam::Player);
    }
    else if (actorType == "npc")
    {
        gDuelManager.onEntityDeath(DuelTeam::NPC);
    }
    const bool roundWinningKill =
        duelPhaseBeforeDeath == DuelPhase::Active &&
        gDuelManager.phase() != DuelPhase::Active;
    notifyReplayKill(
        killer.empty() ? "unknown" : killer,
        actorId,
        roundWinningKill);
    {
        std::string effectiveKiller = killer;
        if (effectiveKiller.empty() || effectiveKiller == "unknown") {
            if (!victim.lastDamagedBy.empty())
                effectiveKiller = victim.lastDamagedBy;
            else
                effectiveKiller = "unknown";
        }
        std::string victimName = victim.username.empty() ? actorId : victim.username;
        std::string weaponName = victim.killedByWeapon.empty() ? "unknown" : victim.killedByWeapon;
        KillfeedManager::instance().onKill(effectiveKiller, victimName, weaponName);

        ReplayKillfeedEvent kfEvent;
        kfEvent.killerId = effectiveKiller;
        kfEvent.killerName = effectiveKiller;
        kfEvent.victimId = actorId;
        kfEvent.victimName = victimName;
        kfEvent.weaponName = weaponName;
        captureReplayKillfeed(kfEvent);
    }
    victim.respawnTimer = (actorType == "npc") ? npcRespawnDelaySeconds : RESPAWN_SECONDS;
    victim.killedBy = killer.empty() ? "unknown" : killer;

    if (actorType == "npc")
        Debug::log(Debug::Category::NpcCombat, "[NPC] Respawning in %.2f seconds\n", victim.respawnTimer);

    Debug::log(Debug::Category::Ragdoll, "[RAGDOLL] player=%s activated=true parts=%zu\n",
           victim.username.c_str(), body.frozenParts.size());
    Debug::log(Debug::Category::Ragdoll, "[RAGDOLL IMPULSE] force=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f)\n",
           body.deathImpulse.x, body.deathImpulse.y, body.deathImpulse.z,
           body.velocity.x, body.velocity.y, body.velocity.z);

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victimPos, 1.0f, 0.9f, 45.0f, 0});

    ReplayEffectEvent corpseEvent;
    corpseEvent.type = "corpse_spawn";
    corpseEvent.position = victimPos;
    corpseEvent.direction = direction;
    corpseEvent.velocity = linearVel;
    corpseEvent.sourceActorId = actorId;
    corpseEvent.targetActorId = body.id;
    captureReplayEffect(corpseEvent);

    mCorpses.push_back(std::move(body));

    // Spawn death ellipsoid effect at the victim position, elongated along the kill direction
    const auto& deCfg = HitEffects::config().deathEllipsoid;
    if (deCfg.enabled) {
        EffectPartSystem::instance().spawnDeathEllipsoid(
            victimPos, direction,
            deCfg.length, deCfg.radius, deCfg.lifetime);
    }

    return true;
}

void DeathSystem::respawn(Player& actor, const std::string& actorId, const World& world)
{
    int spawnIndex = -1;
    SpawnPoint* sp = const_cast<World&>(world).pickSpawnPoint();
    if (sp) {
        actor.pos = sp->position;
        actor.respawnPosition = sp->position;
        for (size_t i = 0; i < world.spawnPoints.size(); ++i) {
            if (&world.spawnPoints[i] == sp) { spawnIndex = (int)i; break; }
        }
    } else {
        actor.pos = actor.respawnPosition;
    }

    actor.vel = glm::vec3(0.0f);
    actor.externalImpulse = glm::vec3(0.0f);
    actor.currentHp = actor.maxHp;
    actor.dead = false;
    actor.proceduralFrozen = false;
    actor.respawnTimer = 0.0f;
    actor.voidDeathTriggered = false;
    actor.spawnFlashTimer = 10.0f;
    actor.killedBy.clear();
    actor.ground.onGround = false;
    playWorldSound("entity/player/spawning", actor.pos, 1.0f);
    Debug::log(Debug::Category::Audio, "[SPAWN FX] playing spawning.wav\n");
    resetAllWeaponRuntimesForSpawn(actor, "DeathSystem::respawn");
    actor.syncLegacyStateToLayers();
    actor.updateModelWorldTransforms();
    emitLifecycleEvent("respawn", actor, actorId, actorId);

    FILE* debugFile = fopen("logs/map_spawn_debug.txt", "a");
    if (debugFile) {
        fprintf(debugFile, "Player spawned spawn_index=%d position=(%.3f %.3f %.3f)\n",
                spawnIndex, actor.pos.x, actor.pos.y, actor.pos.z);
        fclose(debugFile);
    }
}

void DeathSystem::update(
    World& world,
    Player& player,
    NpcSystem& npcs,
    bool instantRespawnPressed,
    float dt)
{
    // Kill any entity that just reached 0 HP
    if (player.currentHp <= 0 && !player.dead) {
        kill(player, player.username, "player", "unknown", player.vel, 12.0f);
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0 && !npc.body.dead) {
            kill(
                npc.body,
                "npc_" + std::to_string(npc.id),
                "npc",
                "unknown",
                npc.body.vel,
                18.0f);
        }
    }

    // Update all dead bodies
    for (DeadBody& body : mCorpses) {
        body.age += dt;

        // Debug freeze: skip physics update
        if (DebugConfig::DEBUG_NPC_DEATH_FREEZE && body.debugFreeze)
            continue;

        // Underworld safety
        if (body.position.z < DEAD_WORLD_FLOOR) {
            DEAD_LOG("[UNDERWORLD] Body '%s' at z=%.1f, cleaning up",
                     body.id.c_str(), body.position.z);
            body.age = CORPSE_TOTAL_SECONDS;
            continue;
        }

        if (body.age < CORPSE_STAGE1_SECONDS) {
            body.blackness = std::clamp(body.age / CORPSE_STAGE1_SECONDS, 0.0f, 1.0f);
            body.fade = 0.0f;

            // Per-frame tick log (0.25s interval)
            if (DebugConfig::DEBUG_NPC_DEATH) {
                body.debugTickTimer += dt;
                if (body.debugTickTimer >= 0.25f) {
                    body.debugTickTimer = 0.0f;
                    float distFromSpawn = glm::length(body.position - body.spawnPosition);
                    float gravDot = glm::dot(body.velocity, body.debugGravity);
                    Debug::log(Debug::Category::Ragdoll, "[RAGDOLL TICK] id=%s pos=(%.2f %.2f %.2f) "
                           "vel=(%.2f %.2f %.2f) speed=%.2f "
                           "angVel=(%.2f %.2f %.2f) "
                           "distFromDeath=%.2f "
                           "gravityDot=%.2f "
                           "sleeping=%d\n",
                           body.id.c_str(),
                           body.position.x, body.position.y, body.position.z,
                           body.velocity.x, body.velocity.y, body.velocity.z,
                           glm::length(body.velocity),
                           body.angularVelocity.x, body.angularVelocity.y, body.angularVelocity.z,
                           distFromSpawn,
                           gravDot,
                           (int)body.sleeping);
                }
            }

            // Sleep check
            if (trySleepBody(body, dt))
                continue;

            // Physics update — single body, no constraints
            updateDeadBodyPhysics(body, world, dt);
        } else {
            body.blackness = 1.0f;
            body.fade = std::clamp(
                (body.age - CORPSE_STAGE1_SECONDS) / CORPSE_STAGE2_SECONDS,
                0.0f, 1.0f);
        }
    }

    // Re-freeze corpses after stepping one frame
    if (DebugConfig::DEBUG_NPC_DEATH_FREEZE) {
        for (DeadBody& body : mCorpses)
            body.debugFreeze = true;
    }

    // Remove expired corpses
    mCorpses.erase(
        std::remove_if(mCorpses.begin(), mCorpses.end(), [](const DeadBody& body) {
            return body.age >= CORPSE_TOTAL_SECONDS;
        }),
        mCorpses.end());

    // Respawn logic
    if (player.dead)
    {
        player.respawnTimer =
            std::max(0.0f, player.respawnTimer - dt);

        bool duelModeActive =
            gDuelManager.phase() != DuelPhase::Off;

        bool shouldRespawn =
            !duelModeActive;

        if (shouldRespawn)
        {
            if (instantRespawnPressed ||
                player.respawnTimer <= 0.0f)
            {
                respawn(
                    player,
                    player.username,
                    world);
            }
        }
    }

    for (Npc& npc : npcs.all())
    {
        if (!npc.body.dead)
            continue;

        npc.body.respawnTimer =
            std::max(0.0f,
            npc.body.respawnTimer - dt);

        bool duelModeActive =
            gDuelManager.phase() != DuelPhase::Off;

        bool shouldRespawn =
            !duelModeActive;

        if (shouldRespawn)
        {
            if (npc.body.respawnTimer <= 0.0f)
            {
                respawn(
                    npc.body,
                    "npc_" + std::to_string(npc.id),
                    world);
                Debug::log(Debug::Category::NpcCombat, "[NPC] Respawned at (%.2f, %.2f, %.2f)\n",
                           npc.body.pos.x, npc.body.pos.y, npc.body.pos.z);
            }
        }
    }
}


