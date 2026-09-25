// 09 01 2026, 00 00
/* purpose
* Implements the authoritative JSON-defined gamemode state machine on the server.
* Owns shared waiting, intermission, countdown, active, results, and switching phases.
* Routes duel, FFA, TDM, Bomb Tag, sandbox, and future rules through shared actor services.
* Does NOT apply damage, simulate movement, or render anything.
* Does NOT touch the client queue/matchmaking or coordinator protocol.
* Does NOT own mode presentation or mode-specific GUI layout.
*/

#include "network/server-gamemode.h"
#include "live-code/live-journal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

#include "network/packets.h"
#include "network/server.h"
#include "npc/npc.h"
#include "npc/npc-internal.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/community-server-config.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "gamemode/map-config.h"
#include "config/gameplay-config.h"
#include "persistence/persistence-queue.h"
#include "persistence/persistence-events.h"
#include "persistence/persistence-emit.h"
#include "config/spawn-velocity-config.h"
#include "network/actor-lifecycle.h"
#include "network/match-lifecycle.h"
#include "network/server-context.h"
#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/game-api.h"
#include "network/actor-state.h"
#include "network/actor-health.h"
#include "live-code/live-behavior.h"

namespace MimitaNet {

static void finalizeServerNpcMirrorSpawn(ServerNpc& npc,
                                         ActorSpawnReason reason,
                                         uint32_t serverTick)
{
    ActorSpawnEvent event;
    event.entityId = npc.entityId;
    event.actorKind = ActorKind::Npc;
    event.reason = reason;
    event.transformEpoch = npc.transformEpoch;
    event.serverTick = serverTick;
    event.position = npc.pos;
    event.lookDirection = npc.aim;
    event = finalizeActorSpawn(event, npc.name.c_str());
    npc.aim = event.lookDirection;
    npc.yaw = std::atan2(event.lookDirection.y, event.lookDirection.x);
    npc.vel = event.velocity;
    npc.knockbackImpulse = glm::vec3(0.0f);
}

ServerGamemodeState& serverGamemodeState()
{
    static ServerGamemodeState state;
    return state;
}

// ── Generic authoritative match capabilities ────────────────────────────
std::uint64_t serverMatchEntity()
{
    return serverGamemodeState().matchEntity;
}

void serverMatchResetEntity()
{
    ServerGamemodeState& d = serverGamemodeState();
    if (d.matchEntity != 0) {
        EntityRegistry::instance().destroy(static_cast<EntityId>(d.matchEntity));
        d.matchEntity = 0;
    }
    d.matchEntity = static_cast<std::uint64_t>(
        EntityRegistry::instance().createGeneric(EntityRealm::Server));
    if (GameSharedStateV1* shared =
            MimitaRuntime::GenericRuntime::instance().sharedState())
        shared->matchEntity = d.matchEntity;
}

bool serverMatchFinish(std::uint32_t winnerKind, std::uint32_t winnerId,
                       std::uint32_t victoryType)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled)
        return false;
    d.matchOver = true;
    d.phase = DUEL_PHASE_RESULTS;
    d.phaseTimer = d.resultsSeconds;
    d.victoryType = static_cast<int>(victoryType);
    if (winnerKind == 2) {
        d.winnerTeam = static_cast<int>(winnerId);
    } else if (winnerKind == 1) {
        d.winnerPlayerId = winnerId;
        d.winnerTeam = -1;
    }
    ++d.stateVersion;
    d.stateBroadcastPending = true;
    return true;
}

// Resolve an actor id (player/NPC) to its server entity. Entity ids are
// domain-tagged; a hot package may also create generic (domain-less) actors, so
// match on the legacy id rather than assuming a domain.
static EntityId resolveActorEntity(std::uint32_t actorId)
{
    for (EntityId entity : EntityRegistry::instance().all()) {
        if (entityRealm(entity) != EntityRealm::Server)
            continue;
        if (entityLegacyId(entity) == actorId)
            return entity;
    }
    return 0;
}

std::int32_t serverMatchActorTeam(std::uint32_t actorId)
{
    // Generic ActorTeamState (the actor entity's component) is the source of
    // truth; the typed map below is only the projection for the scoreboard and
    // wire broadcast.
    const EntityId entity = resolveActorEntity(actorId);
    if (entity != 0) {
        std::int32_t team = -2;
        if (actorStateReadTeam(Ecs::raw(entity), &team))
            return team;
    }
    const ServerGamemodeState& d = serverGamemodeState();
    auto it = d.matchTeams.find(actorId);
    return it == d.matchTeams.end() ? -1 : it->second;
}

bool serverMatchSetTeam(std::uint32_t actorId, std::int32_t team)
{
    serverGamemodeState().matchTeams[actorId] = team;
    // Mirror the decision into the generic component for the actor entity.
    const EntityId entity = resolveActorEntity(actorId);
    if (entity != 0)
        actorStateWriteTeam(Ecs::raw(entity), team);
    return true;
}

bool serverMatchRecordRoundResult(std::uint32_t winnerTeam,
                                  std::uint64_t reasonHash)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled || winnerTeam > 1u)
        return false;
    ++d.roundWins[winnerTeam];
    d.roundOver = true;
    d.winnerTeam = static_cast<int>(winnerTeam);
    d.matchOver = d.roundWins[winnerTeam] >= d.goalValue;
    d.victoryType = 0;
    d.phase = DUEL_PHASE_RESULTS;
    d.phaseTimer = d.resultsSeconds;
    ++d.stateVersion;
    d.stateBroadcastPending = true;
    Debug::warn(Debug::Category::Duel,
        "[ROUND] hot round result winner=%u reason=%llu score=%d-%d matchOver=%d\n",
        winnerTeam, (unsigned long long)reasonHash, d.roundWins[0], d.roundWins[1],
        (int)d.matchOver);
    return true;
}

std::uint32_t serverMapAnchors(GameMapAnchorV1* out, std::uint32_t maxOut)
{
    if (!out || maxOut == 0)
        return 0;
    const ServerGamemodeState& d = serverGamemodeState();
    const MapConfig& mc = MapConfigRegistry::instance().get(d.mapId);
    const std::uint64_t siteKind = gameHash("objective.site");
    const std::uint64_t spawnKind = gameHash("spawn.team");
    std::uint32_t count = 0;
    auto emit = [&](const glm::vec3& p, float radius, std::uint64_t kind,
                    std::uint32_t tag, float yaw) {
        if (count >= maxOut)
            return;
        out[count].position[0] = p.x;
        out[count].position[1] = p.y;
        out[count].position[2] = p.z;
        out[count].radius = radius;
        out[count].kind = kind;
        out[count].tag = tag;
        out[count].yaw = yaw;
        ++count;
    };
    for (const MapBombSite& site : mc.bombSites)
        emit(site.center, site.radius, siteKind, 0u, 0.0f);
    // Team spawn points are map-authored generic anchors (tag = team index).
    for (std::uint32_t team = 0; team < 2; ++team)
        for (const MapTeamSpawn& sp : mc.teamSpawns[team])
            emit(sp.position, 0.0f, spawnKind, team, sp.yaw);
    return count;
}

// Generic authoritative spatial bridge. The generic Transform/Velocity
// components are the source of truth; typed ServerPlayer/ServerNpc fields are
// refreshed from them (FromGeneric) or projected back (ToGeneric).
bool serverProjectActorSpatialFromGeneric(std::uint64_t actorEntity)
{
    const EntityId entity = static_cast<EntityId>(actorEntity);
    const auto* tf = EntityRegistry::instance().tryGet<TransformComponent>(entity);
    if (!tf)
        return false;
    const auto* v = EntityRegistry::instance().tryGet<VelocityComponent>(entity);
    const std::uint32_t actorId = entityLegacyId(entity);
    MimitaNet::ServerContextV1* ctx = MimitaNet::activeServerContext();
    bool done = false;
    if (ctx && ctx->players) {
        auto* players = static_cast<std::unordered_map<uint32_t, ServerPlayer>*>(
            ctx->players);
        auto it = players->find(actorId);
        if (it != players->end()) {
            it->second.pos = tf->position;
            it->second.yaw = tf->yaw;
            if (v) {
                it->second.vel = v->linear;
                it->second.movement.externalImpulse = v->externalImpulse;
            }
            done = true;
        }
    }
    if (ctx && ctx->npcs) {
        auto* npcs = static_cast<std::unordered_map<uint32_t, ServerNpc>*>(ctx->npcs);
        auto it = npcs->find(actorId);
        if (it != npcs->end()) {
            it->second.pos = tf->position;
            if (v)
                it->second.vel = v->linear;
            done = true;
        }
    }
    return done;
}

bool serverProjectActorSpatialToGeneric(std::uint64_t actorEntity)
{
    const EntityId entity = static_cast<EntityId>(actorEntity);
    if (!EntityRegistry::instance().alive(entity))
        return false;
    const std::uint32_t actorId = entityLegacyId(entity);
    MimitaNet::ServerContextV1* ctx = MimitaNet::activeServerContext();
    glm::vec3 pos(0.0f);
    glm::vec3 vel(0.0f);
    glm::vec3 impulse(0.0f);
    float yaw = 0.0f;
    bool have = false;
    if (ctx && ctx->players) {
        auto* players = static_cast<std::unordered_map<uint32_t, ServerPlayer>*>(
            ctx->players);
        auto it = players->find(actorId);
        if (it != players->end()) {
            pos = it->second.pos;
            vel = it->second.vel;
            impulse = it->second.movement.externalImpulse;
            yaw = it->second.yaw;
            have = true;
        }
    }
    if (!have && ctx && ctx->npcs) {
        auto* npcs = static_cast<std::unordered_map<uint32_t, ServerNpc>*>(ctx->npcs);
        auto it = npcs->find(actorId);
        if (it != npcs->end()) {
            pos = it->second.pos;
            vel = it->second.vel;
            have = true;
        }
    }
    if (!have)
        return false;
    Ecs::setTransform(entity, pos, glm::vec3(1.0f, 0.0f, 0.0f), yaw, 0.0f);
    Ecs::setVelocity(entity, vel, impulse);
    return true;
}

bool serverSpawnOrResetActor(GameActorSpawnV1& r)
{
    if (r.actorEntity == 0)
        return false;
    const EntityId entity = static_cast<EntityId>(r.actorEntity);
    if (!EntityRegistry::instance().alive(entity))
        return false;
    const std::uint32_t actorId = entityLegacyId(entity);
    const glm::vec3 pos(r.position[0], r.position[1], r.position[2]);
    const glm::vec3 vel(r.velocity[0], r.velocity[1], r.velocity[2]);

    // Generic authoritative components first: the actor's spatial/health truth.
    Ecs::setTransform(entity, pos, glm::vec3(1.0f, 0.0f, 0.0f), r.yaw, 0.0f);
    Ecs::setVelocity(entity, vel, glm::vec3(0.0f));
    if ((r.flags & 4u) != 0 && r.health > 0)
        Ecs::setHealth(entity, r.health, r.health, false);
    else if ((r.flags & 2u) != 0)
        Ecs::setHealth(entity, r.health > 0 ? r.health : 1, r.health > 0 ? r.health : 1,
                       false);

    // Typed projections the client snapshot still reads (until the snapshot pass).
    MimitaNet::ServerContextV1* ctx = MimitaNet::activeServerContext();
    bool mirrored = false;
    if (ctx && ctx->players) {
        auto* players = static_cast<std::unordered_map<uint32_t, ServerPlayer>*>(
            ctx->players);
        auto it = players->find(actorId);
        if (it != players->end()) {
            ServerPlayer& p = it->second;
            p.pos = pos;
            p.vel = vel;
            p.yaw = r.yaw;
            if ((r.flags & 2u) != 0)
                p.dead = false;
            if ((r.flags & 4u) != 0 && r.health > 0)
                p.health = r.health;
            p.respawnSeconds = 0.0f;
            p.justRespawned = true;
            ++p.transformEpoch;
            mirrored = true;
        }
    }
    if (ctx && ctx->npcs) {
        auto* npcs = static_cast<std::unordered_map<uint32_t, ServerNpc>*>(ctx->npcs);
        auto it = npcs->find(actorId);
        if (it != npcs->end()) {
            ServerNpc& n = it->second;
            n.pos = pos;
            n.vel = vel;
            if ((r.flags & 4u) != 0 && r.health > 0)
                n.health = r.health;
            mirrored = true;
        }
    }
    r.applied = 1u;
    (void)mirrored;
    return true;
}

// Project the generic actor team components (source of truth) onto the typed
// matchTeams map and participant roster. This keeps the scoreboard/broadcast
// alive when a hot mode owns assignment and the cold assign path was skipped.
static void projectGenericActorTeams(ServerGamemodeState& d)
{
    bool added = false;
    for (EntityId entity : EntityRegistry::instance().all()) {
        if (entityRealm(entity) != EntityRealm::Server)
            continue;
        std::int32_t team = -2;
        if (!actorStateReadTeam(Ecs::raw(entity), &team))
            continue;
        const std::uint32_t id = entityLegacyId(entity);
        d.matchTeams[id] = team;
        if (std::find(d.participants.begin(), d.participants.end(), id) ==
            d.participants.end()) {
            d.participants.push_back(id);
            added = true;
        }
    }
    if (added)
        std::sort(d.participants.begin(), d.participants.end());
}

bool serverMatchSetPhase(std::uint32_t phase)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled)
        return false;
    d.phase = static_cast<std::uint8_t>(phase);
    // Starting a fresh round clears the previous match-over lock so a hot mode
    // can run multiple rounds via generic phase changes.
    if (phase == DUEL_PHASE_WAITING || phase == DUEL_PHASE_COUNTDOWN ||
        phase == DUEL_PHASE_ACTIVE || phase == DUEL_PHASE_GO)
        d.matchOver = false;
    ++d.stateVersion;
    d.stateBroadcastPending = true;
    return true;
}

bool serverMatchRespawn(std::uint64_t actorEntity)
{
    // Generic authoritative respawn: the shared actor lifecycle is wired here
    // in the timer/respawn slice. Until then nothing to do.
    (void)actorEntity;
    return false;
}

// Data-driven active-mode routing: mode id -> registered descriptor -> domain.
// The kernel never switches on a mode name; if the package registered a mode
// with this id, its domain becomes active (and only then do its systems and
// event handlers run). Otherwise the cold path owns the match.
static void applyActiveHotMode(ServerGamemodeState& d, const std::string& modeId)
{
    MimitaRuntime::GenericRuntime& runtime = MimitaRuntime::GenericRuntime::instance();
    const std::uint64_t hash = modeId.empty() ? 0 : gameHash(modeId.c_str());
    if (hash != 0 && runtime.hasMode(hash)) {
        d.activeModeId = hash;
        d.activeModeDomain = runtime.modeDomain(hash);
    } else {
        d.activeModeId = 0;
        d.activeModeDomain = 0;
    }
    runtime.setActiveModeDomain(d.activeModeDomain);
}

bool serverMatchRespawnsEnabled()
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return true;      // legacy / sandbox keeps instant respawn
    return d.respawnSeconds != 0.0f;  // 0 == one-life
}

// Generic match lifecycle policy: the kernel publishes the authoritative
// defaults; the active mode domain may claim the policy by handling the event
// and writing out-fields. No mode name is switched on here.
static void dispatchMatchLifecyclePolicy(ServerGamemodeState& d, uint32_t tick)
{
    GameMatchLifecycleV1 policy{};
    policy.matchEntity = d.matchEntity;
    policy.tick = tick;
    policy.phase = d.phase;
    policy.participantCount = (std::uint32_t)d.participants.size();
    policy.countdownSeconds = d.countdownSeconds;
    policy.goSeconds = d.goSeconds;
    policy.intermissionSeconds = d.intermissionSeconds;
    policy.resultsSeconds = d.resultsSeconds;
    policy.timeLimitSeconds = (float)d.timeLimitSeconds;
    policy.respawnSeconds = d.respawnSeconds;
    policy.respawnsEnabled = serverMatchRespawnsEnabled() ? 1u : 0u;

    if (!LiveBehavior::dispatchGameplayEvent64(
            GAME_EVENT_MATCH_LIFECYCLE, &policy, sizeof(policy), tick,
            d.matchEntity, 0))
        return;
    if (!policy.handled)
        return;

    if (policy.outCountdownSeconds > 0.0f)
        d.countdownSeconds = policy.outCountdownSeconds;
    if (policy.outGoSeconds > 0.0f)
        d.goSeconds = policy.outGoSeconds;
    if (policy.outIntermissionSeconds > 0.0f)
        d.intermissionSeconds = policy.outIntermissionSeconds;
    if (policy.outResultsSeconds > 0.0f)
        d.resultsSeconds = policy.outResultsSeconds;
    if (policy.outTimeLimitSeconds > 0.0f)
        d.timeLimitSeconds = (int)policy.outTimeLimitSeconds;
    d.respawnSeconds = policy.outRespawnsEnabled
        ? (policy.outRespawnSeconds >= 0.0f ? policy.outRespawnSeconds : 0.01f)
        : 0.0f;
}

float serverMatchRespawnSeconds()
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return 0.01f;
    return d.respawnSeconds >= 0.0f ? d.respawnSeconds : 0.01f;
}

ActorSpawnProfile serverResolveActorSpawnProfile(uint32_t actorId)
{
    ActorSpawnProfile out;
    const ServerGamemodeState& d = serverGamemodeState();

    // Mode-level movement preset applies to every actor unless the actor's role
    // names its own preset. Validated once through the shared role cache.
    auto applyMovementPreset = [&](const std::string& preset) {
        if (preset.empty()) return;
        if (RoleMovementCache::instance().get(preset)) {
            out.movementPreset = preset;
        } else {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] unknown movement preset \"%s\"; using default movement\n",
                preset.c_str());
        }
    };
    applyMovementPreset(d.movementPreset);

    // Generic authority: the role comes from the actor entity's ActorRoleState
    // component when set (players, NPCs, runtime monsters alike). The typed
    // ActorMatchDescriptor.roleId is a compatibility projection/fallback.
    std::string roleId;
    {
        EntityId actorEntity = EntityRegistry::instance().find(
            EntityRealm::Server, EntityDomain::Player, actorId);
        if (actorEntity == kInvalidEntityId)
            actorEntity = EntityRegistry::instance().find(
                EntityRealm::Server, EntityDomain::Npc, actorId);
        if (actorEntity != kInvalidEntityId) {
            std::uint64_t roleHash = 0;
            if (actorStateReadRoleHash(Ecs::raw(actorEntity), &roleHash) && roleHash != 0) {
                const char* id = actorStateRoleIdForHash(roleHash);
                if (id)
                    roleId = id;
            }
        }
    }
    if (roleId.empty()) {
        auto it = d.matchActors.find(actorId);
        if (it != d.matchActors.end())
            roleId = it->second.roleId;
    }
    if (roleId.empty())
        return out;  // no role: mode-level overrides still apply

    const MatchRoleDefinition* def = MatchRoleRegistry::instance().get(roleId);
    if (!def)
        return out;

    out.hasRole = true;
    out.roleId = def->id;
    out.health = def->health;
    out.startingWeapon = def->startingWeapon;

    if (!def->movementPreset.empty()) {
        // Validate once through the cache so an unknown preset is caught here
        // (and logged with role context) instead of every simulation tick.
        const MovementConfig* mc =
            RoleMovementCache::instance().get(def->movementPreset);
        if (mc) {
            out.movementPreset = def->movementPreset;
            Debug::log(Debug::Category::Duel,
                "[ROLE MOVEMENT] actor=%u role=%s preset=%s groundSpeed=%.1f airSpeed=%.1f jump=%.1f gravity=%.1f dash=%d downDash=%d freeze=%d\n",
                actorId, def->id.c_str(), out.movementPreset.c_str(),
                mc->groundSpeed, mc->airSpeed, mc->jumpVerticalSpeed, mc->gravityZ,
                (int)mc->dashEnabled, (int)mc->downDashEnabled, (int)mc->freezeEnabled);
        } else {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] role %s references unknown movement preset \"%s\"; using default movement\n",
                def->id.c_str(), def->movementPreset.c_str());
        }
    }

    if (!def->weaponSet.empty()) {
        const CommunityWeaponSet* set =
            CommunityServerConfig::instance().weaponSetByKey(def->weaponSet);
        if (set) {
            out.weaponSetId = set->id;
            out.weapons = set->weapons;
        } else {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] role %s references unknown weapon_set \"%s\"\n",
                def->id.c_str(), def->weaponSet.c_str());
        }
    }

    if (!def->behaviorProfile.empty()) {
        // Validate once here (warn with role context) instead of per tick.
        if (BehaviorProfileRegistry::instance().get(def->behaviorProfile)) {
            out.behaviorProfileId = def->behaviorProfile;
        } else {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] role %s references unknown behavior profile \"%s\"; using NPC defaults\n",
                def->id.c_str(), def->behaviorProfile.c_str());
        }
    }
    return out;
}

