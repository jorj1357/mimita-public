// 09 22 2026
/* purpose
* Implements the shared hitscan consequence pipeline self-test.
* See hitscan-outcome-selftest.h for scope.
*/
#include "network/hitscan-outcome-selftest.h"

#include <cstdio>
#include <string>
#include <unordered_map>

#include "combat/weapon-execution.h"
#include "combat/weapon-types.h"
#include "ecs/actor-entities.h"
#include "ecs/entity-types.h"
#include "network/packets.h"
#include "network/server-context.h"
#include "network/server-damage-outcome.h"
#include "network/server-hitscan-outcome.h"
#include "network/server.h"

bool runHitscanOutcomeSelfTest(std::string& report)
{
    report.clear();
    bool ok = true;

    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer> players;
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc> npcs;
    std::unordered_map<std::uint32_t, MimitaNet::ServerProjectile> projectiles;
    std::uint32_t nextProjectileId = 1;
    std::uint32_t serverTick = 1;
    std::uint64_t totalPacketsOut = 0;

    MimitaNet::ServerPlayer& shooter = players[1];
    shooter.id = 1;
    shooter.spawnState = MimitaNet::ServerPlayer::Active;
    shooter.dead = false;
    shooter.health = 100;
    shooter.maxHealth = 100;
    shooter.pos = glm::vec3(0.0f, 0.0f, 0.0f);

    // NPC victim: the authority mutates its health directly and does not depend
    // on a live transport (a transport-less player would be disconnected by the
    // reliable-event queue, which is a headless-test artifact, not a bug).
    MimitaNet::ServerNpc& victim = npcs[1001];
    victim.entityId = 1001;
    victim.health = 100;
    victim.name = "target";
    victim.pos = glm::vec3(10.0f, 0.0f, 0.0f);

    MimitaNet::ServerContextV1 context;
    context.players = &players;
    context.npcs = &npcs;
    context.projectiles = &projectiles;
    context.nextProjectileId = &nextProjectileId;
    context.tick = &serverTick;
    context.totalPacketsOut = &totalPacketsOut;
    MimitaNet::setActiveServerContext(&context);

    WeaponDefinition def;
    def.id = "revolver";
    def.damage = 50.0f;
    def.behaviorType = WeaponBehaviorType::Hitscan;
    def.executionType = WeaponExecutionType::Hitscan;

    WeaponExecution::HitscanTraceResult trace{};
    trace.pelletCount = 1;
    WeaponExecution::HitscanPelletHit pellet{};
    pellet.hit = true;
    pellet.targetPlayerId = 1001;
    pellet.hitPosition = glm::vec3(10.0f, 0.0f, 1.5f);
    pellet.hitNormal = glm::vec3(-1.0f, 0.0f, 0.0f);
    trace.pellets[0] = pellet;
    WeaponExecution::HitscanDamageAggregate agg{};
    agg.targetPlayerId = 1001;
    agg.damage = 50;
    agg.pelletHits = 1;
    agg.hitPosition = pellet.hitPosition;
    agg.hitNormal = pellet.hitNormal;
    trace.aggregates.push_back(agg);

    MimitaNet::serverResolveHitscanOutcome(
        0, players, npcs, shooter, def, trace,
        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
        40.0f, 40.0f, 42u, 1u, 0u, serverTick, totalPacketsOut);

    auto victimIt = npcs.find(1001);
    if (victimIt == npcs.end() || victimIt->second.health != 50)
    {
        char line[160];
        std::snprintf(line, sizeof(line),
                      "[FAIL] victim health=%d expected 50 (damage not applied)\n",
                      victimIt == npcs.end() ? -1 : victimIt->second.health);
        report += line;
        ok = false;
    }

    // Melee/projectile analogue: serverResolveDamageOutcome must apply damage to
    // an NPC victim for a non-hitscan source through the same shared owner.
    {
        victimIt->second.health = 100;
        MimitaNet::ServerOutcomeVictim out{};
        out.entity = (std::uint64_t)Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, 1001);
        out.damage = 30;
        out.hitNormal[2] = 1.0f;
        MimitaNet::serverResolveDamageOutcome(
            0, players, npcs, &shooter, &def, GAME_DAMAGE_SOURCE_MELEE, 7u, 0u,
            &out, 1, serverTick, totalPacketsOut);
        if (victimIt->second.health != 70)
        {
            char line[160];
            std::snprintf(line, sizeof(line),
                          "[FAIL] melee/projectile outcome health=%d expected 70\n",
                          victimIt->second.health);
            report += line;
            ok = false;
        }
    }

    MimitaNet::setActiveServerContext(nullptr);
    if (ok)
        report += "[OK] hitscan + melee/projectile consequences applied through the shared owner\n";
    return ok;
}
