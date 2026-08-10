#include "combat/death-system.h"
#include "combat/weapon-runtime.h"
#include "config/ragdoll-death-config.h"
#include "entities/death-ghost.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <glad/glad.h>
#include "config.h"
#include "debug/debug-log.h"
#include "perf/perf-spike.h"
#include "perf/perf-frame.h"
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
#include "game/spawn-override.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "terminal/terminal-state.h"
#include "killfeed/killfeed.h"

extern DuelManager gDuelManager;

extern Renderer* gRenderer;

namespace {
constexpr float RESPAWN_SECONDS = 0.01f;  // instant respawn: next update tick

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

void DeathSystem::healKillerToFull(Player& player, const std::string& killerName)
{
    const int newHp = DevOverrides::healthOverrideEnabled
        ? DevOverrides::healthOverrideValue
        : player.maxHp;
    const int healed = newHp - player.currentHp;
    if (healed <= 0)
        return;
    player.currentHp = newHp;
    HitEffects::spawnHealthGainedEffect(player.pos);
    EffectPartSystem::instance().spawnDamage(player.pos, killerName, -healed);
}

bool DeathSystem::kill(
    Player& victim,
    const std::string& actorId,
    const std::string& actorType,
    const std::string& killer,
    const glm::vec3& shotDirection,
    float lethalForce)
{
    MIMITA_PERF_SCOPE("DeathSystem::Kill");
    using clock = std::chrono::steady_clock;
    auto tStart = clock::now();
    perfSetCorrelation(actorId.c_str());

    if (victim.dead) {
        perfClearCorrelation();
        return false;
    }

    glm::vec3 direction = glm::length(shotDirection) > 0.001f
        ? glm::normalize(shotDirection)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    // Step 1: capture pre-death momentum BEFORE freezing
    glm::vec3 linearVel = victim.vel;
    glm::vec3 externalVel = victim.externalImpulse;
    glm::vec3 victimPos = victim.pos;
    glm::quat victimRotation = glm::angleAxis(glm::radians(victim.yaw), glm::vec3(0.0f, 0.0f, 1.0f));

    // Step 2: spawn a SEPARATE death visual (fall-over clone) so the real
    // player body is never pinned, frozen, or hidden by the death anim.
    // Only the first death presenter for a life spawns the ghost.
    if (!victim.networkDeathPresented)
    {
        victim.networkDeathPresented = true;
        DeathGhostSystem::instance().spawnFromPlayer(
            victim, direction, actorId);
    }

    // Step 3: mark the victim dead for gameplay only (respawn logic, hit
    // gating). The body itself is left untouched so it respawns cleanly.
    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.inputWishMove = glm::vec2(0.0f);
    victim.currentHp = 0;
    victim.dead = true;

    if (DebugConfig::DEBUG_DEATH_TIMELINE)
        Debug::log(Debug::Category::Ragdoll, "[DEATH TIMELINE] t=%lldms freeze+pose complete\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - tStart).count());

    printf("[DEATH] victim=%s hitDir=(%.2f %.2f %.2f) damage=%.0f\n",
           victim.username.c_str(), direction.x, direction.y, direction.z, lethalForce);

    // ── Heal the killer (local path) ──────────────────────────────
    {
        std::string killerName = killer;
        if (killerName.empty() || killerName == "unknown") {
            if (!victim.lastDamagedBy.empty())
                killerName = victim.lastDamagedBy;
        }
        if (!killerName.empty() && killerName != "unknown")
        {
            if (gpPlayer && killerName == gpPlayer->username)
            {
                healKillerToFull(*gpPlayer, killerName);
            }
            else if (gpNpcSystem)
            {
                for (Npc& npc : gpNpcSystem->all())
                {
                    if (npc.body.username == killerName)
                    {
                        int healed = npc.body.maxHp - npc.body.currentHp;
                        if (healed > 0)
                        {
                            npc.body.currentHp = npc.body.maxHp;
                            HitEffects::spawnHealthGainedEffect(npc.body.pos);
                            EffectPartSystem::instance().spawnDamage(npc.body.pos, killerName, -healed);
                        }
                        break;
                    }
                }
            }
        }
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

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victimPos, 1.0f, 0.9f, 45.0f, 0});

    if (DebugConfig::DEBUG_DEATH_TIMELINE || DebugConfig::DEBUG_DEATH_PERF)
        Debug::log(Debug::Category::Ragdoll, "[DEATH PERF] kill() total=%.3fms actor=%s\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - tStart).count(),
            actorId.c_str());

    perfClearCorrelation();
    return true;
}

void DeathSystem::respawn(Player& actor, const std::string& actorId, const World& world)
{
    MIMITA_PERF_SCOPE("DeathSystem::Respawn");
    using clock = std::chrono::steady_clock;
    auto tStart = clock::now();

    int spawnIndex = -1;
    glm::vec3 overridePos;
    if (tryGetSpawnOverride(overridePos)) {
        actor.pos = overridePos;
        actor.respawnPosition = overridePos;
        Debug::log(Debug::Category::General, "[PLAYER RESPAWN] override active spawning at (%.1f %.1f %.1f)\n",
                   overridePos.x, overridePos.y, overridePos.z);
    } else {
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
    }

    actor.vel = glm::vec3(0.0f);
    actor.externalImpulse = glm::vec3(0.0f);
    // Apply developer HP override if enabled
    bool isNpc = actorId.find("npc_") == 0;
    if (!isNpc && DevOverrides::playerHealthOverrideEnabled) {
        actor.maxHp = DevOverrides::playerHealthOverrideValue;
        actor.currentHp = DevOverrides::playerHealthOverrideValue;
    } else if (DevOverrides::healthOverrideEnabled) {
        actor.maxHp = DevOverrides::healthOverrideValue;
        actor.currentHp = DevOverrides::healthOverrideValue;
    } else {
        actor.currentHp = actor.maxHp;
    }
    actor.dead = false;
    actor.deathAnim = Player::DeathAnimState{};
    actor.proceduralFrozen = false;
    actor.networkDeathPresented = false;  // allow the next death to present
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

    if (DebugConfig::DEBUG_DEATH_TIMELINE || DebugConfig::DEBUG_DEATH_PERF)
        Debug::log(Debug::Category::Ragdoll, "[DEATH PERF] respawn() total=%.3fms actor=%s\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - tStart).count(),
            actorId.c_str());
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

    // Tick death animation for dead entities
    auto tickDeathAnim = [](Player& p) {
        if (!p.dead || !p.deathAnim.active)
            return;
        p.deathAnim.tick++;
        if (p.deathAnim.tick >= p.deathAnim.totalTicks)
            p.deathAnim.active = false;
    };
    tickDeathAnim(player);
    for (Npc& npc : npcs.all())
        tickDeathAnim(npc.body);

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