void serverStartMode(const ServerGamemodeState& rules)
{
    ServerGamemodeState& d = serverGamemodeState();
    d.enabled = true;
    d.mapOnly = false;
    d.mode = rules.matchMode == "tdm" ? ServerMode::TeamDeathmatch
        : rules.matchMode == "ffa" ? ServerMode::FreeForAll
        : rules.matchMode == "duel" ? ServerMode::Duel : ServerMode::Sandbox;
    d.communityMode = "sandbox";
    d.communityWeaponSetId = 1;
    d.appliedCommunityWeaponSetId = 0;
    d.communityWeaponSetExplicit = false;
    d.communityScores.clear();
    d.communityTeams.clear();
    d.communityTeamScore[0] = d.communityTeamScore[1] = 0;
    d.communityRoundOver = false;
    d.communityRoundResetMs = 0;
    d.phase = DUEL_PHASE_WAITING;
    d.stateBroadcastPending = false;
    d.matchOver = false;
    d.scoreA = 0;
    d.scoreB = 0;
    d.goalValue = rules.goalValue;
    d.countdownSeconds = rules.countdownSeconds;
    d.rematchSeconds = rules.rematchSeconds;
    d.teamAName = rules.teamAName;
    d.teamBName = rules.teamBName;
    d.playerAId = 0;
    d.playerBId = 0;
    d.winnerPlayerId = 0;
    applyActiveHotMode(d, rules.matchMode);
    d.spawnsAssigned = false;
    d.stateSent = false;
    d.spawnOffsetRadius = rules.spawnOffsetRadius;
    d.mapPool = rules.mapPool;
    d.rotateMaps = rules.rotateMaps;
    d.mapId = rules.mapId;
    d.duelId = 0;
    d.mapVersion = 0;
    d.spawnAnchorVersion = 0;
    d.respawnSequence = 0;
    d.stateVersion = 0;
    d.spawnAnchorIndex = 0;
    d.usedMaps.clear();
    d.usedMaps.insert(rules.mapId);
    d.hasPendingManualMap = false;
    d.pendingManualMap.clear();
    d.autoMapRotation = false;
    d.mapRotationMinutes = 15;
    d.nextMapRotationMs = 0;
    d.mapChangeCountdownStartMs = 0;
    d.pendingAutomaticMap.clear();

    // ── FFA/TDM fields ─────────────────────────────────────────────
    d.matchMode = rules.matchMode;
    d.countdownStartTick = 0;
    d.matchStartTick = 0;
    d.matchTimeLimitTick = 0;
    d.phaseTimer = 0.0f;
    d.intermissionSeconds = rules.intermissionSeconds;
    d.resultsSeconds = rules.resultsSeconds;
    d.timeLimitSeconds = rules.timeLimitSeconds;
    d.respawnSeconds = rules.respawnSeconds;
    d.killHeals = rules.killHeals;
    d.winCondition = rules.winCondition;
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;
    d.matchTeams.clear();
    d.participants.clear();
    d.participantNames.clear();
    d.victoryType = 0;
    d.winnerTeam = -1;
    d.killEventCounter = 0;
    d.pendingKillEvents.clear();

    // ── Forced gameplay overrides / objective rounds ───────────────
    d.aimMode.clear();
    d.movementPreset.clear();
    d.healthbarOverride = false;
    d.appliedRulesRevision = 0;
    d.objectiveRounds = false;
    d.roundWins[0] = d.roundWins[1] = 0;
    d.roundNumber = 0;
    d.roundOver = false;
    d.roundEndReason.clear();
    d.teamSpawnPoints[0].clear();
    d.teamSpawnPoints[1].clear();
    d.objectiveBombState = BOMB_OBJ_NONE;
    d.objectiveBombCarrierId = 0;
    d.objectiveBombPos = glm::vec3(0.0f);
    d.objectiveBombTimer = 0.0f;
    d.objectiveBombPlantProgress = 0.0f;
    d.objectiveBombDefuseProgress = 0.0f;
    d.appliedRulesRevision = GamemodeRegistry::instance().revision();

    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] enabled mode=%s goal=%d countdown=%.1fs rematch=%.1fs teams=%s/%s rotate=%d pool=%zu offset=%.1f timeLimit=%d intermission=%d results=%d\n",
        d.matchMode.c_str(), d.goalValue, d.countdownSeconds, d.rematchSeconds,
        d.teamAName.c_str(), d.teamBName.c_str(), (int)d.rotateMaps, d.mapPool.size(),
        d.spawnOffsetRadius, d.timeLimitSeconds, (int)d.intermissionSeconds, (int)d.resultsSeconds);
}

void serverGamemodeStart(const ServerGamemodeState& rules)
{
    serverStartMode(rules);
}

void serverCommunityMapStart(const std::vector<std::string>& mapPool,
                             const std::string& mapId,
                             bool autoRotation,
                             uint32_t rotationMinutes,
                             int weaponSetId)
{
    ServerGamemodeState rules;
    rules.mapPool = mapPool;
    rules.mapId = mapId;
    rules.rotateMaps = false;
    rules.mapOnly = true;
    rules.autoMapRotation = autoRotation;
    rules.mapRotationMinutes = std::clamp(rotationMinutes, 1u, 9999u);
    serverStartMode(rules);
    ServerGamemodeState& state = serverGamemodeState();
    state.mapOnly = true;
    state.mode = ServerMode::Sandbox;
    state.communityMode = "sandbox";
    state.communityWeaponSetId = std::max(1, weaponSetId);
    state.communityWeaponSetExplicit = true;
    state.autoMapRotation = rules.autoMapRotation;
    state.mapRotationMinutes = rules.mapRotationMinutes;
    state.nextMapRotationMs = nowMs() + (uint64_t)state.mapRotationMinutes * 60000ull;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY MAP RUNTIME] map=%s auto=%d intervalMinutes=%u pool=%zu\n",
        mapId.c_str(), (int)autoRotation, state.mapRotationMinutes, mapPool.size());
}

void serverCommunitySetWeaponSet(int weaponSetId)
{
    ServerGamemodeState& state = serverGamemodeState();
    if (!state.enabled || !state.mapOnly) return;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    if (!config.weaponSetById(weaponSetId)) return;
    state.communityWeaponSetId = weaponSetId;
    state.communityWeaponSetExplicit = true;
    state.appliedCommunityWeaponSetId = 0;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY WEAPON SET] selected=%d\n", state.communityWeaponSetId);
}

bool serverCommunityWeaponAllowed(const std::string& weaponId)
{
    const ServerGamemodeState& state = serverGamemodeState();
    const bool communityMode = state.communityMode == "sandbox"
        || state.communityMode == "free_for_all"
        || state.communityMode == "team_deathmatch"
        || state.hasBombFeature;
    if (!communityMode) return true;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    return config.weaponAllowed(state.communityWeaponSetId, weaponId);
}

int serverCommunityWeaponNativeSlot(int logicalSlot)
{
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const std::string* id = config.weaponForSlot(serverGamemodeState().communityWeaponSetId, logicalSlot);
    if (!id) return logicalSlot;
    const WeaponDefinition* def = WeaponRegistry::instance().get(*id);
    return def ? def->slot : -1;
}

int serverCommunityWeaponLogicalSlot(const std::string& weaponId)
{
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const int slot = config.slotForWeapon(serverGamemodeState().communityWeaponSetId, weaponId);
    return slot > 0 ? slot : -1;
}

void serverCommunitySetMode(const std::string& modeId)
{
    ServerGamemodeState& state = serverGamemodeState();
    if (!state.enabled || modeId.empty()) return;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.modes().empty()) config.load();
    state.communityMode = modeId;
    state.communityScores.clear();
    state.communityTeams.clear();
    state.communityTeamScore[0] = state.communityTeamScore[1] = 0;
    state.communityRoundOver = false;
    state.communityRoundResetMs = 0;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY MODE] selected=%s\n", state.communityMode.c_str());
}

void serverCommunityStartMatch(bool skipIntermission, const std::string& requestedMode)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;

    if (!requestedMode.empty())
        d.communityMode = requestedMode;

    // ── Look up the community mode and resolve its gamemode_id ───────
    const CommunityServerConfig& communityConfig = CommunityServerConfig::instance();
    const CommunityMode* cm = communityConfig.modeById(d.communityMode);
    std::string resolvedGamemodeId;
    if (cm) {
        // Use gamemode_id to look up the actual gamemode config. This bridges
        // onlinemodes.json (community menu) to gamemodes/*.json (gameplay rules).
        resolvedGamemodeId = cm->gamemodeId;
    } else if (MimitaRuntime::GenericRuntime::instance().hasMode(
                   gameHash(d.communityMode.c_str()))) {
        // A runtime-registered hot gamemode selected directly by its id.
        resolvedGamemodeId = d.communityMode;
    } else {
        return;  // unknown mode — cannot start
    }

    // Defer a live mode switch until the current mode has shown its results.
    if (!d.mapOnly && !d.matchMode.empty() && d.matchMode != resolvedGamemodeId &&
        d.phase != DUEL_PHASE_WAITING && d.phase != DUEL_PHASE_RESULTS)
    {
        d.pendingModeSwitch = true;
        d.pendingModeSwitchCountdown = skipIntermission;
        d.pendingGamemodeId = resolvedGamemodeId;
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = 5.0f;
        d.matchOver = true;
        if (d.matchMode == "ffa") {
            int best = -1;
            for (const auto& kv : d.ffaKills)
                if (kv.second > best) { best = kv.second; d.winnerPlayerId = kv.first; }
        } else if (d.matchMode == "tdm") {
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
        }
        d.stateBroadcastPending = true;
        ++d.stateVersion;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE SWITCH] old=%s new=%s resultsSeconds=5 countdownAfter=%d\n",
            d.matchMode.c_str(), resolvedGamemodeId.c_str(), (int)skipIntermission);
        return;
    }

    // Set match mode from community mode id (used for routing and state machine)
    d.matchMode = resolvedGamemodeId;
    applyActiveHotMode(d, resolvedGamemodeId);
    // DEPRECATED: the old ServerMode enum and short-form matchMode strings
    // are kept for backward compatibility with duel/FFA/TDM code paths.
    // New modes should use matchMode directly (the community mode id).
    d.mode = ServerMode::Sandbox;

    // Load gamemode config using the resolved gamemode_id
    const Gamemode& gm = GamemodeRegistry::instance().get(resolvedGamemodeId);
    d.goalValue = gm.goalValue;
    // An explicit GUI/runtime selection wins over the gamemode default.
    if (!d.communityWeaponSetExplicit && gm.weaponSetId > 0)
        d.communityWeaponSetId = gm.weaponSetId;
    d.timeLimitSeconds = gm.timeLimitSeconds;
    d.respawnSeconds = gm.respawnSeconds;
    d.killHeals = gm.killHeals;
    d.winCondition = gm.winCondition;
    d.intermissionSeconds = (float)gm.intermissionSeconds;
    d.resultsSeconds = (float)gm.resultsSeconds;
    d.countdownSeconds = gm.countdownSeconds;
    d.goSeconds = gm.goSeconds;
    d.spawnOffsetRadius = gm.spawnOffsetRadius;
    d.hasBombFeature = gm.features.bombHolderText;
    d.bombTagActive = false;

    // ── Visual/settings overrides from gamemode ────────────────────
    d.cameraFov = gm.cameraFov;
    d.ragdollExplicit = gm.ragdollExplicit;
    d.ragdollEnabled = gm.ragdollEnabled;
    d.bloodExplicit = gm.bloodExplicit;
    d.bloodEnabled = gm.bloodEnabled;

    // ── Forced gameplay overrides from gamemode ────────────────────
    d.aimMode = gm.aimMode;
    d.movementPreset = gm.movementPreset;
    d.healthbarOverride = gm.healthbar.explicitValue;
    d.healthbarAimModeEnabled = gm.healthbar.aimModeEnabled;
    d.healthbarShowName = gm.healthbar.showNameInAimMode;
    d.healthbarShowHpText = gm.healthbar.showHpTextInAimMode;
    d.healthbarShowBar = gm.healthbar.showBarInAimMode;
    d.healthbarMaxDistance = gm.healthbar.maxDistance;
    // Server-side authority uses the same forced aim mode as clients.
    if (!d.aimMode.empty()) {
        GameplayAimMode parsed;
        if (gameplayAimModeFromString(d.aimMode, parsed))
            GameplayConfig::instance().setAimModeOverride(parsed);
        else
            Debug::warn(Debug::Category::Duel,
                "[GAMEMODE] mode %s has unknown aim_mode \"%s\"; keeping player setting\n",
                resolvedGamemodeId.c_str(), d.aimMode.c_str());
    } else {
        GameplayConfig::instance().clearAimModeOverride();
    }
    d.appliedRulesRevision = GamemodeRegistry::instance().revision();

    // ── Objective rounds + bomb ────────────────────────────────────
    d.objectiveRounds = (gm.winCondition == "objective_rounds");
    d.roundWins[0] = d.roundWins[1] = 0;
    d.roundNumber = 0;
    d.roundOver = false;
    d.roundEndReason.clear();
    d.objectiveBombPlantSeconds = gm.bombPlantSeconds;
    d.objectiveBombDefuseSeconds = gm.bombDefuseSeconds;
    d.objectiveBombTimerMax = gm.bombTimerSeconds;
    d.objectiveBombExplosionRadius = gm.bombExplosionRadius;
    d.objectiveBombExplosionDamage = gm.bombExplosionDamage;
    d.objectiveBombState = BOMB_OBJ_NONE;
    d.objectiveBombCarrierId = 0;
    d.objectiveBombPlantProgress = 0.0f;
    d.objectiveBombDefuseProgress = 0.0f;
    d.objectiveBombTimer = 0.0f;
    d.objectiveBombPos = glm::vec3(0.0f);

    // ── NPC waves ──────────────────────────────────────────────────
    d.npcWaves = (gm.winCondition == "npc_waves");
    d.waveStartCount = gm.waveStartCount;
    d.waveIncrement = gm.waveIncrement;
    d.waveNumber = 0;
    d.waveBest.clear();

    Debug::warn(Debug::Category::Duel,
        "[MATCH RULES] mode=%s respawn=%.2fs respawns=%d kill_heals=%d win=%s roles=%zu\n",
        resolvedGamemodeId.c_str(), d.respawnSeconds, (int)serverMatchRespawnsEnabled(),
        (int)d.killHeals, d.winCondition.empty() ? "default" : d.winCondition.c_str(),
        gm.roleCounts.size());

    // modestart enters the configured intermission. modestartnow enters the
    // countdown directly; serverGamemodeTick owns the authoritative 3-2-1.
    d.mapOnly = false;
    d.lastBroadcastTick = 0;
    d.stateBroadcastPending = true;
    if (gm.useModeMaps && !gm.maps.empty()) {
        // Opt-in mode-specific pool: a mode that asks for its own maps plays
        // only those, so Counter-Strike stays on its authored map. Other modes
        // keep the shared good-map pool behavior.
        d.mapPool = gm.maps;
        d.rotateMaps = (gm.maps.size() > 1);
    } else {
        d.rotateMaps = d.autoMapRotation;
    }
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.matchTeams.clear();
    d.matchActors.clear();
    d.participants.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;
    d.pendingKillEvents.clear();
    d.hasPendingKill = false;
    d.spawnsAssigned = false;
    d.startCountdownImmediately = skipIntermission;
    d.phase = DUEL_PHASE_INTERMISSION;
    d.phaseTimer = skipIntermission ? 0.0f : d.intermissionSeconds;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    ++d.stateVersion;
    ++d.duelId;

    // ── Mode-specific activation ─────────────────────────────────────
    // Each mode that needs extra initialization gets its entry point called here.
    // This replaces the old hardcoded if/else if chain.
    if (d.hasBombFeature) {
        serverBombTagStartMatch(skipIntermission);
    }

    Debug::warn(Debug::Category::Duel,
        "[MODESTART] community=%s gamemode=%s phase=%s goal=%d timeLimit=%d intermission=%.0f\n",
        d.communityMode.c_str(), resolvedGamemodeId.c_str(),
        skipIntermission ? "COUNTDOWN_PENDING" : "INTERMISSION",
        d.goalValue,
        d.timeLimitSeconds, d.intermissionSeconds);
}

namespace {

// Defined later in this translation unit's unnamed namespace; used by the
// shared state machine and the objective-bomb helpers.
void broadcastBombTagState(SOCKET sock,
                           ServerGamemodeState& d,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint64_t& totalPacketsOut);
glm::vec3 getEntityRootPos(const ServerPlayer& p);
glm::vec3 getEntityRootPos(const ServerNpc& n);

uint32_t countActivePlayers(const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    uint32_t count = 0;
    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active)
            ++count;
    }
    return count;
}

