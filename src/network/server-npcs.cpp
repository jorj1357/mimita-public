// 07 31 2026, 21 30
/* purpose
* Implements server-side NPC simulation by reusing the real client NpcSystem.
* Online NPCs therefore behave exactly like local ones: full movement physics on
* the map collision, chase/orbit/strafe/retreat AI, aiming, and firing.
* The existing ServerNpc map is kept as the snapshot/broadcast representation and
* is rebuilt from the simulated NPCs every tick, so all existing damage,
* projectile, and snapshot code paths stay unchanged.
* Does NOT render NPCs, spawn hit effects for the host, or own client prediction.
* Does NOT change packet schemas or the player damage pipeline.
*/

#include "network/server.h"
#include "network/actor-lifecycle.h"
#include "network/server-gamemode.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "void-death/void-death.h"

#include "npc/npc.h"
#include "npc/npc-internal.h"
#include "npc/npc-combat.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-behavior.h"
#include "npc/npc-navigation.h"
#include "npc/recast-navigation.h"
#include "npc/npc-targeting.h"

#include "debug/structured-log.h"
#include "perf/perf.h"
#include "npc/npc-avatar.h"
#include "entities/player.h"
#include "world/world.h"
#include "map/map-loader-collision.h"
#include "config/spawn-velocity-config.h"
#include "config/actor-collision-config.h"
#include "game/spawn-utils.h"
#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/actor-pair-collision.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "network/network-weapons.h"
#include "network/packets.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace MimitaNet {

// How long a killed online NPC stays dead before respawning at its spawn
// point. Kept above the 180-tick (3s) fall-over death animation so clients see
// the body freeze, fall over, and disappear before the NPC respawns.
// I WANT IT TO BE CUT OFF like show the death anim AND the alive NPC respawn and moving too
// like just constant action
// constexpr float SERVER_NPC_RESPAWN_SECONDS = 3.5f;
// what if 8 10 2026 we set to be instant so we dont need anim to play before respawn 
// 8 10 2026 keep this its so fun 
constexpr float SERVER_NPC_RESPAWN_SECONDS = 0.01f;

void broadcastNpcDamageEvent(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t tick,
    uint64_t& totalPacketsOut,
    uint32_t shooterPlayerId,
    const ServerNpc& npc,
    int damage,
    bool killed,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& dir,
    const glm::vec3& normal,
    uint8_t weapon,
    uint8_t attackerEntityType,
    float contactForce)
{
    NpcDamageEventPacket ev{};
    ev.header.type = PACKET_NPC_DAMAGE_EVENT;
    ev.header.tick = tick;
    ev.header.playerId = shooterPlayerId;
    ev.eventId = nextReliableGameplayEventId();
    ev.eventSessionId = serverReliableEventSessionId();
    ev.npcEntityId = npc.entityId;
    ev.shooterPlayerId = shooterPlayerId;
    ev.attackerEntityType = attackerEntityType;
    ev.contactForce = contactForce;
    ev.damage = damage;
    ev.npcHealth = npc.health;
    ev.killed = killed ? 1 : 0;
    ev.originX = origin.x; ev.originY = origin.y; ev.originZ = origin.z;
    ev.hitX = hit.x; ev.hitY = hit.y; ev.hitZ = hit.z;
    ev.dirX = dir.x; ev.dirY = dir.y; ev.dirZ = dir.z;
    ev.normalX = normal.x; ev.normalY = normal.y; ev.normalZ = normal.z;
    ev.weapon = weapon;
    ev.impactType = SHOT_IMPACT_ENTITY;

    // NPC damage/kill is reliable: it carries the kill-heal and killfeed, so it
    // must not be lost under packet loss. Retransmits until every client ACKs.
    queueReliableGameplayEventToAll(
        sock, players, &ev, sizeof(ev), ev.eventId, ev.eventSessionId,
        totalPacketsOut);

    printf("%s [NPC DAMAGE BROADCAST] shooter=%u npc=%u damage=%d health=%d killed=%d weapon=%u\n",
           serverTimestamp(), shooterPlayerId, npc.entityId, damage, npc.health,
           (int)killed, (unsigned)weapon);
}

// Isolated navigation route proof. Runs a bounded set of named queries against
// the real map collision via the same adapter the NPCs use, and reports each as
// its own event. This is deliberately separate from NPC-movement evidence: it
// proves the adapter can route on the real map without claiming gameplay.
static void runRecastIsolatedSelfTest(const World& npcWorld,
                                      const HeadlessWorld& hw)
{
    const char* env = std::getenv("MIMITA_NPC_NAV_SELFTEST");
    if (!env || (std::string(env) != "1" && std::string(env) != "true"))
        return;

    RecastNavigationBackend& backend = RecastNavigationBackend::instance();
    const NavigationAgentProfile profile;

    auto emit = [](const char* scenario, const RecastNavigationResult& r) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcMovement, StructuredLevel::Important,
            "npc.nav.selftest", "server",
            r.success ? "route_ok" :
                (r.failure.empty() ? "route_failed" : r.failure.c_str()),
            0,
            nlohmann::json{
                {"scenario", scenario},
                {"available", r.available},
                {"success", r.success},
                {"failure", r.failure},
                {"polygon_count", r.polygonCount},
                {"path_length", r.pathLength},
                {"start_poly_found", r.startPolyFound},
                {"dest_poly_found", r.destPolyFound},
                {"start_projection_m", r.startProjectionDistance},
                {"dest_projection_m", r.destProjectionDistance},
                {"query_ms", r.queryMilliseconds}},
            __FILE__, __LINE__, __FUNCTION__);
    };

    // The exact long route recorded as the P0 blocker in
    // docs/changelog/2026-10-06/20261006_164809/events.jsonl.
    emit("recorded_long_route",
         backend.query(npcWorld, glm::vec3(-868.0f, 122.6f, 2343.4f),
                       glm::vec3(-778.4f, 122.6f, 2349.3f), profile));

    if (hw.spawnPoints.size() >= 2) {
        emit("spawn_to_spawn",
             backend.query(npcWorld, hw.spawnPoints[0].position,
                           hw.spawnPoints[1].position, profile));
    }

    // Invalid input must fail explicitly, never silently project.
    emit("outside_map",
         backend.query(npcWorld, hw.boundsMin + glm::vec3(-5000.0f),
                       hw.boundsMax + glm::vec3(5000.0f), profile));
}