void broadcastDuelState(SOCKET sock,
                        const ServerGamemodeState& d,
                        const std::unordered_map<uint32_t, ServerPlayer>& players,
                        uint64_t& totalPacketsOut)
{
    DuelStatePacket pkt{};
    pkt.header.type = PACKET_DUEL_STATE;
    pkt.header.tick = 0;
    pkt.phase = d.phase;
    pkt.duelId = d.duelId;
    pkt.mapVersion = d.mapVersion;
    pkt.spawnAnchorVersion = d.spawnAnchorVersion;
    pkt.respawnSequence = d.respawnSequence;
    pkt.stateVersion = d.stateVersion;
    std::strncpy(pkt.mapId, d.mapId.c_str(), sizeof(pkt.mapId) - 1);
    pkt.spawnAnchorIndex = d.spawnAnchorIndex;
    pkt.anchorX = d.spawnA.x;
    pkt.anchorY = d.spawnA.y;
    pkt.anchorZ = d.spawnA.z;
    auto a = players.find(d.playerAId);
    auto b = players.find(d.playerBId);
    if (a != players.end()) {
        pkt.playerASpawnGeneration = a->second.spawnGeneration;
        pkt.playerASpawnX = a->second.duelSpawnPos.x;
        pkt.playerASpawnY = a->second.duelSpawnPos.y;
        pkt.playerASpawnZ = a->second.duelSpawnPos.z;
    }
    if (b != players.end()) {
        pkt.playerBSpawnGeneration = b->second.spawnGeneration;
        pkt.playerBSpawnX = b->second.duelSpawnPos.x;
        pkt.playerBSpawnY = b->second.duelSpawnPos.y;
        pkt.playerBSpawnZ = b->second.duelSpawnPos.z;
    }
    pkt.matchOver = d.matchOver ? 1 : 0;
    pkt.scoreA = d.scoreA;
    pkt.scoreB = d.scoreB;
    pkt.goalValue = d.goalValue;
    pkt.countdownLeft = d.countdown;
    pkt.phaseTimer = d.phaseTimer;
    pkt.rematchLeft = d.rematchLeft;
    pkt.playerAId = d.playerAId;
    pkt.playerBId = d.playerBId;
    pkt.winnerPlayerId = d.winnerPlayerId;
    std::strncpy(pkt.teamAName, d.teamAName.c_str(), sizeof(pkt.teamAName) - 1);
    std::strncpy(pkt.teamBName, d.teamBName.c_str(), sizeof(pkt.teamBName) - 1);

    // ── FFA/TDM extension fields ───────────────────────────────────
    std::strncpy(pkt.matchMode, d.matchMode.c_str(), sizeof(pkt.matchMode) - 1);
    pkt.matchStartTick = d.matchStartTick;
    pkt.serverTick = d.currentServerTick;
    pkt.victoryType = d.victoryType;
    // Objective-round modes present round wins through the shared team-score
    // fields so the generic HUD shows the current round score.
    pkt.redTeamKills = d.objectiveRounds ? d.roundWins[0] : d.redTeamKills;
    pkt.blueTeamKills = d.objectiveRounds ? d.roundWins[1] : d.blueTeamKills;
    pkt.timeLimitSeconds = d.timeLimitSeconds;
    pkt.intermissionSeconds = (int32_t)d.intermissionSeconds;
    pkt.resultsSeconds = (int32_t)d.resultsSeconds;
    pkt.goSeconds = d.goSeconds;

    // ── Gamemode visual overrides ──────────────────────────────────
    pkt.cameraFov = d.cameraFov;
    pkt.ragdollEnabled = d.ragdollExplicit ? (d.ragdollEnabled ? 2 : 1) : 0;
    pkt.bloodEnabled = d.bloodExplicit ? (d.bloodEnabled ? 2 : 1) : 0;

    // ── Forced gameplay overrides ──────────────────────────────────
    std::strncpy(pkt.aimMode, d.aimMode.c_str(), sizeof(pkt.aimMode) - 1);
    pkt.healthbarOverride = d.healthbarOverride ? 1 : 0;
    pkt.healthbarAimModeEnabled = d.healthbarAimModeEnabled ? 1 : 0;
    pkt.healthbarShowName = d.healthbarShowName ? 1 : 0;
    pkt.healthbarShowHpText = d.healthbarShowHpText ? 1 : 0;
    pkt.healthbarShowBar = d.healthbarShowBar ? 1 : 0;
    pkt.healthbarMaxDistance = d.healthbarMaxDistance;

    // ── Generic score snapshot (temporary bridge to the legacy packet/UI) ──
    // Authoritative score is owned by the active mode package (dynamic
    // components). The mode provides `match.score.snapshot`; this bridge maps
    // its generic entries onto the legacy fields. No provider => cold fields.
    GameMatchScoreSnapshotV1 snapshot{};
    {
        void* provider = MimitaRuntime::GenericRuntime::instance().capability(
            gameHash("match.score.snapshot"));
        if (provider) {
            GameplayContextV1* ctx = LiveBehavior::hostContext(d.currentServerTick);
            if (ctx)
                reinterpret_cast<GameMatchScoreSnapshotFn>(provider)(ctx, &snapshot);
        }
    }
    if (snapshot.count > 0) {
        std::vector<GameMatchScoreEntryV1> actors;
        for (std::uint32_t i = 0; i < snapshot.count && i < GAME_MAX_MATCH_SCORES; ++i) {
            const GameMatchScoreEntryV1& e = snapshot.entries[i];
            if (e.kind == 1) {
                if (e.ownerId == 0) pkt.redTeamKills = e.score;
                else pkt.blueTeamKills = e.score;
            } else {
                actors.push_back(e);
                if (e.ownerId == d.playerAId) pkt.scoreA = e.score;
                if (e.ownerId == d.playerBId) pkt.scoreB = e.score;
            }
        }
        std::sort(actors.begin(), actors.end(),
                  [](const GameMatchScoreEntryV1& a, const GameMatchScoreEntryV1& b) {
                      return a.score > b.score;
                  });
        for (int i = 0; i < 3 && i < (int)actors.size(); ++i) {
            const uint32_t owner = (uint32_t)actors[i].ownerId;
            pkt.ffaLeaderIds[i] = owner;
            pkt.ffaLeaderScores[i] = actors[i].score;
            auto nameIt = d.participantNames.find(owner);
            if (nameIt != d.participantNames.end())
                std::strncpy(pkt.ffaLeaderNames[i], nameIt->second.c_str(),
                             sizeof(pkt.ffaLeaderNames[i]) - 1);
            else
                std::snprintf(pkt.ffaLeaderNames[i], sizeof(pkt.ffaLeaderNames[i]),
                              "NPC-%u", owner);
        }
    }
    // FFA top-3 leaderboard (cold fallback when no mode score provider)
    else if (d.matchMode == "ffa") {
        std::vector<std::pair<uint32_t, int>> sorted;
        for (const auto& kv : d.ffaKills)
            sorted.push_back({kv.first, kv.second});
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        for (int i = 0; i < 3 && i < (int)sorted.size(); ++i) {
            pkt.ffaLeaderIds[i] = sorted[i].first;
            pkt.ffaLeaderScores[i] = sorted[i].second;
            auto nameIt = d.participantNames.find(sorted[i].first);
            if (nameIt != d.participantNames.end())
                std::strncpy(pkt.ffaLeaderNames[i], nameIt->second.c_str(), sizeof(pkt.ffaLeaderNames[i]) - 1);
            else
                std::snprintf(pkt.ffaLeaderNames[i], sizeof(pkt.ffaLeaderNames[i]),
                              "NPC-%u", sorted[i].first);
        }
    }

    // Participant IDs and teams
    pkt.participantCount = (uint8_t)std::min((size_t)32, d.participants.size());
    for (uint8_t i = 0; i < pkt.participantCount; ++i) {
        const uint32_t actorId = d.participants[i];
        pkt.participantIds[i] = actorId;
        auto teamIt = d.matchTeams.find(actorId);
        pkt.participantTeams[i] = teamIt != d.matchTeams.end() ? (uint8_t)teamIt->second : 0xFF;
        auto actorIt = d.matchActors.find(actorId);
        if (actorIt != d.matchActors.end()) {
            pkt.participantRoles[i] =
                (uint8_t)MatchRoleRegistry::instance().indexOf(actorIt->second.roleId);
            pkt.participantStates[i] = (uint8_t)actorIt->second.state;
        } else {
            pkt.participantRoles[i] = 0;
            pkt.participantStates[i] = (uint8_t)ActorState::Alive;
        }
        // Per-actor match stats from the authoritative mode counters.
        auto killIt = d.ffaKills.find(actorId);
        auto deathIt = d.ffaDeaths.find(actorId);
        pkt.participantKills[i] = killIt != d.ffaKills.end() ? killIt->second : 0;
        pkt.participantDeaths[i] = deathIt != d.ffaDeaths.end() ? deathIt->second : 0;
        pkt.participantScores[i] = pkt.participantKills[i];
        auto playerIt = players.find(actorId);
        if (playerIt != players.end())
            std::snprintf(pkt.participantNames[i],
                          sizeof(pkt.participantNames[i]), "%s",
                          playerIt->second.name.c_str());
    }

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        const bool sent = result == ReliableGameplayEventQueueResult::Queued;
        Debug::log(Debug::Category::Duel,
            "[ServerGamemode] sent state duelId=%u version=%u phase=%u mode=%s map=%s player=%u sent=%d score=%d-%d red=%d blue=%d\n",
            d.duelId, d.stateVersion, (unsigned)d.phase, d.matchMode.c_str(), d.mapId.c_str(),
            kv.second.id, (int)sent, d.scoreA, d.scoreB, d.redTeamKills, d.blueTeamKills);
    }
}

// Pick ONE random map spawn point as the match anchor. Both teams always
// spawn near this single point (with a fresh random XY offset each spawn), so
// respawns land right back in the fight — max action, no map editing needed.
void assignGamemodeSpawns(ServerGamemodeState& d, const HeadlessWorld& world)
{
    d.spawnsAssigned = true;
    if (!world.spawnPoints.empty())
    {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, world.spawnPoints.size() - 1);
        const size_t anchorIndex = dist(rng);
        d.spawnAnchorIndex = (uint32_t)anchorIndex;
        ++d.spawnAnchorVersion;
        const glm::vec3 anchor = world.spawnPoints[anchorIndex].position;
        d.spawnA = anchor;
        d.spawnB = anchor;
        Debug::log(Debug::Category::Duel,
            "[DuelAnchor] map=%s anchorIndex=%zu anchor=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_str(), anchorIndex, anchor.x, anchor.y, anchor.z);
    }
    else
    {
        d.spawnA = glm::vec3(1.0f, 5.0f, 30.0f);
        d.spawnB = d.spawnA;
        Debug::warn(Debug::Category::Duel,
            "[DuelFallback] map=%s reason=no_spawn_points final=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_str(), d.spawnA.x, d.spawnA.y, d.spawnA.z);
    }
    // Per-map team spawns (config/maps/<map>.json). When present, team modes
    // use these instead of the single match anchor; otherwise the anchor is the
    // fallback so maps without a config keep working.
    const MapConfig& mc = MapConfigRegistry::instance().get(d.mapId);
    d.teamSpawnPoints[0].clear();
    d.teamSpawnPoints[1].clear();
    for (const MapTeamSpawn& sp : mc.teamSpawns[0]) d.teamSpawnPoints[0].push_back(sp.position);
    for (const MapTeamSpawn& sp : mc.teamSpawns[1]) d.teamSpawnPoints[1].push_back(sp.position);

    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] anchor=(%.1f %.1f %.1f) spawns=%zu teamSpawnsT=%zu teamSpawnsCT=%zu\n",
        d.spawnA.x, d.spawnA.y, d.spawnA.z, world.spawnPoints.size(),
        d.teamSpawnPoints[0].size(), d.teamSpawnPoints[1].size());
}

// Team-aware spawn: team 0 = T, 1 = CT. Falls back to the single anchor when the
// map has no per-team spawns, so non-team modes and unconfigured maps are
// unchanged.
glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d, int team)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-d.spawnOffsetRadius, d.spawnOffsetRadius);
    if (team >= 0 && team < 2 && !d.teamSpawnPoints[team].empty()) {
        std::uniform_int_distribution<size_t> pick(0, d.teamSpawnPoints[team].size() - 1);
        return d.teamSpawnPoints[team][pick(rng)] + glm::vec3(dist(rng), dist(rng), 0.0f);
    }
    return d.spawnA + glm::vec3(dist(rng), dist(rng), 0.0f);
}

// The anchor plus a random XY offset (so nobody can predict the exact spot).
glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d)
{
    return gamemodeSpawnPoint(d, -1);
}

void assignGamemodeParticipants(ServerGamemodeState& d,
                    const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    d.playerAId = 0;
    d.playerBId = 0;
    for (const auto& kv : players)
    {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        if (d.playerAId == 0)
            d.playerAId = kv.first;
        else
            d.playerBId = kv.first;
    }
}

// Place both duelists near the match anchor with full HP and full ammo.
void teleportGamemodeParticipantsToSpawns(ServerGamemodeState& d,
                              std::unordered_map<uint32_t, ServerPlayer>& players)
{
    auto place = [&](uint32_t playerId)
    {
        auto it = players.find(playerId);
        if (it == players.end()) return;
        ServerPlayer& p = it->second;
        const glm::vec3 spawn = gamemodeSpawnPoint(d);
        p.duelSpawnPos = spawn;
        p.hasDuelSpawnPos = true;
        const glm::vec3 offset = spawn - d.spawnA;
        Debug::log(Debug::Category::Duel,
            "[DuelSpawn] player=%u map=%s anchor=(%.3f,%.3f,%.3f) offset=(%.3f,%.3f,%.3f) final=(%.3f,%.3f,%.3f)\n",
            playerId, d.mapId.c_str(), d.spawnA.x, d.spawnA.y, d.spawnA.z,
            offset.x, offset.y, offset.z, spawn.x, spawn.y, spawn.z);
        p.respawnSeconds = 0.0f;
        if (!p.dead)
        {
            beginAuthoritativeTransform(p, spawn,
                SpawnVelocityConfig::instance().enabled()
                    ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
                    : glm::vec3(0.0f), p.yaw, "duel-spawn");
            p.justRespawned = true;
        }
    };
    place(d.playerAId);
    place(d.playerBId);
}

void beginGamemodeCountdown(ServerGamemodeState& d,
                        std::unordered_map<uint32_t, ServerPlayer>& players)
{
    ++d.duelId;
    ++d.respawnSequence;
    ++d.stateVersion;
    d.matchOver = false;
    d.scoreA = 0;
    d.scoreB = 0;
    d.winnerPlayerId = 0;
    serverMatchResetEntity();
    d.phase = DUEL_PHASE_COUNTDOWN;
    d.countdown = d.countdownSeconds;
    Debug::log(Debug::Category::Duel,
        "[ServerGamemode] selected authoritative map=%s duelId=%u stateVersion=%u\n",
        d.mapId.c_str(), d.duelId, d.stateVersion);
    teleportGamemodeParticipantsToSpawns(d, players);
    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] countdown started players=%u/%u\n", d.playerAId, d.playerBId);
}

// loadHeadlessWorld appends, so clear everything it populates before a reload.
void clearHeadlessWorld(HeadlessWorld& world)
{
    world.triangles.clear();
    world.boundsMin = glm::vec3(0.0f);
    world.boundsMax = glm::vec3(0.0f);
    world.spawnPoints.clear();
    world.collisionChunks.clear();
    world.collisionLargeTriangles.clear();
    world.collisionSubGrids.clear();
}

void broadcastMapChange(SOCKET sock,
                        const ServerGamemodeState& d,
                        const std::string& mapId,
                        const std::unordered_map<uint32_t, ServerPlayer>& players,
                        uint64_t& totalPacketsOut)
{
    MapChangePacket pkt{};
    pkt.header.type = PACKET_MAP_CHANGE;
    pkt.header.tick = 0;
    std::strncpy(pkt.mapId, mapId.c_str(), sizeof(pkt.mapId) - 1);
    pkt.duelId = d.duelId;
    pkt.mapVersion = d.mapVersion;
    for (const auto& kv : players)
    {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        const bool sent = result == ReliableGameplayEventQueueResult::Queued;
        Debug::log(Debug::Category::Duel,
            "[DuelMap] send map=%s player=%u reliable=1 sent=%d version=%u\n",
            mapId.c_str(), kv.second.id, (int)sent, d.mapVersion);
        Debug::log(Debug::Category::Duel,
            "[DuelPacketSend] type=MapChangePacket reliable=1 player=%u sent=%d map=%s\n",
            kv.second.id, (int)sent, mapId.c_str());
        if (sent)
            ++totalPacketsOut;
    }
}

void broadcastCommunityNotification(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::string& message,
    uint16_t durationTicks,
    uint64_t& totalPacketsOut)
{
    ServerNotificationPacket packet{};
    packet.header.type = PACKET_SERVER_NOTIFICATION;
    packet.eventId = nextReliableGameplayEventId();
    packet.eventSessionId = serverReliableEventSessionId();
    packet.durationTicks = durationTicks;
    std::strncpy(packet.title, "MiMITA Server", sizeof(packet.title) - 1);
    std::strncpy(packet.message, message.c_str(), sizeof(packet.message) - 1);
    for (auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, kv.second, &packet, sizeof(packet), packet.eventId,
            reliableGameplayEventSessionForPlayer(kv.second), totalPacketsOut);
        Debug::log(Debug::Category::Networking,
            "[SERVER NOTIFICATION SEND] player=%u queued=%d message=%s\n",
            kv.second.id, result == ReliableGameplayEventQueueResult::Queued,
            message.c_str());
    }
}

// Load a map into a fresh temp world; true only if it loads AND has real
// spawn points, so players always anchor at spawn points, never under the map.
bool tryLoadDuelMap(const std::string& mapId, HeadlessWorld& out)
{
    const std::string path = "assets/maps/" + mapId + ".glb";
    HeadlessWorld candidate;
    if (!loadHeadlessWorld(path.c_str(), candidate))
        return false;
    if (candidate.spawnPoints.empty())
        return false;
    out = std::move(candidate);
    return true;
}

// Move a loaded temp world into the live world + NPC collision world.
void commitGamemodeMap(ServerGamemodeState& d, HeadlessWorld& world, World& npcWorld,
                   HeadlessWorld& tmp, const std::string& mapId)
{
    clearHeadlessWorld(world);
    world = std::move(tmp);
    buildNpcWorldCollision(npcWorld, world);
    setServerMapId(mapId);
    d.mapId = mapId;
    ++d.mapVersion;
    ++d.stateVersion;
}

// Reload the world for a chosen map (changemap / rotation commit). Only ever
// touches the live world after the new map is confirmed loaded, so a failed
// swap never empties the world (players never fall under it).
bool reloadGamemodeMap(SOCKET sock,
                   ServerGamemodeState& d,
                   std::unordered_map<uint32_t, ServerPlayer>& players,
                   HeadlessWorld& world,
                   World& npcWorld,
                   std::unordered_map<uint32_t, ServerNpc>& npcs,
                   NpcSystem& npcSystem,
                   const std::string& mapId,
                   uint64_t& totalPacketsOut)
{
    HeadlessWorld tmp;
    if (!tryLoadDuelMap(mapId, tmp))
    {
        Debug::error(Debug::Category::Duel,
            "[DUEL SERVER] map swap failed or has no spawn points: %s\n", mapId.c_str());
        return false;
    }
    commitGamemodeMap(d, world, npcWorld, tmp, mapId);
    assignGamemodeSpawns(d, world);
    broadcastMapChange(sock, d, mapId, players, totalPacketsOut);
    teleportGamemodeParticipantsToSpawns(d, players);
    for (Npc& npc : npcSystem.all())
    {
        const glm::vec3 spawn = gamemodeSpawnPoint(d);
        npc.body.pos = spawn;
        npc.body.respawnPosition = spawn;
        npc.body.vel = glm::vec3(0.0f);
        npc.body.externalImpulse = glm::vec3(0.0f);
        npc.body.currentHp = npc.body.maxHp;
        npc.body.dead = false;
        npc.body.respawnTimer = 0.0f;
        finalizeServerNpcSpawn(npc, ActorSpawnReason::MapChange);
        npc.body.syncLegacyStateToLayers();
        npc.body.updateModelWorldTransforms();
        auto mirror = npcs.find(npc.id);
        if (mirror != npcs.end())
        {
            mirror->second.pos = spawn;
            mirror->second.vel = glm::vec3(0.0f);
            mirror->second.health = npc.body.currentHp;
            ++mirror->second.transformEpoch;
        }
    }
    // Map changes are actor lifecycle boundaries, not duel-only teleports.
    // Every active player receives a fresh authoritative spawn on the new map.
    for (auto& kv : players) {
        ServerPlayer& p = kv.second;
        if (p.spawnState != ServerPlayer::Active) continue;
        p.duelSpawnPos = gamemodeSpawnPoint(d);
        p.hasDuelSpawnPos = true;
        beginAuthoritativeTransform(p, p.duelSpawnPos,
                                    SpawnVelocityConfig::instance().enabled()
                                        ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
                                        : glm::vec3(0.0f), p.yaw,
                                    "map-change-respawn");
        completeAuthoritativeSpawn(sock, p, false);
    }
    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] map changed live to %s (spawns=%zu)\n",
        mapId.c_str(), world.spawnPoints.size());
    return true;
}

// Round-robin rotation: pick a map not used this cycle (never the one just
// played), skip maps that fail to load or have no spawn points, and cycle the
// whole pool before repeating. Returns true if the map changed.
bool rotateToNextGamemodeMap(SOCKET sock,
                         ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         HeadlessWorld& world,
                         World& npcWorld,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         NpcSystem& npcSystem,
                         uint64_t& totalPacketsOut)
{
    if (d.mapPool.size() <= 1)
        return false;

    auto unusedCandidates = [&]() {
        std::vector<std::string> v;
        for (const auto& m : d.mapPool)
            if (!d.usedMaps.count(m) && m != d.mapId)
                v.push_back(m);
        return v;
    };

    std::vector<std::string> candidates = unusedCandidates();
    if (candidates.empty())
    {
        // Whole pool used this cycle — start fresh, still avoiding the current map.
        d.usedMaps.clear();
        d.usedMaps.insert(d.mapId);
        candidates = unusedCandidates();
    }

    std::mt19937 rng(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);

    for (const std::string& cand : candidates)
    {
        HeadlessWorld tmp;
        if (tryLoadDuelMap(cand, tmp))
        {
            d.usedMaps.insert(cand);
            commitGamemodeMap(d, world, npcWorld, tmp, cand);
            assignGamemodeSpawns(d, world);
            broadcastMapChange(sock, d, cand, players, totalPacketsOut);
            teleportGamemodeParticipantsToSpawns(d, players);
            for (Npc& npc : npcSystem.all())
            {
                const glm::vec3 spawn = gamemodeSpawnPoint(d);
                npc.body.pos = spawn;
                npc.body.respawnPosition = spawn;
                npc.body.vel = glm::vec3(0.0f);
                npc.body.externalImpulse = glm::vec3(0.0f);
                npc.body.currentHp = npc.body.maxHp;
                npc.body.dead = false;
                npc.body.respawnTimer = 0.0f;
                finalizeServerNpcSpawn(npc, ActorSpawnReason::MapChange);
                npc.body.syncLegacyStateToLayers();
                npc.body.updateModelWorldTransforms();
                auto mirror = npcs.find(npc.id);
                if (mirror != npcs.end())
                {
                    mirror->second.pos = spawn;
                    mirror->second.vel = glm::vec3(0.0f);
                    mirror->second.health = npc.body.currentHp;
                    ++mirror->second.transformEpoch;
                }
            }
            Debug::warn(Debug::Category::Duel,
                "[DUEL SERVER] rotated to map %s (spawns=%zu)\n",
                cand.c_str(), world.spawnPoints.size());
            return true;
        }
        // Failed to load or has no spawn points — skip it this cycle.
        d.usedMaps.insert(cand);
    }
    return false; // nothing valid — keep the current map
}

// ── FFA/TDM match helpers ───────────────────────────────────────────────

// Assign one authoritative match identity to every participant (human or NPC)
// from a single path. Roles come from config/roles.json; a gamemode may declare
// per-role counts. When no roles are configured, teams fall back to the legacy
// round-robin assignment and no role is set.
void assignMatchParticipants(ServerGamemodeState& d,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             std::unordered_map<uint32_t, ServerNpc>* npcs = nullptr)
{
    d.participants.clear();
    d.participantNames.clear();
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.matchTeams.clear();
    d.matchActors.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;

    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active) {
            d.participants.push_back(kv.first);
            d.participantNames[kv.first] = kv.second.name;
            d.ffaKills[kv.first] = 0;
            d.ffaDeaths[kv.first] = 0;
        }
    }

    if (npcs) {
        for (const auto& kv : *npcs) {
            // Round-based modes keep dead NPCs on the roster so they are revived
            // at the next round instead of being dropped from the match. Other
            // modes keep skipping dead NPCs.
            if (kv.second.health <= 0 && !d.objectiveRounds) continue;
            d.participants.push_back(kv.first);
            d.participantNames[kv.first] = kv.second.name.empty()
                ? "NPC-" + std::to_string(kv.first) : kv.second.name;
            d.ffaKills[kv.first] = 0;
            d.ffaDeaths[kv.first] = 0;
        }
    }

    // Sort by ID for deterministic assignment
    std::sort(d.participants.begin(), d.participants.end());

    // Seed one descriptor per participant with the correct controller type.
    // Humans and NPCs get the same structure and go through the same path below.
    for (uint32_t id : d.participants) {
        ActorMatchDescriptor desc;
        desc.controller = (players.find(id) != players.end())
            ? ActorController::Human : ActorController::Npc;
        desc.state = ActorState::Alive;
        desc.teamId = -1;
        d.matchActors[id] = std::move(desc);
    }

    // Role assignment from the gamemode's declared role counts. Roles are
    // apportioned proportionally across the participant list (largest-remainder
    // style) so a mode with e.g. 8 hunters + 4 juggernauts still shows both
    // roles in a small match and stays deterministic for a given config + set.
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    struct RoleSlot { std::string id; int cap; int assigned; };
    std::vector<RoleSlot> roles;
    int totalSlots = 0;
    for (const auto& rc : gm.roleCounts) {
        if (rc.second <= 0) continue;
        if (!MatchRoleRegistry::instance().get(rc.first)) {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] gamemode %s references unknown role \"%s\"\n",
                d.matchMode.c_str(), rc.first.c_str());
            continue;
        }
        roles.push_back({rc.first, rc.second, 0});
        totalSlots += rc.second;
    }

    for (size_t i = 0; i < d.participants.size() && totalSlots > 0; ++i) {
        int best = -1;
        double bestScore = 0.0;
        for (int r = 0; r < (int)roles.size(); ++r) {
            if (roles[r].assigned >= roles[r].cap) continue;
            // Cumulative target for this position minus what the role already
            // holds. The most under-served role is dealt next.
            const double want =
                (double)roles[r].cap * (double)(i + 1) / (double)totalSlots;
            const double score = want - (double)roles[r].assigned;
            if (best < 0 || score > bestScore) {
                best = r;
                bestScore = score;
            }
        }
        if (best < 0) break;
        roles[best].assigned++;

        ActorMatchDescriptor& desc = d.matchActors[d.participants[i]];
        desc.roleId = roles[best].id;
        if (const MatchRoleDefinition* def =
                MatchRoleRegistry::instance().get(desc.roleId)) {
            // Role preset wins; otherwise the mode-level preset applies to all.
            if (!def->movementPreset.empty())
                desc.movementProfileId = def->movementPreset;
            else if (!d.movementPreset.empty())
                desc.movementProfileId = d.movementPreset;
            desc.weaponProfileId = def->weaponSet;
            if (desc.controller == ActorController::Npc)
                desc.behaviorProfileId = def->behaviorProfile;
            if (def->team >= 0)
                desc.teamId = def->team;
        }
    }

    // Team assignment. A role-declared team wins; otherwise only explicit team
    // modes (TDM, or a team elimination mode) get the legacy round-robin. FFA
    // stays teamless even though its parsed team_names defaults to RED/BLUE.
    for (size_t i = 0; i < d.participants.size(); ++i) {
        const uint32_t id = d.participants[i];
        ActorMatchDescriptor& desc = d.matchActors[id];
        int team = desc.teamId;
        if (team < 0 && d.npcWaves)
            // NPC waves are players-vs-NPCs: everyone NPC shares one team so
            // they target the players and never each other.
            team = (desc.controller == ActorController::Npc) ? 1 : 0;
        else if (team < 0 && (d.matchMode == "tdm" ||
                              d.winCondition == "last_team_standing"))
            team = (int)(i % 2);
        desc.teamId = team;
        if (team >= 0)
            d.matchTeams[id] = team;

        auto playerIt = players.find(id);
        if (playerIt != players.end())
            playerIt->second.matchTeam = team;
        if (npcs) {
            auto npcIt = npcs->find(id);
            if (npcIt != npcs->end())
                npcIt->second.matchTeam = team;
        }
    }

    // Generic actor state (players and NPCs alike): team/role/profile become
    // entity dynamic components. The typed match/snapshot fields above are
    // projections (bridges), not the long-term owners.
    for (uint32_t id : d.participants) {
        const ActorMatchDescriptor& desc = d.matchActors[id];
        const EntityDomain domain = (desc.controller == ActorController::Npc)
            ? EntityDomain::Npc : EntityDomain::Player;
        const EntityId entity = Ecs::ensure(EntityRealm::Server, domain, id);
        actorStateWriteTeam(Ecs::raw(entity), desc.teamId);
        actorStateWriteRole(Ecs::raw(entity), desc.roleId.c_str());
        actorStateWriteProfile(Ecs::raw(entity), desc.movementProfileId.c_str(),
                               desc.behaviorProfileId.c_str());
    }

    if (d.matchMode == "tdm") {
        Debug::log(Debug::Category::Duel,
            "[FFA/TDM] Assigned %zu players to teams (red=%d blue=%d)\n",
            d.participants.size(), d.redTeamKills, d.blueTeamKills);
    }
    Debug::log(Debug::Category::Duel,
        "[MATCH ACTORS] assigned=%zu mode=%s roleCounts=%zu\n",
        d.participants.size(), d.matchMode.c_str(), gm.roleCounts.size());

    // One-shot per-actor identity dump (once per assignment, Duel category).
    // Mirrors the "actorlist" console command for headless/server testing.
    for (uint32_t id : d.participants) {
        const ActorMatchDescriptor& a = d.matchActors[id];
        Debug::log(Debug::Category::Duel,
            "[MATCH ACTOR] id=%u controller=%s role=%s team=%d state=%s "
            "movement=%s weapon=%s behavior=%s\n",
            id,
            a.controller == ActorController::Npc ? "npc" : "human",
            a.roleId.empty() ? "none" : a.roleId.c_str(),
            a.teamId,
            a.state == ActorState::Alive ? "alive" :
            a.state == ActorState::Dead ? "dead" :
            a.state == ActorState::Respawning ? "respawning" : "spectating",
            a.movementProfileId.empty() ? "none" : a.movementProfileId.c_str(),
            a.weaponProfileId.empty() ? "none" : a.weaponProfileId.c_str(),
            a.behaviorProfileId.empty() ? "none" : a.behaviorProfileId.c_str());
    }
}

// Advance each participant's authoritative match lifecycle state. This is the
// single owner of the Alive/Dead/Respawning/Spectating transitions and maps the
// legacy `dead`/health fields onto ActorState:
//   !dead                     -> Alive
//   dead + respawns enabled   -> Dead (one tick) -> Respawning -> Alive
//   dead + no respawn enabled -> Dead (one tick) -> Spectating (terminal)
void updateActorStates(ServerGamemodeState& d,
                       const std::unordered_map<uint32_t, ServerPlayer>& players,
                       const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const bool respawns = serverMatchRespawnsEnabled();
    for (auto& kv : d.matchActors) {
        ActorMatchDescriptor& desc = kv.second;
        const ActorState before = desc.state;

        bool dead = false;
        bool found = false;
        auto pIt = players.find(kv.first);
        if (pIt != players.end()) {
            dead = pIt->second.dead;
            found = true;
        } else {
            auto nIt = npcs.find(kv.first);
            if (nIt != npcs.end()) {
                dead = nIt->second.health <= 0;
                found = true;
            }
        }
        if (!found) continue;

        desc.state = nextActorState(before, dead, respawns);

        if (desc.state != before) {
            auto nameOf = [](ActorState s) {
                switch (s) {
                    case ActorState::Alive:      return "alive";
                    case ActorState::Dead:       return "dead";
                    case ActorState::Respawning: return "respawning";
                    case ActorState::Spectating: return "spectating";
                }
                return "unknown";
            };
            Debug::log(Debug::Category::Duel,
                "[ACTOR STATE] id=%u role=%s team=%d %s -> %s\n",
                kv.first, desc.roleId.empty() ? "none" : desc.roleId.c_str(),
                desc.teamId, nameOf(before), nameOf(desc.state));
        }
    }
}

void resetGamemodeActorsAtMapSpawn(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    NpcSystem& npcSystem)
{
    for (uint32_t pid : d.participants) {
        int team = -1;
        auto teamIt = d.matchTeams.find(pid);
        if (teamIt != d.matchTeams.end()) team = teamIt->second;
        const glm::vec3 spawn = gamemodeSpawnPoint(d, team);
        auto playerIt = players.find(pid);
        if (playerIt != players.end()) {
            ServerPlayer& p = playerIt->second;
            // Rebuild the authoritative inventory from the currently
            // selected community weapon set at every managed-mode boundary.
            // This prevents a broad initial inventory from surviving into a
            // later restricted FFA/TDM round.
            resetPlayerForSpawn(p, true);
            p.duelSpawnPos = spawn;
            p.hasDuelSpawnPos = true;
            p.respawnSeconds = 0.0f;
            beginAuthoritativeTransform(p, spawn,
                SpawnVelocityConfig::instance().enabled()
                    ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
                    : glm::vec3(0.0f), p.yaw, "gamemode-spawn");
            p.justRespawned = true;
            continue;
        }

        auto mirrorIt = npcs.find(pid);
        if (mirrorIt == npcs.end()) continue;
        const ActorSpawnProfile profile = serverResolveActorSpawnProfile(pid);
        for (Npc& npc : npcSystem.all()) {
            if (npc.id != pid) continue;
            npc.body.pos = spawn;
            npc.body.respawnPosition = spawn;
            npc.body.vel = glm::vec3(0.0f);
            npc.body.externalImpulse = glm::vec3(0.0f);
            // Role health override applies unless a host healthall override is set.
            const int npcOverrideHp = serverGameOverrides().maxHpOverride;
            const int npcMaxHp = npcOverrideHp > 0 ? npcOverrideHp
                : (profile.health > 0 ? profile.health : npc.body.maxHp);
            npc.body.maxHp = npcMaxHp;
            npc.body.currentHp = npcMaxHp;
            npc.body.dead = false;
            npc.body.respawnTimer = 0.0f;
            npc.movementProfileId = profile.movementPreset;
            npc.navigator.reset();
            npc.traversal.reset();
            npc.prevHadTarget = false;
            npc.reactionTimer = 0.0f;
            npc.serverTargetId = 0;
            // Resolve the role behavior profile once for this life.
            npc.behaviorProfileId = profile.behaviorProfileId;
            npc.behavior = resolveNpcBehavior(profile.behaviorProfileId);
            if (npc.behavior.active && npc.behavior.aggression >= 0.0f)
                npc.tuning.aggression = npc.behavior.aggression;
            npcMindReset(npc);
            Debug::log(Debug::Category::NpcCombat,
                "[NPC BEHAVIOR] actor=%u role=%s profile=%s aim=%.1f react=%.2f cadence=%.2f aggr=%.2f range=%.1f\n",
                npc.id, profile.roleId.c_str(),
                profile.behaviorProfileId.empty() ? "default" : profile.behaviorProfileId.c_str(),
                npc.behavior.aimErrorDeg, npc.behavior.reactionDelay,
                npc.behavior.fireCadenceMultiplier, npc.behavior.aggression,
                npc.behavior.preferredRange);
            if (!profile.weapons.empty()) {
                // Role loadout is authoritative; the global set is not consulted.
                npcApplyLoadout(npc, profile.weapons, profile.startingWeapon);
            } else {
                // Legacy: filter the global loadout by the gamemode weapon set.
                for (auto it = npc.body.weaponRuntimes.begin();
                     it != npc.body.weaponRuntimes.end(); ) {
                    if (!serverCommunityWeaponAllowed(it->first))
                        it = npc.body.weaponRuntimes.erase(it);
                    else
                        ++it;
                }
                if (!serverCommunityWeaponAllowed(npc.body.equippedWeaponId)) {
                    npc.body.equippedWeaponId.clear();
                    npc.body.equippedSlot = -1;
                    npc.body.hasValidWeapon = false;
                    if (!npc.body.weaponRuntimes.empty()) {
                        npc.body.equippedWeaponId = npc.body.weaponRuntimes.begin()->first;
                        if (const WeaponDefinition* def =
                                WeaponRegistry::instance().get(npc.body.equippedWeaponId))
                            npc.body.equippedSlot = def->slot;
                        npc.body.hasValidWeapon = true;
                    }
                }
            }
            finalizeServerNpcSpawn(npc, ActorSpawnReason::GamemodeStart);
            npc.body.syncLegacyStateToLayers();
            npc.body.updateModelWorldTransforms();
            Debug::log(Debug::Category::Duel,
                "[ROLE SPAWN] actor=%u controller=npc role=%s hp=%d weaponSet=%d weapons=%zu equipped=%s\n",
                npc.id, profile.hasRole ? profile.roleId.c_str() : "none",
                npc.body.currentHp, profile.weaponSetId, profile.weapons.size(),
                npc.body.equippedWeaponId.c_str());
            mirrorIt->second.pos = spawn;
            mirrorIt->second.vel = glm::vec3(0.0f);
            mirrorIt->second.health = npc.body.currentHp;
            ++mirrorIt->second.transformEpoch;
            break;
        }
        Debug::log(Debug::Category::Duel,
            "[GamemodeSpawn] actor=%u spawn=(%.3f,%.3f,%.3f)\n",
            pid, spawn.x, spawn.y, spawn.z);
    }
}

void resetMatchScores(ServerGamemodeState& d)
{
    d.scoreA = 0;
    d.scoreB = 0;
    d.redTeamKills = 0;
    d.blueTeamKills = 0;
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    for (uint32_t pid : d.participants) {
        d.ffaKills[pid] = 0;
        d.ffaDeaths[pid] = 0;
    }
}

void beginMatchCountdown(ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         NpcSystem& npcSystem,
                         uint32_t currentTick)
{
    ++d.duelId;
    ++d.respawnSequence;
    ++d.stateVersion;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    d.victoryType = 0;
    serverMatchResetEntity();
    d.countdownStartTick = currentTick;
    d.matchStartTick = currentTick + (uint32_t)(d.countdownSeconds * 60.0f);
    d.countdown = d.countdownSeconds;
    d.matchTimeLimitTick = 0;
    d.lastBroadcastTick = currentTick;
    resetMatchScores(d);
    d.phase = DUEL_PHASE_COUNTDOWN;
    resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
    Debug::log(Debug::Category::Duel,
        "[ServerMatch] countdown started mode=%s duelId=%u matchStartTick=%u timeLimitTick=%u participants=%zu\n",
        d.matchMode.c_str(), d.duelId, d.matchStartTick, d.matchTimeLimitTick, d.participants.size());
}

static void emitGamemodeMatchPersistence(ServerGamemodeState& d, uint32_t tick,
                                      const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    PersistenceMatchEvent event;
    event.eventId = "match_" + std::to_string(tick) + "_" + std::to_string(d.duelId);
    event.matchId = "match_" + std::to_string(d.duelId);
    event.mode = d.matchMode;
    event.victoryType = d.victoryType == 0 ? "score_limit" : "time_limit";
    event.redScore = d.objectiveRounds ? d.roundWins[0] : d.redTeamKills;
    event.blueScore = d.objectiveRounds ? d.roundWins[1] : d.blueTeamKills;
    event.winnerTeam = d.winnerTeam == 0 ? "red" : "blue";
    event.winnerPlayerId = (int64_t)d.winnerPlayerId;

    for (auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        PersistenceMatchParticipant p;
        p.userId = kv.second.accountId > 0 ? (int64_t)kv.second.accountId : 0;
        p.username = kv.second.name;
        auto teamIt = d.matchTeams.find(kv.first);
        p.team = (teamIt != d.matchTeams.end() && teamIt->second == 0) ? "red" : "blue";
        p.kills = kv.second.kills;
        p.deaths = kv.second.deaths;

        if (d.matchMode == "ffa") {
            auto killIt = d.ffaKills.find(kv.first);
            p.kills = killIt != d.ffaKills.end() ? killIt->second : 0;
            auto deathIt = d.ffaDeaths.find(kv.first);
            p.deaths = deathIt != d.ffaDeaths.end() ? deathIt->second : 0;
            p.won = (kv.first == d.winnerPlayerId);
        } else if (d.matchMode == "tdm") {
            auto killIt = d.ffaKills.find(kv.first);
            p.kills = killIt != d.ffaKills.end() ? killIt->second : 0;
            auto deathIt = d.ffaDeaths.find(kv.first);
            p.deaths = deathIt != d.ffaDeaths.end() ? deathIt->second : 0;
            p.won = (p.team == event.winnerTeam);
        } else {
            p.won = (kv.first == d.winnerPlayerId);
        }
        event.participants.push_back(std::move(p));
    }

    PersistenceQueue::instance().enqueueMatchResult(event);
    Debug::warn(Debug::Category::Duel,
        "[PERSISTENCE] Match result emitted: mode=%s winner=%s participants=%zu\n",
        event.mode.c_str(),
        event.winnerTeam.empty() ? std::to_string(d.winnerPlayerId).c_str() : event.winnerTeam.c_str(),
        event.participants.size());
}