void buildNpcWorldCollision(World& npcWorld, const HeadlessWorld& hw)
{
    // The headless world is already decimated, unioned, and face-cleaned by the
    // map loader. Re-decimating that output collapses union vertices and can
    // create open seams/T-junctions that the client never sees. Reuse it as-is.
    npcWorld.collisionMesh.triangles = hw.triangles;
    npcWorld.collisionChunkSize = hw.collisionChunkSize;

    // Build the same acceleration grids the client builds: chunk cells PLUS
    // the coarse large-triangle grid + sub-grids. Reusing the client builder
    // guarantees server NPC collision can never drift apart from the client's.
    // Without the coarse grid, big map pieces (floors/walls spanning > 256
    // chunks, e.g. chainofjudgement's arena floor and walls) were silently
    // dropped by appendChunkTrianglesForAABB, so server NPCs fell through the
    // floor and shot through walls.
    buildCollisionChunks(npcWorld, nullptr);

    printf("[SERVER NPC WORLD] built CPU collision: triangles=%zu chunks=%zu "
           "largeTris=%zu largeChunks=%zu treeNodes=%zu\n",
           npcWorld.collisionMesh.triangles.size(), npcWorld.collisionChunks.size(),
           npcWorld.collisionLargeTriangles.size(), npcWorld.collisionLargeChunks.size(),
           npcWorld.collisionTree.nodes.size());

    const char* compare = std::getenv("MIMITA_NPC_NAV_COMPARE");
    const char* backend = std::getenv("MIMITA_NPC_NAV_BACKEND");
    const bool compareEnabled =
        compare && (std::string(compare) == "1" || std::string(compare) == "true");
    const bool recastBackendEnabled =
        backend && (std::string(backend) == "recast" ||
                     std::string(backend) == "compare");
    // Also prewarm when any loaded behavior profile selects the external
    // backend, so the first bake never lands inside a fixed gameplay tick.
    bool profileWantsRecast = false;
    for (const auto& profile : BehaviorProfileRegistry::instance().all()) {
        if (profile.navigationBackend == "recast" ||
            profile.navigationBackend == "compare") {
            profileWantsRecast = true;
            break;
        }
    }
    if (compareEnabled || recastBackendEnabled || profileWantsRecast) {
        NavigationAgentProfile profile;
        const RecastNavigationResult nav =
            RecastNavigationBackend::instance().prepare(npcWorld, profile);
        // A successful bake only proves the navmesh was built. It deliberately
        // does not claim a route exists; route success is reported per query by
        // npc.nav.compare. Walkable/poly counts prove the converted geometry
        // produced a usable surface (a Z-up/Y-up mismatch yields zero).
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcMovement, StructuredLevel::Important,
            nav.available ? "npc.navmesh.bake-success" : "npc.navmesh.bake-failed",
            "server", nav.available ? "prepared" :
                (nav.failure.empty() ? "failed" : nav.failure.c_str()), 0,
            nlohmann::json{
                {"backend", "recast_detour"},
                {"coordinate_system", nav.coordinateSystem},
                {"available", nav.available},
                {"navmesh_version", nav.navmeshVersion},
                {"triangle_count", npcWorld.collisionMesh.triangles.size()},
                {"walkable_triangles", nav.walkableTriangleCount},
                {"navmesh_polys", nav.navMeshPolyCount},
                {"navmesh_verts", nav.navMeshVertCount},
                {"recast_bounds_min", {nav.recastBoundsMin.x, nav.recastBoundsMin.y,
                                       nav.recastBoundsMin.z}},
                {"recast_bounds_max", {nav.recastBoundsMax.x, nav.recastBoundsMax.y,
                                       nav.recastBoundsMax.z}},
                {"build_ms", nav.buildMilliseconds},
                {"failure", nav.available ? "" : nav.failure}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    runRecastIsolatedSelfTest(npcWorld, hw);
}

// Adopt newly spawned ServerNpc entries (from npc_spawn requests or startup)
// into the real NpcSystem so they get full AI simulation.
static void applyRoleStartingAmmo(Npc& npc, const ActorSpawnProfile& profile);

static void adoptNewServerNpcs(const std::unordered_map<uint32_t, ServerNpc>& npcs,
                               NpcSystem& npcSystem,
                               std::unordered_set<uint32_t>& npcIdsAlive,
                               const World& world)
{
    for (const auto& kv : npcs)
    {
        bool alreadySimulated = false;
        for (const Npc& n : npcSystem.all())
        {
            if (n.id == kv.first) { alreadySimulated = true; break; }
        }
        const bool alreadyTracked = npcIdsAlive.find(kv.first) != npcIdsAlive.end();
        if (alreadyTracked && !alreadySimulated)
            continue;
        npcIdsAlive.insert(kv.first);
        if (!alreadySimulated)
        {
            const glm::vec3 safeSpawn = resolveSafeStandingSpawn(
                world, kv.second.pos, 0.60f, 2.0f);
            npcSystem.spawnNpc(kv.first, kv.second.difficulty, safeSpawn);
        }
        // Map-mode startup NPCs can exist before the managed match roster is
        // built. Give them the configured NPC team role before resolving their
        // spawn profile, so role movement presets apply in Sandbox too.
        ServerGamemodeState& modeState = serverGamemodeState();
        bool assignedModeRole = false;
        if (modeState.matchActors.find(kv.first) == modeState.matchActors.end()) {
            const Gamemode& mode = GamemodeRegistry::instance().get(modeState.matchMode);
            for (size_t teamIndex = 0; teamIndex < mode.teams.size(); ++teamIndex) {
                const GamemodeTeam& team = mode.teams[teamIndex];
                if (!team.npcs) continue;
                const std::string& roleId = team.npcRole.empty() ? team.role : team.npcRole;
                const MatchRoleDefinition* role = roleId.empty()
                    ? nullptr : MatchRoleRegistry::instance().get(roleId);
                if (!role) continue;

                ActorMatchDescriptor descriptor;
                descriptor.controller = ActorController::Npc;
                descriptor.state = ActorState::Alive;
                descriptor.teamId = static_cast<int>(teamIndex);
                descriptor.roleId = role->id;
                descriptor.movementProfileId = role->movementPreset;
                descriptor.weaponProfileId = role->weaponSet;
                modeState.matchActors[kv.first] = std::move(descriptor);
                modeState.matchTeams[kv.first] = static_cast<int>(teamIndex);
                assignedModeRole = true;
                break;
            }
        }
        if (alreadySimulated && !assignedModeRole)
            continue;
        // Apply the role profile (health, movement, avatar, behavior, loadout)
        // so a roster NPC gets its intended first life. Shared with the
        // gamemode spawn-reset path; no CS-only branch.
        const ActorSpawnProfile profile = serverResolveActorSpawnProfile(kv.first);
        const Gamemode& mode = GamemodeRegistry::instance().get(modeState.matchMode);
        const auto actorProfile = modeState.matchActors.find(kv.first);
        const int resolvedTeam = actorProfile != modeState.matchActors.end()
            ? actorProfile->second.teamId : kv.second.matchTeam;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcCombat, StructuredLevel::Important,
            "npc.roster-profile-resolved", std::to_string(kv.first),
            "npc-adopt", modeState.currentServerTick,
            nlohmann::json{
                {"actor", kv.first},
                {"team", resolvedTeam},
                {"role", profile.roleId},
                {"profile", profile.behaviorProfileId},
                {"movement_preset", profile.movementPreset},
                {"actor_preset", profile.actorPresetId},
                {"mode", modeState.matchMode},
                {"mode_preset", mode.actorPresetId},
                {"source", "serverResolveActorSpawnProfile"}},
            __FILE__, __LINE__, __FUNCTION__);
        // Apply the healthall override to the newly adopted real NPC body.
        for (Npc& n : npcSystem.all())
        {
            if (n.id == kv.first)
            {
                // Authoritative team lives on the real body too, so the
                // per-tick mirror rebuild and the local brain both see it.
                n.body.matchTeam = resolvedTeam;
                if (serverGameOverrides().maxHpOverride > 0)
                {
                    n.body.maxHp = serverGameOverrides().maxHpOverride;
                    n.body.currentHp = n.body.maxHp;
                }
                else if (profile.health > 0)
                {
                    n.body.maxHp = profile.health;
                    n.body.currentHp = n.body.maxHp;
                }
                n.movementProfileId = profile.movementPreset;
                n.actorPresetId = profile.actorPresetId;
                serverMarkActorPresetApplied(n.id, profile.actorPresetId);
                if (!profile.avatarName.empty())
                    n.avatarName = profile.avatarName;
                n.behaviorProfileId = profile.behaviorProfileId;
                n.behavior = resolveNpcBehavior(profile.behaviorProfileId);
                npcApplyBehaviorDifficulty(n);
                if (n.behavior.active && n.behavior.aggression >= 0.0f)
                    n.tuning.aggression = n.behavior.aggression;
                if (!profile.weapons.empty())
                    npcApplyLoadout(n, profile.weapons, profile.startingWeapon);
                applyRoleStartingAmmo(n, profile);
                // A declared disaster owns the NPC loadout for this life. Pure
                // data lookup from replicated state; never a mode-name branch.
                if (const std::string* disasterWeapon =
                        MimitaGamemode::disasterWeaponForActor(
                            serverGamemodeState().disaster, n.id))
                    npcApplyLoadout(n, {*disasterWeapon}, *disasterWeapon);
                finalizeServerNpcSpawn(n, ActorSpawnReason::NpcCreate);
            }
        }
    }
}

static void applyRoleStartingAmmo(Npc& npc, const ActorSpawnProfile& profile)
{
    for (auto& entry : npc.body.weaponRuntimes) {
        const auto it = profile.weaponAmmo.find(entry.first);
        if (it == profile.weaponAmmo.end()) continue;
        if (it->second.first >= 0) entry.second.currentAmmo = it->second.first;
        if (it->second.second >= 0) entry.second.reserveAmmo = it->second.second;
    }
}

// Destroy real NPCs whose ServerNpc was removed this tick (killed / void death)
// and echo authoritative health + knockback from the ServerNpc map into the
// real NPC bodies (the map is the damage target for existing server code).
static void syncServerNpcDamageToNpc(const std::unordered_map<uint32_t, ServerNpc>& npcs,
                                     NpcSystem& npcSystem,
                                     std::unordered_set<uint32_t>& npcIdsAlive)
{
    for (auto it = npcIdsAlive.begin(); it != npcIdsAlive.end(); )
    {
        if (npcs.find(*it) == npcs.end())
        {
            npcSystem.destroySelected({*it});
            it = npcIdsAlive.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (const auto& kv : npcs)
    {
        for (Npc& n : npcSystem.all())
        {
            if (n.id != kv.first) continue;
            if (n.body.currentHp != kv.second.health) {
                printf("[SERVER SYNC NPC DAMAGE] npcId=%u serverHealth=%d npcBodyHp=%d -> syncing to %d\n",
                       kv.first, kv.second.health, n.body.currentHp, kv.second.health);
                n.body.currentHp = kv.second.health;
            }
            if (glm::length(kv.second.knockbackImpulse) > 0.05f)
                n.body.externalImpulse += kv.second.knockbackImpulse;
            // A killed ServerNpc must stop acting immediately and start its
            // respawn countdown. updateOneNpc early-returns on dead, so the
            // body freezes until respawnServerNpc resets it.
            if (kv.second.health <= 0 && !n.body.dead)
            {
                n.body.currentHp = 0;
                n.body.dead = true;
                n.body.respawnTimer = serverMatchRespawnsEnabled()
                    ? serverMatchRespawnSeconds() : -1.0f;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::NpcCombat, StructuredLevel::Important,
                    "npc.death", std::to_string(n.id), "health_depleted",
                    (uint32_t)(n.sensors.time * 60.0f),
                    nlohmann::json{
                        {"actor", n.id},
                        {"team", n.body.matchTeam},
                        {"profile", n.behaviorProfileId},
                        {"preset", n.actorPresetId},
                        {"killed_by", n.body.killedBy},
                        {"respawn_seconds", n.body.respawnTimer},
                        {"position", {n.body.pos.x, n.body.pos.y, n.body.pos.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            break;
        }
    }
}

void finalizeServerNpcSpawn(Npc& npc, ActorSpawnReason reason)
{
    const ServerGamemodeState& modeState = serverGamemodeState();
    const Gamemode& mode = GamemodeRegistry::instance().get(modeState.matchMode);
    if (mode.npcAvatar.configured) {
        const ActorSpawnProfile profile = serverResolveActorSpawnProfile(npc.id);
        assignNpcAvatarForMode(npc, mode.npcAvatar, modeState.matchMode,
                               profile.avatarForced ? profile.avatarName : "",
                               profile.allowedAvatars,
                               profile.deniedAvatars);
    }
    npc.wakeupTimer = static_cast<float>(
        NpcDifficultyConfig::instance().settings().spawnActionDelayTicks) / 60.0f;

    ActorSpawnEvent lifecycleEvent;
    lifecycleEvent.entityId = npc.id;
    lifecycleEvent.actorKind = ActorKind::Npc;
    lifecycleEvent.reason = reason;
    lifecycleEvent.transformEpoch = npc.transformEpoch;
    lifecycleEvent.position = npc.body.pos;
    lifecycleEvent.lookDirection = glm::vec3(std::cos(npc.body.yaw),
                                             std::sin(npc.body.yaw), 0.0f);
    lifecycleEvent = finalizeActorSpawn(lifecycleEvent, npc.avatarName.c_str());
    npc.body.yaw = std::atan2(lifecycleEvent.lookDirection.y,
                              lifecycleEvent.lookDirection.x);
    npc.body.vel = lifecycleEvent.velocity;
    npc.body.externalImpulse = glm::vec3(0.0f);
    npc.body.dead = false;
    npc.body.respawnTimer = 0.0f;
    serverMeleeResetNpcForSpawn(npc.id);
    // Melee/Payload: grant the mode's spawn protection on NPC respawn.
    {
        const ServerGamemodeState& mode = serverGamemodeState();
        const uint32_t protection = mode.payloadMode
            ? (uint32_t)std::max(0, mode.payloadSpawnProtectionTicks) : 60u;
        serverMeleeNoteSpawn(npc.id, protection);
    }
    npc.body.syncLegacyStateToLayers();
    npc.body.updateModelWorldTransforms();
}

// Reset a killed server NPC body back to full health at its spawn point so the
// snapshot pipeline re-admits it (rebuildServerNpcMap skips dead bodies).
static void respawnServerNpc(Npc& npc, const World& world)
{
    const glm::vec3 rawSpawn = effectiveServerSpawn(npc.body.respawnPosition);
    const glm::vec3 spawnPos = resolveSafeStandingSpawn(world, rawSpawn, 0.60f, 2.0f);
    npc.body.pos = spawnPos;
    npc.body.respawnPosition = spawnPos;
    // New life: bump the lifecycle counter so clients detect the respawn.
    npc.transformEpoch = static_cast<uint16_t>((npc.transformEpoch % 65535) + 1);
    const Gamemode& mode = GamemodeRegistry::instance().get(serverGamemodeState().matchMode);
    if (!mode.npcAvatar.configured)
        assignNpcAvatar(npc);
    // Reapply the actor's role stats/loadout for the new life so role identity
    // is never lost across respawn.
    const ActorSpawnProfile profile = serverResolveActorSpawnProfile(npc.id);
    if (!mode.npcAvatar.configured && !profile.avatarName.empty())
        npc.avatarName = profile.avatarName;
    const int overrideHp = serverGameOverrides().maxHpOverride;
    const int maxHp = overrideHp > 0 ? overrideHp
        : (profile.health > 0 ? profile.health : npc.body.maxHp);
    npc.body.maxHp = maxHp;
    npc.body.currentHp = maxHp;
    npc.movementProfileId = profile.movementPreset;
    npc.actorPresetId = profile.actorPresetId;
    serverMarkActorPresetApplied(npc.id, profile.actorPresetId);
    npc.navigator.reset();
    npc.traversal.reset();
    npc.prevHadTarget = false;
    npc.reactionTimer = 0.0f;
    npc.serverTargetId = 0;
    npc.lastNavGoalKind = -1;  // re-emit npc.goal-changed for the new life
    npc.lastNavGoalActor = 0;
    npc.lastJumpReason = 0;
    npc.lastWallAvoidDir = glm::vec3(0.0f);
    npc.wasStuck = false;
    npc.movementDecisionTimer = 0.0f;
    npc.movementStuckEpisodeActive = false;
    npc.movementStuckClearTimer = 0.0f;
    npc.movementStuckWallAvoidCount = 0;
    npc.movementStuckRecoveryCount = 0;
    npc.movementStuckJumpCount = 0;
    npc.lastNavFailureTick = 0;
    npc.lastNavFailureGoalKind = -1;
    npc.lastNavFailureReason.clear();
    npc.lastObjectiveContextKey = -1;
    npc.hasLoggedUtilityDecision = false;
    npc.lastLoggedUtilityGoal = -1;
    npc.lastLoggedUtilityAction = -1;
    npc.hasLoggedPursuitState = false;
    npc.hasLoggedDash = false;
    npc.lastLoggedPursuitTarget = 0;
    npc.lastLoggedPursuitVisible = false;
    npc.lastLoggedPursuitRemembered = false;
    npc.grenadeAvailabilityLogged = false;
    npc.lastLoggedGrenadeWeapon.clear();
    npc.jumpCooldown = 0.0f;
    npc.cycleWeapon.clear();
    npc.cycleVisitShotsRemaining = 0;
    npc.cycleVisitShotsTotal = 0;
    npc.cycleVisitSerial = 0;
    npc.cycleVisitComplete = false;
    npc.hasDecisionGoalDistance = false;
    npc.lastDecisionGoalDistance = 0.0f;
    // Reapply the role behavior profile for the new life.
    npc.behaviorProfileId = profile.behaviorProfileId;
    npc.behavior = resolveNpcBehavior(profile.behaviorProfileId);
    npcApplyBehaviorDifficulty(npc);
    if (npc.behavior.active && npc.behavior.aggression >= 0.0f)
        npc.tuning.aggression = npc.behavior.aggression;
    if (!profile.weapons.empty())
        npcApplyLoadout(npc, profile.weapons, profile.startingWeapon);
    // A declared disaster owns the NPC loadout for this life (data lookup).
    if (const std::string* disasterWeapon =
            MimitaGamemode::disasterWeaponForActor(serverGamemodeState().disaster, npc.id))
        npcApplyLoadout(npc, {*disasterWeapon}, *disasterWeapon);
    npc.body.killedBy.clear();
    npc.body.spawnFlashTimer = 10.0f;
    npc.attackCooldown = npcSpawnFireDelaySeconds(npc);
    resetAllWeaponRuntimesForSpawn(npc.body, "server-npc-respawn");
    applyRoleStartingAmmo(npc, profile);
    finalizeServerNpcSpawn(npc, ActorSpawnReason::Respawn);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcCombat, StructuredLevel::Important,
        "npc.respawn", std::to_string(npc.id), "respawned",
        (uint32_t)(npc.sensors.time * 60.0f),
        nlohmann::json{
            {"actor", npc.id},
            {"team", npc.body.matchTeam},
            {"profile", npc.behaviorProfileId},
            {"preset", npc.actorPresetId},
            {"transform_epoch", npc.transformEpoch},
            {"position", {npc.body.pos.x, npc.body.pos.y, npc.body.pos.z}}},
        __FILE__, __LINE__, __FUNCTION__);
    printf("%s [SERVER NPC RESPAWN] id=%u pos=(%.2f,%.2f,%.2f)\n",
           serverTimestamp(), npc.id, spawnPos.x, spawnPos.y, spawnPos.z);
}

// Broadcast the visual/sound of an NPC's weapon firing to every client so the
// shot (muzzle flash, tracer, sound, weapon trigger) shows up remotely.
static void broadcastNpcFiring(SOCKET sock,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               NpcSystem& npcSystem,
                               std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                               uint32_t& nextProjectileId,
                               uint32_t tick,
                               uint64_t& totalPacketsOut)
{
    for (Npc& n : npcSystem.all())
    {
        if (n.body.dead || n.body.currentHp <= 0) continue;
        // Broadcast only on the tick the NPC actually fired so clients get
        // exactly one shot/sound/tracer per bullet (no per-frame spam).
        if (!n.justFired) continue;
        n.justFired = false;
        const WeaponDefinition* wdef = WeaponRegistry::instance().get(n.body.equippedWeaponId);
        if (!wdef) continue;
        const uint8_t netWeapon = networkWeaponTypeForDefinition(*wdef);
        if (netWeapon == NETWORK_WEAPON_NONE) continue;

        glm::vec3 origin = n.body.pos + NpcCombat::npcMuzzleOffset();
        glm::vec3 dir;
        glm::vec3 hit;
        if (n.hasLastShot && glm::length(n.lastShotEnd - n.lastShotOrigin) > 0.001f)
        {
            // Broadcast the real fired shot so the remote tracer goes exactly
            // where the damage ray went (look == shoot == bullet endpoint).
            origin = n.lastShotOrigin;
            hit = n.lastShotEnd;
            dir = glm::normalize(hit - origin);
        }
        else
        {
            dir = glm::length(n.currentFacing) > 0.001f
                ? glm::normalize(n.currentFacing)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            hit = origin + dir * 100.0f;
        }

        // For projectile weapons (rocket, grenade), create a ServerProjectile
        // and broadcast a ProjectileSpawnEventPacket — same as player rockets.
        // This is what makes the projectile visible on all clients.
        if (networkWeaponTypeIsProjectile(netWeapon))
        {
            auto cp = [&](const char* key, float fallback) -> float {
                auto it = wdef->customParams.find(key);
                return it != wdef->customParams.end() ? it->second : fallback;
            };

            ServerProjectile projectile;
            projectile.id = nextProjectileId++;
            if (nextProjectileId == 0) nextProjectileId = 1;
            projectile.ownerPlayerId = 0; // NPC (not a player)
            projectile.ownerNpcId = n.id; // the NPC that fired this
            projectile.fireSerial = 0;
            projectile.weaponType = netWeapon;
            projectile.weaponDefNetworkId =
                MimitaNet::weaponDefNetworkIdFor(wdef->id);
            projectile.position = origin;
            projectile.previousPosition = origin;

            float speed = wdef->projectileSpeed > 0.0f ? wdef->projectileSpeed : 40.0f;
            float upBias = cp("upBias", 0.0f);
            projectile.velocity = dir * speed + glm::vec3(0.0f, 0.0f, upBias);

            projectile.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            if (netWeapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
            {
                glm::vec3 forward = glm::length(dir) > 0.0001f ? dir : glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 refUp = std::fabs(forward.z) < 0.99f
                    ? glm::vec3(0.0f, 0.0f, 1.0f)
                    : glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 right = glm::normalize(glm::cross(forward, refUp));
                projectile.angularVelocity = right * cp("angSpeed", 6.0f);
            }

            projectile.lifetime = wdef->projectileLifetime > 0.0f ? wdef->projectileLifetime : 5.0f;
            projectile.radius = wdef->projectileRadius > 0.0f ? wdef->projectileRadius : 0.3f;
            projectile.splashRadius = cp("splashRadius", 8.0f);
            projectile.splashDamage = cp("rocketDirectDamage", 150.0f);
            projectile.splashExponent = cp("splashExponent", 2.0f);
            projectile.knockbackStrength = cp("knockbackStrength", 160.0f);
            projectile.selfKnockbackMultiplier = cp("selfKnockbackMultiplier", 1.0f);
            projectile.gravity = cp("gravity", 20.0f);
            projectile.drag = cp("drag", 0.15f);
            projectile.restitution = cp("bounceRestitution", 0.35f);
            projectile.friction = cp("bounceFriction", 0.5f);
            projectile.armingDistance = cp("armingDistance", 2.0f);
            projectile.armingTime = cp("armingTime", 0.0f);
            projectile.minBounceSpeed = cp("minBounceSpeed", 0.1f);
            projectile.angularDrag = cp("angularDrag", 0.3f);
            projectile.maxBounceCount = (int)cp("maxBounceCount", 10.0f);
            projectile.explodeOnPlayerImpact = cp("explodeOnPlayerImpact", 1.0f) > 0.0f;
            projectile.explodeOnWorldImpact = cp("explodeOnWorldImpact", 0.0f) > 0.0f;
            projectile.explodeOnLifetime = cp("explodeOnLifetime", 1.0f) > 0.0f;
            projectile.splashLineOfSight = true;
            projectile.spawnTick = tick;

            ProjectileSpawnEventPacket spawn{};
            spawn.header.type = PACKET_PROJECTILE_SPAWN_EVENT;
            spawn.header.tick = tick;
            spawn.projectileId = projectile.id;
            spawn.ownerPlayerId = 0;
            spawn.fireSerial = 0;
            spawn.weapon = netWeapon;
            spawn.weaponDefNetworkId = projectile.weaponDefNetworkId;
            spawn.posX = projectile.position.x;
            spawn.posY = projectile.position.y;
            spawn.posZ = projectile.position.z;
            spawn.velX = projectile.velocity.x;
            spawn.velY = projectile.velocity.y;
            spawn.velZ = projectile.velocity.z;
            spawn.rotX = projectile.rotation.x;
            spawn.rotY = projectile.rotation.y;
            spawn.rotZ = projectile.rotation.z;
            spawn.rotW = projectile.rotation.w;
            spawn.angVelX = projectile.angularVelocity.x;
            spawn.angVelY = projectile.angularVelocity.y;
            spawn.angVelZ = projectile.angularVelocity.z;
            spawn.spawnTick = tick;
            spawn.lifetime = projectile.lifetime;
            spawn.radius = projectile.radius;

            projectiles[projectile.id] = projectile;

            if (netWeapon == NETWORK_WEAPON_GRENADE_LAUNCHER) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::GrenadeLauncher, StructuredLevel::Important,
                    "npc.grenade-projectile-created", std::to_string(n.id),
                    "projectile_spawn", tick,
                    nlohmann::json{{"actor", n.id}, {"projectile_id", projectile.id},
                                   {"weapon", wdef->id},
                                   {"position", {projectile.position.x, projectile.position.y,
                                                  projectile.position.z}},
                                   {"velocity", {projectile.velocity.x, projectile.velocity.y,
                                                  projectile.velocity.z}},
                                   {"lifetime", projectile.lifetime}},
                    __FILE__, __LINE__, __FUNCTION__);
            }

            // Broadcast to ALL players (NPC has no "shooter client" to skip)
            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&spawn, sizeof(spawn));
                else
                    sendto(sock, (const char*)&spawn, sizeof(spawn), 0,
                           (sockaddr*)&pe.second.addr,
                           sizeof(pe.second.addr));
                ++totalPacketsOut;
            }

            DBG(NpcCombat, "npc=%u weapon=%s projectileId=%u "
                "position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f)",
                n.id, wdef->id.c_str(), projectile.id,
                projectile.position.x, projectile.position.y, projectile.position.z,
                projectile.velocity.x, projectile.velocity.y, projectile.velocity.z);

            StructuredLogger::instance().writeEvent(
                StructuredCategory::Weapons, StructuredLevel::Important,
                "npc.projectile-created", std::to_string(projectile.id),
                "authoritative NPC projectile spawned", tick,
                nlohmann::json{
                    {"projectile_id", projectile.id},
                    {"owner_npc_id", n.id},
                    {"weapon", wdef->id},
                    {"weapon_def_network_id", projectile.weaponDefNetworkId},
                    {"lifetime", projectile.lifetime},
                    {"explode_on_world", projectile.explodeOnWorldImpact},
                    {"explode_on_lifetime", projectile.explodeOnLifetime},
                    {"pass_through_world", cp("passThroughWorld", 0.0f) > 0.0f},
                    {"pass_through_actors", cp("passThroughActors", 0.0f) > 0.0f}},
                __FILE__, __LINE__, __FUNCTION__);
        }

        // For multi-pellet weapons (shotgun), broadcast a PelletBlastEventPacket
        // so remote clients see all 15 pellet tracers + hear the sound once.
        // Skip the ShotEventPacket for these — the PelletBlastEventPacket handles
        // sound, muzzle flash, and per-pellet rendering.
        if (n.pelletResultCount > 1)
        {
            PelletBlastEventPacket blast{};
            blast.header.type = PACKET_PELLET_BLAST_EVENT;
            blast.header.tick = tick;
            blast.header.playerId = n.id;
            blast.shooterPlayerId = n.id;
            blast.weapon = netWeapon;
            blast.pelletCount = (uint8_t)n.pelletResultCount;
            blast.spreadSeed = n.pelletSpreadSeed;
            blast.shotSerial = n.shotSerialCounter++;
            blast.lastServerTick = tick;
            blast.originX = origin.x;
            blast.originY = origin.y;
            blast.originZ = origin.z;
            blast.baseDirX = dir.x;
            blast.baseDirY = dir.y;
            blast.baseDirZ = dir.z;
            blast.maxRange = 100.0f;
            blast.targetCount = 0;

            int pelletsToSend = std::min(n.pelletResultCount, (int)MAX_NETWORK_PELLETS);
            for (int p = 0; p < pelletsToSend; ++p)
            {
                NetworkPelletResult& pr = blast.pellets[p];
                pr.hitX = n.pelletResults[p].hitPos.x;
                pr.hitY = n.pelletResults[p].hitPos.y;
                pr.hitZ = n.pelletResults[p].hitPos.z;
                pr.normalX = n.pelletResults[p].hitNormal.x;
                pr.normalY = n.pelletResults[p].hitNormal.y;
                pr.normalZ = n.pelletResults[p].hitNormal.z;
                pr.impactType = n.pelletResults[p].hitEntity ? PELLET_IMPACT_PLAYER :
                    (n.pelletResults[p].hitWorld ? PELLET_IMPACT_WORLD : PELLET_IMPACT_NONE);
                pr.pelletIndex = (uint8_t)p;
            }

            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&blast, sizeof(blast));
                else
                    sendto(sock, (const char*)&blast, sizeof(blast), 0,
                           (sockaddr*)&pe.second.addr,
                           sizeof(pe.second.addr));
                ++totalPacketsOut;
            }

            n.pelletResultCount = 0; // consumed

            printf("%s [NPC PELLET BLAST] npc=%u weapon=%s pellets=%d\n",
                   serverTimestamp(), n.id, wdef->id.c_str(), pelletsToSend);
        }
        else
        {
        // Broadcast the ShotEventPacket (sound + muzzle flash on clients).
        // For projectile weapons (rocket, grenade), skip the tracer — the
        // ProjectileSpawnEventPacket renders the actual projectile instead.
        ShotEventPacket ev{};
        ev.header.type = PACKET_SHOT_EVENT;
        ev.header.tick = tick;
        ev.header.playerId = n.id;
        ev.shotSerial = 0;
        ev.clientTimeMs = 0;
        ev.shooterPlayerId = n.id;
        ev.targetPlayerId = 0;
        ev.weapon = netWeapon;
        uint16_t effectFlags = SHOT_EFFECT_MUZZLE |
            SHOT_EFFECT_SHOOT_SOUND | SHOT_EFFECT_WEAPON_TRIGGER;
        if (!networkWeaponTypeIsProjectile(netWeapon))
            effectFlags |= SHOT_EFFECT_TRACER;
        ev.impactType = SHOT_IMPACT_NONE;
        if (n.lastShotHitWorld) {
            ev.impactType = SHOT_IMPACT_WORLD;
            effectFlags |= SHOT_EFFECT_WORLD_IMPACT | SHOT_EFFECT_DEBRIS | SHOT_EFFECT_HIT_SOUND;
        }
        ev.effectFlags = effectFlags;
        ev.originX = origin.x; ev.originY = origin.y; ev.originZ = origin.z;
        ev.hitX = hit.x; ev.hitY = hit.y; ev.hitZ = hit.z;
        ev.dirX = dir.x; ev.dirY = dir.y; ev.dirZ = dir.z;
        ev.normalX = n.lastShotNormal.x;
        ev.normalY = n.lastShotNormal.y;
        ev.normalZ = n.lastShotNormal.z;

        for (const auto& pe : players)
        {
            if (pe.second.transport)
                pe.second.transport->send(&ev, sizeof(ev));
            else
                sendto(sock, (const char*)&ev, sizeof(ev), 0,
                       (sockaddr*)&pe.second.addr,
                       sizeof(pe.second.addr));
            ++totalPacketsOut;
        }

        printf("%s [NPC FIRED] npc=%u weapon=%s dir=(%.2f %.2f %.2f)\n",
               serverTimestamp(), n.id, wdef->id.c_str(), dir.x, dir.y, dir.z);
        } // end else (single-shot weapons)
    }
}

// Rebuild the snapshot/broadcast ServerNpc map from the simulated NPCs.
// History is preserved across the rebuild so hit-rewind validation keeps the
// per-tick poses the clients actually saw (the map is cleared and repopulated
// every tick, which would otherwise discard all historical samples).
static void rebuildServerNpcMap(std::unordered_map<uint32_t, ServerNpc>& npcs,
                                NpcSystem& npcSystem,
                                std::unordered_set<uint32_t>& npcIdsAlive)
{
    std::unordered_map<uint32_t, ServerNpc> next;
    next.reserve(npcs.size());
    npcIdsAlive.clear();
    for (Npc& n : npcSystem.all())
    {
        // Dead NPCs stay in the broadcast (health 0) so clients render their
        // full fall-over death animation before the NPC respawns. Alive NPCs
        // and freshly-respawned ones are broadcast as before.
        ServerNpc sn;
        sn.entityId = n.id;
        sn.transformEpoch = n.transformEpoch;
        sn.name = n.body.username.empty()
            ? "NPC " + std::to_string(n.id)
            : n.body.username;
        sn.avatarName = n.avatarName;
        sn.pos = n.body.pos;
        sn.vel = n.body.vel;
        sn.aim = (glm::length(n.currentFacing) > 0.001f)
            ? glm::normalize(n.currentFacing)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        sn.yaw = n.body.yaw;
        sn.sizeScale = n.body.sizeScale;
        sn.health = n.body.currentHp;
        // Preserve the authoritative team. This map is rebuilt from scratch
        // every tick, so the team must be carried over from the previous
        // record (or the real Npc body) or it resets to -1 and every NPC
        // becomes hostile to everyone (including teammates).
        sn.matchTeam = n.body.matchTeam;
        sn.onGround = n.body.ground.hasWorldContact;
        sn.difficulty = n.difficulty;
        sn.equippedSlot = n.body.equippedSlot;
        sn.equippedWeaponId = n.body.equippedWeaponId;
        sn.weaponState = 0;
        sn.replayControlled = n.replayControlled;
        sn.replayTrackId = n.replayTrackId;
        sn.hasReplayWeaponShape = n.hasReplayWeaponShape;
        sn.replayBladePreviousA = n.replayBladePreviousA;
        sn.replayBladePreviousB = n.replayBladePreviousB;
        sn.replayBladeCurrentA = n.replayBladeCurrentA;
        sn.replayBladeCurrentB = n.replayBladeCurrentB;
        sn.replayBladeRadius = n.replayBladeRadius;
        sn.bodyPartCount = 0;
        if (n.replayControlled && !n.replayBodyParts.empty())
        {
            for (const Npc::ReplayBodyPart& part : n.replayBodyParts)
            {
                if (sn.bodyPartCount >= sn.bodyParts.size())
                    break;
                ServerNpcBodyPartSample& sample =
                    sn.bodyParts[sn.bodyPartCount++];
                sample.cx = part.center.x;
                sample.cy = part.center.y;
                sample.cz = part.center.z;
                sample.hx = part.half.x;
                sample.hy = part.half.y;
                sample.hz = part.half.z;
                sample.bodyPart = static_cast<uint8_t>(WeaponExecution::HitBodyPart::Torso);
            }
        }
        else
        {
            for (const PhysicalBodyPart& part : n.body.physicalBody.parts)
            {
                if (sn.bodyPartCount >= sn.bodyParts.size())
                    break;
                glm::vec3 low(std::numeric_limits<float>::max());
                glm::vec3 high = -low;
                for (int corner = 0; corner < 8; ++corner)
                {
                    const glm::vec3 local(
                        (corner & 1) ? part.collider.localMax.x : part.collider.localMin.x,
                        (corner & 2) ? part.collider.localMax.y : part.collider.localMin.y,
                        (corner & 4) ? part.collider.localMax.z : part.collider.localMin.z);
                    const glm::vec3 world = glm::vec3(part.worldTransform * glm::vec4(local, 1.0f));
                    low = glm::min(low, world);
                    high = glm::max(high, world);
                }
                if (glm::any(glm::greaterThan(low, high)))
                    continue;
                ServerNpcBodyPartSample& sample =
                    sn.bodyParts[sn.bodyPartCount++];
                const glm::vec3 center = (low + high) * 0.5f;
                const glm::vec3 half = (high - low) * 0.5f;
                sample.cx = center.x; sample.cy = center.y; sample.cz = center.z;
                sample.hx = half.x; sample.hy = half.y; sample.hz = half.z;
                sample.bodyPart = static_cast<uint8_t>(WeaponExecution::HitBodyPart::Torso);
            }
        }
        auto oldIt = npcs.find(sn.entityId);
        if (oldIt != npcs.end())
        {
            sn.dashSerial = oldIt->second.dashSerial;
            sn.groundJumpSerial = oldIt->second.groundJumpSerial;
            sn.airJumpSerial = oldIt->second.airJumpSerial;
            sn.downDashSerial = oldIt->second.downDashSerial;
            sn.freezeSerial = oldIt->second.freezeSerial;
        }
        if (n.body.dash.didDash)
            ++sn.dashSerial;
        if (n.body.jump.didGroundJump)
            ++sn.groundJumpSerial;
        if (n.body.jump.didAirJump)
            ++sn.airJumpSerial;
        if (n.body.dash.didDownDash)
            ++sn.downDashSerial;
        if (n.body.freeze.didFreeze)
            ++sn.freezeSerial;
        {
            const auto& npcRt = n.body.weaponRuntimes.find(n.body.equippedWeaponId);
            if (npcRt != n.body.weaponRuntimes.end())
            {
                const WeaponRuntime& rt = npcRt->second;
                if (rt.shootEffectTimer > 0.0f)
                    sn.weaponState |= NET_WEAPON_STATE_FIRING;
                if (rt.isReloading)
                    sn.weaponState |= NET_WEAPON_STATE_RELOADING;
                if (rt.currentAmmo <= 0)
                    sn.weaponState |= NET_WEAPON_STATE_EMPTY;
            }
        }
        if (oldIt != npcs.end())
        {
            sn.posHistory = std::move(oldIt->second.posHistory);
            sn.lastPhysicalWeaponShape = oldIt->second.lastPhysicalWeaponShape;
            sn.hasLastPhysicalWeaponShape = oldIt->second.hasLastPhysicalWeaponShape;
            if (oldIt->second.transformEpoch == sn.transformEpoch &&
                oldIt->second.equippedWeaponId == sn.equippedWeaponId) {
                sn.meleeActionActive = oldIt->second.meleeActionActive;
                sn.meleeActionId = oldIt->second.meleeActionId;
                sn.meleeActionStartTick = oldIt->second.meleeActionStartTick;
                sn.meleeNextAttackTick = oldIt->second.meleeNextAttackTick;
            }
            // Procedural room membership belongs to the authoritative
            // ServerNpc record. Preserve it when rebuilding the broadcast
            // map, otherwise a dungeon NPC becomes room 0 on the next tick
            // and room-clear detection can never find it.
            sn.proceduralRoomNumber = oldIt->second.proceduralRoomNumber;
            // Fall back to the previous team when the body has none (the body
            // team is only authoritative once roster assignment has run).
            if (sn.matchTeam < 0)
                sn.matchTeam = oldIt->second.matchTeam;
        }
        static std::unordered_set<uint32_t> sNpcTraceSeen;
        const bool firstTraceObservation = sNpcTraceSeen.insert(sn.entityId).second;
        const bool lifecycleChanged = firstTraceObservation || oldIt == npcs.end() ||
            oldIt->second.health != sn.health ||
            oldIt->second.transformEpoch != sn.transformEpoch ||
            oldIt->second.avatarName != sn.avatarName;
        if (lifecycleChanged) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.npc.lifecycle.trace", std::to_string(sn.entityId),
                oldIt == npcs.end() ? "NPC entered snapshot map" :
                    "NPC lifecycle or avatar changed in snapshot map",
                0,
                {{"actor_id", sn.entityId}, {"name", sn.name},
                 {"avatar", sn.avatarName}, {"alive", sn.health > 0},
                 {"health", sn.health}, {"transform_epoch", sn.transformEpoch},
                 {"position", {sn.pos.x, sn.pos.y, sn.pos.z}},
                 {"snapshot_included", true}, {"previous_present", oldIt != npcs.end()},
                 {"previous_health", oldIt != npcs.end() ? oldIt->second.health : -1},
                 {"previous_transform_epoch", oldIt != npcs.end()
                    ? oldIt->second.transformEpoch : 0}},
                __FILE__, __LINE__, __FUNCTION__);
        }
        next[sn.entityId] = std::move(sn);
        npcIdsAlive.insert(sn.entityId);
    }
    for (const auto& oldNpc : npcs) {
        if (npcIdsAlive.find(oldNpc.first) == npcIdsAlive.end()) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.npc.lifecycle.trace", std::to_string(oldNpc.first),
                "NPC removed from snapshot map", 0,
                {{"actor_id", oldNpc.first}, {"name", oldNpc.second.name},
                 {"avatar", oldNpc.second.avatarName}, {"alive", false},
                 {"health", oldNpc.second.health},
                 {"transform_epoch", oldNpc.second.transformEpoch},
                 {"position", {oldNpc.second.pos.x, oldNpc.second.pos.y,
                               oldNpc.second.pos.z}},
                 {"snapshot_included", false}, {"previous_present", true}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    npcs = std::move(next);
}

// Record the position actually broadcast to clients this tick so hit-rewind
// reads exactly what attackers saw (same contract as ServerPlayer::posHistory).
void pushNpcPositionHistory(ServerNpc& npc, uint32_t tick)
{
    ServerNpcPositionSample sample;
    sample.pos = npc.pos;
    sample.vel = npc.vel;
    sample.yaw = npc.yaw;
    sample.tick = tick;
    sample.partCount = npc.bodyPartCount;
    for (uint8_t i = 0; i < npc.bodyPartCount && i < sample.parts.size(); ++i)
        sample.parts[i] = npc.bodyParts[i];
    npc.posHistory.push_back(sample);
    while (npc.posHistory.size() > ServerNpc::MAX_POS_HISTORY)
        npc.posHistory.pop_front();
}

// Body-part AABBs the NPC rendered at a past broadcast tick, for the server's
// authoritative re-trace to validate against what the attacker actually saw.
bool getNpcBodyPartsAtTick(const ServerNpc& npc, uint32_t targetTick,
                           const ServerNpcBodyPartSample** outParts,
                           uint8_t* outPartCount)
{
    if (npc.posHistory.empty())
        return false;
    const ServerNpcPositionSample* sample = &npc.posHistory.back();
    if (targetTick <= npc.posHistory.front().tick)
        sample = &npc.posHistory.front();
    else
    {
        for (auto it = npc.posHistory.rbegin(); it != npc.posHistory.rend(); ++it)
        {
            if (it->tick <= targetTick)
            {
                sample = &(*it);
                break;
            }
        }
    }
    if (sample->partCount == 0)
        return false;
    *outParts = sample->parts.data();
    *outPartCount = sample->partCount;
    return true;
}

const std::vector<ServerPlayerBodyPartTemplate>* standardPlayerBodyTemplate()
{
    return gServerBodyTemplate.empty() ? nullptr : &gServerBodyTemplate;
}

// Interpolate the NPC's broadcast pose for a past server tick.
bool getNpcPositionAtTick(const ServerNpc& npc, uint32_t targetTick, glm::vec3& outPos)
{
    if (npc.posHistory.empty())
        return false;
    if (targetTick >= npc.posHistory.back().tick)
    {
        outPos = npc.posHistory.back().pos;
        return true;
    }
    if (targetTick <= npc.posHistory.front().tick)
    {
        outPos = npc.posHistory.front().pos;
        return true;
    }
    for (int i = (int)npc.posHistory.size() - 1; i > 0; --i)
    {
        if (npc.posHistory[i].tick == targetTick)
        {
            outPos = npc.posHistory[i].pos;
            return true;
        }
        if (npc.posHistory[i].tick < targetTick)
        {
            const auto& a = npc.posHistory[i];
            const auto& b = npc.posHistory[i + 1];
            const float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            return true;
        }
    }
    outPos = npc.posHistory.front().pos;
    return true;
}

// Like getNpcPositionAtTick but also returns the broadcast yaw at that tick so
// NPC body-part hitboxes are reconstructed with the facing the attacker saw.
bool getNpcPoseAtTick(const ServerNpc& npc, uint32_t targetTick,
                      glm::vec3& outPos, float& outYaw)
{
    if (npc.posHistory.empty())
        return false;
    const auto& back = npc.posHistory.back();
    if (targetTick >= back.tick)
    {
        outPos = back.pos;
        outYaw = back.yaw;
        return true;
    }
    const auto& front = npc.posHistory.front();
    if (targetTick <= front.tick)
    {
        outPos = front.pos;
        outYaw = front.yaw;
        return true;
    }
    int lo = 0;
    int hi = (int)npc.posHistory.size() - 1;
    while (lo < hi - 1)
    {
        int mid = (lo + hi) / 2;
        if (npc.posHistory[mid].tick <= targetTick)
            lo = mid;
        else
            hi = mid;
    }
    const auto& a = npc.posHistory[lo];
    const auto& b = npc.posHistory[lo + 1];
    const float frac = (b.tick > a.tick)
        ? float(targetTick - a.tick) / float(b.tick - a.tick)
        : 0.0f;
    outPos = glm::mix(a.pos, b.pos, frac);
    outYaw = a.yaw + (b.yaw - a.yaw) * frac;
    return true;
}

// One authoritative actor-pair pass for every server actor. Players and NPCs
// use the same capsules, broadphase, support, and push response as clients; the
// server result is what snapshots and reconciliation carry.
static void resolveServerActorPairCollisions(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    NpcSystem& npcSystem,
    const HeadlessWorld& playerWorld,
    World& npcWorld,
    uint32_t tick)
{
    static thread_local std::vector<ActorPairCollisionActor> actors;
    static thread_local std::vector<ActorPairCollisionOutput> outputs;
    static thread_local std::vector<ServerPlayer*> playerPointers;
    static thread_local std::vector<Npc*> npcPointers;

    actors.clear();
    playerPointers.clear();
    npcPointers.clear();

    const ActorCollisionConfig& collisionConfig =
        ActorCollisionConfig::instance();
    const ActorCollisionData& data = collisionConfig.data();

    for (auto& entry : players)
    {
        ServerPlayer& player = entry.second;
        if (player.spawnState != ServerPlayer::Active)
            continue;

        ActorPairCollisionActor actor;
        actor.id = player.id;
        actor.kind = ActorPairKind::Player;
        actor.pos = player.pos;
        actor.prevPos = player.hasActorCollisionPreviousPos
            ? player.actorCollisionPreviousPos : player.pos;
        actor.vel = player.vel;
        actor.externalImpulse = player.movement.externalImpulse;
        actor.yaw = player.yaw;
        actor.sizeScale = player.sizeScale;
        actor.dead = player.dead;
        actor.staticCollider = player.dead;
        actor.onGround = player.onGround;
        actor.stableOnGround = player.movement.ground.stableOnGround ||
                               player.onGround;
        actor.profile = &collisionConfig.profileFor(player.actorPresetId);
        actor.previousSupportKey = player.actorCollisionSupportKey;
        actor.previousSupportVelocity = player.actorCollisionSupportVelocity;
        actors.push_back(actor);
        playerPointers.push_back(&player);
    }

    const std::size_t playerActorCount = playerPointers.size();

    for (Npc& npc : npcSystem.all())
    {
        // Replay proxies participate in actor-pair collision so the local
        // player receives the same recorded pushes. Their recorded transform is
        // restored by the playback owner before the next contact tick.
        const bool dead = npc.body.dead || npc.body.currentHp <= 0;
        ActorPairCollisionActor actor;
        actor.id = npc.id;
        actor.kind = ActorPairKind::Npc;
        actor.pos = npc.body.pos;
        actor.prevPos = dead ? npc.body.pos : npc.previousPosition;
        actor.vel = npc.body.vel;
        actor.externalImpulse = npc.body.externalImpulse;
        actor.yaw = npc.body.yaw;
        actor.sizeScale = npc.body.sizeScale;
        actor.dead = dead;
        actor.staticCollider = dead;
        actor.onGround = npc.body.ground.onGround;
        actor.stableOnGround = npc.body.ground.stableOnGround;
        actor.profile = &collisionConfig.profileFor(npc.actorPresetId);
        actor.previousSupportKey = npc.body.collision.supportActorKey;
        actor.previousSupportVelocity = npc.body.collision.supportActorVelocity;
        actors.push_back(actor);
        npcPointers.push_back(&npc);
    }

    if (actors.empty())
        return;

    outputs.assign(actors.size(), ActorPairCollisionOutput{});
    ActorPairCollisionStats stats;
    resolveActorPairCollisions(actors.data(), actors.size(), SERVER_DT,
                               data, outputs.data(), &stats, "server", tick);

    Perf::state().current.collisionPairs =
        static_cast<int>(stats.candidatePairs);
    Perf::state().current.charVsChar += stats.totalMs;

    for (std::size_t i = 0; i < playerActorCount; ++i)
    {
        ServerPlayer& player = *playerPointers[i];
        const ActorPairCollisionActor& actor = actors[i];
        const ActorPairCollisionOutput& out = outputs[i];
        if (player.dead)
            continue;
        if (!out.corrected && !out.pushed &&
            out.supportKey == actor.previousSupportKey)
            continue;

        const glm::vec3 addedImpulse =
            out.externalImpulse - actor.externalImpulse;
        player.pos = out.pos;
        player.vel = out.vel + addedImpulse;
        player.movement.externalImpulse = out.externalImpulse;
        player.onGround = out.onGround;
        player.movement.ground.onGround = out.onGround;
        player.movement.ground.stableOnGround = out.stableOnGround;
        player.movement.ground.hasWorldContact =
            player.movement.ground.hasWorldContact || out.onGround;
        player.actorCollisionSupportKey = out.supportKey;
        player.actorCollisionSupportVelocity = out.supportVelocity;
        player.actorCollisionChanged = out.corrected || out.pushed;

        if (out.corrected && out.supportKey == 0)
            resolveWorldCollision(player, playerWorld);
        player.movement.ground.onGround = player.onGround;
        player.movement.ground.stableOnGround =
            player.onGround || player.movement.ground.stableOnGround;

        player.lastAcceptedClientPosition = player.pos;
        player.lastAcceptedClientVelocity = player.vel;
        player.simBroadcastPos = player.pos;
        player.hasSimBroadcastPos = true;
    }

    for (std::size_t i = 0; i < npcPointers.size(); ++i)
    {
        Npc& npc = *npcPointers[i];
        const bool dead = npc.body.dead || npc.body.currentHp <= 0;
        if (dead)
            continue;
        const std::size_t actorIndex = playerActorCount + i;
        const ActorPairCollisionActor& actor = actors[actorIndex];
        const ActorPairCollisionOutput& out = outputs[actorIndex];
        if (!out.corrected && !out.pushed &&
            out.supportKey == actor.previousSupportKey)
            continue;

        npc.body.pos = out.pos;
        npc.body.vel = out.vel;
        npc.body.externalImpulse = out.externalImpulse;
        npc.body.ground.onGround = out.onGround;
        npc.body.ground.stableOnGround = out.stableOnGround;
        npc.body.ground.hasWorldContact =
            npc.body.ground.hasWorldContact || out.onGround;
        npc.body.collision.supportActorKey = out.supportKey;
        npc.body.collision.supportActorVelocity = out.supportVelocity;

        if (out.corrected && out.supportKey == 0)
        {
            bool grounded = false;
            doCollisions(npc.body, npcWorld, grounded, SERVER_DT);
        }
        npc.body.updateModelWorldTransforms();
    }

    static uint32_t lastProfileTick = 0;
    if (tick - lastProfileTick >= 60)
    {
        lastProfileTick = tick;
        nlohmann::json activeProfiles = nlohmann::json::object();
        for (const ActorPairCollisionActor& actor : actors)
        {
            if (!actor.profile) continue;
            activeProfiles[actor.profile->id] = {
                {"capsule_radius_m", actor.profile->capsuleRadiusM},
                {"capsule_height_m", actor.profile->capsuleHeightM},
                {"dead_capsule_length_m", actor.profile->deadCapsuleLengthM}};
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Collision, StructuredLevel::Important,
            "actor.collision.profile", "server",
            stats.truncated ? "candidate_cap_reached" : "tick_profile",
            tick,
            nlohmann::json{
                {"source", "server"},
                {"actors", stats.actorCount},
                {"cell_entries", stats.cellEntries},
                {"candidate_pairs", stats.candidatePairs},
                {"narrow_pairs", stats.narrowPairs},
                {"contacts", stats.contacts},
                {"supports", stats.supports},
                {"pushes", stats.pushes},
                {"player_player_candidates", stats.playerPlayerCandidates},
                {"player_npc_candidates", stats.playerNpcCandidates},
                {"npc_npc_candidates", stats.npcNpcCandidates},
                {"player_player_contacts", stats.playerPlayerContacts},
                {"player_npc_contacts", stats.playerNpcContacts},
                {"npc_npc_contacts", stats.npcNpcContacts},
                {"broadphase_ms", stats.broadphaseMs},
                {"collision_ms", stats.totalMs},
                {"frame_ms", Perf::state().lastFrameTimeMs},
                {"active_profiles", activeProfiles},
                {"config_revision", data.revision}},
            __FILE__, __LINE__, __FUNCTION__);
    }
}

void simulateSharedNpcs(SOCKET sock,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        std::unordered_map<uint32_t, ServerNpc>& npcs,
                        NpcSystem& npcSystem,
                        const HeadlessWorld& playerWorld,
                        World& world,
                        Player& mirrorPlayer,
                        std::unordered_set<uint32_t>& npcIdsAlive,
                        std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                        uint32_t& nextProjectileId,
                        uint32_t tick,
                        uint64_t& totalPacketsOut)
{
    Perf::ScopedTimer npcSimulationTimer("Server::NpcSimulation");
    ActorCollisionConfig::instance().pollHotReload(tick);
    adoptNewServerNpcs(npcs, npcSystem, npcIdsAlive, world);
    syncServerNpcDamageToNpc(npcs, npcSystem, npcIdsAlive);

    // Keep the authoritative roster and health mirrors synchronized, but skip
    // all NPC decision, movement, physics, and actor-pair work until ACTIVE.
    const ServerGamemodeState& matchState = serverGamemodeState();
    const Gamemode& matchMode = GamemodeRegistry::instance().get(matchState.matchMode);
    const bool pauseNpcSimulation =
        matchState.phase == DUEL_PHASE_COUNTDOWN ||
        (matchState.phase == DUEL_PHASE_GO && matchMode.features.freezeDuringGo);
    static bool npcSimulationPaused = false;
    static uint64_t npcSimulationPauseStartedMs = 0;
    if (pauseNpcSimulation)
    {
        if (!npcSimulationPaused)
        {
            npcSimulationPaused = true;
            npcSimulationPauseStartedMs = nowMs();
            size_t livingNpcs = 0;
            for (const Npc& npc : npcSystem.all())
                if (!npc.body.dead && npc.body.currentHp > 0) ++livingNpcs;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.simulation.paused", matchState.matchMode,
                "match_countdown", tick,
                nlohmann::json{{"phase", matchState.phase},
                               {"npc_count", npcSystem.all().size()},
                               {"living_npc_count", livingNpcs}},
                __FILE__, __LINE__, __FUNCTION__);
        }
        return;
    }
    if (npcSimulationPaused)
    {
        npcSimulationPaused = false;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcMovement, StructuredLevel::Important,
            "npc.simulation.resumed", matchState.matchMode,
            "countdown_complete", tick,
            nlohmann::json{{"paused_ms", nowMs() - npcSimulationPauseStartedMs},
                           {"phase", matchState.phase},
                           {"npc_count", npcSystem.all().size()}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    // Mode-level NPC profiles are the fallback for unassigned NPCs (Sandbox
    // today). Re-resolve them here so editing the active gamemode JSON changes
    // living NPCs without a respawn. Role/actor-preset NPCs keep their own
    // profile and are not overwritten by this mode fallback.
    bool modeProfileChanged = false;
    const Gamemode& activeGamemode =
        GamemodeRegistry::instance().get(serverGamemodeState().matchMode);
    const std::string& modeProfileId = activeGamemode.npcBehaviorProfile;
    if (!modeProfileId.empty() &&
        BehaviorProfileRegistry::instance().get(modeProfileId))
    {
        for (Npc& n : npcSystem.all())
        {
            if (!n.actorPresetId.empty())
                continue;
            if (n.behaviorProfileId == modeProfileId)
                continue;
            n.behaviorProfileId = modeProfileId;
            n.behavior = resolveNpcBehavior(modeProfileId);
            npcApplyBehaviorDifficulty(n);
            modeProfileChanged = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.profile-applied", std::to_string(n.id), "mode_profile",
                (uint32_t)(n.sensors.time * 60.0f),
                nlohmann::json{
                    {"actor", n.id},
                    {"team", n.body.matchTeam},
                    {"mode", serverGamemodeState().matchMode},
                    {"profile", modeProfileId}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    if (modeProfileChanged)
        npcSystem.refreshDifficultyTuning();

    // Each NPC targets its own nearest actor. A mode may own its targeting
    // policy (team modes, e.g. Counter-Strike opposite_team); otherwise the
    // legacy npc-difficulty.json targetMode/damageOtherNpcs applies unchanged.
    // Damage stays server-authoritative: an NPC's damage to its mirror is routed
    // onto the real target, so NPCs behave exactly like players.
    auto npcTeamOf = [&](const Npc& npc) -> int {
        auto it = npcs.find(npc.id);
        return it != npcs.end() ? it->second.matchTeam : npc.body.matchTeam;
    };
    auto playerTeamOf = [&](const ServerPlayer& player) -> int {
        // The roster map is authoritative. The player mirror can be one
        // packet behind during round setup, which must not make every human
        // look teamless and therefore non-hostile.
        auto it = serverGamemodeState().matchTeams.find(player.id);
        return it != serverGamemodeState().matchTeams.end()
            ? it->second : player.matchTeam;
    };
    const auto& npcDifficulty = NpcDifficultyConfig::instance().settings();
    const bool teamMatch = serverGamemodeState().objectiveRounds;

    NpcTargetingPolicy targeting;
    const Gamemode& activeGamemodeForTargeting =
        GamemodeRegistry::instance().get(serverGamemodeState().matchMode);
    if (activeGamemodeForTargeting.npcTargeting.configured) {
        targeting.configured = true;
        bool modeOk = false;
        targeting.mode = npcTargetingModeFromString(
            activeGamemodeForTargeting.npcTargeting.mode, modeOk);
        if (!modeOk) targeting.mode = NpcTargetingMode::Closest;
        targeting.includePlayers = activeGamemodeForTargeting.npcTargeting.includePlayers;
        targeting.includeNpcs = activeGamemodeForTargeting.npcTargeting.includeNpcs;
    } else {
        // Legacy fallback: identical to the previous npc-difficulty behavior.
        targeting.configured = false;
        targeting.mode = npcDifficulty.targetMode == "player"
            ? NpcTargetingMode::Player : NpcTargetingMode::Closest;
        targeting.includePlayers = true;
        targeting.includeNpcs = npcDifficulty.damageOtherNpcs || teamMatch;
    }
    const bool playersOnly = targeting.mode == NpcTargetingMode::Player;
    const bool considerPlayers = npcTargetingIncludesPlayers(targeting);
    const bool allowNpcTargets = npcTargetingIncludesNpcs(targeting);
    const bool freezeWaveBanner = serverGamemodeState().npcWaves
        && serverGamemodeState().waveBannerVisible
        && npcDifficulty.freezeDuringWaveBanner;
    for (Npc& n : npcSystem.all())
    {
        // Replay-controlled target proxies are driven by the scenario track,
        // not AI, targeting, navigation, or normal physics.
        if (n.replayControlled)
            continue;
        if (n.body.dead || n.body.currentHp <= 0)
            continue;
        if (freezeWaveBanner)
            continue;

        const uint32_t prevTarget = n.serverTargetId;
        const int myTeam = npcTeamOf(n);
        ServerPlayer* nearestPlayer = nullptr;
        Npc* nearestNpc = nullptr;

        auto chooseNearestPlayer = [&]() {
            float bestD2 = std::numeric_limits<float>::max();
            if (considerPlayers) for (auto& kv : players)
            {
                ServerPlayer& p = kv.second;
                if (p.dead || p.downed || p.connectionStale) continue;
                // Team gate: never target a same-team human. Applies in every
                // target mode, including "player" mode.
                if (!npcTargetingIsHostile(targeting, myTeam, playerTeamOf(p))) continue;
                const glm::vec3 d = p.pos - n.body.pos;
                const float d2 = glm::dot(d, d);
                if (d2 < bestD2)
                {
                    bestD2 = d2;
                    nearestPlayer = &p;
                    nearestNpc = nullptr;
                }
            }
        };

        if (playersOnly)
        {
            // Player-priority mode: prefer the nearest live hostile human, but
            // still respect teams and fall back to hostile NPCs when no enemy
            // human is in play (so both squads actually fight each other).
            chooseNearestPlayer();
            if (!nearestPlayer && allowNpcTargets) {
                float bestD2 = std::numeric_limits<float>::max();
                for (Npc& other : npcSystem.all()) {
                    if (&other == &n) continue;
                    if (other.body.dead || other.body.currentHp <= 0) continue;
                    if (!npcTargetingIsHostile(targeting, myTeam, npcTeamOf(other))) continue;
                    const glm::vec3 d = other.body.pos - n.body.pos;
                    const float d2 = glm::dot(d, d);
                    if (d2 < bestD2) { bestD2 = d2; nearestNpc = &other; nearestPlayer = nullptr; }
                }
            }
        }
        else if (n.behavior.active)
        {
            // Scored target selection. Weights come only from the behavior
            // profile; the legacy nearest-hostile path is used without one.
            const NpcBehaviorTuning& b = n.behavior;
            auto scoreCandidate = [&](const glm::vec3& pos, int hp, int maxHp,
                                      float threat01) {
                const glm::vec3 d = pos - n.body.pos;
                const float dist = glm::length(d);
                const float distanceScore = 1.0f / (1.0f + dist);
                const float healthFrac = maxHp > 0
                    ? glm::clamp((float)hp / (float)maxHp, 0.0f, 1.0f) : 1.0f;
                const float vulnerability = 1.0f - healthFrac;
                return distanceScore * b.distanceTargetBias
                     + vulnerability * b.lowHealthTargetBias
                     + threat01 * b.threatBias;
            };

            float bestScore = -1e30f;
            float currentScore = -1e30f;
            if (considerPlayers) for (auto& kv : players)
            {
                ServerPlayer& p = kv.second;
                if (p.dead || p.downed || p.connectionStale) continue;
                if (!npcTargetingIsHostile(targeting, myTeam, playerTeamOf(p))) continue;
                float s = scoreCandidate(p.pos, p.health, std::max(1, p.maxHealth), 0.5f);
                const bool isCurrent = (p.id == n.serverTargetId);
                if (isCurrent) s += b.targetStickiness;
                if (s > bestScore) { bestScore = s; nearestPlayer = &p; nearestNpc = nullptr; }
                if (isCurrent) currentScore = s;
            }
            if (allowNpcTargets) for (Npc& other : npcSystem.all())
            {
                if (&other == &n) continue;
                if (other.body.dead || other.body.currentHp <= 0) continue;
                if (!npcTargetingIsHostile(targeting, myTeam, npcTeamOf(other))) continue;
                float threat01 = 0.0f;
                if (const WeaponDefinition* wd =
                        WeaponRegistry::instance().get(other.body.equippedWeaponId))
                    threat01 = glm::clamp(wd->damage / 50.0f, 0.0f, 1.0f);
                float s = scoreCandidate(other.body.pos, other.body.currentHp,
                                         other.body.maxHp, threat01);
                const bool isCurrent = (other.id == n.serverTargetId);
                if (isCurrent) s += b.targetStickiness;
                if (s > bestScore) { bestScore = s; nearestNpc = &other; nearestPlayer = nullptr; }
                if (isCurrent) currentScore = s;
            }

            // Switch only when a new candidate beats the current target by the
            // configured threshold (reduces target thrashing).
            if ((nearestPlayer || nearestNpc) && n.serverTargetId != 0 &&
                currentScore > -1e29f)
            {
                const uint32_t bestId = nearestPlayer ? nearestPlayer->id : nearestNpc->id;
                if (bestId != n.serverTargetId &&
                    bestScore <= currentScore + b.targetSwitchThreshold)
                {
                    bool kept = false;
                    for (auto& kv : players)
                    {
                        if (kv.second.id == n.serverTargetId && !kv.second.dead)
                        { nearestPlayer = &kv.second; nearestNpc = nullptr; kept = true; break; }
                    }
                    if (!kept)
                        for (Npc& other : npcSystem.all())
                            if (other.id == n.serverTargetId && !other.body.dead &&
                                other.body.currentHp > 0)
                            { nearestNpc = &other; nearestPlayer = nullptr; kept = true; break; }
                }
            }
        }
        else
        {
            // Legacy nearest-hostile (unchanged when no behavior profile applies).
            float bestD2 = std::numeric_limits<float>::max();
            if (considerPlayers) for (auto& kv : players)
            {
                ServerPlayer& p = kv.second;
                if (p.dead || p.downed || p.connectionStale) continue;
                if (!npcTargetingIsHostile(targeting, myTeam, playerTeamOf(p))) continue;
                const glm::vec3 d = p.pos - n.body.pos;
                const float d2 = glm::dot(d, d);
                if (d2 < bestD2) { bestD2 = d2; nearestPlayer = &p; nearestNpc = nullptr; }
            }
            if (allowNpcTargets) for (Npc& other : npcSystem.all())
            {
                if (&other == &n) continue;
                if (other.body.dead || other.body.currentHp <= 0) continue;
                if (!npcTargetingIsHostile(targeting, myTeam, npcTeamOf(other))) continue;
                const glm::vec3 d = other.body.pos - n.body.pos;
                const float d2 = glm::dot(d, d);
                if (d2 < bestD2) { bestD2 = d2; nearestNpc = &other; nearestPlayer = nullptr; }
            }
        }

        n.serverTargetId = nearestPlayer ? nearestPlayer->id
                        : (nearestNpc ? nearestNpc->id : 0);
        if (n.serverTargetId != prevTarget)
        {
            float targetDist = 0.0f;
            if (nearestPlayer) targetDist = glm::length(nearestPlayer->pos - n.body.pos);
            else if (nearestNpc) targetDist = glm::length(nearestNpc->body.pos - n.body.pos);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.target-changed", std::to_string(n.id), "target_changed", tick,
                nlohmann::json{
                    {"actor", n.id},
                    {"team", n.body.matchTeam},
                    {"profile", n.behaviorProfileId},
                    {"preset", n.actorPresetId},
                    {"previous_target", prevTarget},
                    {"target_actor", n.serverTargetId},
                    {"target_distance", targetDist},
                    {"pos", {n.body.pos.x, n.body.pos.y, n.body.pos.z}}},
                __FILE__, __LINE__, __FUNCTION__);
        }

        if (nearestPlayer)
        {
            mirrorPlayer.pos = nearestPlayer->pos;
            mirrorPlayer.vel = nearestPlayer->vel;
            mirrorPlayer.yaw = nearestPlayer->yaw;
            mirrorPlayer.currentHp = nearestPlayer->health;
            mirrorPlayer.dead = nearestPlayer->dead;
            mirrorPlayer.username = nearestPlayer->name;
            mirrorPlayer.matchTeam = playerTeamOf(*nearestPlayer);
        }
        else if (nearestNpc)
        {
            const int targetHp = (npcs.find(nearestNpc->id) != npcs.end())
                ? npcs.find(nearestNpc->id)->second.health
                : nearestNpc->body.currentHp;
            mirrorPlayer.pos = nearestNpc->body.pos;
            mirrorPlayer.vel = nearestNpc->body.vel;
            mirrorPlayer.yaw = nearestNpc->body.yaw;
            mirrorPlayer.currentHp = targetHp;
            mirrorPlayer.dead = nearestNpc->body.dead;
            mirrorPlayer.username = nearestNpc->body.username;
            mirrorPlayer.matchTeam = npcTeamOf(*nearestNpc);
        }
        else
        {
            // No live hostile target: still advance the NPC's physics so it
            // falls from its spawn and stands on the floor instead of hovering
            // frozen in the air. A dead mirror means no targeting, no combat,
            // and no damage.
            mirrorPlayer.pos = n.body.pos;
            mirrorPlayer.vel = glm::vec3(0.0f);
            mirrorPlayer.yaw = n.body.yaw;
            mirrorPlayer.currentHp = 0;
            mirrorPlayer.dead = true;
        }
        // Clear the mirror's impulse so it only carries the knockback applied
        // THIS tick by processPlayerHit (same knockback a player's shot gives).
        mirrorPlayer.externalImpulse = glm::vec3(0.0f);
        const int hpBefore = mirrorPlayer.currentHp;

        npcSystem.updateOneWithTarget(n.id, world, mirrorPlayer, SERVER_DT);

        // Ground clamp: the decimated headless collision world can miss the
        // floor, so a server NPC that ends up below the floor gets pinned back
        // onto the nearest floor triangle and marked grounded — it never sinks
        // through the ground forever. Legitimate airborne/jumping NPCs above the
        // floor are untouched; the clamp only corrects downward violations.
        {
            constexpr float REST_HEIGHT = 1.8f; // capsule half-height (feet at pos.z - 1.8)
            const float floorZ = NpcNavigation::groundHeightAt(
                world, n.body.pos, 100.0f, 5.0f);
            if (floorZ > -1e5f)
            {
                const float restZ = floorZ + REST_HEIGHT;
                if (n.body.pos.z < restZ)
                {
                    n.body.pos.z = restZ;
                    if (n.body.vel.z < 0.0f)
                        n.body.vel.z = 0.0f;
                    n.body.ground.hasWorldContact = true;
                    n.body.ground.stableOnGround = true;
                    n.body.ground.onGround = true;
                    n.body.ground.realWorldContactThisFrame = true;
                }
            }
        }

        if ((nearestPlayer || nearestNpc) && hpBefore > mirrorPlayer.currentHp)
        {
            const int damage = hpBefore - mirrorPlayer.currentHp;
            // Forward the exact knockback processPlayerHit applied to the mirror
            // so NPC shots push the victim like a player's shot (also fixes the
            // client HP bar, which only applies confirmed HP when knockback exists).
            const glm::vec3 knockback = mirrorPlayer.externalImpulse;
            const glm::vec3 realHit = n.lastShotEnd;
            const glm::vec3 realNormal = glm::length(n.lastShotNormal) > 0.001f
                ? glm::normalize(n.lastShotNormal) : glm::vec3(0.0f, 0.0f, 1.0f);
            uint8_t hitWeapon = NETWORK_WEAPON_NONE;
            if (const WeaponDefinition* nwdef = WeaponRegistry::instance().get(n.body.equippedWeaponId))
                hitWeapon = networkWeaponTypeForDefinition(*nwdef);
            const char* wId = networkWeaponTypeName(hitWeapon);
            std::string wDisp = wId;
            if (const WeaponDefinition* wd = WeaponRegistry::instance().get(wId))
                if (!wd->displayName.empty()) wDisp = wd->displayName;

            if (nearestPlayer)
            {
                ServerActorDamageRequest damageRequest;
                damageRequest.attacker = {
                    n.id, ServerActorKind::Npc, nullptr, nullptr};
                damageRequest.victim = {
                    nearestPlayer->id, ServerActorKind::Player, nullptr, nullptr};
                damageRequest.damage = damage;
                damageRequest.knockback = knockback;
                damageRequest.source = ServerDamageSource::Hitscan;
                damageRequest.serverTick = tick;
                ServerDamageResult result = applyActorDamage(
                    players, npcs, damageRequest);
                // Track NPC damage for kill attribution: if this NPC's damage
                // brings the player to 0 HP on the next tick (or the player dies
                // from self-damage shortly after), attribute the kill to this NPC.
                nearestPlayer->lastNpcDamageSourceId = n.id;
                nearestPlayer->lastNpcDamageTick = tick;
                queueServerDamageConfirmedEvent(
                    sock, players, tick, totalPacketsOut, 0, *nearestPlayer, damage, result,
                    realHit, realNormal, knockback,
                    ServerDamageSource::Hitscan, hitWeapon, 0, 0, n.id,
                    n.body.equippedWeaponId);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::NpcCombat, StructuredLevel::Important,
                    "npc.damage", std::to_string(n.id), "npc_hits_player",
                    tick,
                    nlohmann::json{
                        {"actor", n.id},
                        {"team", n.body.matchTeam},
                        {"profile", n.behaviorProfileId},
                        {"victim_actor", nearestPlayer->id},
                        {"weapon", n.body.equippedWeaponId},
                        {"damage", damage},
                        {"health_before", result.healthBefore},
                        {"health_after", result.healthAfter},
                        {"accepted", result.applied},
                        {"knockback", {knockback.x, knockback.y, knockback.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
                const ServerGamemodeState& gms = serverGamemodeState();
                DBG(Network,
                    "SERVER_NPC_KILLS_PLAYER proc=server npcId=%u npcName=\"%s\" "
                    "playerId=%u playerName=\"%s\" weaponId=\"%s\" weaponDisplay=\"%s\" "
                    "damage=%d healthBefore=%d healthAfter=%d killed=%d applied=%d "
                    "serverTick=%u serverCode=\"%s\" gamemode=\"%s\" matchMode=\"%s\" "
                    "phase=%d mapOnly=%d enabled=%d",
                    n.id, n.body.username.c_str(),
                    nearestPlayer->id, nearestPlayer->name.c_str(),
                    wId, wDisp.c_str(),
                    damage, result.healthBefore, result.healthAfter,
                    (int)result.killed, (int)result.applied,
                    tick, getServerCoordinatorCode().c_str(),
                    gms.communityMode.c_str(), gms.matchMode.c_str(),
                    (int)gms.phase, (int)gms.mapOnly, (int)gms.enabled);
            }
            else if (nearestNpc)
            {
                auto victimIt = npcs.find(nearestNpc->id);
                if (victimIt != npcs.end() && victimIt->second.health > 0)
                {
                    ServerNpc& victim = victimIt->second;
                    victim.health = std::max(0, victim.health - damage);
                    victim.knockbackImpulse += knockback;
                    nearestNpc->body.currentHp = victim.health;
                    nearestNpc->body.killedByWeapon = wId;
                    nearestNpc->body.lastDamagedBy = n.body.username;
                    const bool killed = victim.health <= 0;
                    if (killed)
                    {
                        victim.health = 0;
                        nearestNpc->body.currentHp = 0;
                        nearestNpc->body.dead = true;
                        nearestNpc->body.respawnTimer = serverMatchRespawnsEnabled()
                            ? serverMatchRespawnSeconds() : -1.0f;
                        serverGamemodeRecordKill(sock, players, &npcs,
                            n.id, ENTITY_NPC, nearestNpc->id, ENTITY_NPC,
                            wId, wDisp, tick,
                            n.body.pos, nearestNpc->body.pos, tick, totalPacketsOut);
                    }
                    DBG(Network,
                        "SERVER_NPC_KILLS_NPC proc=server npcId=%u npcName=\"%s\" "
                        "victimNpcId=%u victimName=\"%s\" weaponId=\"%s\" weaponDisplay=\"%s\" "
                        "damage=%d victimHealth=%d killed=%d serverTick=%u",
                        n.id, n.body.username.c_str(),
                        nearestNpc->id, nearestNpc->body.username.c_str(),
                        wId, wDisp.c_str(), damage, victim.health,
                        (int)killed, tick);
                }
            }
        }
    }

    // Capsule recovery: keep the NPC root out of static solid geometry without
    // scanning the whole map for every NPC every tick. One candidate is checked
    // per tick, rotating through the roster. A candidate qualifies when the
    // navigation code reports it stuck, or on a staggered low-frequency
    // fallback. The deep inside-solid query is cheap enough at this rate even
    // on large maps (funworld3 has ~44k collision triangles).
    {
        std::vector<Npc>& allNpcs = npcSystem.all();
        if (!allNpcs.empty()) {
            static size_t recoveryCursor = 0;
            static std::unordered_map<uint32_t, uint32_t> recoveryCooldownTick;
            const size_t start = recoveryCursor % allNpcs.size();
            for (size_t offset = 0; offset < allNpcs.size(); ++offset) {
                const size_t index = (start + offset) % allNpcs.size();
                Npc& candidate = allNpcs[index];
                if (candidate.replayControlled || candidate.body.dead ||
                    candidate.body.currentHp <= 0)
                    continue;
                const auto cooldownIt = recoveryCooldownTick.find(candidate.id);
                if (cooldownIt != recoveryCooldownTick.end() &&
                    tick < cooldownIt->second)
                    continue;
                const bool stuck =
                    NpcNavigation::isStuck(candidate) ||
                    candidate.wasStuck ||
                    candidate.movementStuckEpisodeActive;
                const bool periodic =
                    ((tick + candidate.id) % 120u) == 0u;
                if (!stuck && !periodic)
                    continue;
                recoveryCursor = index + 1;
                if (worldPointInsideSolid(world, candidate.body.pos)) {
                    // Cheap recovery: use the broadphase ground query instead of
                    // the full multi-candidate safe-spawn search. The latter can
                    // scan the whole 44k-triangle map several times and create a
                    // multi-hundred-ms server spike when an NPC stays inside
                    // geometry. If this single correction cannot find a clear
                    // standing point, wait for the cooldown rather than stalling
                    // the whole server.
                    const glm::vec3 before = candidate.body.pos;
                    glm::vec3 resolved = before;
                    const float floorZ = NpcNavigation::groundHeightAt(
                        world, before, 100.0f, 5.0f);
                    if (floorZ > -1e5f)
                        resolved.z = floorZ + 1.8f;
                    recoveryCooldownTick[candidate.id] = tick + 120u;
                    if (glm::length(resolved - before) > 0.001f &&
                        !worldPointInsideSolid(world, resolved)) {
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::Collision, StructuredLevel::Important,
                            "npc.capsule.recovered", std::to_string(candidate.id),
                            "npc root was inside static solid geometry", tick,
                            {{"npc_id", candidate.id},
                             {"from", {before.x, before.y, before.z}},
                             {"to", {resolved.x, resolved.y, resolved.z}}},
                            __FILE__, __LINE__, __FUNCTION__);
                        candidate.body.pos = resolved;
                        candidate.body.vel = glm::vec3(0.0f);
                    }
                }
                // At most one deep inside-solid check per server tick.
                break;
            }
        }
    }

    // Authoritative actor-pair collision runs after all player/NPC movement
    // and damage for this tick, before dead bodies are rebuilt into the
    // broadcast map. Players and NPCs share this one pass.
    resolveServerActorPairCollisions(players, npcSystem, playerWorld, world, tick);

    // Respawn killed NPCs (updateOneNpc freezes dead bodies; this loop drives
    // their countdown and resets them so rebuildServerNpcMap re-admits them).
    // One-life modes never revive: the dead body stays Spectating for the round.
    const bool npcRespawns = serverMatchRespawnsEnabled();
    const bool npcWaveMode = serverGamemodeState().npcWaves;
    for (Npc& n : npcSystem.all())
    {
        if (n.replayControlled) continue;
        if (!n.body.dead) continue;
        if (!npcRespawns) continue;
        // NPC Waves actors are one-life per wave. Reviving them makes the wave
        // clear condition unreachable even when every spawned NPC is killed.
        if (npcWaveMode) continue;
        // Procedural encounter NPCs never revive: a room completes exactly when
        // its alive count reaches zero. Dead bodies linger for their normal
        // death presentation only.
        if (serverProceduralWorldOwnsNpc(n.id)) continue;
        n.body.respawnTimer = std::max(0.0f, n.body.respawnTimer - SERVER_DT);
        if (n.body.respawnTimer <= 0.0f)
            respawnServerNpc(n, world);
    }

    // The NPC map is the authoritative lifecycle gate for its rockets. This
    // runs before the next NPC fire broadcast, so an NPC that died this tick
    // cannot leave a projectile alive or fire again while its body respawns.
    cancelDeadNpcProjectiles(sock, players, npcs, projectiles, tick, totalPacketsOut);

    // Broadcast NPC weapon fire so clients see/hear the shot.
    // For projectile weapons (rocket, grenade), also creates ServerProjectile
    // and broadcasts ProjectileSpawnEventPacket — same path as player rockets.
    broadcastNpcFiring(sock, players, npcSystem, projectiles, nextProjectileId, tick, totalPacketsOut);

    // Once-per-second per-NPC fire-state summary: which gate would block firing
    // (state, ammo, reload, cooldown, distance, LOS). One aggregate line, never
    // per-frame spam.
    {
        static uint64_t lastFireStateLog = 0;
        const uint64_t nowFire = nowMs();
        if (nowFire - lastFireStateLog >= 1000)
        {
            lastFireStateLog = nowFire;
            std::string summary;
            for (const Npc& n : npcSystem.all())
            {
                const auto& rtIt = n.body.weaponRuntimes.find(n.body.equippedWeaponId);
                const bool hasRt = rtIt != n.body.weaponRuntimes.end();
                const float planarSpeed =
                    glm::length(glm::vec2(n.body.vel.x, n.body.vel.y));
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "id=%u state=%s spd=%.1f ammo=%d reserve=%d reloading=%d cd=%.2f "
                    "dist=%.1f los=%d hasTarget=%d; ",
                    n.id, npcStateName(n.stateMachine.currentState).c_str(),
                    planarSpeed,
                    hasRt ? rtIt->second.currentAmmo : -1,
                    hasRt ? rtIt->second.reserveAmmo : -1,
                    hasRt ? (int)rtIt->second.isReloading : -1,
                    n.attackCooldown,
                    n.sensors.targetDistance,
                    (int)n.cachedLoSBlocked, (int)n.sensors.hasTarget);
                summary += buf;
            }
            Debug::warn(Debug::Category::NpcCombat,
                "[SERVER NPC FIRE-STATE] %s\n", summary.c_str());
        }
    }

    rebuildServerNpcMap(npcs, npcSystem, npcIdsAlive);

    // Record each NPC's broadcast pose for hit-rewind validation. This runs
    // after the map rebuild so the recorded pose is exactly what the snapshot
    // built for this tick will contain (what clients see and shoot at).
    for (auto& kv : npcs)
        pushNpcPositionHistory(kv.second, tick);

    static uint64_t lastNpcPosLog = 0;
    const uint64_t now = nowMs();
    if (now - lastNpcPosLog >= 1000)
    {
        lastNpcPosLog = now;
        size_t living = 0;
        for (const auto& kv : npcs)
            if (kv.second.health > 0)
                ++living;
        printf("[SERVER NPC SUMMARY] count=%zu living=%zu dead=%zu\n",
               npcs.size(), living, npcs.size() - living);
    }
}

SnapshotEntity makeNpcEntity(const ServerNpc& npc)
{
    SnapshotEntity out{};
    out.networkEntityId = npc.entityId;
    out.entityType = ENTITY_NPC;
    out.active = 1;
    out.ownerClientId = 0;
    out.transformEpoch = npc.transformEpoch;
    out.px = npc.pos.x; out.py = npc.pos.y; out.pz = npc.pos.z;
    out.vx = npc.vel.x; out.vy = npc.vel.y; out.vz = npc.vel.z;
    out.aimX = npc.aim.x; out.aimY = npc.aim.y; out.aimZ = npc.aim.z;
    out.yaw = npc.yaw;
    out.sizeScale = npc.sizeScale;
    out.health = npc.health;
    out.meleePercent = npc.meleePercent;
    out.onGround = npc.onGround ? 1 : 0;
    out.equippedSlot = npc.equippedSlot;
    out.weaponState = npc.weaponState;
    serverWriteWeaponActionSnapshot(npc.equippedWeaponId, npc.meleeActionActive,
        npc.meleeActionId, npc.meleeActionStartTick, out);
    out.dashSerial = npc.dashSerial;
    out.groundJumpSerial = npc.groundJumpSerial;
    out.airJumpSerial = npc.airJumpSerial;
    out.downDashSerial = npc.downDashSerial;
    out.freezeSerial = npc.freezeSerial;
    out.sizeScale = npc.sizeScale;
    out.stateFlags = 0;
    if (npc.onGround)
        out.stateFlags |= NET_STATE_ON_GROUND;
    if (glm::length(npc.vel) > 0.5f)
        out.stateFlags |= NET_STATE_WALKING;

    if (npc.health <= 0)
    {
        const uint16_t stateFlagsBeforeDeathSanitize = out.stateFlags;
        const uint32_t dashSerialBeforeDeathSanitize = out.dashSerial;
        const uint32_t groundJumpSerialBeforeDeathSanitize = out.groundJumpSerial;
        const uint32_t airJumpSerialBeforeDeathSanitize = out.airJumpSerial;
        const uint32_t downDashSerialBeforeDeathSanitize = out.downDashSerial;
        const uint32_t freezeSerialBeforeDeathSanitize = out.freezeSerial;

        // Dead NPCs remain replicated so the client can render their corpse,
        // but they must not replicate live movement or action presentation.
        out.stateFlags = 0;
        out.weaponState = 0;
        out.dashSerial = 0;
        out.groundJumpSerial = 0;
        out.airJumpSerial = 0;
        out.downDashSerial = 0;
        out.freezeSerial = 0;

        static uint64_t lastDeadNpcSnapshotDiagnosticMs = 0;
        const uint64_t nowDeadNpcSnapshot = nowMs();
        if (nowDeadNpcSnapshot - lastDeadNpcSnapshotDiagnosticMs >= 1000)
        {
            lastDeadNpcSnapshotDiagnosticMs = nowDeadNpcSnapshot;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.dead.npc.snapshot.sanitized",
                std::to_string(npc.entityId),
                "dead NPC snapshot presentation state cleared", 0,
                { {"actor_id", npc.entityId}, {"health", npc.health},
                  {"state_flags_before", stateFlagsBeforeDeathSanitize},
                  {"dash_serial_before", dashSerialBeforeDeathSanitize},
                  {"ground_jump_serial_before", groundJumpSerialBeforeDeathSanitize},
                  {"air_jump_serial_before", airJumpSerialBeforeDeathSanitize},
                  {"down_dash_serial_before", downDashSerialBeforeDeathSanitize},
                  {"freeze_serial_before", freezeSerialBeforeDeathSanitize},
                  {"state_flags_after", out.stateFlags},
                  {"dash_serial_after", out.dashSerial},
                  {"ground_jump_serial_after", out.groundJumpSerial},
                  {"air_jump_serial_after", out.airJumpSerial},
                  {"down_dash_serial_after", out.downDashSerial},
                  {"freeze_serial_after", out.freezeSerial} },
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    copyName(out.displayName, npc.name);
    std::memset(out.avatarName, 0, sizeof(out.avatarName));
    std::strncpy(out.avatarName, npc.avatarName.c_str(), sizeof(out.avatarName) - 1);
    return out;
}

} // namespace MimitaNet