void checkMatchWinConditions(ServerGamemodeState& d, uint32_t tick,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             uint64_t& totalPacketsOut)
{
    // ── Generic last-team-standing / last-man-standing elimination ──
    // A team is in play while it has at least one participant whose actor state
    // is Alive or Respawning. Works identically for humans and NPCs.
    if (d.winCondition == "last_team_standing") {
        bool anyTeam = false;
        std::unordered_map<int, int> aliveByTeam;
        std::unordered_set<int> teams;
        int aliveActors = 0;
        uint32_t lastAliveActor = 0;

        for (uint32_t id : d.participants) {
            auto tIt = d.matchTeams.find(id);
            const int team = (tIt != d.matchTeams.end()) ? tIt->second : -1;
            auto aIt = d.matchActors.find(id);
            const bool inPlay = aIt != d.matchActors.end() &&
                (aIt->second.state == ActorState::Alive ||
                 aIt->second.state == ActorState::Respawning);
            if (inPlay) { ++aliveActors; lastAliveActor = id; }
            if (team >= 0) {
                anyTeam = true;
                teams.insert(team);
                if (inPlay) aliveByTeam[team]++;
            }
        }

        int winnerTeam = -1;
        uint32_t winnerActor = 0;
        bool decided = false;

        if (anyTeam) {
            int aliveTeams = 0;
            for (int t : teams)
                if (aliveByTeam[t] > 0) { ++aliveTeams; winnerTeam = t; }
            if (teams.size() >= 2 && aliveTeams <= 1) decided = true;
            if (decided) {
                for (uint32_t id : d.participants) {
                    auto tIt = d.matchTeams.find(id);
                    if (tIt != d.matchTeams.end() && tIt->second == winnerTeam) {
                        winnerActor = id;
                        break;
                    }
                }
            }
        } else if (d.participants.size() >= 2 && aliveActors <= 1) {
            // FFA elimination: each actor is its own "team".
            decided = true;
            winnerActor = lastAliveActor;
        }

        if (decided) {
            d.matchOver = true;
            d.phase = DUEL_PHASE_RESULTS;
            d.victoryType = 0;
            d.winnerTeam = winnerTeam;
            d.winnerPlayerId = winnerActor;
            d.phaseTimer = d.resultsSeconds;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            emitGamemodeMatchPersistence(d, tick, players);
            Debug::warn(Debug::Category::Duel,
                "[ELIMINATION] last_team_standing winnerTeam=%d winnerActor=%u "
                "aliveActors=%d mode=%s\n",
                winnerTeam, winnerActor, aliveActors, d.matchMode.c_str());
            return;
        }
    }

    // ── Generic match evaluation (a hot mode owns the outcome) ─────────
    // The active mode's domain handler may decide the match. When it handles,
    // the cold mode-specific score-limit branches below are skipped.
    {
        GameMatchEvaluateV1 evaluate{};
        evaluate.matchEntity = d.matchEntity;
        evaluate.tick = tick;
        evaluate.phase = d.phase;
        if (LiveBehavior::dispatchMatchEvaluate(evaluate, tick)) {
            if (evaluate.outEndMatch) {
                serverMatchFinish(evaluate.outWinnerKind, evaluate.outWinnerId,
                                  evaluate.outVictoryType);
                broadcastDuelState(sock, d, players, totalPacketsOut);
                emitGamemodeMatchPersistence(d, tick, players);
            }
            return;
        }
    }

    if (d.matchMode == "ffa") {
        for (const auto& kv : d.ffaKills) {
            if (kv.second >= d.goalValue) {
                d.matchOver = true;
                d.phase = DUEL_PHASE_RESULTS;
                d.winnerPlayerId = kv.first;
                d.victoryType = 0;  // ScoreLimit
                d.phaseTimer = d.resultsSeconds;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                emitGamemodeMatchPersistence(d, tick, players);
                Debug::warn(Debug::Category::Duel,
                    "[DUEL SERVER] FFA match over winner=%u score=%d goal=%d\n",
                    kv.first, kv.second, d.goalValue);
                return;
            }
        }
    } else if (d.matchMode == "tdm") {
        if (d.redTeamKills >= d.goalValue || d.blueTeamKills >= d.goalValue) {
            d.matchOver = true;
            d.phase = DUEL_PHASE_RESULTS;
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
            d.victoryType = 0;  // ScoreLimit
            d.phaseTimer = d.resultsSeconds;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            emitGamemodeMatchPersistence(d, tick, players);
            Debug::warn(Debug::Category::Duel,
                "[DUEL SERVER] TDM match over winnerTeam=%d red=%d blue=%d goal=%d\n",
                d.winnerTeam, d.redTeamKills, d.blueTeamKills, d.goalValue);
            return;
        }
    }

    // Time limit check
    if (d.matchTimeLimitTick > 0 && tick >= d.matchTimeLimitTick) {
        d.matchOver = true;
        d.phase = DUEL_PHASE_RESULTS;
        d.victoryType = 1;  // TimeLimit
        d.phaseTimer = d.resultsSeconds;
        if (d.matchMode == "ffa") {
            int best = -1;
            for (const auto& kv : d.ffaKills) {
                if (kv.second > best) {
                    best = kv.second;
                    d.winnerPlayerId = kv.first;
                }
            }
            emitGamemodeMatchPersistence(d, tick, players);
        } else if (d.matchMode == "tdm") {
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
            emitGamemodeMatchPersistence(d, tick, players);
        }
        ++d.stateVersion;
        broadcastDuelState(sock, d, players, totalPacketsOut);
        Debug::warn(Debug::Category::Duel,
            "[DUEL SERVER] Time limit reached mode=%s red=%d blue=%d\n",
            d.matchMode.c_str(), d.redTeamKills, d.blueTeamKills);
    }
}

// ── NPC wave lifecycle (win_condition == "npc_waves") ──────────────────

// Remove every NPC currently in the wave roster. The next server tick's
// syncServerNpcDamageToNpc destroys the matching simulated bodies.
void clearWaveNpcs(std::unordered_map<uint32_t, ServerNpc>& npcs,
                   NpcSystem& npcSystem)
{
    npcs.clear();
    npcSystem.destroyAll();
}

// Add `count` fresh NPCs for the current wave at the match anchor. Entries in
// the ServerNpc map are adopted into the simulated NpcSystem on the next tick.
void spawnWaveNpcs(ServerGamemodeState& d,
                   std::unordered_map<uint32_t, ServerNpc>& npcs,
                   int count)
{
    for (int i = 0; i < count; ++i) {
        ServerNpc npc;
        npc.entityId = EntityRegistry::instance().allocateLegacyId(
            EntityRealm::Server, EntityDomain::Npc);
        npc.origin = GAME_NPC_ORIGIN_GAMEMODE;
        npc.name = "Wave " + std::to_string(d.waveNumber) +
                   " NPC " + std::to_string(i + 1);
        npc.pos = gamemodeSpawnPoint(d);
        npc.yaw = 0.0f;
        // Generic origin authority at creation: a wave NPC must never be
        // classified as an automatic startup NPC by reconciliation.
        {
            const EntityId npcIdentity = Ecs::ensure(
                EntityRealm::Server, EntityDomain::Npc, npc.entityId);
            actorStateWriteOrigin(Ecs::raw(npcIdentity), npc.origin, 0);
            actorStateWriteLifecycle(Ecs::raw(npcIdentity), 1u, 0u, 0.0f);
        }
        npcs[npc.entityId] = npc;
    }
}

// Start one NPC wave: wipe the previous wave, spawn round(N) enemies, rebuild
// the participant roster, and run the shared 3-2-1-GO countdown so the player
// is never dropped straight into a fight.
void beginNpcWave(SOCKET sock,
                  ServerGamemodeState& d,
                  std::unordered_map<uint32_t, ServerPlayer>& players,
                  std::unordered_map<uint32_t, ServerNpc>& npcs,
                  NpcSystem& npcSystem,
                  uint32_t tick,
                  uint64_t& totalPacketsOut)
{
    if (d.waveNumber < 1) d.waveNumber = 1;
    clearWaveNpcs(npcs, npcSystem);
    const int count = std::max(1, d.waveStartCount +
                                  (d.waveNumber - 1) * d.waveIncrement);
    spawnWaveNpcs(d, npcs, count);
    assignMatchParticipants(d, players, &npcs);
    beginMatchCountdown(d, players, npcs, npcSystem, tick);
    const std::string msg = "WAVE " + std::to_string(d.waveNumber) +
                            " - " + std::to_string(count) + " ENEMIES";
    broadcastServerChatMessage(sock, players, tick, totalPacketsOut, msg.c_str());
    Debug::warn(Debug::Category::Duel,
        "[WAVES] round=%d enemies=%d participants=%zu tick=%u\n",
        d.waveNumber, count, d.participants.size(), tick);
}

// Resolve the NPC-wave round. Records each player's highest round, ends the
// run when no player is left, and advances the round when every enemy is dead.
void checkWaveConditions(ServerGamemodeState& d, uint32_t tick, SOCKET sock,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         uint64_t& totalPacketsOut)
{
    if (!d.npcWaves || d.phase != DUEL_PHASE_ACTIVE) return;

    int humanParticipants = 0;
    int alivePlayers = 0;
    int aliveNpcs = 0;
    for (uint32_t id : d.participants) {
        auto aIt = d.matchActors.find(id);
        const bool inPlay = aIt != d.matchActors.end() &&
            (aIt->second.state == ActorState::Alive ||
             aIt->second.state == ActorState::Respawning);
        if (players.find(id) != players.end()) {
            ++humanParticipants;
            int& best = d.waveBest[id];
            if (d.waveNumber > best) best = d.waveNumber;
            if (inPlay) ++alivePlayers;
        } else if (inPlay) {
            ++aliveNpcs;
        }
    }

    // Every player is out of lives: the run is over. Show the best round.
    // Guarded on a human being present so an empty server does not loop
    // NPC-only runs forever after the last player leaves.
    if (humanParticipants > 0 && alivePlayers <= 0) {
        d.matchOver = true;
        d.winnerPlayerId = 0;
        d.winnerTeam = -1;
        d.victoryType = 0;
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = d.resultsSeconds;
        ++d.stateVersion;
        broadcastDuelState(sock, d, players, totalPacketsOut);
        for (const auto& kv : d.waveBest) {
            auto nameIt = d.participantNames.find(kv.first);
            const std::string name = nameIt != d.participantNames.end()
                ? nameIt->second : ("player " + std::to_string(kv.first));
            const std::string line = name + " reached round " +
                std::to_string(kv.second);
            broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
                                       line.c_str());
        }
        Debug::warn(Debug::Category::Duel,
            "[WAVES] run over round=%d participants=%zu\n",
            d.waveNumber, d.participants.size());
        return;
    }

    // Every enemy is dead: brief pause, then the next wave's countdown.
    if (aliveNpcs <= 0) {
        d.matchOver = false;
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = 2.0f;
        ++d.stateVersion;
        broadcastDuelState(sock, d, players, totalPacketsOut);
        Debug::warn(Debug::Category::Duel,
            "[WAVES] round %d cleared; next wave\n", d.waveNumber);
    }
}

} // namespace

// ── Objective rounds + bomb (Counter-Strike style) ────────────────────
// These are generic enough for any defuse-style mode; the JSON mode decides
// whether they run (win_condition == "objective_rounds", objective_bomb feature).

static bool bombSiteContains(const MapConfig& mc, const glm::vec3& p, int* siteIndexOut)
{
    for (int i = 0; i < (int)mc.bombSites.size(); ++i) {
        const MapBombSite& site = mc.bombSites[i];
        const glm::vec3 delta = p - site.center;
        if (glm::dot(delta, delta) <= site.radius * site.radius) {
            if (siteIndexOut) *siteIndexOut = i;
            return true;
        }
    }
    return false;
}

static bool objectiveTeamAlive(const ServerGamemodeState& d, int team)
{
    for (uint32_t id : d.participants) {
        auto tIt = d.matchTeams.find(id);
        if (tIt == d.matchTeams.end() || tIt->second != team) continue;
        auto aIt = d.matchActors.find(id);
        if (aIt != d.matchActors.end() &&
            (aIt->second.state == ActorState::Alive ||
             aIt->second.state == ActorState::Respawning))
            return true;
    }
    return false;
}

static bool objectiveActorPos(const ServerGamemodeState&,
                              uint32_t id,
                              const std::unordered_map<uint32_t, ServerPlayer>& players,
                              const std::unordered_map<uint32_t, ServerNpc>& npcs,
                              glm::vec3& out)
{
    auto pIt = players.find(id);
    if (pIt != players.end()) { out = getEntityRootPos(pIt->second); return true; }
    auto nIt = npcs.find(id);
    if (nIt != npcs.end()) { out = nIt->second.pos; return true; }
    return false;
}

static uint32_t objectivePickCarrier(const ServerGamemodeState& d)
{
    // Called at round start, immediately after every participant is revived, so
    // the replicated ActorState may still read Spectating from the last round.
    // Team membership is the only reliable input here.
    for (uint32_t id : d.participants) {
        auto tIt = d.matchTeams.find(id);
        if (tIt == d.matchTeams.end() || tIt->second != 0) continue;
        return id;
    }
    return 0;
}

// Advances the bomb for one active tick. Sets d.roundOver + roundEndReason when
// the bomb decides the round (exploded / defused). Does not transition phase.
static void updateObjectiveBomb(ServerGamemodeState& d,
                                std::unordered_map<uint32_t, ServerPlayer>& players,
                                const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (!d.objectiveRounds || d.phase != DUEL_PHASE_ACTIVE || d.roundOver)
        return;

    const MapConfig& mc = MapConfigRegistry::instance().get(d.mapId);
    const float pickupRadius = 2.0f;
    const float defuseRadius = 2.5f;

    switch (d.objectiveBombState) {
    case BOMB_OBJ_CARRIED: {
        auto cIt = players.find(d.objectiveBombCarrierId);
        const bool carrierValid = cIt != players.end() && !cIt->second.dead &&
            cIt->second.spawnState == ServerPlayer::Active;
        if (!carrierValid) {
            // Carrier disappeared without a kill event: drop in place.
            d.objectiveBombState = BOMB_OBJ_DROPPED;
            d.objectiveBombCarrierId = 0;
            d.objectiveBombPlantProgress = 0.0f;
            d.stateBroadcastPending = true;
            break;
        }
        d.objectiveBombPos = getEntityRootPos(cIt->second);
        // Plant while the carrier holds position inside a bomb site.
        if (bombSiteContains(mc, d.objectiveBombPos, nullptr)) {
            d.objectiveBombPlantProgress += SERVER_DT;
            if (d.objectiveBombPlantProgress >= d.objectiveBombPlantSeconds) {
                d.objectiveBombState = BOMB_OBJ_PLANTED;
                d.objectiveBombCarrierId = 0;
                d.objectiveBombTimer = d.objectiveBombTimerMax;
                d.objectiveBombPlantProgress = d.objectiveBombPlantSeconds;
                d.stateBroadcastPending = true;
                Debug::warn(Debug::Category::Duel,
                    "[BOMB] planted at (%.1f %.1f %.1f) timer=%.0fs\n",
                    d.objectiveBombPos.x, d.objectiveBombPos.y, d.objectiveBombPos.z,
                    d.objectiveBombTimerMax);
            }
        } else {
            d.objectiveBombPlantProgress = 0.0f;
        }
        break;
    }
    case BOMB_OBJ_DROPPED: {
        for (uint32_t id : d.participants) {
            auto tIt = d.matchTeams.find(id);
            if (tIt == d.matchTeams.end() || tIt->second != 0) continue;
            auto aIt = d.matchActors.find(id);
            if (aIt == d.matchActors.end() || aIt->second.state != ActorState::Alive) continue;
            glm::vec3 p;
            if (!objectiveActorPos(d, id, players, npcs, p)) continue;
            if (glm::distance(p, d.objectiveBombPos) <= pickupRadius) {
                d.objectiveBombState = BOMB_OBJ_CARRIED;
                d.objectiveBombCarrierId = id;
                d.objectiveBombPlantProgress = 0.0f;
                d.objectiveBombDefuseProgress = 0.0f;
                d.stateBroadcastPending = true;
                Debug::log(Debug::Category::Duel,
                    "[BOMB] picked up by actor=%u\n", id);
                break;
            }
        }
        break;
    }
    case BOMB_OBJ_PLANTED: {
        bool defusing = false;
        for (uint32_t id : d.participants) {
            auto tIt = d.matchTeams.find(id);
            if (tIt == d.matchTeams.end() || tIt->second != 1) continue;
            auto aIt = d.matchActors.find(id);
            if (aIt == d.matchActors.end() || aIt->second.state != ActorState::Alive) continue;
            glm::vec3 p;
            if (!objectiveActorPos(d, id, players, npcs, p)) continue;
            if (glm::distance(p, d.objectiveBombPos) <= defuseRadius) { defusing = true; break; }
        }
        if (defusing) {
            d.objectiveBombDefuseProgress += SERVER_DT;
            if (d.objectiveBombDefuseProgress >= d.objectiveBombDefuseSeconds) {
                d.objectiveBombState = BOMB_OBJ_DEFUSED;
                d.roundOver = true;
                d.roundEndReason = "bomb_defused";
                d.stateBroadcastPending = true;
                Debug::warn(Debug::Category::Duel, "[BOMB] defused\n");
                break;
            }
        } else {
            d.objectiveBombDefuseProgress = 0.0f;
        }
        d.objectiveBombTimer -= SERVER_DT;
        if (d.objectiveBombTimer <= 0.0f) {
            d.objectiveBombTimer = 0.0f;
            d.objectiveBombState = BOMB_OBJ_EXPLODED;
            d.roundOver = true;
            d.roundEndReason = "bomb_exploded";
            d.stateBroadcastPending = true;
            for (uint32_t id : d.participants) {
                glm::vec3 p;
                if (!objectiveActorPos(d, id, players, npcs, p)) continue;
                if (glm::distance(p, d.objectiveBombPos) > d.objectiveBombExplosionRadius) continue;
                auto pIt = players.find(id);
                if (pIt != players.end() && !pIt->second.dead) {
                    pIt->second.dead = true;
                    pIt->second.health = 0;
                    pIt->second.respawnSeconds = serverMatchRespawnsEnabled()
                        ? serverMatchRespawnSeconds() : -1.0f;
                    ++pIt->second.deaths;
                }
            }
            Debug::warn(Debug::Category::Duel,
                "[BOMB] exploded at (%.1f %.1f %.1f) radius=%.1f\n",
                d.objectiveBombPos.x, d.objectiveBombPos.y, d.objectiveBombPos.z,
                d.objectiveBombExplosionRadius);
        }
        break;
    }
    default:
        break;
    }
}

// Decides the round for objective_rounds. On a decision, records the round win,
// advances the phase to RESULTS, and declares the match over at goal_value.
static bool checkObjectiveRoundEnd(ServerGamemodeState& d, uint32_t tick,
                                   SOCKET sock,
                                   std::unordered_map<uint32_t, ServerPlayer>& players,
                                   uint64_t& totalPacketsOut)
{
    if (!d.objectiveRounds || d.phase != DUEL_PHASE_ACTIVE) return false;

    int winnerTeam = -1;
    const char* reason = "elimination";
    if (d.objectiveBombState == BOMB_OBJ_DEFUSED) {
        winnerTeam = 1; reason = "bomb_defused";
    } else if (d.objectiveBombState == BOMB_OBJ_EXPLODED) {
        winnerTeam = 0; reason = "bomb_exploded";
    } else {
        const bool tAlive = objectiveTeamAlive(d, 0);
        const bool ctAlive = objectiveTeamAlive(d, 1);
        if (!tAlive && !ctAlive) { winnerTeam = 1; }         // T failed to close it out
        else if (!tAlive) { winnerTeam = 1; }
        else if (!ctAlive) { winnerTeam = 0; }
        // Round clock: expired with no plant means CT held the sites.
        if (winnerTeam < 0 && d.matchTimeLimitTick > 0 && tick >= d.matchTimeLimitTick &&
            d.objectiveBombState != BOMB_OBJ_PLANTED) {
            winnerTeam = 1; reason = "time";
        }
    }
    if (winnerTeam < 0) return false;

    ++d.roundWins[winnerTeam];
    d.roundOver = true;
    d.roundEndReason = reason;
    d.winnerTeam = winnerTeam;
    d.matchOver = d.roundWins[winnerTeam] >= d.goalValue;
    d.victoryType = 0;
    d.phaseTimer = d.resultsSeconds;
    d.phase = DUEL_PHASE_RESULTS;
    ++d.stateVersion;
    broadcastDuelState(sock, d, players, totalPacketsOut);
    if (d.matchOver)
        emitGamemodeMatchPersistence(d, tick, players);
    Debug::warn(Debug::Category::Duel,
        "[CS ROUND] round=%d winnerTeam=%d reason=%s score=%d-%d matchOver=%d\n",
        d.roundNumber, winnerTeam, reason, d.roundWins[0], d.roundWins[1], (int)d.matchOver);
    return true;
}

// Starts a fresh objective round (or the first one). Resets the bomb, assigns
// the carrier, and runs the shared countdown/spawn path.
static void beginObjectiveRound(ServerGamemodeState& d,
                                std::unordered_map<uint32_t, ServerPlayer>& players,
                                std::unordered_map<uint32_t, ServerNpc>& npcs,
                                NpcSystem& npcSystem,
                                uint32_t tick)
{
    ++d.roundNumber;
    d.roundOver = false;
    d.roundEndReason.clear();
    d.objectiveBombState = BOMB_OBJ_CARRIED;
    d.objectiveBombCarrierId = 0;
    d.objectiveBombPos = glm::vec3(0.0f);
    d.objectiveBombPlantProgress = 0.0f;
    d.objectiveBombDefuseProgress = 0.0f;
    d.objectiveBombTimer = 0.0f;
    beginMatchCountdown(d, players, npcs, npcSystem, tick);
    // Carrier is chosen after the spawn assignment so the actor set is current.
    d.objectiveBombCarrierId = objectivePickCarrier(d);
    if (d.objectiveBombCarrierId == 0)
        d.objectiveBombState = BOMB_OBJ_DROPPED;
    d.stateBroadcastPending = true;
    // Generic round-start fact: a hot objective/round mode resets its own state
    // from this; the kernel does not know the mode's round policy.
    GameMatchRoundStartV1 rs{};
    rs.matchEntity = d.matchEntity;
    rs.roundNumber = d.roundNumber;
    rs.tick = tick;
    rs.phase = d.phase;
    LiveBehavior::dispatchGameplayEvent64(GAME_EVENT_MATCH_ROUND_START, &rs,
                                          sizeof(rs), tick, d.matchEntity, 0);
    Debug::warn(Debug::Category::Duel,
        "[CS ROUND] starting round=%d carrier=%u score=%d-%d\n",
        d.roundNumber, d.objectiveBombCarrierId, d.roundWins[0], d.roundWins[1]);
}

void serverGamemodeTick(SOCKET sock,
                    std::unordered_map<uint32_t, ServerPlayer>& players,
                    HeadlessWorld& world,
                    World& npcWorld,
                    std::unordered_map<uint32_t, ServerNpc>& npcs,
                    NpcSystem& npcSystem,
                    std::unordered_set<uint32_t>& npcIdsAlive,
                    uint32_t tick,
                    uint64_t& totalPacketsOut)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    d.currentServerTick = tick;

    // ── Live JSON rule reload ──────────────────────────────────────
    // Editing a gamemode JSON re-applies its rules to the running match at a
    // safe tick boundary. Phase, scores, spawns, and round state are preserved;
    // only policy values change.
    if (!d.mapOnly && !d.matchMode.empty()) {
        const uint64_t rev = GamemodeRegistry::instance().revision();
        if (rev != d.appliedRulesRevision) {
            const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
            d.goalValue = gm.goalValue;
            d.timeLimitSeconds = gm.timeLimitSeconds;
            d.respawnSeconds = gm.respawnSeconds;
            d.killHeals = gm.killHeals;
            d.winCondition = gm.winCondition;
            d.intermissionSeconds = (float)gm.intermissionSeconds;
            d.resultsSeconds = (float)gm.resultsSeconds;
            d.cameraFov = gm.cameraFov;
            d.ragdollExplicit = gm.ragdollExplicit;
            d.ragdollEnabled = gm.ragdollEnabled;
            d.bloodExplicit = gm.bloodExplicit;
            d.bloodEnabled = gm.bloodEnabled;
            d.aimMode = gm.aimMode;
            d.movementPreset = gm.movementPreset;
            d.healthbarOverride = gm.healthbar.explicitValue;
            d.healthbarAimModeEnabled = gm.healthbar.aimModeEnabled;
            d.healthbarShowName = gm.healthbar.showNameInAimMode;
            d.healthbarShowHpText = gm.healthbar.showHpTextInAimMode;
            d.healthbarShowBar = gm.healthbar.showBarInAimMode;
            d.healthbarMaxDistance = gm.healthbar.maxDistance;
            d.objectiveRounds = (gm.winCondition == "objective_rounds");
            d.npcWaves = (gm.winCondition == "npc_waves");
            d.waveStartCount = gm.waveStartCount;
            d.waveIncrement = gm.waveIncrement;
            d.objectiveBombPlantSeconds = gm.bombPlantSeconds;
            d.objectiveBombDefuseSeconds = gm.bombDefuseSeconds;
            d.objectiveBombTimerMax = gm.bombTimerSeconds;
            d.objectiveBombExplosionRadius = gm.bombExplosionRadius;
            d.objectiveBombExplosionDamage = gm.bombExplosionDamage;
            if (!d.aimMode.empty()) {
                GameplayAimMode parsed;
                if (gameplayAimModeFromString(d.aimMode, parsed))
                    GameplayConfig::instance().setAimModeOverride(parsed);
            } else {
                GameplayConfig::instance().clearAimModeOverride();
            }
            d.appliedRulesRevision = rev;
            d.stateBroadcastPending = true;
            ++d.stateVersion;
            Debug::warn(Debug::Category::Duel,
                "[GAMEMODE] live rules reloaded mode=%s goal=%d fov=%.0f aim=%s move=%s\n",
                d.matchMode.c_str(), d.goalValue, d.cameraFov,
                d.aimMode.empty() ? "player" : d.aimMode.c_str(),
                d.movementPreset.empty() ? "player" : d.movementPreset.c_str());
        }
    }

    updateActorStates(d, players, npcs);
    if (!d.mapOnly && d.appliedCommunityWeaponSetId != d.communityWeaponSetId)
    {
        resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
        d.appliedCommunityWeaponSetId = d.communityWeaponSetId;
        ++d.stateVersion;
        d.stateBroadcastPending = true;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE LOADOUT] set=%d applied actors=%zu tick=%u\n",
            d.communityWeaponSetId, d.participants.size(), tick);
    }
    if (d.mapOnly)
    {
        const uint64_t now = nowMs();
        if (d.communityRoundOver && now >= d.communityRoundResetMs) {
            d.communityRoundOver = false;
            d.communityScores.clear();
            d.communityTeamScore[0] = d.communityTeamScore[1] = 0;
            Debug::log(Debug::Category::Networking, "[COMMUNITY MATCH] new round mode=%s\n", d.communityMode.c_str());
        }
        // Promote one kill event per tick from the queue into hasPendingKill
        // so community/mapOnly FFA and TDM scoring actually runs.
        if (!d.hasPendingKill && !d.pendingKillEvents.empty())
        {
            d.currentKill = std::move(d.pendingKillEvents.front());
            d.pendingKillEvents.pop_front();
            const ServerGamemodeKillEvent& ev = d.currentKill;
            d.pendingKillerId = ev.killerId;
            d.pendingVictimId = ev.victimId;
            d.pendingKillerIsNpc = ev.killerEntityType == ENTITY_NPC;
            d.pendingVictimIsNpc = ev.victimEntityType == ENTITY_NPC;
            d.hasPendingKill = true;
        }
        if (d.hasPendingKill) {
            const uint32_t killer = d.pendingKillerId;
            d.hasPendingKill = false;
            if (!d.communityRoundOver && killer != 0) {
                if (d.communityMode == "team_deathmatch") {
                    std::vector<uint32_t> ids;
                    for (const auto& kv : players)
                        if (kv.second.spawnState == ServerPlayer::Active) ids.push_back(kv.first);
                    std::sort(ids.begin(), ids.end());
                    for (size_t i = 0; i < ids.size(); ++i)
                        d.communityTeams[ids[i]] = (int)(i % 2);
                    const auto teamIt = d.communityTeams.find(killer);
                    if (teamIt != d.communityTeams.end()) {
                        const int team = teamIt->second;
                        if (++d.communityTeamScore[team] >= 30) {
                            d.communityRoundOver = true;
                            const std::string message = "Team " + std::to_string(team + 1) +
                                " wins Team Deathmatch (30 kills)!";
                            broadcastCommunityNotification(sock, players, message, 300, totalPacketsOut);
                            d.communityRoundResetMs = now + 5000;
                        }
                    }
                } else if (d.communityMode == "free_for_all") {
                    const int score = ++d.communityScores[killer];
                    // Sync to ffaKills so broadcastDuelState leaderboard renders correctly
                    d.ffaKills[killer] = score;
                    if (score >= 20) {
                        d.communityRoundOver = true;
                        const auto winner = players.find(killer);
                        const std::string name = winner == players.end() ? "Player" : winner->second.name;
                        const std::string message = name + " wins Free For All (20 kills)!";
                        broadcastCommunityNotification(sock, players, message, 300, totalPacketsOut);
                        d.communityRoundResetMs = now + 5000;
                    }
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                }
            }
        }
        if (d.hasPendingManualMap && d.pendingAutomaticMap.empty()) {
            d.pendingAutomaticMap = d.pendingManualMap;
            d.pendingManualMap.clear();
            d.hasPendingManualMap = false;
            d.mapChangeCountdownStartMs = now;
        }
        if (d.autoMapRotation && d.pendingAutomaticMap.empty() && now >= d.nextMapRotationMs) {
            std::vector<std::string> candidates;
            for (const auto& candidate : d.mapPool)
                if (candidate != d.mapId && !d.usedMaps.count(candidate)) candidates.push_back(candidate);
            if (candidates.empty()) {
                d.usedMaps.clear();
                d.usedMaps.insert(d.mapId);
                for (const auto& candidate : d.mapPool)
                    if (candidate != d.mapId) candidates.push_back(candidate);
            }
            if (!candidates.empty()) {
                std::mt19937 rng(std::random_device{}());
                std::shuffle(candidates.begin(), candidates.end(), rng);
                d.pendingAutomaticMap = candidates.front();
                d.mapChangeCountdownStartMs = now;
                Debug::warn(Debug::Category::Networking,
                    "[COMMUNITY MAP ROTATION] current=%s next=%s candidates=%zu (randomized)\n",
                    d.mapId.c_str(), d.pendingAutomaticMap.c_str(), candidates.size());
            } else {
                d.nextMapRotationMs = now + (uint64_t)d.mapRotationMinutes * 60000ull;
            }
        }
        if (!d.pendingAutomaticMap.empty()) {
            const uint64_t elapsed = now - d.mapChangeCountdownStartMs;
            const uint64_t interval = 30000;
            const uint64_t remaining = elapsed >= interval ? 0 : interval - elapsed;
            static std::string lastNoticeMap;
            static uint32_t lastNotice = UINT32_MAX;
            if (lastNoticeMap != d.pendingAutomaticMap) {
                lastNoticeMap = d.pendingAutomaticMap;
                lastNotice = UINT32_MAX;
            }
            const uint32_t seconds = (uint32_t)((remaining + 999) / 1000);
            if (remaining > 0 && remaining <= interval &&
                (seconds == 30 || seconds == 5 || seconds == 3 || seconds == 2 || seconds == 1) &&
                seconds != lastNotice) {
                lastNotice = seconds;
                std::string msg = "Server changing map to " + d.pendingAutomaticMap +
                                  " in " + std::to_string(seconds) + " sec...";
                broadcastCommunityNotification(sock, players, msg, 180, totalPacketsOut);
                Debug::warn(Debug::Category::Networking, "[COMMUNITY MAP NOTICE] %s\n", msg.c_str());
            }
            if (remaining == 0) {
                const std::string next = d.pendingAutomaticMap;
                d.pendingAutomaticMap.clear();
                lastNoticeMap.clear();
                lastNotice = UINT32_MAX;
                if (reloadGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, next, totalPacketsOut)) {
                    d.nextMapRotationMs = now + (uint64_t)d.mapRotationMinutes * 60000ull;
                    npcSystem.destroyAll();
                    size_t spawnIndex = 0;
                    for (auto& kv : npcs) {
                        ServerNpc& npc = kv.second;
                        if (!world.spawnPoints.empty()) {
                            const auto& sp = world.spawnPoints[spawnIndex % world.spawnPoints.size()];
                            npc.pos = effectiveServerSpawn(sp.position);
                            npc.yaw = sp.yaw;
                            ++spawnIndex;
                        }
                        npc.health = 100;
                        ++npc.transformEpoch;
                    }
                    npcIdsAlive.clear();
                    Debug::warn(Debug::Category::Networking,
                        "[COMMUNITY MAP CHANGE] loaded=%s nextRotationMs=%llu\n",
                        next.c_str(), (unsigned long long)d.nextMapRotationMs);
                } else {
                    d.nextMapRotationMs = now + 5000;
                }
            }
        }
        return;
    }
    (void)tick;

    // Host-only changemap command: swap the map live on the next tick.
    if (d.hasPendingManualMap)
    {
        const std::string mapId = d.pendingManualMap;
        d.hasPendingManualMap = false;
        d.pendingManualMap.clear();
        if (!mapId.empty())
        {
            if (reloadGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, mapId, totalPacketsOut)) {
                npcSystem.destroyAll();
                size_t spawnIndex = 0;
                for (auto& kv : npcs) {
                    ServerNpc& npc = kv.second;
                    if (!world.spawnPoints.empty()) {
                        const auto& sp = world.spawnPoints[spawnIndex % world.spawnPoints.size()];
                        npc.pos = effectiveServerSpawn(sp.position);
                        npc.yaw = sp.yaw;
                        ++spawnIndex;
                    }
                    npc.health = 100;
                    ++npc.transformEpoch;
                }
                npcIdsAlive.clear();
            }
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
    }

    // Promote one ordered kill event per fixed server tick into the legacy
    // respawn/scoring body below. The queue prevents later kills from
    // overwriting earlier kills; the body remains the single score owner.
    if (!d.hasPendingKill && !d.pendingKillEvents.empty())
    {
        d.currentKill = std::move(d.pendingKillEvents.front());
        d.pendingKillEvents.pop_front();
        const ServerGamemodeKillEvent& event = d.currentKill;
        d.pendingKillerId = event.killerId;
        d.pendingVictimId = event.victimId;
        d.pendingKillerIsNpc = event.killerEntityType == ENTITY_NPC;
        d.pendingVictimIsNpc = event.victimEntityType == ENTITY_NPC;
        d.hasPendingKill = true;
        Debug::log(Debug::Category::Duel,
            "[GAMEMODE KILL QUEUE] event=%u killer=%s kind=%u victim=%s kind=%u remaining=%zu tick=%u\n",
            event.eventId, event.killerName.c_str(), (unsigned)event.killerEntityType,
            event.victimName.c_str(), (unsigned)event.victimEntityType,
            d.pendingKillEvents.size(), tick);
        DBG(Network,
            "KILL_QUEUE_PROMOTE eventId=%u killer=%u killerKind=%u victim=%u victimKind=%u "
            "weapon=\"%s\" remaining=%zu tick=%u phase=%d matchMode=\"%s\" enabled=%d mapOnly=%d",
            event.eventId, event.killerId, (unsigned)event.killerEntityType,
            event.victimId, (unsigned)event.victimEntityType,
            event.weaponDisplayName.c_str(), d.pendingKillEvents.size(),
            tick, (int)d.phase, d.matchMode.c_str(), (int)d.enabled, (int)d.mapOnly);
    }

    // Process one queued kill recorded by authoritative damage.
    if (d.hasPendingKill)
    {
        d.hasPendingKill = false;
        const uint32_t killerId = d.pendingKillerId;
        const uint32_t victimId = d.pendingVictimId;

        // Objective bomb: a killed carrier drops the bomb where it died.
        if (d.objectiveBombState == BOMB_OBJ_CARRIED &&
            victimId == d.objectiveBombCarrierId) {
            d.objectiveBombState = BOMB_OBJ_DROPPED;
            d.objectiveBombCarrierId = 0;
            d.objectiveBombPos = d.currentKill.victimPos;
            d.objectiveBombPlantProgress = 0.0f;
            Debug::warn(Debug::Category::Duel,
                "[BOMB] carrier %u died; bomb dropped at (%.1f %.1f %.1f)\n",
                victimId, d.objectiveBombPos.x, d.objectiveBombPos.y, d.objectiveBombPos.z);
            d.stateBroadcastPending = true;
        }

        // The victim's respawn delay/state was assigned by the lethal damage
        // path (server-damage / server-npcs). Here we only pin the respawn
        // anchor; do NOT zero the timer or the gamemode delay would be lost.
        auto victimIt = players.find(victimId);
        if (!d.pendingVictimIsNpc && victimIt != players.end())
        {
            int victimTeam = -1;
            auto vTeamIt = d.matchTeams.find(victimId);
            if (vTeamIt != d.matchTeams.end()) victimTeam = vTeamIt->second;
            victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d, victimTeam);
        }
        else if (d.pendingVictimIsNpc)
        {
            // Pin the dead NPC's respawn anchor to the current match spawn so a
            // later map change cannot resurrect it into stale/void space.
            for (Npc& n : npcSystem.all()) {
                if (n.id == victimId) {
                    n.body.respawnPosition = gamemodeSpawnPoint(d);
                    break;
                }
            }
        }

        // ── NPC mind events: killer confidence + perceivable witnessed deaths ──
        {
            glm::vec3 victimPos(0.0f);
            bool haveVictimPos = false;
            if (!d.pendingVictimIsNpc && victimIt != players.end()) {
                victimPos = victimIt->second.pos;
                haveVictimPos = true;
            } else {
                for (const Npc& n : npcSystem.all())
                    if (n.id == victimId) { victimPos = n.body.pos; haveVictimPos = true; break; }
            }
            if (d.pendingKillerIsNpc) {
                for (Npc& k : npcSystem.all())
                    if (k.id == killerId) { npcMindOnKill(k); break; }
            }
            if (haveVictimPos) {
                auto teamOf = [&](uint32_t id) -> int {
                    auto it = d.matchTeams.find(id);
                    return it != d.matchTeams.end() ? it->second : -1;
                };
                const int victimTeam = teamOf(victimId);
                for (Npc& w : npcSystem.all()) {
                    if (w.id == victimId) continue;
                    if (w.body.dead || w.body.currentHp <= 0) continue;
                    if (!npcMindCanPerceive(w, npcWorld, victimPos)) continue;
                    const int wTeam = teamOf(w.id);
                    const bool ally = (wTeam >= 0 && victimTeam >= 0 && wTeam == victimTeam);
                    npcMindOnWitnessedDeath(w, victimId, victimPos, ally);
                }
            }
        }

        // Tell the killer where the victim respawned (tracer).
        if (killerId != victimId && !d.pendingVictimIsNpc)
        {
            DuelEnemySpawnPacket tracer{};
            tracer.header.type = PACKET_DUEL_ENEMY_SPAWN;
            tracer.header.tick = 0;
            tracer.enemyPlayerId = victimId;
            glm::vec3 spawnPos = victimIt != players.end() && victimIt->second.hasDuelSpawnPos
                ? victimIt->second.duelSpawnPos : glm::vec3(0.0f);
            tracer.posX = spawnPos.x;
            tracer.posY = spawnPos.y;
            tracer.posZ = spawnPos.z;
            tracer.duelId = d.duelId;
            tracer.mapVersion = d.mapVersion;
            tracer.spawnAnchorVersion = d.spawnAnchorVersion;
            tracer.respawnSequence = ++d.respawnSequence;
            ++d.stateVersion;

            auto killerIt = players.find(killerId);
            if (killerIt != players.end() && killerIt->second.spawnState == ServerPlayer::Active)
            {
                const uint32_t eventId = nextReliableGameplayEventId();
                const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
                    sock, killerIt->second, &tracer, sizeof(tracer), eventId,
                    reliableGameplayEventSessionForPlayer(killerIt->second), totalPacketsOut);
                const bool sent = result == ReliableGameplayEventQueueResult::Queued;
                Debug::log(Debug::Category::Duel,
                    "[DuelPacketSend] type=DuelEnemySpawnPacket reliable=1 player=%u enemy=%u sent=%d pos=(%.3f,%.3f,%.3f)\n",
                    killerIt->second.id, victimId, (int)sent, tracer.posX, tracer.posY, tracer.posZ);
                if (sent)
                    ++totalPacketsOut;
            }
        }

        // Generic occurrence fact. A hot mode handler that sets `handled` owns
        // the authoritative scoring decision for this kill; otherwise the cold
        // per-mode scoring below runs (fallback until each mode migrates).
        GameActorKilledV1 killedEvent{};
        killedEvent.killerId = killerId;
        killedEvent.victimId = victimId;
        killedEvent.killerIsNpc = d.pendingKillerIsNpc ? 1u : 0u;
        killedEvent.victimIsNpc = d.pendingVictimIsNpc ? 1u : 0u;
        killedEvent.tick = tick;
        const bool hotScored = LiveBehavior::dispatchActorKilled(killedEvent, tick);

        // Score only counts during the active phase, and never for a suicide.
        if (!hotScored && d.phase == DUEL_PHASE_ACTIVE && !d.matchOver &&
            killerId != victimId)
        {
            // Duel 1v1 scoring (original behavior)
            if (d.matchMode == "duel") {
                if (killerId == d.playerAId)
                    ++d.scoreA;
                else if (killerId == d.playerBId)
                    ++d.scoreB;

                if (d.scoreA >= d.goalValue || d.scoreB >= d.goalValue)
                {
                    d.matchOver = true;
                    d.phase = DUEL_PHASE_MATCH_END;
                    d.winnerPlayerId = killerId;
                    d.rematchLeft = d.rematchSeconds;
                    emitGamemodeMatchPersistence(d, tick, players);
                    Debug::warn(Debug::Category::Duel,
                        "[DUEL SERVER] match over winner=%u score=%d-%d goal=%d\n",
                        d.winnerPlayerId, d.scoreA, d.scoreB, d.goalValue);
                }
            }
            // FFA scoring
            else if (d.matchMode == "ffa") {
                const int killsBefore = d.ffaKills[killerId];
                const int deathsBefore = d.ffaDeaths[victimId];
                ++d.ffaKills[killerId];
                ++d.ffaDeaths[victimId];
                DBG(Network,
                    "FFA_SCORED killer=%u killerKind=%d victim=%u victimKind=%d "
                    "killsBefore=%d killsAfter=%d deathsBefore=%d deathsAfter=%d "
                    "tick=%u phase=%d matchOver=%d",
                    killerId, (int)d.pendingKillerIsNpc,
                    victimId, (int)d.pendingVictimIsNpc,
                    killsBefore, d.ffaKills[killerId],
                    deathsBefore, d.ffaDeaths[victimId],
                    tick, (int)d.phase, (int)d.matchOver);
            }
            // TDM scoring
            else if (d.matchMode == "tdm") {
                ++d.ffaKills[killerId];
                ++d.ffaDeaths[victimId];
                auto teamIt = d.matchTeams.find(killerId);
                if (teamIt != d.matchTeams.end()) {
                    int victimTeam = -1;
                    auto vtIt = d.matchTeams.find(victimId);
                    if (vtIt != d.matchTeams.end()) victimTeam = vtIt->second;
                    // Only score if killer and victim are on different teams
                    if (victimTeam >= 0 && teamIt->second != victimTeam) {
                        if (teamIt->second == 0) ++d.redTeamKills;
                        else ++d.blueTeamKills;
                    }
                }
            }
        }
        else
        {
            DBG(Network,
                "KILL_SCORE_SKIPPED killer=%u killerKind=%d victim=%u victimKind=%d "
                "reason=%s tick=%u phase=%d matchOver=%d suicide=%d matchMode=\"%s\"",
                killerId, (int)d.pendingKillerIsNpc,
                victimId, (int)d.pendingVictimIsNpc,
                d.phase != DUEL_PHASE_ACTIVE ? "phase_not_active" :
                    d.matchOver ? "match_over" : "suicide",
                tick, (int)d.phase, (int)d.matchOver,
                (int)(killerId == victimId), d.matchMode.c_str());
        }

        // Persist the authoritative kill once, from the unified event.
        const ServerGamemodeKillEvent& kill = d.currentKill;
        const std::string& persistWeapon = kill.weaponDisplayName.empty()
            ? kill.weaponId : kill.weaponDisplayName;
        if (!d.pendingKillerIsNpc && !d.pendingVictimIsNpc)
        {
            emitPvPKillPersistenceEvent(players, killerId, victimId,
                persistWeapon, tick, kill.killerPos, kill.victimPos);
        }
        else if (!d.pendingKillerIsNpc && d.pendingVictimIsNpc)
        {
            emitNpcKillPersistenceEvent(players, killerId, victimId,
                persistWeapon, tick, kill.killerPos, kill.victimPos);
        }

        broadcastDuelState(sock, d, players, totalPacketsOut);
    }

    // ── Shared FFA/TDM/elimination match mode state machine ─────────
    // Any mode with a generic win condition (e.g. last_team_standing) uses the
    // same lifecycle instead of requiring a mode-specific branch. A registered
    // hot mode (activeModeDomain != 0) owns its lifecycle policy; no mode name
    // is switched on to enter this path.
    const bool hotModeOwnsLifecycle = d.activeModeDomain != 0;
    if (hotModeOwnsLifecycle ||
        d.winCondition == "last_team_standing" || d.objectiveRounds || d.npcWaves)
    {
        if (hotModeOwnsLifecycle)
            dispatchMatchLifecyclePolicy(d, tick);
        if (d.stateBroadcastPending)
        {
            // The command changed the authoritative mode/phase between ticks.
            // Send that state now so every client can show intermission or the
            // immediate countdown without waiting for the periodic broadcast.
            broadcastDuelState(sock, d, players, totalPacketsOut);
            if (d.objectiveRounds)
                broadcastBombTagState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
        // Generic phase ownership: a hot mode may own the phase clock entirely.
        // It writes MatchPhaseOwnership on the match entity and drives phases
        // through match.setPhase; the cold transitions below are then skipped.
        // No mode name is consulted.
        if (d.matchEntity != 0) {
            std::uint32_t phaseOwned = 0;
            if (MimitaRuntime::DynamicComponentStore::instance().read(
                    static_cast<EntityId>(d.matchEntity),
                    gameHash("MatchPhaseOwnership"), &phaseOwned,
                    sizeof(phaseOwned)) &&
                phaseOwned != 0) {
                projectGenericActorTeams(d);
                return;
            }
        }
        switch (d.phase)
        {
        case DUEL_PHASE_WAITING:
            if (countActivePlayers(players) >= 2 ||
                (countActivePlayers(players) >= 1 && (!npcs.empty() || d.npcWaves)))
            {
                // If the current map has no spawn points, rotate.
                if (world.spawnPoints.empty())
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                if (d.npcWaves) {
                    d.waveNumber = 0;
                    d.waveBest.clear();
                    beginNpcWave(sock, d, players, npcs, npcSystem, tick, totalPacketsOut);
                } else if (d.objectiveRounds) {
                    assignMatchParticipants(d, players, &npcs);
                    d.roundWins[0] = d.roundWins[1] = 0;
                    d.roundNumber = 0;
                    beginObjectiveRound(d, players, npcs, npcSystem, tick);
                } else {
                    assignMatchParticipants(d, players, &npcs);
                    beginMatchCountdown(d, players, npcs, npcSystem, tick);
                }
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::warn(Debug::Category::Duel,
                    "[FFA/TDM] Countdown started mode=%s participants=%zu\n",
                    d.matchMode.c_str(), d.participants.size());
            }
            break;

        case DUEL_PHASE_COUNTDOWN:
            if (tick >= d.matchStartTick)
            {
                d.phase = DUEL_PHASE_GO;
                d.phaseTimer = d.goSeconds;
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] GO shown mode=%s tick=%u duration=%.2f\n",
                    d.matchMode.c_str(), tick, d.goSeconds);
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 30)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_GO:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                d.phase = DUEL_PHASE_ACTIVE;
                d.matchStartTick = tick;
                if (d.timeLimitSeconds > 0)
                    d.matchTimeLimitTick = tick + (uint32_t)(d.timeLimitSeconds * 60.0f);
                resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Match ACTIVE mode=%s tick=%u\n",
                    d.matchMode.c_str(), tick, d.matchStartTick);
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 15)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_ACTIVE: {
            // NPC waves, objective rounds, and scored modes each own their
            // round/win resolution behind the shared lifecycle.
            // Generic objective ownership: a hot objective behavior may own the
            // whole objective state machine. It writes ObjectiveOwnership on the
            // match entity; the cold bomb policy below is then bypassed. No mode
            // or objective-type name is consulted.
            std::uint32_t objectiveOwned = 0;
            if (d.matchEntity != 0 &&
                MimitaRuntime::DynamicComponentStore::instance().read(
                    static_cast<EntityId>(d.matchEntity),
                    gameHash("ObjectiveOwnership"), &objectiveOwned,
                    sizeof(objectiveOwned)) &&
                objectiveOwned != 0)
                projectGenericActorTeams(d);
            if (d.npcWaves) {
                checkWaveConditions(d, tick, sock, players, totalPacketsOut);
            } else if (d.objectiveRounds) {
                if (objectiveOwned == 0) {
                    updateObjectiveBomb(d, players, npcs);
                    checkObjectiveRoundEnd(d, tick, sock, players, totalPacketsOut);
                }
            } else {
                checkMatchWinConditions(d, tick, sock, players, totalPacketsOut);
            }
            if (d.phase != DUEL_PHASE_ACTIVE) break;  // win condition triggered
            // Objective bomb state is fairly volatile; keep clients in sync.
            if (d.objectiveRounds && objectiveOwned == 0 &&
                tick - d.objectiveBombBroadcastTick >= 30) {
                d.objectiveBombBroadcastTick = tick;
                broadcastBombTagState(sock, d, players, totalPacketsOut);
            }
            // Periodic broadcast
            if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                {
                    std::string scores;
                    for (const auto& kv : d.ffaKills) {
                        if (!scores.empty()) scores += ", ";
                        auto nameIt = d.participantNames.find(kv.first);
                        scores += (nameIt != d.participantNames.end() ? nameIt->second : "id" + std::to_string(kv.first))
                                  + "=" + std::to_string(kv.second);
                    }
                    DBG(Network,
                        "HEARTBEAT mode=\"%s\" phase=%d tick=%u participants=%zu scores=[%s] "
                        "queueSize=%zu enabled=%d mapOnly=%d serverCode=\"%s\"",
                        d.matchMode.c_str(), (int)d.phase, tick,
                        d.participants.size(), scores.c_str(),
                        d.pendingKillEvents.size(), (int)d.enabled, (int)d.mapOnly,
                        getServerCoordinatorCode().c_str());
                }
            }
            break;
        }

        case DUEL_PHASE_RESULTS:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                if (d.pendingModeSwitch && !d.pendingGamemodeId.empty())
                {
                    const std::string nextMode = d.pendingGamemodeId;
                    const bool directCountdown = d.pendingModeSwitchCountdown;
                    d.pendingModeSwitch = false;
                    d.pendingModeSwitchCountdown = false;
                    d.pendingGamemodeId.clear();
                    d.phase = DUEL_PHASE_WAITING;
                    d.matchMode.clear();
                    serverCommunityStartMatch(directCountdown, nextMode);
                    return;
                }
                if (d.npcWaves && !d.matchOver)
                {
                    // Wave cleared: advance the round and run the next 3-2-1.
                    ++d.waveNumber;
                    beginNpcWave(sock, d, players, npcs, npcSystem, tick, totalPacketsOut);
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    break;
                }
                if (d.npcWaves && d.matchOver)
                {
                    // Run over: show the best round, then a fresh run at round 1.
                    d.phase = DUEL_PHASE_INTERMISSION;
                    d.phaseTimer = (float)d.intermissionSeconds;
                    d.waveNumber = 0;
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    Debug::warn(Debug::Category::Duel,
                        "[WAVES] run over; intermission %.0fs\n", d.intermissionSeconds);
                    break;
                }
                if (d.objectiveRounds && !d.matchOver)
                {
                    // Between-round pause: start the next round directly (no
                    // full intermission) so the match keeps its momentum.
                    beginObjectiveRound(d, players, npcs, npcSystem, tick);
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    broadcastBombTagState(sock, d, players, totalPacketsOut);
                    break;
                }
                if (d.objectiveRounds && d.matchOver)
                {
                    // Match decided: full intermission, then a fresh match.
                    d.phase = DUEL_PHASE_INTERMISSION;
                    d.phaseTimer = (float)d.intermissionSeconds;
                    d.roundWins[0] = d.roundWins[1] = 0;
                    d.roundNumber = 0;
                    d.roundOver = false;
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    Debug::warn(Debug::Category::Duel,
                        "[CS ROUND] match over; intermission %.0fs\n", d.intermissionSeconds);
                    break;
                }
                d.phase = DUEL_PHASE_INTERMISSION;
                d.phaseTimer = d.intermissionSeconds;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Intermission started mode=%s duration=%.0f\n",
                    d.matchMode.c_str(), d.intermissionSeconds);
            }
            else if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_INTERMISSION:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                // Rotate map if configured
                if (d.rotateMaps && d.mapPool.size() > 1)
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                if (d.npcWaves) {
                    d.waveNumber = 0;
                    d.waveBest.clear();
                    beginNpcWave(sock, d, players, npcs, npcSystem, tick, totalPacketsOut);
                } else if (d.objectiveRounds) {
                    assignMatchParticipants(d, players, &npcs);
                    d.roundWins[0] = d.roundWins[1] = 0;
                    d.roundNumber = 0;
                    beginObjectiveRound(d, players, npcs, npcSystem, tick);
                } else {
                    assignMatchParticipants(d, players, &npcs);
                    beginMatchCountdown(d, players, npcs, npcSystem, tick);
                }
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Countdown started mode=%s participants=%zu\n",
                    d.matchMode.c_str(), d.participants.size());
            }
            else if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        default:
            break;
        }
        return;
    }

    // ── Objective rounds without the shared FFA/TDM machine ─────────
    // (objectiveRounds is already handled above by the widened condition.)

    // ── Bomb Tag match mode state machine ──────────────────────────
    if (d.hasBombFeature)
    {
        if (d.stateBroadcastPending)
        {
            broadcastDuelState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
        serverBombTagTick(sock, players, world, npcs, npcSystem, tick, totalPacketsOut);
        return;
    }

    // ── Original duel 1v1 state machine ─────────────────────────────
    switch (d.phase)
    {
    case DUEL_PHASE_WAITING:
        if (countActivePlayers(players) >= 2)
        {
            assignGamemodeParticipants(d, players);
            // If the current map has no spawn points (e.g. the host picked a
            // spawn-less map), rotate to a spawn-capable one before starting.
            if (world.spawnPoints.empty())
                rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
            // Drop the practice NPC(s) once the real duel is about to start.
            npcs.clear();
            npcSystem.destroyAll();
            npcIdsAlive.clear();
            assignGamemodeSpawns(d, world);
            beginGamemodeCountdown(d, players);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_COUNTDOWN:
        // A duelist vanished before the fight — fall back to waiting.
        if (players.find(d.playerAId) == players.end() ||
            players.find(d.playerBId) == players.end())
        {
            d.phase = DUEL_PHASE_WAITING;
            d.stateSent = false;
            break;
        }
        d.countdown -= SERVER_DT;
        if (d.countdown <= 0.0f)
        {
            d.phase = DUEL_PHASE_ACTIVE;
            d.stateSent = false;
            ++d.stateVersion;
            Debug::log(Debug::Category::Duel,
                "[ServerGamemode] countdown complete duelId=%u stateVersion=%u phase=ACTIVE\n",
                d.duelId, d.stateVersion);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        // During countdown, do NOT broadcast every tick. Reliable delivery of
        // every countdown snapshot creates a huge backlog that blocks the
        // ACTIVE transition from being delivered promptly. The initial
        // countdown start is broadcast by beginGamemodeCountdown(); the ACTIVE
        // transition is broadcast above. Clients interpolate countdown locally.
        break;

    case DUEL_PHASE_ACTIVE:
        if (players.find(d.playerAId) == players.end() ||
            players.find(d.playerBId) == players.end())
        {
            d.phase = DUEL_PHASE_WAITING;
            d.stateSent = false;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        else if (tick - d.lastBroadcastTick >= 60)
        {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_MATCH_END:
        if (players.find(d.playerAId) == players.end() ||
            players.find(d.playerBId) == players.end())
        {
            // Opponent left during the end screen — no rematch.
            d.stateSent = false;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            break;
        }
        d.rematchLeft -= SERVER_DT;
        if (d.rematchLeft <= 0.0f)
        {
            // Rotate to a fresh map we weren't just on (skips bad/spawn-less maps).
            if (d.rotateMaps && d.mapPool.size() > 1)
                rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
            // Each new match picks a fresh random anchor (fights spread around).
            assignGamemodeSpawns(d, world);
            Debug::log(Debug::Category::Duel, "[DUEL SERVER] rematch\n");
            beginGamemodeCountdown(d, players);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        else if (tick - d.lastBroadcastTick >= 60)
        {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    default:
        break;
    }
}

void serverGamemodeRematchNow()
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    // The MATCH_END branch starts the next duel when rematchLeft hits 0.
    d.rematchLeft = 0.0f;
}

std::string serverActiveTeamList()
{
    const ServerGamemodeState& d = serverGamemodeState();
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.modes().empty()) config.load();
    const CommunityMode* cm = config.modeById(d.communityMode);
    const std::string id = cm ? cm->gamemodeId : d.communityMode;
    const Gamemode& gm = GamemodeRegistry::instance().get(id);
    if (gm.teamNames.empty()) return "no teams";
    std::string out;
    for (size_t i = 0; i < gm.teamNames.size(); ++i) {
        if (!out.empty()) out += " | ";
        out += gm.teamNames[i] + " = " + std::to_string(i + 1);
    }

    return out;
}

bool serverRequestTeamChange(uint32_t playerId, int requestedTeam,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             uint32_t tick, uint64_t& totalPacketsOut,
                             std::string& message)
{
    ServerGamemodeState& d = serverGamemodeState();
    auto playerIt = players.find(playerId);
    if (playerIt == players.end()) { message = "player not found"; return false; }
    const CommunityMode* cm = CommunityServerConfig::instance().modeById(d.communityMode);
    const std::string id = cm ? cm->gamemodeId : d.communityMode;
    const Gamemode& gm = GamemodeRegistry::instance().get(id);
    if (requestedTeam < 0 || requestedTeam >= static_cast<int>(gm.teamNames.size())) {
        message = gm.teamNames.empty() ? "active gamemode has no teams" : "invalid team";
        return false;
    }
    playerIt->second.matchTeam = requestedTeam;
    d.matchTeams[playerId] = requestedTeam;
    message = "switched to " + gm.teamNames[static_cast<size_t>(requestedTeam)];
    broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
        (playerIt->second.name + " " + message).c_str());
    Debug::warn(Debug::Category::Duel,
        "[MATCH TEAM] player=%u team=%d mode=%s result=accepted\n",
        playerId, requestedTeam, id.c_str());
    return true;
}

void serverRespawnAllActors(SOCKET sock,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    ServerGamemodeState& d = serverGamemodeState();
    for (auto& kv : players) {
        ServerPlayer& player = kv.second;
        if (player.spawnState != ServerPlayer::Active) continue;
        player.duelSpawnPos = gamemodeSpawnPoint(d);
        player.hasDuelSpawnPos = true;
        player.pos = player.duelSpawnPos;
        completeAuthoritativeSpawn(sock, player, false);
    }
    for (auto& kv : npcs) {
        ServerNpc& npc = kv.second;
        npc.health = 100;
        ++npc.transformEpoch;
        // Reset the generic authoritative health + lifecycle component so the
        // mirror projection cannot resurrect a dead body from stale authority.
        const EntityId npcIdentity =
            Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, npc.entityId);
        actorHealthInit(Ecs::raw(npcIdentity), 100);
        Ecs::setHealth(npcIdentity, 100, 100, false);
        actorStateWriteLifecycle(Ecs::raw(npcIdentity), npc.transformEpoch, 0u,
                                 0.0f);
        finalizeServerNpcMirrorSpawn(npc, ActorSpawnReason::RespawnAll, tick);
    }
    d.hasPendingKill = false;
    d.pendingKillerId = 0;
    d.pendingVictimId = 0;
    d.pendingKillerIsNpc = false;
    d.pendingVictimIsNpc = false;
    d.pendingKillEvents.clear();
    Debug::warn(Debug::Category::Duel,
        "[MATCH RESPAWN ALL] players=%zu npcs=%zu tick=%u\n",
        players.size(), npcs.size(), tick);
    broadcastDuelState(sock, d, players, totalPacketsOut);
}

void serverGamemodeRequestMapChange(const std::string& mapId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled || mapId.empty()) return;
    d.hasPendingManualMap = true;
    d.pendingManualMap = mapId;
}

void serverGamemodeRecordKill(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>* npcs,
    uint32_t killerId, uint8_t killerEntityType,
    uint32_t victimId, uint8_t victimEntityType,
    const std::string& weaponId,
    const std::string& weaponDisplayName,
    uint64_t correlationId,
    const glm::vec3& killerPos,
    const glm::vec3& victimPos,
    uint32_t tick,
    uint64_t& totalPacketsOut)
{
    {
        LiveEventJournal::Fields f;
        f.tick = tick;
        f.entityId = victimId;
        f.actorId = std::to_string(killerId);
        f.result = "begin";
        f.extra = std::string("\"killer_entity_type\":") +
            std::to_string(killerEntityType) + ",\"victim_entity_type\":" +
            std::to_string(victimEntityType) + ",\"weapon_id\":\"" +
            weaponId + "\"";
        LiveEventJournal::instance().record("server.kill_record_begin", f);
    }
    // Credit the kill. Heal the player killer only when the active gamemode
    // rule allows it (kill_heals); one-life / tactical modes set false.
    if (killerEntityType == ENTITY_PLAYER && killerId != 0 && killerId != victimId)
    {
        auto attacker = players.find(killerId);
        if (attacker != players.end())
        {
            attacker->second.kills += 1;
            const ServerGamemodeState& rules = serverGamemodeState();
            if (matchKillHeals(rules.enabled, rules.killHeals)) {
                // Heal to the actor's role-resolved life maximum, not a flat 100.
                attacker->second.health = attacker->second.maxHealth > 0
                    ? attacker->second.maxHealth : serverMaxHp();
            }
            Debug::log(Debug::Category::Duel,
                "[KILL HEAL] killer=%u mode=%s kill_heals=%d healed=%d\n",
                killerId, rules.matchMode.c_str(), (int)rules.killHeals,
                (int)matchKillHeals(rules.enabled, rules.killHeals));
        }
    }

    auto resolveName = [&](uint32_t id, uint8_t type) -> std::string {
        if (type == ENTITY_NPC)
        {
            if (npcs)
            {
                auto it = npcs->find(id);
                if (it != npcs->end() && !it->second.name.empty())
                    return it->second.name;
            }
            return "NPC-" + std::to_string(id);
        }
        auto it = players.find(id);
        return it != players.end() ? it->second.name
                                   : "player_" + std::to_string(id);
    };

    // Exactly one reliable killfeed event for every client, regardless of
    // gamemode enable state so sandbox/loose kills still present.
    KillEventPacket killPkt{};
    killPkt.header.type = PACKET_KILL_EVENT;
    killPkt.header.tick = tick;
    killPkt.eventId = nextReliableGameplayEventId();
    killPkt.eventSessionId = serverReliableEventSessionId();
    killPkt.killerId = killerId;
    killPkt.victimId = victimId;
    killPkt.serverTick = tick;
    killPkt.correlationId = correlationId;
    killPkt.killerEntityType = killerEntityType;
    killPkt.victimEntityType = victimEntityType;
    const std::string killerName = resolveName(killerId, killerEntityType);
    const std::string victimName = resolveName(victimId, victimEntityType);
    std::strncpy(killPkt.killerName, killerName.c_str(), sizeof(killPkt.killerName) - 1);
    std::strncpy(killPkt.victimName, victimName.c_str(), sizeof(killPkt.victimName) - 1);
    std::strncpy(killPkt.weaponDisplay, weaponDisplayName.c_str(), sizeof(killPkt.weaponDisplay) - 1);
    queueReliableGameplayEventToAll(sock, players, &killPkt, sizeof(killPkt),
        killPkt.eventId, killPkt.eventSessionId, totalPacketsOut);
    Debug::log(Debug::Category::Networking,
        "[KILL EVENT] killer=%s kind=%u victim=%s kind=%u weapon=\"%s\" tick=%u event=%u\n",
        killerName.c_str(), (unsigned)killerEntityType, victimName.c_str(),
        (unsigned)victimEntityType, weaponDisplayName.c_str(), tick, killPkt.eventId);

    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;

    ServerGamemodeKillEvent event;
    event.killerId = killerId;
    event.victimId = victimId;
    event.killerEntityType = killerEntityType;
    event.victimEntityType = victimEntityType;
    event.killerName = killerName;
    event.victimName = victimName;
    event.weaponId = weaponId;
    event.weaponDisplayName = weaponDisplayName;
    event.killerPos = killerPos;
    event.victimPos = victimPos;
    event.eventId = ++d.killEventCounter;
    event.correlationId = correlationId;
    event.serverTick = tick;
    DBG(Network,
        "GAMEMODE_ENQUEUE type=%s killer=%u kind=%u victim=%u kind=%u "
        "weaponId=\"%s\" weaponDisplay=\"%s\" eventId=%u queueSize=%zu tick=%u",
        killerEntityType == ENTITY_NPC ? "NPC_KILLS_PLAYER" : "PLAYER_KILL",
        killerId, (unsigned)killerEntityType, victimId, (unsigned)victimEntityType,
        weaponId.c_str(), weaponDisplayName.c_str(),
        event.eventId, d.pendingKillEvents.size() + 1, tick);
    d.pendingKillEvents.push_back(std::move(event));
}

// ── Bomb Tag ──────────────────────────────────────────────────────────

namespace {

// Shuffle bag for bomb holder selection. Ensures every eligible player
// is selected once before the bag is reshuffled.
struct BombShuffleBag {
    std::vector<uint32_t> order;
    int position = 0;

    void build(const std::vector<uint32_t>& eligible) {
        order = eligible;
        // Fisher-Yates shuffle
        for (int i = (int)order.size() - 1; i > 0; --i) {
            int j = (int)((uint32_t)std::rand() % (uint32_t)(i + 1));
            std::swap(order[i], order[j]);
        }
        position = 0;
    }

    uint32_t next() {
        if (order.empty()) return 0;
        if (position >= (int)order.size()) {
            // Bag exhausted — rebuild with current eligible list
            // Caller must call build() before next() if they want a fresh bag.
            // If called without build(), just wrap around.
            position = 0;
        }
        return order[position++];
    }

    void remove(uint32_t id) {
        auto it = std::find(order.begin() + position, order.end(), id);
        if (it != order.end()) {
            order.erase(it);
            if (position >= (int)order.size() && !order.empty())
                position = 0;
        }
    }
};

BombShuffleBag sBombShuffleBag;

// Simple sphere overlap test for bomb contact detection on the server.
// Uses player body-part sphere positions if available, otherwise root position.
bool bombSphereOverlap(const glm::vec3& aPos, float aRadius,
                       const glm::vec3& bPos, float bRadius)
{
    float dist = glm::length(aPos - bPos);
    return dist < (aRadius + bRadius);
}

// Get a rough position for an entity (root position for players, body.pos for NPCs).
glm::vec3 getEntityRootPos(const ServerPlayer& p) {
    return p.pos;
}

glm::vec3 getEntityRootPos(const ServerNpc& n) {
    return n.pos;
}

// Find the bomb holder's world position from server state.
glm::vec3 getBombHolderPosition(const ServerGamemodeState& d,
                                const std::unordered_map<uint32_t, ServerPlayer>& players,
                                const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (d.bombOwnerType == 1 /* player */) {
        auto it = players.find(d.bombOwnerPlayerId);
        if (it != players.end()) return getEntityRootPos(it->second);
    } else if (d.bombOwnerType == 2 /* npc */) {
        auto it = npcs.find((uint32_t)d.bombOwnerNpcIndex);
        if (it != npcs.end()) return getEntityRootPos(it->second);
    }
    return glm::vec3(0.0f);
}

// Select a new bomb holder using the shuffle bag.
void selectNewBombHolder(ServerGamemodeState& d,
                         const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    std::vector<uint32_t> eligible;
    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active && !kv.second.dead)
            eligible.push_back(kv.first);
    }
    if (eligible.empty()) {
        d.bombOwnerType = 0;
        d.bombOwnerPlayerId = 0;
        d.bombOwnerNpcIndex = 0;
        return;
    }
    // Check if current bag is valid for current eligible set
    bool needRebuild = sBombShuffleBag.order.empty();
    if (!needRebuild) {
        // Check if all remaining bag entries are still eligible
        for (uint32_t id : sBombShuffleBag.order) {
            bool found = false;
            for (uint32_t e : eligible) {
                if (e == id) { found = true; break; }
            }
            if (!found) { needRebuild = true; break; }
        }
    }
    if (needRebuild) {
        sBombShuffleBag.build(eligible);
    }
    uint32_t chosen = sBombShuffleBag.next();
    if (chosen == 0) {
        // Fallback: random pick
        chosen = eligible[(uint32_t)std::rand() % eligible.size()];
    }
    d.bombOwnerType = 1;
    d.bombOwnerPlayerId = chosen;
    d.bombOwnerNpcIndex = 0;

    Debug::log(Debug::Category::Duel,
        "[BOMB TAG] bomb assigned to player=%u eligible=%zu\n",
        chosen, eligible.size());
}

// Broadcast bomb tag state to all active clients.
void broadcastBombTagState(SOCKET sock,
                           ServerGamemodeState& d,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint64_t& totalPacketsOut)
{
    BombTagStatePacket pkt{};
    pkt.header.type = PACKET_BOMB_TAG_STATE;
    pkt.header.tick = 0;
    pkt.duelId = d.duelId;
    pkt.stateVersion = d.stateVersion;
    pkt.phase = d.phase;
    pkt.bombOwnerType = d.bombOwnerType;
    pkt.bombOwnerPlayerId = d.bombOwnerPlayerId;
    pkt.bombOwnerNpcIndex = d.bombOwnerNpcIndex;
    pkt.timerTicksRemaining = d.bombTimerTicks;
    pkt.inactiveTicksRemaining = d.bombInactiveTicks;
    pkt.serverTick = d.currentServerTick;

    glm::vec3 bombPos = getBombHolderPosition(d, players, {});
    pkt.bombPosX = bombPos.x;
    pkt.bombPosY = bombPos.y;
    pkt.bombPosZ = bombPos.z;

    // Objective bomb modes (Counter-Strike) reuse this packet for their state.
    if (d.objectiveRounds) {
        pkt.objectiveState = d.objectiveBombState;
        pkt.bombOwnerType = (d.objectiveBombState == BOMB_OBJ_CARRIED &&
                             d.objectiveBombCarrierId != 0)
            ? BOMB_OWNER_PLAYER : BOMB_OWNER_NONE;
        pkt.bombOwnerPlayerId = d.objectiveBombCarrierId;
        pkt.bombOwnerNpcIndex = 0;
        pkt.inactiveTicksRemaining = 0;
        pkt.timerTicksRemaining = (uint32_t)std::max(0.0f, d.objectiveBombTimer * 60.0f);
        pkt.bombPosX = d.objectiveBombPos.x;
        pkt.bombPosY = d.objectiveBombPos.y;
        pkt.bombPosZ = d.objectiveBombPos.z;
        if (d.objectiveBombPlantSeconds > 0.0f)
            pkt.plantPercent = (uint8_t)std::min(100.0f,
                d.objectiveBombPlantProgress / d.objectiveBombPlantSeconds * 100.0f);
        if (d.objectiveBombDefuseSeconds > 0.0f)
            pkt.defusePercent = (uint8_t)std::min(100.0f,
                d.objectiveBombDefuseProgress / d.objectiveBombDefuseSeconds * 100.0f);
    }

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        Debug::log(Debug::Category::Duel,
            "[BOMB TAG] sent state player=%u phase=%u owner=%u timer=%u inactive=%u\n",
            kv.second.id, d.phase, d.bombOwnerPlayerId, d.bombTimerTicks, d.bombInactiveTicks);
    }
}

// Broadcast pass visualization event to all clients.
void broadcastBombTagPass(SOCKET sock,
                          ServerGamemodeState& d,
                          const std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint32_t oldOwnerPlayerId, uint32_t newOwnerPlayerId,
                          const glm::vec3& oldPos, const glm::vec3& newPos,
                          float passDist, float rewoundDist,
                          uint64_t& totalPacketsOut)
{
    BombTagPassEventPacket pkt{};
    pkt.header.type = PACKET_BOMB_TAG_PASS_EVENT;
    pkt.header.tick = 0;
    pkt.eventId = nextReliableGameplayEventId();
    pkt.eventSessionId = serverReliableEventSessionId();
    pkt.serverTick = d.currentServerTick;
    pkt.oldOwnerPlayerId = oldOwnerPlayerId;
    pkt.newOwnerPlayerId = newOwnerPlayerId;
    pkt.oldBombPosX = oldPos.x;
    pkt.oldBombPosY = oldPos.y;
    pkt.oldBombPosZ = oldPos.z;
    pkt.newBombPosX = newPos.x;
    pkt.newBombPosY = newPos.y;
    pkt.newBombPosZ = newPos.z;
    pkt.passDistance = passDist;
    pkt.serverRewoundDistance = rewoundDist;
    pkt.accepted = 1;

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), pkt.eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
    }
    ++d.bombPassCounter;
    Debug::log(Debug::Category::Duel,
        "[BOMB TAG PASS] old=%u new=%u dist=%.2f rewound=%.2f passes=%u\n",
        oldOwnerPlayerId, newOwnerPlayerId, passDist, rewoundDist, d.bombPassCounter);
}

} // anonymous namespace

void serverBombTagStartMatch(bool skipIntermission)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;

    // Load bomb tag gamemode config using the gamemode_id from the JSON.
    const Gamemode& gm = GamemodeRegistry::instance().get("bombtag");
    d.bombTimerTicksMax = (uint32_t)(gm.bombTimerTicks > 0 ? gm.bombTimerTicks : 900);
    d.bombInactiveTicksMax = (uint32_t)(gm.inactiveTicks > 0 ? gm.inactiveTicks : 60);
    d.bombBlinkTicks = (uint32_t)(gm.blinkTicks > 0 ? gm.blinkTicks : 30);
    d.bombMaxPassSanityDist = gm.maxPassSanityDistance > 0.0f ? gm.maxPassSanityDistance : 3.0f;
    d.bombTagActive = true;
    d.bombPassCounter = 0;
    d.bombExplosionCounter = 0;
    d.bombTimerTicks = d.bombTimerTicksMax;
    d.bombInactiveTicks = 0;
    d.bombOwnerType = 0;
    d.bombOwnerPlayerId = 0;
    d.bombOwnerNpcIndex = 0;

    // Standard match lifecycle
    d.countdownSeconds = gm.countdownSeconds;
    d.goSeconds = gm.goSeconds;
    d.intermissionSeconds = (float)gm.intermissionSeconds;
    d.resultsSeconds = (float)gm.resultsSeconds;
    d.timeLimitSeconds = 0;  // infinite
    d.mapOnly = false;
    d.lastBroadcastTick = 0;
    d.stateBroadcastPending = true;
    d.phase = DUEL_PHASE_INTERMISSION;
    d.phaseTimer = skipIntermission ? 0.0f : d.intermissionSeconds;
    d.matchOver = false;
    d.spawnOffsetRadius = gm.spawnOffsetRadius;
    ++d.stateVersion;
    ++d.duelId;

    Debug::warn(Debug::Category::Duel,
        "[BOMB TAG] match starting timerTicks=%u inactiveTicks=%u blinkTicks=%u sanityDist=%.1f\n",
        d.bombTimerTicksMax, d.bombInactiveTicksMax, d.bombBlinkTicks, d.bombMaxPassSanityDist);
}

void serverBombTagTick(SOCKET sock,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       HeadlessWorld& world,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       NpcSystem& npcSystem,
                       uint32_t tick,
                       uint64_t& totalPacketsOut)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled || !d.bombTagActive) return;
    d.currentServerTick = tick;

    // ── Handle pending kill from explosion ───────────────────────────
    if (d.hasPendingKill)
    {
        d.hasPendingKill = false;
        const uint32_t victimId = d.pendingVictimId;
        // Instant respawn at spawn point
        auto victimIt = players.find(victimId);
        if (victimIt != players.end()) {
            victimIt->second.respawnSeconds = 0.0f;
            victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d);
        }
    }

    // ── State machine ────────────────────────────────────────────────
    switch (d.phase) {
    case DUEL_PHASE_WAITING:
        if (countActivePlayers(players) >= 1) {
            // Start bomb tag with available players
            if (world.spawnPoints.empty())
                assignGamemodeSpawns(d, world);
            assignMatchParticipants(d, players, &npcs);
            beginMatchCountdown(d, players, npcs, npcSystem, tick);
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
            Debug::warn(Debug::Category::Duel,
                "[BOMB TAG] countdown started participants=%zu\n", d.participants.size());
        }
        break;

    case DUEL_PHASE_COUNTDOWN:
        if (tick >= d.matchStartTick) {
            d.phase = DUEL_PHASE_GO;
            d.phaseTimer = d.goSeconds;
            ++d.stateVersion;
            d.lastBroadcastTick = tick;
            Debug::log(Debug::Category::Duel,
                "[BOMB TAG] GO shown tick=%u\n", tick);
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_GO:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            d.phase = DUEL_PHASE_ACTIVE;
            d.matchStartTick = tick;
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            // Select first bomb holder
            selectNewBombHolder(d, players);
            resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
            ++d.stateVersion;
            d.lastBroadcastTick = tick;
            Debug::warn(Debug::Category::Duel,
                "[BOMB TAG] ACTIVE tick=%u holder=%u timerTicks=%u\n",
                tick, d.bombOwnerPlayerId, d.bombTimerTicks);
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_ACTIVE:
    {
        // ── Bomb holder disconnected or died? Transfer immediately ───
        if (d.bombOwnerType == 1 && d.bombOwnerPlayerId != 0) {
            auto holderIt = players.find(d.bombOwnerPlayerId);
            if (holderIt == players.end() || holderIt->second.spawnState != ServerPlayer::Active
                || holderIt->second.dead) {
                Debug::warn(Debug::Category::Duel,
                    "[BOMB TAG] holder lost id=%u — transferring\n",
                    d.bombOwnerPlayerId);
                d.bombOwnerType = 0;
                d.bombOwnerPlayerId = 0;
                d.bombInactiveTicks = 0;
                selectNewBombHolder(d, players);
                ++d.stateVersion;
                broadcastBombTagState(sock, d, players, totalPacketsOut);
            }
        }

        // ── Bomb timer countdown ──────────────────────────────────
        if (d.bombTimerTicks > 0)
            --d.bombTimerTicks;

        // ── Inactive grace period ─────────────────────────────────
        if (d.bombInactiveTicks > 0)
            --d.bombInactiveTicks;

        // ── Bomb explosion ────────────────────────────────────────
        if (d.bombTimerTicks == 0 && d.bombOwnerType != 0) {
            // Kill the bomb holder
            uint32_t victimId = d.bombOwnerPlayerId;
            auto victimIt = players.find(victimId);
            if (victimIt != players.end() && !victimIt->second.dead) {
                // Apply lethal damage via the normal server damage path
                victimIt->second.health = 0;
                victimIt->second.dead = true;
                victimIt->second.respawnSeconds = serverMatchRespawnsEnabled()
                    ? serverMatchRespawnSeconds() : -1.0f;
                ++victimIt->second.deaths;

                // Credit kill to a random other player (explosion is environment)
                // For bomb tag, we credit no one — bomb explosion is environmental
                d.hasPendingKill = true;
                d.pendingKillerId = 0;
                d.pendingVictimId = victimId;

                Debug::warn(Debug::Category::Duel,
                    "[BOMB TAG] explosion! victim=%u deaths=%u explosions=%u\n",
                    victimId, victimIt->second.deaths, d.bombExplosionCounter + 1);
            }
            ++d.bombExplosionCounter;

            // Select new bomb holder and reset timer
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            selectNewBombHolder(d, players);

            // Respawn the killed player instantly
            if (victimIt != players.end()) {
                victimIt->second.dead = false;
                victimIt->second.health = 100;
                victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d);
                victimIt->second.respawnSeconds = 0.0f;
                beginAuthoritativeTransform(victimIt->second,
                    victimIt->second.duelSpawnPos,
                    SpawnVelocityConfig::instance().enabled()
                        ? SpawnVelocityConfig::instance().computeSpawnImpulse(victimIt->second.yaw)
                        : glm::vec3(0.0f),
                    victimIt->second.yaw, "bomb-respawn");
            }

            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
            break;
        }

        // ── Physical contact detection (bomb pass) ────────────────
        if (d.bombInactiveTicks == 0 && d.bombOwnerType == 1 && !d.matchOver) {
            // Player holds bomb — check contact with other players
            auto holderIt = players.find(d.bombOwnerPlayerId);
            if (holderIt != players.end() && !holderIt->second.dead) {
                glm::vec3 holderPos = getEntityRootPos(holderIt->second);
                float bombRadius = 0.5f;
                float targetRadius = 1.5f;

                for (const auto& kv : players) {
                    if (kv.first == d.bombOwnerPlayerId) continue;
                    if (kv.second.spawnState != ServerPlayer::Active) continue;
                    if (kv.second.dead) continue;

                    glm::vec3 targetPos = getEntityRootPos(kv.second);
                    float dist = glm::length(holderPos - targetPos);

                    // Sanity distance check
                    if (dist > d.bombMaxPassSanityDist) continue;

                    // Physical overlap check
                    if (bombSphereOverlap(holderPos, bombRadius, targetPos, targetRadius)) {
                        // Transfer bomb
                        glm::vec3 oldPos = holderPos;
                        d.bombOwnerType = 1;
                        d.bombOwnerPlayerId = kv.first;
                        d.bombOwnerNpcIndex = 0;
                        d.bombInactiveTicks = d.bombInactiveTicksMax;

                        glm::vec3 newPos = getEntityRootPos(kv.second);
                        broadcastBombTagPass(sock, d, players,
                            d.bombOwnerPlayerId, kv.first,
                            oldPos, newPos, dist, dist, totalPacketsOut);
                        ++d.stateVersion;
                        broadcastBombTagState(sock, d, players, totalPacketsOut);
                        Debug::log(Debug::Category::Duel,
                            "[BOMB TAG] PASS %u -> %u dist=%.2f\n",
                            d.bombOwnerPlayerId, kv.first, dist);
                        break;
                    }
                }
            }
        }

        // ── Periodic state broadcast ──────────────────────────────
        if (tick - d.lastBroadcastTick >= 10) {
            d.lastBroadcastTick = tick;
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;
    }

    case DUEL_PHASE_RESULTS:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            if (d.pendingModeSwitch && !d.pendingGamemodeId.empty()) {
                const std::string nextMode = d.pendingGamemodeId;
                const bool directCountdown = d.pendingModeSwitchCountdown;
                d.pendingModeSwitch = false;
                d.pendingModeSwitchCountdown = false;
                d.pendingGamemodeId.clear();
                d.phase = DUEL_PHASE_WAITING;
                d.matchMode.clear();
                serverCommunityStartMatch(directCountdown, nextMode);
                return;
            }
            d.phase = DUEL_PHASE_INTERMISSION;
            d.phaseTimer = d.intermissionSeconds;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        } else if (tick - d.lastBroadcastTick >= 60) {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_INTERMISSION:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            assignGamemodeSpawns(d, world);
            assignMatchParticipants(d, players, &npcs);
            beginMatchCountdown(d, players, npcs, npcSystem, tick);
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        } else if (tick - d.lastBroadcastTick >= 60) {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    default:
        break;
    }
}

} // namespace MimitaNet
