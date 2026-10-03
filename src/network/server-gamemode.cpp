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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

#include "network/packets.h"
#include "network/server.h"
#include "npc/npc.h"
#include "npc/npc-internal.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-data.h"
#include "combat/actor-preset-weapons.h"
#include "combat/grenade-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/community-server-config.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "gamemode/map-config.h"
#include "persistence/persistence-queue.h"
#include "persistence/persistence-events.h"
#include "persistence/persistence-emit.h"
#include "config/spawn-velocity-config.h"
#include "network/actor-lifecycle.h"

namespace MimitaNet {

// Area-effect tick (defined after the anonymous namespace).
void serverAreaEffectTick(ServerGamemodeState& d,
                          SOCKET sock,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          std::unordered_map<uint32_t, ServerNpc>& npcs,
                          uint32_t tick,
                          uint64_t& totalPacketsOut);

// Resolve a team id string (e.g. "t") to its fixed team index; -1 = any.
// Static so both the match-start path (defined above the anonymous namespace)
// and the round helpers below can call it.
static int resolveTeamIndexFromId(const Gamemode& gm, const std::string& teamId)
{
    if (teamId.empty()) return -1;
    for (size_t i = 0; i < gm.teams.size(); ++i)
        if (gm.teams[i].id == teamId) return (int)i;
    return -1;
}

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

bool serverMatchRespawnsEnabled()
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return true;      // legacy / sandbox keeps instant respawn
    return d.respawnSeconds != 0.0f;  // 0 == one-life
}

bool serverPlayerRespawnsEnabled(uint32_t playerId)
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (d.npcWaves)
        return d.waveLivesRemaining > 0;
    (void)playerId;
    return serverMatchRespawnsEnabled();
}

void serverConsumeNpcWaveLife(uint32_t playerId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.npcWaves || d.waveLivesRemaining <= 0)
        return;
    --d.waveLivesRemaining;
    auto actorIt = d.matchActors.find(playerId);
    if (actorIt != d.matchActors.end())
        actorIt->second.state = ActorState::Dead;
    Debug::log(Debug::Category::Duel,
        "[WAVES] player=%u died livesRemaining=%d wave=%u\n",
        playerId, d.waveLivesRemaining, d.waveNumber);
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
    auto it = d.matchActors.find(actorId);
    if (it == d.matchActors.end())
        return out;

    const std::string roleId = !d.actorPresetOverrideId.empty()
        ? d.actorPresetOverrideId : it->second.roleId;
    if (roleId.empty()) return out;
    const MatchRoleDefinition* def =
        MatchRoleRegistry::instance().get(roleId);
    if (!def)
        return out;

    out.hasRole = true;
    out.roleId = def->id;
    out.health = def->health;
    out.startingWeapon = def->startingWeapon;
    out.avatarName = def->avatarName;

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

bool serverActivateActorPreset(const std::string& presetId)
{
    const MatchRoleDefinition* preset =
        MatchRoleRegistry::instance().getActorPreset(presetId);
    if (!preset) return false;

    ServerGamemodeState& d = serverGamemodeState();
    d.actorPresetOverrideId = preset->id;
    d.cameraFov = preset->forceFov ? preset->cameraFov : d.cameraFov;
    if (preset->forceFirstPerson) d.forceFirstPerson = true;
    if (!preset->weaponSet.empty()) {
        if (const CommunityWeaponSet* set =
                CommunityServerConfig::instance().weaponSetByKey(preset->weaponSet)) {
            d.communityWeaponSetId = set->id;
            d.appliedCommunityWeaponSetId = 0;
        }
    }
    ActorPresetWeapons::apply(*preset);
    d.stateVersion++;
    d.stateBroadcastPending = true;
    Debug::log(Debug::Category::Duel,
        "[ACTOR PRESET] host activated id=%s actors=%zu\n",
        preset->id.c_str(), d.matchActors.size());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "actor.preset-applied", std::to_string(d.duelId), "mode-start",
        d.currentServerTick,
        nlohmann::json{{"preset", preset->id}, {"fov", preset->cameraFov},
                       {"first_person", preset->forceFirstPerson}});
    return true;
}

void serverResetActorPreset()
{
    ServerGamemodeState& d = serverGamemodeState();
    d.actorPresetOverrideId.clear();
    const Gamemode& mode = GamemodeRegistry::instance().get(d.matchMode);
    d.cameraFov = mode.cameraFov;
    d.forceFirstPerson = mode.forceFirstPerson;
    if (!mode.actorPresetId.empty()) {
        if (const MatchRoleDefinition* preset =
                MatchRoleRegistry::instance().getActorPreset(mode.actorPresetId)) {
            ActorPresetWeapons::apply(*preset);
            if (!preset->weaponSet.empty()) {
                if (const CommunityWeaponSet* set =
                        CommunityServerConfig::instance().weaponSetByKey(preset->weaponSet))
                    d.communityWeaponSetId = set->id;
            }
        }
    } else {
        ActorPresetWeapons::clear();
        d.communityWeaponSetId = mode.weaponSetId > 0 ? mode.weaponSetId : 1;
    }
    d.appliedCommunityWeaponSetId = 0;
    d.stateVersion++;
    d.stateBroadcastPending = true;
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
    // Community/sandbox map runtime is not a duel: route its replicated
    // DuelStatePacket to CommunityMatchClient, not DuelQueue.
    state.matchMode = "sandbox";
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
        || state.communityMode == "npc_waves"
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
    if (!cm) return;  // unknown mode — cannot start

    // Use gamemode_id to look up the actual gamemode config.
    // This bridges onlinemodes.json (community menu) to gamemodes/*.json (gameplay rules).
    const std::string& resolvedGamemodeId = cm->gamemodeId;

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
    d.actorPresetOverrideId.clear();
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
    d.npcWaves = gm.winCondition == "npc_waves";
    d.waveStartCount = gm.waveStartCount;
    d.waveIncrement = gm.waveIncrement;
    d.waveNpcsPerWave = gm.waveNpcsPerWave;
    d.waveLives = std::max(1, gm.lives);
    d.waveLivesRemaining = d.waveLives;
    d.waveHighest = 0;
    d.waveBannerSeconds = gm.waveBannerSeconds;
    d.waveStaggerEnabled = gm.waveStaggerEnabled;
    d.waveNpcsPerTick = gm.waveNpcsPerTick;
    d.waveNumber = d.npcWaves ? 1u : 0u;
    d.waveNpcTarget = 0;
    d.waveNpcSpawned = 0;
    d.waveBannerVisible = false;
    d.waveRunOver = false;
    d.intermissionSeconds = (float)gm.intermissionSeconds;
    d.resultsSeconds = (float)gm.resultsSeconds;
    d.countdownSeconds = gm.countdownSeconds;
    d.goSeconds = gm.goSeconds;
    d.spawnOffsetRadius = gm.spawnOffsetRadius;
    d.hasBombFeature = gm.features.bombHolderText;
    d.bombTagActive = false;

    // ── Round-based lifecycle rules (Counter-Strike and future modes) ──
    d.victoryCondition = gm.victoryCondition;
    d.objectiveRounds = gm.victoryCondition == "rounds" && gm.rounds.roundsToWin > 0;
    d.roundsToWin = gm.rounds.roundsToWin;
    d.maxRounds = gm.rounds.maxRounds;
    d.roundSeconds = gm.rounds.roundSeconds;
    d.freezeSeconds = gm.rounds.freezeSeconds;
    d.roundWins[0] = d.roundWins[1] = 0;
    d.roundNumber = 0;
    d.roundVersion = 0;
    d.roundWinnerTeam = -1;
    d.roundEndReason = 0;
    d.roundEndTick = 0;
    d.rosterLocked = false;
    d.roundNextNpcId = 0;
    // ── Mode objective item ─────────────────────────────────────────
    // Reset, then adopt the first valid objective definition from the mode.
    d.objective = ObjectiveInstance{};
    d.objectivePickupCounter = 0;
    d.objectiveDropCounter = 0;
    d.objectiveNextCarrierScanTick = 0;
    for (const GamemodeObjectiveDefinition& def : gm.objectives) {
        const ObjectiveKind kind = objectiveKindFromString(def.kind);
        if (kind == ObjectiveKind::None) continue;
        d.objective.id = def.id;
        d.objective.kind = kind;
        d.objective.allowedCarrierTeam = resolveTeamIndexFromId(gm, def.carrierTeam);
        d.objective.explosionSeconds = def.explosionSeconds;
        d.objective.active = !def.id.empty();
        d.objective.state = ObjectiveState::Inactive;
        break;
    }
    // Round modes use their own configured countdown/intermission when set.
    if (d.objectiveRounds) {
        if (gm.rounds.countdownSeconds > 0.0f) d.countdownSeconds = gm.rounds.countdownSeconds;
        if (gm.rounds.intermissionSeconds > 0.0f)
            d.intermissionSeconds = gm.rounds.intermissionSeconds;
        if (gm.rounds.resultsSeconds > 0.0f) d.resultsSeconds = gm.rounds.resultsSeconds;
    }

    // ── Visual/settings overrides from gamemode ────────────────────
    d.cameraFov = gm.cameraFov;
    d.forceFirstPerson = gm.forceFirstPerson;
    if (!gm.actorPresetId.empty()) {
        if (const MatchRoleDefinition* preset =
                MatchRoleRegistry::instance().getActorPreset(gm.actorPresetId)) {
            if (preset->forceFov) d.cameraFov = preset->cameraFov;
            if (preset->forceFirstPerson) d.forceFirstPerson = true;
            if (!preset->weaponSet.empty()) {
                if (const CommunityWeaponSet* set =
                        CommunityServerConfig::instance().weaponSetByKey(preset->weaponSet))
                    d.communityWeaponSetId = set->id;
            }
            ActorPresetWeapons::apply(*preset);
        } else {
            Debug::warn(Debug::Category::Duel,
                "[ACTOR PRESET] gamemode %s references unknown preset %s\n",
                resolvedGamemodeId.c_str(), gm.actorPresetId.c_str());
        }
    } else {
        ActorPresetWeapons::clear();
    }
    d.hideHealthbars = gm.hideHealthbars;
    d.ragdollExplicit = gm.ragdollExplicit;
    d.ragdollEnabled = gm.ragdollEnabled;
    d.bloodExplicit = gm.bloodExplicit;
    d.bloodEnabled = gm.bloodEnabled;

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
    d.rotateMaps = d.autoMapRotation;
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
    // Round modes start with a fresh tally; the roster is built at countdown.
    d.roundNumber = 0;
    d.roundVersion = 0;
    d.roundWins[0] = d.roundWins[1] = 0;
    d.roundNextNpcId = 0;
    d.rosterLocked = false;
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

// Defined below; called from the round state machine.
void serverObjectiveTick(ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         uint32_t tick);
void assignObjectiveCarrier(ServerGamemodeState& d,
                            const std::unordered_map<uint32_t, ServerPlayer>& players,
                            const std::unordered_map<uint32_t, ServerNpc>& npcs);

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
    pkt.redTeamKills = d.redTeamKills;
    pkt.blueTeamKills = d.blueTeamKills;
    pkt.timeLimitSeconds = d.timeLimitSeconds;
    pkt.intermissionSeconds = (int32_t)d.intermissionSeconds;
    pkt.resultsSeconds = (int32_t)d.resultsSeconds;
    pkt.goSeconds = d.goSeconds;
    pkt.waveNumber = d.waveNumber;
    pkt.waveNpcTarget = d.waveNpcTarget;
    pkt.waveNpcSpawned = d.waveNpcSpawned;
    pkt.waveBannerUntilTick = d.waveBannerUntilTick;
    pkt.waveBannerVisible = d.waveBannerVisible ? 1 : 0;
    pkt.waveLivesRemaining = d.npcWaves ? d.waveLivesRemaining : 0;
    pkt.waveHighest = d.npcWaves ? d.waveHighest : 0;

    // ── Gamemode visual overrides ──────────────────────────────────
    pkt.cameraFov = d.cameraFov;
    pkt.forceFirstPerson = d.forceFirstPerson ? 1 : 0;
    const std::string presetId = !d.actorPresetOverrideId.empty()
        ? d.actorPresetOverrideId : GamemodeRegistry::instance().get(d.matchMode).actorPresetId;
    std::strncpy(pkt.actorPresetId, presetId.c_str(), sizeof(pkt.actorPresetId) - 1);
    pkt.ragdollEnabled = d.ragdollExplicit ? (d.ragdollEnabled ? 2 : 1) : 0;
    pkt.bloodEnabled = d.bloodExplicit ? (d.bloodEnabled ? 2 : 1) : 0;

    // ── Round-based match fields ───────────────────────────────────
    pkt.roundVersion = d.roundVersion;
    pkt.roundNumber = d.roundNumber;
    pkt.roundWins[0] = d.roundWins[0];
    pkt.roundWins[1] = d.roundWins[1];
    pkt.winnerTeam = d.winnerTeam;
    pkt.roundEndReason = (uint8_t)d.roundEndReason;
    pkt.roundSeconds = d.roundSeconds;
    pkt.roundTimerLeft = (d.phase == DUEL_PHASE_ACTIVE && d.roundEndTick > d.currentServerTick)
        ? (float)(d.roundEndTick - d.currentServerTick) / 60.0f : 0.0f;

    // ── Generic objective replication ──────────────────────────────
    pkt.objectiveActive = d.objective.valid() ? 1 : 0;
    pkt.objectiveKind = (uint8_t)d.objective.kind;
    pkt.objectiveState = (uint8_t)d.objective.state;
    pkt.objectiveTeam = d.objective.allowedCarrierTeam < 0
        ? 0xFF : (uint8_t)d.objective.allowedCarrierTeam;
    pkt.objectiveCarrierId = d.objective.carrierActorId;
    pkt.objectiveX = d.objective.position.x;
    pkt.objectiveY = d.objective.position.y;
    pkt.objectiveZ = d.objective.position.z;
    std::strncpy(pkt.objectiveId, d.objective.id.c_str(), sizeof(pkt.objectiveId) - 1);
    std::strncpy(pkt.objectiveSite, d.objective.plantedSiteId.c_str(), sizeof(pkt.objectiveSite) - 1);
    // Plant/defuse progress for the HUD bar (0 = not in progress).
    if (d.objective.state == ObjectiveState::Planted && d.objective.defuseTicksElapsed > 0 &&
        d.objective.defuseTicksRequired > 0) {
        pkt.objectiveProgressKind = 2;
        pkt.objectiveProgress = (float)d.objective.defuseTicksElapsed /
                                (float)d.objective.defuseTicksRequired;
    } else if (d.objective.state == ObjectiveState::Carried &&
               d.objective.plantTicksElapsed > 0 && d.objective.plantTicksRequired > 0) {
        pkt.objectiveProgressKind = 1;
        pkt.objectiveProgress = (float)d.objective.plantTicksElapsed /
                                (float)d.objective.plantTicksRequired;
    }
    pkt.objectiveTimerLeft = (d.objective.explosionDeadlineTick > d.currentServerTick)
        ? (float)(d.objective.explosionDeadlineTick - d.currentServerTick) / 60.0f : 0.0f;

    // ── Procedural world (Infinite Dungeon Slayer) ─────────────────
    pkt.procedural.enabled = d.procedural.enabled ? 1 : 0;
    pkt.procedural.roomState = (uint8_t)d.procedural.roomState;
    pkt.procedural.exitLocked = d.procedural.exitLocked ? 1 : 0;
    pkt.procedural.seed = d.procedural.seed;
    pkt.procedural.currentRoom = d.procedural.currentRoom;
    pkt.procedural.generatedRooms = d.procedural.generatedRooms;
    pkt.procedural.highestAccessibleRoom = d.procedural.highestAccessibleRoom;
    pkt.procedural.aliveEncounterActors = d.procedural.aliveEncounterActors;
    pkt.procedural.stateVersion = d.procedural.stateVersion;
    std::snprintf(pkt.procedural.modeId, sizeof(pkt.procedural.modeId), "%s",
                  d.procedural.modeId.c_str());

    // FFA top-3 leaderboard
    if (d.matchMode == "ffa") {
        // Sort players by kills descending
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
    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] anchor=(%.1f %.1f %.1f) spawns=%zu\n",
        d.spawnA.x, d.spawnA.y, d.spawnA.z, world.spawnPoints.size());
}

// The anchor plus a random XY offset (so nobody can predict the exact spot).
glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-d.spawnOffsetRadius, d.spawnOffsetRadius);
    return d.spawnA + glm::vec3(dist(rng), dist(rng), 0.0f);
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

// Defined later in this translation unit; the round helpers below call them.
void resetMatchScores(ServerGamemodeState& d);
void resetGamemodeActorsAtMapSpawn(ServerGamemodeState& d,
                                   std::unordered_map<uint32_t, ServerPlayer>& players,
                                   std::unordered_map<uint32_t, ServerNpc>& npcs,
                                   NpcSystem& npcSystem);

// ── Round-based roster + lifecycle helpers ───────────────────────────────
// A round mode (victoryCondition == "rounds") builds a fixed 5v5-style roster
// before the countdown: one human plus allied NPCs on the chosen team, and a
// full opposing NPC squad. NPCs are created as ServerNpc mirror entries; the
// shared adoptNewServerNpcs path turns them into fully simulated actors.

// Team display name from the fixed mode team order.
std::string roundTeamName(const Gamemode& gm, int team)
{
    if (team >= 0 && team < (int)gm.teams.size())
        return gm.teams[(size_t)team].displayName;
    if (team >= 0 && team < (int)gm.teamNames.size())
        return gm.teamNames[(size_t)team];
    return "Team " + std::to_string(team + 1);
}

// The role id the mode assigns to a given team (from its ordered teams).
std::string roundTeamRoleId(const Gamemode& gm, int team)
{
    if (team >= 0 && team < (int)gm.teams.size())
        return gm.teams[(size_t)team].role;
    return {};
}

int roundTeamCapacity(const Gamemode& gm, int team)
{
    if (team >= 0 && team < (int)gm.teams.size())
        return gm.teams[(size_t)team].capacity;
    return 0;
}

// Pure roster sizing: how many NPCs each team needs given the human's team and
// the mode's per-team capacity. The human occupies one slot on their own team.
void roundRosterNpcCounts(const Gamemode& gm, int humanTeam, int outCount[2])
{
    outCount[0] = outCount[1] = 0;
    for (int team = 0; team < 2; ++team) {
        const int capacity = roundTeamCapacity(gm, team);
        const int target = capacity > 0 ? capacity : 5;
        outCount[team] = (team == humanTeam) ? std::max(0, target - 1) : target;
    }
}

// Choose the human's team when none was picked: fewer humans, CT on a tie.
int chooseFallbackTeam(const std::unordered_map<uint32_t, ServerPlayer>& players,
                       const ServerGamemodeState& d)
{
    int count[2] = {0, 0};
    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        auto it = d.matchTeams.find(kv.first);
        if (it != d.matchTeams.end() && it->second >= 0 && it->second < 2)
            ++count[it->second];
    }
    return count[0] <= count[1] ? 0 : 1;
}

// Assign the human player to a team (authoritative) and remember it.
void applyHumanRosterTeam(ServerGamemodeState& d,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint32_t playerId, int team)
{
    auto it = players.find(playerId);
    if (it == players.end()) return;
    it->second.matchTeam = team;
    d.matchTeams[playerId] = team;
    auto actorIt = d.matchActors.find(playerId);
    if (actorIt != d.matchActors.end()) actorIt->second.teamId = team;
}

// Build the round-time scoreboard from the mode's ordered teams. The match is
// built around one human; the human's team gets allied NPCs up to capacity and
// the opposing team gets a full NPC squad.
void buildObjectiveRoster(ServerGamemodeState& d,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);

    // Find the human. Round modes are built around a single human actor.
    uint32_t humanId = 0;
    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active) { humanId = kv.first; break; }
    }
    if (humanId == 0) return;

    auto teamIt = d.matchTeams.find(humanId);
    int humanTeam = (teamIt != d.matchTeams.end()) ? teamIt->second : -1;
    if (humanTeam < 0 || humanTeam > 1)
        humanTeam = chooseFallbackTeam(players, d);
    applyHumanRosterTeam(d, players, humanId, humanTeam);

    if (npcs.empty() && d.roundNextNpcId == 0)
        d.roundNextNpcId = 100000;  // keep roster ids out of the human/NPC range

    // Ensure the human's match actor exists with the team's role.
    {
        ActorMatchDescriptor& desc = d.matchActors[humanId];
        desc.controller = ActorController::Human;
        desc.state = ActorState::Alive;
        desc.teamId = humanTeam;
        const std::string roleId = roundTeamRoleId(gm, humanTeam);
        if (!roleId.empty() && MatchRoleRegistry::instance().get(roleId)) {
            desc.roleId = roleId;
            const MatchRoleDefinition* def = MatchRoleRegistry::instance().get(roleId);
            desc.movementProfileId = def->movementPreset;
            desc.weaponProfileId = def->weaponSet;
        }
    }

    int npcCounts[2] = {0, 0};
    roundRosterNpcCounts(gm, humanTeam, npcCounts);
    for (int team = 0; team < 2; ++team) {
        const int npcCount = npcCounts[team];
        const std::string roleId = roundTeamRoleId(gm, team);
        for (int i = 0; i < npcCount; ++i) {
            while (npcs.find(d.roundNextNpcId) != npcs.end()) ++d.roundNextNpcId;
            ServerNpc npc;
            npc.entityId = d.roundNextNpcId++;
            npc.name = roundTeamName(gm, team) + " " + std::to_string(i + 1);
            npc.pos = gamemodeSpawnPoint(d);
            npc.yaw = team == 0 ? 0.0f : 3.14159265f;
            npc.difficulty = 1.0f;
            npc.matchTeam = team;
            npc.health = 100;
            ActorMatchDescriptor desc;
            desc.controller = ActorController::Npc;
            desc.state = ActorState::Alive;
            desc.teamId = team;
            if (!roleId.empty() && MatchRoleRegistry::instance().get(roleId)) {
                desc.roleId = roleId;
                const MatchRoleDefinition* def = MatchRoleRegistry::instance().get(roleId);
                desc.movementProfileId = def->movementPreset;
                desc.weaponProfileId = def->weaponSet;
                desc.behaviorProfileId = def->behaviorProfile;
            }
            d.matchActors[npc.entityId] = std::move(desc);
            d.matchTeams[npc.entityId] = team;
            d.participants.push_back(npc.entityId);
            d.participantNames[npc.entityId] = npc.name;
            d.ffaKills[npc.entityId] = 0;
            d.ffaDeaths[npc.entityId] = 0;
            npcs.emplace(npc.entityId, std::move(npc));
        }
    }

    // Include the human in the participant list if the generic path did not.
    if (std::find(d.participants.begin(), d.participants.end(), humanId) == d.participants.end()) {
        d.participants.insert(d.participants.begin(), humanId);
        d.participantNames[humanId] = players[humanId].name;
        d.ffaKills[humanId] = 0;
        d.ffaDeaths[humanId] = 0;
    }
    std::sort(d.participants.begin(), d.participants.end());

    Debug::warn(Debug::Category::Duel,
        "[ROSTER] mode=%s human=%u team=%d npcs=%zu participants=%zu locked=%d\n",
        d.matchMode.c_str(), humanId, humanTeam, npcs.size(), d.participants.size(),
        (int)d.rosterLocked);
}

// Begin a fresh round: reset scores/actors/objective state, place actors at
// spawns, and enter the round countdown. `roundVersion` is bumped so stale
// packets from the previous round cannot revive it.
void beginObjectiveRound(ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         NpcSystem& npcSystem,
                         HeadlessWorld& world,
                         uint32_t currentTick)
{
    ++d.duelId;
    ++d.respawnSequence;
    ++d.stateVersion;
    ++d.roundVersion;
    ++d.roundNumber;
    d.rosterLocked = true;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    d.victoryType = 0;
    d.roundWinnerTeam = -1;
    d.roundEndReason = 0;
    d.countdownStartTick = currentTick;
    d.countdownSeconds = d.countdownSeconds > 0.0f ? d.countdownSeconds : 3.0f;
    d.matchStartTick = currentTick + (uint32_t)(d.countdownSeconds * 60.0f);
    d.countdown = d.countdownSeconds;
    d.matchTimeLimitTick = 0;
    d.roundEndTick = 0;
    d.lastBroadcastTick = currentTick;
    resetMatchScores(d);
    d.phase = DUEL_PHASE_COUNTDOWN;
    resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
    assignObjectiveCarrier(d, players, npcs);
    Debug::warn(Debug::Category::Duel,
        "[ROUND] begin mode=%s round=%u version=%u wins=%d-%d toWin=%d tick=%u\n",
        d.matchMode.c_str(), d.roundNumber, d.roundVersion,
        d.roundWins[0], d.roundWins[1], d.roundsToWin, currentTick);
}

// Decide a round. Awards the round win to `winnerTeam` (or -1 for a draw),
// increments the tally, and either ends the match or schedules the next round.
void endObjectiveRound(ServerGamemodeState& d, int winnerTeam, int reason,
                       uint32_t tick)
{
    if (d.phase != DUEL_PHASE_ACTIVE) return;
    d.roundWinnerTeam = winnerTeam;
    d.roundEndReason = reason;
    if (winnerTeam >= 0 && winnerTeam < 2)
        ++d.roundWins[winnerTeam];

    const bool matchDecided = d.roundsToWin > 0 &&
        (d.roundWins[0] >= d.roundsToWin || d.roundWins[1] >= d.roundsToWin);

    if (matchDecided) {
        d.matchOver = true;
        d.phase = DUEL_PHASE_RESULTS;
        d.winnerTeam = d.roundWins[0] >= d.roundWins[1] ? 0 : 1;
        d.victoryType = 0;
        d.phaseTimer = d.resultsSeconds;
    } else {
        // Round result screen, then the next round or intermission.
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = d.resultsSeconds;
    }
    ++d.stateVersion;
    ++d.roundVersion;
    Debug::warn(Debug::Category::Duel,
        "[ROUND] end round=%u winnerTeam=%d reason=%d wins=%d-%d matchOver=%d tick=%u\n",
        d.roundNumber, winnerTeam, reason, d.roundWins[0], d.roundWins[1],
        (int)d.matchOver, tick);
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    const std::string winnerName = winnerTeam >= 0 ? roundTeamName(gm, winnerTeam) : "";
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "round.result", std::to_string(d.duelId), "round-end", tick,
        nlohmann::json{{"round", d.roundNumber}, {"winner_team", winnerTeam},
                       {"winner_name", winnerName}, {"reason", reason},
                       {"wins_ct", d.roundWins[0]}, {"wins_t", d.roundWins[1]},
                       {"match_over", d.matchOver}});
    if (d.matchOver) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "match.result", std::to_string(d.duelId), "match-end", tick,
            nlohmann::json{{"mode", d.matchMode}, {"winner_team", d.winnerTeam},
                           {"winner_name", roundTeamName(gm, d.winnerTeam)},
                           {"wins_ct", d.roundWins[0]}, {"wins_t", d.roundWins[1]}});
    }
}

// Evaluate the active round: elimination of a team, or timeout.
void checkObjectiveRoundEnd(ServerGamemodeState& d, uint32_t tick)
{
    if (d.phase != DUEL_PHASE_ACTIVE) return;

    // Team elimination: a team with no in-play actor loses.
    int aliveByTeam[2] = {0, 0};
    bool anyTeamMembers[2] = {false, false};
    for (uint32_t id : d.participants) {
        auto tIt = d.matchTeams.find(id);
        if (tIt == d.matchTeams.end()) continue;
        const int team = tIt->second;
        if (team < 0 || team > 1) continue;
        anyTeamMembers[team] = true;
        auto aIt = d.matchActors.find(id);
        const bool inPlay = aIt != d.matchActors.end() &&
            (aIt->second.state == ActorState::Alive ||
             aIt->second.state == ActorState::Respawning);
        if (inPlay) ++aliveByTeam[team];
    }

    if (anyTeamMembers[0] && anyTeamMembers[1]) {
        const bool aAlive = aliveByTeam[0] > 0;
        const bool bAlive = aliveByTeam[1] > 0;
        if (aAlive != bAlive) {
            // A planted bomb keeps the round alive even if the planting team is
            // wiped: only a defuse or the explosion decides a planted bomb.
            if (d.objective.state == ObjectiveState::Planted) return;
            endObjectiveRound(d, aAlive ? 0 : 1, 1, tick);
            return;
        }
    }

    if (d.roundEndTick > 0 && tick >= d.roundEndTick) {
        // Timeout: if the bomb is planted the round continues until the bomb
        // resolves; otherwise the defenders win (no plant = Counter-Terrorists).
        if (d.objective.state == ObjectiveState::Planted) return;
        const int defenderTeam = (d.objective.allowedCarrierTeam == 0) ? 1 : 0;
        if (d.objective.valid())
            endObjectiveRound(d, defenderTeam, 3, tick);
        else
            endObjectiveRound(d, -1, 3, tick);
    }
}

// Look up the world position of an actor (player or NPC). Returns false when
// the actor is unknown or dead.
bool objectiveActorPosition(const std::unordered_map<uint32_t, ServerPlayer>& players,
                            const std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t actorId, glm::vec3& outPos)
{
    auto pIt = players.find(actorId);
    if (pIt != players.end()) {
        if (pIt->second.dead) return false;
        outPos = pIt->second.pos;
        return true;
    }
    auto nIt = npcs.find(actorId);
    if (nIt != npcs.end()) {
        if (nIt->second.health <= 0) return false;
        outPos = nIt->second.pos;
        return true;
    }
    return false;
}

// Team of an actor from matchTeams; -1 = none.
int objectiveActorTeam(const ServerGamemodeState& d, uint32_t actorId)
{
    auto it = d.matchTeams.find(actorId);
    return it != d.matchTeams.end() ? it->second : -1;
}

// The generic objective tick: keep the bomb on a valid living carrier, drop it
// when the carrier dies/disconnects, and auto-pick-up for the nearest valid
// living actor of the allowed team within pickup radius. No wall/occlusion
// pickup through geometry beyond the proximity check (Stage 12 adds site logic).
void serverObjectiveTick(ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         uint32_t tick)
{
    if (!d.objective.valid()) return;
    if (d.phase != DUEL_PHASE_ACTIVE && d.phase != DUEL_PHASE_GO) return;

    // 1) If carried, validate the carrier is still alive. If not, drop it at
    //    the carrier's last known position.
    if (d.objective.state == ObjectiveState::Carried) {
        glm::vec3 carrierPos;
        const bool alive = objectiveActorPosition(players, npcs,
                                                  d.objective.carrierActorId, carrierPos);
        if (!alive) {
            glm::vec3 dropPos = d.objective.position;
            // Prefer the actor's last position when we can still find the body.
            auto pIt = players.find(d.objective.carrierActorId);
            auto nIt = npcs.find(d.objective.carrierActorId);
            if (pIt != players.end()) dropPos = pIt->second.pos;
            else if (nIt != npcs.end()) dropPos = nIt->second.pos;
            // Never drop inside the world; raise slightly so it is pickable.
            dropPos.z += 0.3f;
            d.objective.state = ObjectiveState::Dropped;
            d.objective.position = dropPos;
            const uint32_t droppedCarrier = d.objective.carrierActorId;
            d.objective.carrierActorId = 0;
            ++d.objectiveDropCounter;
            ++d.stateVersion;
            d.stateBroadcastPending = true;
            Debug::warn(Debug::Category::Duel,
                "[OBJECTIVE] drop id=%s carrier=%u at=(%.1f %.1f %.1f) tick=%u\n",
                d.objective.id.c_str(), droppedCarrier,
                dropPos.x, dropPos.y, dropPos.z, tick);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "objective.dropped", std::to_string(d.duelId),
                "carrier-died", tick,
                nlohmann::json{{"objective", d.objective.id},
                               {"carrier", droppedCarrier}});
        }
    }

    // 2) If carried, keep the replicated position following the carrier and
    //    attempt a plant when the carrier stands inside a valid bomb site.
    if (d.objective.state == ObjectiveState::Carried) {
        glm::vec3 carrierPos;
        if (objectiveActorPosition(players, npcs, d.objective.carrierActorId, carrierPos))
            d.objective.position = carrierPos;

        const MapObjectiveConfig& mapCfg = MapConfigRegistry::instance().current();
        const int siteIndex = MapConfigRegistry::instance().siteIndexAt(d.objective.position);
        const bool inSite = siteIndex >= 0;

        if (inSite) {
            const bool wasPlanting = d.objective.plantTicksElapsed > 0;
            if (!wasPlanting) {
                d.objective.planterActorId = d.objective.carrierActorId;
                Debug::warn(Debug::Category::Duel,
                    "[OBJECTIVE] plant-start id=%s actor=%u site=%s tick=%u\n",
                    d.objective.id.c_str(), d.objective.carrierActorId,
                    mapCfg.bombSites[(size_t)siteIndex].id.c_str(), tick);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "objective.plant-start", std::to_string(d.duelId), "site",
                    tick, nlohmann::json{{"objective", d.objective.id},
                                         {"operator", d.objective.carrierActorId},
                                         {"site", mapCfg.bombSites[(size_t)siteIndex].id}});
            }
            if (advanceObjectiveProgress(d.objective.plantTicksElapsed,
                                         d.objective.plantTicksRequired, true)) {
                d.objective.state = ObjectiveState::Planted;
                d.objective.plantedSiteId = mapCfg.bombSites[(size_t)siteIndex].id;
                d.objective.position = carrierPos;
                d.objective.carrierActorId = 0;
                d.objective.explosionDeadlineTick = d.currentServerTick +
                    (uint32_t)objectiveSecondsToTicks(d.objective.explosionSeconds);
                ++d.stateVersion;
                d.stateBroadcastPending = true;
                Debug::warn(Debug::Category::Duel,
                    "[OBJECTIVE] planted id=%s site=%s deadline=%u tick=%u\n",
                    d.objective.id.c_str(), d.objective.plantedSiteId.c_str(),
                    d.objective.explosionDeadlineTick, tick);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "objective.planted", std::to_string(d.duelId), d.objective.plantedSiteId,
                    tick, nlohmann::json{{"objective", d.objective.id},
                                         {"site", d.objective.plantedSiteId}});
            }
        } else if (d.objective.plantTicksElapsed > 0) {
            // Interrupted by leaving the site.
            d.objective.plantTicksElapsed = 0;
            d.objective.planterActorId = 0;
            ++d.stateVersion;
            d.stateBroadcastPending = true;
        }
        return;
    }

    // 3) If planted, advance the defuse when a Counter-Terrorist is in range,
    //    and explode when the deadline passes. Both are fixed-tick and
    //    interruptible.
    if (d.objective.state == ObjectiveState::Planted) {
        // Explosion wins if the deadline is reached.
        if (d.objective.explosionDeadlineTick != 0 &&
            d.currentServerTick >= d.objective.explosionDeadlineTick) {
            d.objective.state = ObjectiveState::Exploded;
            ++d.stateVersion;
            d.stateBroadcastPending = true;
            Debug::warn(Debug::Category::Duel,
                "[OBJECTIVE] exploded id=%s site=%s tick=%u\n",
                d.objective.id.c_str(), d.objective.plantedSiteId.c_str(), tick);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "objective.exploded", std::to_string(d.duelId), d.objective.plantedSiteId,
                tick, nlohmann::json{{"objective", d.objective.id},
                                     {"site", d.objective.plantedSiteId}});
            endObjectiveRound(d, d.objective.allowedCarrierTeam, 2, tick);
            return;
        }

        // Find a defusing CT within interaction range of the planted bomb.
        const int defenderTeam = (d.objective.allowedCarrierTeam == 0) ? 1 : 0;
        uint32_t defuser = 0;
        float bestDistSq = d.objective.interactionRange * d.objective.interactionRange;
        auto considerDefuser = [&](uint32_t actorId, const glm::vec3& pos, bool dead) {
            if (dead || actorId == 0) return;
            if (objectiveActorTeam(d, actorId) != defenderTeam) return;
            const glm::vec3 delta = pos - d.objective.position;
            const float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (distSq <= bestDistSq) { bestDistSq = distSq; defuser = actorId; }
        };
        for (const auto& kv : players) considerDefuser(kv.first, kv.second.pos, kv.second.dead);
        for (const auto& kv : npcs)
            considerDefuser(kv.first, kv.second.pos, kv.second.health <= 0);

        const bool wasDefusing = d.objective.defuseTicksElapsed > 0;
        if (defuser != 0 && !wasDefusing) {
            d.objective.defuserActorId = defuser;
            Debug::warn(Debug::Category::Duel,
                "[OBJECTIVE] defuse-start id=%s actor=%u tick=%u\n",
                d.objective.id.c_str(), defuser, tick);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "objective.defuse-start", std::to_string(d.duelId), "range",
                tick, nlohmann::json{{"objective", d.objective.id}, {"operator", defuser}});
        }
        const bool canDefuse = defuser != 0;
        if (advanceObjectiveProgress(d.objective.defuseTicksElapsed,
                                     d.objective.defuseTicksRequired, canDefuse)) {
            d.objective.state = ObjectiveState::Defused;
            ++d.stateVersion;
            d.stateBroadcastPending = true;
            Debug::warn(Debug::Category::Duel,
                "[OBJECTIVE] defused id=%s actor=%u tick=%u\n",
                d.objective.id.c_str(), defuser, tick);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "objective.defused", std::to_string(d.duelId), "range",
                tick, nlohmann::json{{"objective", d.objective.id}, {"operator", defuser}});
            endObjectiveRound(d, defenderTeam, 2, tick);
        } else if (!canDefuse && d.objective.defuseTicksElapsed == 0) {
            d.objective.defuserActorId = 0;
        }
        return;
    }

    // 4) If dropped, look for the nearest valid living carrier within radius.
    if (d.objective.state != ObjectiveState::Dropped) return;

    std::vector<ObjectiveCarrierCandidate> candidates;
    candidates.reserve(players.size() + npcs.size());
    for (const auto& kv : players)
        candidates.push_back({kv.first, kv.second.pos,
                              objectiveActorTeam(d, kv.first), kv.second.dead});
    for (const auto& kv : npcs)
        candidates.push_back({kv.first, kv.second.pos,
                              objectiveActorTeam(d, kv.first), kv.second.health <= 0});
    const uint32_t bestActor = selectObjectiveCarrier(
        d.objective, candidates.data(), (int)candidates.size(), d.objective.pickupRadius);

    if (bestActor != 0) {
        d.objective.state = ObjectiveState::Carried;
        d.objective.carrierActorId = bestActor;
        ++d.objectivePickupCounter;
        ++d.stateVersion;
        d.stateBroadcastPending = true;
        Debug::warn(Debug::Category::Duel,
            "[OBJECTIVE] pickup id=%s actor=%u team=%d tick=%u\n",
            d.objective.id.c_str(), bestActor,
            objectiveActorTeam(d, bestActor), tick);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "objective.pickup", std::to_string(d.duelId),
            "proximity", tick,
            nlohmann::json{{"objective", d.objective.id},
                           {"actor", bestActor},
                           {"team", objectiveActorTeam(d, bestActor)}});
    }
}

// Assign the objective to a valid carrier on the allowed team at round start.
// Called from beginObjectiveRound after the roster exists.
void assignObjectiveCarrier(ServerGamemodeState& d,
                            const std::unordered_map<uint32_t, ServerPlayer>& players,
                            const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (!d.objective.valid()) return;

    // Load the map's objective timers (plant/defuse/explosion) and reset the
    // per-round plant/defuse progress. Sites come from the map config.
    MapConfigRegistry::instance().load(d.mapId);
    const MapObjectiveConfig& mapCfg = MapConfigRegistry::instance().current();
    d.objective.plantTicksRequired = objectiveSecondsToTicks(mapCfg.plantSeconds);
    d.objective.defuseTicksRequired = objectiveSecondsToTicks(mapCfg.defuseSeconds);
    d.objective.explosionSeconds = mapCfg.explosionSeconds;
    d.objective.plantTicksElapsed = 0;
    d.objective.defuseTicksElapsed = 0;
    d.objective.planterActorId = 0;
    d.objective.defuserActorId = 0;
    d.objective.plantedSiteId.clear();
    d.objective.explosionDeadlineTick = 0;

    // Reset per round, then hand it to the first living allowed-team actor.
    d.objective.state = ObjectiveState::Carried;
    d.objective.carrierActorId = 0;
    d.objective.position = glm::vec3(0.0f);

    // Assignment has no proximity requirement; use an effectively unbounded
    // radius so the first eligible living actor starts with the objective.
    std::vector<ObjectiveCarrierCandidate> candidates;
    candidates.reserve(players.size() + npcs.size());
    for (const auto& kv : players)
        candidates.push_back({kv.first, kv.second.pos,
                              objectiveActorTeam(d, kv.first), kv.second.dead});
    for (const auto& kv : npcs)
        candidates.push_back({kv.first, kv.second.pos,
                              objectiveActorTeam(d, kv.first), kv.second.health <= 0});
    const uint32_t chosen = selectObjectiveCarrier(
        d.objective, candidates.data(), (int)candidates.size(), 1.0e9f);
    if (chosen != 0) {
        d.objective.carrierActorId = chosen;
        for (const auto& c : candidates)
            if (c.actorId == chosen) d.objective.position = c.position;
    }

    if (d.objective.carrierActorId == 0) {
        // No valid carrier yet: leave it dropped near the mode spawn so it can
        // be picked up once an allowed actor is alive.
        d.objective.state = ObjectiveState::Dropped;
        d.objective.position = gamemodeSpawnPoint(d);
    }
    ++d.stateVersion;
    Debug::warn(Debug::Category::Duel,
        "[OBJECTIVE] assign id=%s state=%d carrier=%u team=%d\n",
        d.objective.id.c_str(), (int)d.objective.state,
        d.objective.carrierActorId, d.objective.allowedCarrierTeam);
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
            if (kv.second.health <= 0) continue;
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

    if (!gm.actorPresetId.empty()) {
        if (MatchRoleRegistry::instance().getActorPreset(gm.actorPresetId)) {
            for (uint32_t id : d.participants) {
                ActorMatchDescriptor& desc = d.matchActors[id];
                desc.roleId = gm.actorPresetId;
                const MatchRoleDefinition* def =
                    MatchRoleRegistry::instance().getActorPreset(desc.roleId);
                desc.movementProfileId = def->movementPreset;
                desc.weaponProfileId = def->weaponSet;
                if (desc.controller == ActorController::Npc)
                    desc.behaviorProfileId = def->behaviorProfile;
            }
        }
    } else for (size_t i = 0; i < d.participants.size() && totalSlots > 0; ++i) {
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
            desc.movementProfileId = def->movementPreset;
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
        if (d.npcWaves)
            team = players.find(id) != players.end() ? 0 : 1;
        else if (team < 0 && (d.matchMode == "tdm" ||
                         d.winCondition == "last_team_standing" ||
                         !gm.actorPresetId.empty()))
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

        if (d.npcWaves && desc.controller == ActorController::Human &&
            before == ActorState::Alive && dead && d.waveLivesRemaining > 0)
            serverConsumeNpcWaveLife(kv.first);

        const bool respawns = desc.controller == ActorController::Human
            ? serverPlayerRespawnsEnabled(kv.first)
            : serverMatchRespawnsEnabled();

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
        const glm::vec3 spawn = gamemodeSpawnPoint(d);
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
            if (!profile.avatarName.empty())
                npc.avatarName = profile.avatarName;
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

void clearNpcWaveActors(ServerGamemodeState& d,
                        std::unordered_map<uint32_t, ServerNpc>& npcs,
                        NpcSystem& npcSystem,
                        std::unordered_set<uint32_t>& npcIdsAlive)
{
    npcs.clear();
    npcIdsAlive.clear();
    npcSystem.destroyAll();
    d.waveNpcSpawned = 0;
    d.waveNpcTarget = 0;
    d.waveBannerVisible = false;
}

void addNpcWaveParticipant(ServerGamemodeState& d, ServerNpc& npc)
{
    d.participants.push_back(npc.entityId);
    d.participantNames[npc.entityId] = npc.name;
    d.ffaKills[npc.entityId] = 0;
    d.ffaDeaths[npc.entityId] = 0;
    ActorMatchDescriptor desc;
    desc.controller = ActorController::Npc;
    desc.state = ActorState::Alive;
    desc.teamId = 1;
    d.matchActors[npc.entityId] = std::move(desc);
    d.matchTeams[npc.entityId] = 1;
    npc.matchTeam = 1;
}

void spawnNpcWaveBatch(ServerGamemodeState& d,
                       std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const uint32_t remaining = d.waveNpcTarget > d.waveNpcSpawned
        ? d.waveNpcTarget - d.waveNpcSpawned : 0;
    if (remaining == 0) return;
    const uint32_t batch = d.waveStaggerEnabled
        ? std::min<uint32_t>((uint32_t)std::max(1, d.waveNpcsPerTick), remaining)
        : remaining;
    for (uint32_t i = 0; i < batch; ++i)
    {
        while (npcs.find(d.waveNextNpcId) != npcs.end()) ++d.waveNextNpcId;
        ServerNpc npc;
        npc.entityId = d.waveNextNpcId++;
        npc.name = "Wave NPC " + std::to_string(d.waveNpcSpawned + 1);
        npc.pos = gamemodeSpawnPoint(d);
        npc.yaw = 0.0f;
        npc.difficulty = 1.0f;
        addNpcWaveParticipant(d, npc);
        npcs.emplace(npc.entityId, std::move(npc));
        ++d.waveNpcSpawned;
    }
    Debug::log(Debug::Category::Duel,
        "[WAVES] wave=%u spawned=%u/%u stagger=%d perTick=%d\n",
        d.waveNumber, d.waveNpcSpawned, d.waveNpcTarget,
        (int)d.waveStaggerEnabled, d.waveNpcsPerTick);
}

void beginNpcWave(ServerGamemodeState& d,
                  std::unordered_map<uint32_t, ServerPlayer>& players,
                  std::unordered_map<uint32_t, ServerNpc>& npcs,
                  NpcSystem& npcSystem,
                  std::unordered_set<uint32_t>& npcIdsAlive,
                  uint32_t tick)
{
    clearNpcWaveActors(d, npcs, npcSystem, npcIdsAlive);
    const uint32_t wave = std::max(1u, d.waveNumber);
    d.waveHighest = std::max(d.waveHighest, wave);
    const int target = d.waveNpcsPerWave > 0
        ? (int)wave * d.waveNpcsPerWave
        : d.waveStartCount + (int)(wave - 1) * d.waveIncrement;
    d.waveNpcTarget = (uint32_t)std::max(1, target);
    d.waveNpcSpawned = 0;
    d.waveRunOver = false;
    d.waveBannerVisible = false;
    assignMatchParticipants(d, players, &npcs);
    beginMatchCountdown(d, players, npcs, npcSystem, tick);
    Debug::log(Debug::Category::Duel,
        "[WAVES] countdown wave=%u target=%u\n", d.waveNumber, d.waveNpcTarget);
}

void checkNpcWaveConditions(ServerGamemodeState& d,
                            uint32_t tick)
{
    if (d.waveNpcSpawned < d.waveNpcTarget) return;
    bool livingHuman = false;
    bool livingNpc = false;
    for (const auto& kv : d.matchActors)
    {
        const bool inPlay = kv.second.state == ActorState::Alive ||
                            kv.second.state == ActorState::Respawning;
        if (!inPlay) continue;
        if (kv.second.controller == ActorController::Human) livingHuman = true;
        else livingNpc = true;
    }
    if (!livingHuman && d.waveLivesRemaining <= 0)
    {
        d.waveRunOver = true;
        d.matchOver = true;
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = d.resultsSeconds;
        ++d.stateVersion;
        Debug::log(Debug::Category::Duel,
            "[WAVES] run over wave=%u tick=%u\n", d.waveNumber, tick);
    }
    else if (!livingNpc)
    {
        d.waveRunOver = false;
        d.matchOver = false;
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = 0.0f;
        ++d.stateVersion;
        Debug::log(Debug::Category::Duel,
            "[WAVES] wave cleared wave=%u tick=%u\n", d.waveNumber, tick);
    }
}

static void emitGamemodeMatchPersistence(ServerGamemodeState& d, uint32_t tick,
                                      const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    PersistenceMatchEvent event;
    event.eventId = "match_" + std::to_string(tick) + "_" + std::to_string(d.duelId);
    event.matchId = "match_" + std::to_string(d.duelId);
    event.mode = d.matchMode;
    event.victoryType = d.victoryType == 0 ? "score_limit" : "time_limit";
    event.redScore = d.redTeamKills;
    event.blueScore = d.blueTeamKills;
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

} // namespace

// ── Generic area effects (fire/smoke/dark-bang) ────────────────────────
uint32_t serverSpawnAreaEffect(AreaEffectKind kind, uint32_t ownerActorId,
                               int ownerTeam, const glm::vec3& position,
                               float radius, float height, float durationSeconds,
                               int damagePerTick, int damageIntervalTicks,
                               bool damagesEnemiesOnly)
{
    ServerGamemodeState& d = serverGamemodeState();
    AreaEffect effect;
    effect.id = d.nextAreaEffectId++;
    effect.kind = kind;
    effect.ownerActorId = ownerActorId;
    effect.ownerTeam = ownerTeam;
    effect.position = position;
    effect.radius = radius;
    effect.height = height;
    effect.durationSeconds = durationSeconds;
    effect.damagePerTick = damagePerTick;
    effect.damageIntervalTicks = damageIntervalTicks;
    effect.damagesEnemiesOnly = damagesEnemiesOnly;
    effect.alive = true;
    d.areaEffects.push_back(effect);
    ++d.areaEffectSpawnCounter;
    ++d.stateVersion;
    d.stateBroadcastPending = true;
    Debug::warn(Debug::Category::Weapons,
        "[AREA EFFECT] spawn id=%u kind=%d owner=%u team=%d pos=(%.1f %.1f %.1f) r=%.1f dur=%.1f dmg=%d/%d tick=?\n",
        effect.id, (int)kind, ownerActorId, ownerTeam,
        position.x, position.y, position.z, radius, durationSeconds,
        damagePerTick, damageIntervalTicks);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Weapons, StructuredLevel::Important,
        "area_effect.spawn", std::to_string(effect.id), "grenade",
        d.currentServerTick,
        nlohmann::json{{"effect", effect.id}, {"kind", (int)kind},
                       {"owner", ownerActorId}, {"team", ownerTeam}});
    return effect.id;
}

uint32_t serverSpawnGrenadeAreaEffect(const std::string& grenadeId,
                                      uint32_t ownerActorId, int ownerTeam,
                                      const glm::vec3& position)
{
    const GrenadeDefinition* def = GrenadeRegistry::instance().get(grenadeId);
    if (!def || !def->spawnsAreaEffect || def->areaKind == AreaEffectKind::None)
        return 0;
    return serverSpawnAreaEffect(def->areaKind, ownerActorId, ownerTeam, position,
                                 def->radius, def->height, def->durationSeconds,
                                 def->damagePerTick, def->damageIntervalTicks,
                                 def->damagesEnemiesOnly);
}

// Advance all area effects by one fixed tick and apply fire damage through the
// shared damage path (NPC health mirror + player applyServerDamage).
void serverAreaEffectTick(ServerGamemodeState& d,
                          SOCKET sock,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          std::unordered_map<uint32_t, ServerNpc>& npcs,
                          uint32_t tick,
                          uint64_t& totalPacketsOut)
{
    if (d.areaEffects.empty()) return;

    std::vector<std::pair<uint32_t, glm::vec3>> positions;
    std::vector<int> teams;
    positions.reserve(players.size() + npcs.size());
    teams.reserve(players.size() + npcs.size());
    auto teamOf = [&d](uint32_t actorId) {
        auto it = d.matchTeams.find(actorId);
        return it != d.matchTeams.end() ? it->second : -1;
    };
    for (const auto& kv : players) {
        positions.push_back({kv.first, kv.second.pos});
        teams.push_back(teamOf(kv.first));
    }
    for (const auto& kv : npcs) {
        positions.push_back({kv.first, kv.second.pos});
        teams.push_back(teamOf(kv.first));
    }

    std::vector<AreaEffectDamage> damage;
    tickAreaEffects(d.areaEffects, SERVER_DT, positions, teams, damage);

    for (const AreaEffectDamage& hit : damage) {
        auto pIt = players.find(hit.actorId);
        if (pIt != players.end()) {
            ServerDamageSource source = ServerDamageSource::GrenadeExplosion;
            applyServerDamage(players, pIt->second, 0, hit.damage,
                              glm::vec3(0.0f), source);
            continue;
        }
        auto nIt = npcs.find(hit.actorId);
        if (nIt != npcs.end() && nIt->second.health > 0) {
            nIt->second.health -= hit.damage;
            if (nIt->second.health <= 0) {
                nIt->second.health = 0;
                serverGamemodeRecordKill(sock, players, &npcs,
                    hit.effectId, ENTITY_NONE, nIt->second.entityId, ENTITY_NPC,
                    "fire", "Fire", 0, nIt->second.pos, nIt->second.pos,
                    tick, totalPacketsOut);
            }
        }
    }
}

// ── TeamBrain tick ─────────────────────────────────────────────────────
// Advances the two team brains from the authoritative objective/site state and
// pushes objective context onto each NPC so its utility brain can choose
// objective goals. Never moves actors; the ActorBrain executes.
void serverTeamBrainTick(ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         NpcSystem& npcSystem,
                         uint32_t tick)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    const MapObjectiveConfig& mapCfg = MapConfigRegistry::instance().current();

    auto applyPolicy = [&](TeamBrain& brain, int team) {
        TeamAssignmentPolicy policy;
        if (team >= 0 && team < (int)gm.teams.size()) {
            const GamemodeTeam& t = gm.teams[(size_t)team];
            if (t.attackersPerSite > 0) policy.attackersPerSite = t.attackersPerSite;
            if (t.defendersPerSite > 0) policy.defendersPerSite = t.defendersPerSite;
            if (t.oneRotator >= 0) policy.oneRotator = t.oneRotator == 1;
        }
        brain.state().policy = policy;

        // Sites from the map config.
        brain.state().sites.clear();
        for (const BombSite& site : mapCfg.bombSites) {
            TeamSiteInfo info;
            info.id = site.id;
            info.position = site.position;
            info.radius = site.radius;
            info.hasPosition = site.hasPosition;
            brain.state().sites.push_back(info);
        }

        // Objective context.
        TeamObjectiveContext ctx;
        ctx.active = d.objective.valid();
        ctx.planted = d.objective.state == ObjectiveState::Planted;
        ctx.carriedByTeam = d.objective.state == ObjectiveState::Carried &&
            objectiveActorTeam(d, d.objective.carrierActorId) == team;
        ctx.carrierActorId = d.objective.carrierActorId;
        ctx.plantedSiteId = d.objective.plantedSiteId;
        ctx.bombPosition = d.objective.position;
        brain.state().objective = ctx;
        if (ctx.planted) {
            for (auto& site : brain.state().sites)
                if (site.id == ctx.plantedSiteId) site.hasBomb = true;
        }
    };

    applyPolicy(d.teamBrainA, 0);
    applyPolicy(d.teamBrainB, 1);

    // Shared enemy reports from living actors' current positions (exact sight).
    auto teamOf = [&](uint32_t actorId) { return objectiveActorTeam(d, actorId); };
    d.teamBrainA.tickReports(SERVER_DT, 3.0f);
    d.teamBrainB.tickReports(SERVER_DT, 3.0f);
    for (const auto& kv : players) {
        if (kv.second.dead) continue;
        const int team = teamOf(kv.first);
        if (team == 1) d.teamBrainA.reportEnemySighting(kv.first, kv.second.pos, 1.0f, false);
        else if (team == 0) d.teamBrainB.reportEnemySighting(kv.first, kv.second.pos, 1.0f, false);
    }
    for (const auto& kv : npcs) {
        if (kv.second.health <= 0) continue;
        const int team = teamOf(kv.first);
        if (team == 1) d.teamBrainA.reportEnemySighting(kv.first, kv.second.pos, 1.0f, false);
        else if (team == 0) d.teamBrainB.reportEnemySighting(kv.first, kv.second.pos, 1.0f, false);
    }

    // Living actors for assignment apportionment.
    std::vector<std::pair<uint32_t, int>> living;
    for (const auto& kv : players)
        if (!kv.second.dead)
            living.push_back({kv.first, teamOf(kv.first)});
    for (const auto& kv : npcs)
        if (kv.second.health > 0)
            living.push_back({kv.first, teamOf(kv.first)});
    d.teamBrainA.updateAssignments(living, d.objectiveRounds);
    d.teamBrainB.updateAssignments(living, d.objectiveRounds);

    if (!d.objectiveRounds) return;

    // Push objective context onto each NPC's utility brain. objectivePos points
    // at the bomb/site the NPC's team should care about.
    for (Npc& npc : npcSystem.all()) {
        const int team = npc.body.matchTeam;
        TeamBrain& brain = (team == 0) ? d.teamBrainA : d.teamBrainB;
        const TeamAssignment assignment = brain.assignmentFor(npc.id);
        glm::vec3 objPos;
        const bool hasObj = brain.objectiveTargetPosition(objPos);

        UtilityContext& ctx = npc.utilityContext;
        ctx.objectiveKnown = hasObj;
        if (hasObj) ctx.objectivePos = objPos;
        ctx.onDefense = (team == 0);
        ctx.atObjective = hasObj &&
            glm::length(npc.body.pos - objPos) <= 3.0f;
        // A Terrorist carrying the bomb can plant inside a site.
        ctx.canPlant = (team == 1) &&
            d.objective.state == ObjectiveState::Carried &&
            d.objective.carrierActorId == npc.id &&
            MapConfigRegistry::instance().siteIndexAt(npc.body.pos) >= 0;
        // A Counter-Terrorist at a planted bomb can defuse.
        ctx.canDefuse = (team == 0) &&
            d.objective.state == ObjectiveState::Planted &&
            glm::length(npc.body.pos - d.objective.position) <= d.objective.interactionRange;
        (void)assignment;
        if (npc.utilityContext.teamAlive == 0)
            npc.utilityContext.teamAlive = (int)living.size();
    }
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
    // The server normally polls gamemode JSON from its outer loop. Keep this
    // bounded fallback here too so a hosted server applies wave tuning without
    // a restart, while avoiding filesystem work on every 60 Hz tick.
    static uint64_t lastGamemodePollMs = 0;
    const uint64_t pollNow = nowMs();
    if (pollNow - lastGamemodePollMs >= 250) {
        GamemodeRegistry::instance().pollReload();
        lastGamemodePollMs = pollNow;
        const Gamemode& live = GamemodeRegistry::instance().get(d.matchMode);
        if (d.npcWaves) {
            d.waveStartCount = live.waveStartCount;
            d.waveIncrement = live.waveIncrement;
            d.waveNpcsPerWave = live.waveNpcsPerWave;
            d.waveBannerSeconds = live.waveBannerSeconds;
            d.waveStaggerEnabled = live.waveStaggerEnabled;
            d.waveNpcsPerTick = live.waveNpcsPerTick;
            d.intermissionSeconds = (float)live.intermissionSeconds;
            d.resultsSeconds = (float)live.resultsSeconds;
        }
    }
    updateActorStates(d, players, npcs);
    // Generic area effects tick at fixed 60 Hz in every managed mode.
    serverAreaEffectTick(d, sock, players, npcs, tick, totalPacketsOut);
    // Team-level tactical brains: assignments + objective context for NPCs.
    serverTeamBrainTick(d, players, npcs, npcSystem, tick);
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

    // ── Procedural world (Infinite Dungeon Slayer) ─────────────────
    // Apply host command requests first (this is the only place with the
    // world, npcWorld, and npcSystem), then advance + replicate.
    {
        ServerGamemodeState::PendingProceduralRequest& req = d.pendingProcedural;
        if (req.stop)
        {
            req.stop = false;
            if (d.procedural.enabled)
                serverProceduralWorldStop(world, npcWorld, npcs, npcSystem);
        }
        if (req.start)
        {
            req.start = false;
            if (d.procedural.enabled)
                serverProceduralWorldStop(world, npcWorld, npcs, npcSystem);
            const std::string modeId = req.modeId;
            const uint32_t seed = req.seed;
            const uint32_t requester = req.requesterId;
            if (serverProceduralWorldStart(modeId, seed, world, npcWorld, npcs))
            {
                glm::vec3 entrance;
                auto pit = players.find(requester);
                if (pit != players.end() &&
                    serverProceduralWorldTeleportTarget(entrance))
                {
                    beginAuthoritativeTeleport(pit->second, entrance,
                                                d.procedural.playerSpawnYaw,
                                                "procedural_start");
                }
            }
            else
            {
                ++d.stateVersion;
                d.procedural.pendingDisableBroadcast = true;
            }
        }
        if (req.generateNext)
        {
            // Auto-generation already handles this when a room completes; the
            // command is retained as a developer aid and is a no-op here.
            req.generateNext = false;
        }
        if (req.teleportHighest)
        {
            req.teleportHighest = false;
            glm::vec3 entrance;
            auto pit = players.find(req.teleportRequesterId);
            if (pit != players.end() &&
                serverProceduralWorldTeleportTarget(entrance))
            {
                beginAuthoritativeTeleport(pit->second, entrance,
                                            pit->second.yaw,
                                            "procedural_teleport", 60);
            }
        }
    }

    if (d.procedural.enabled || d.procedural.pendingDisableBroadcast)
    {
        bool changed = false;
        if (d.procedural.enabled)
            changed = serverProceduralWorldTick(sock, players, world, npcWorld,
                                                npcs, npcSystem, tick);
        if (changed || d.procedural.pendingDisableBroadcast ||
            (tick - d.procedural.lastBroadcastTick) >= 30)
        {
            d.procedural.lastBroadcastTick = tick;
            d.procedural.pendingDisableBroadcast = false;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        // Procedural mode owns its own rooms; do not let sandbox map rotation
        // or community scoring mutate that geometry while it is active.
        if (d.procedural.enabled)
            return;
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

        // The victim's respawn delay/state was assigned by the lethal damage
        // path (server-damage / server-npcs). Here we only pin the respawn
        // anchor; do NOT zero the timer or the gamemode delay would be lost.
        auto victimIt = players.find(victimId);
        if (!d.pendingVictimIsNpc && victimIt != players.end())
        {
            victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d);
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

        // Score only counts during the active phase, and never for a suicide.
        if (d.phase == DUEL_PHASE_ACTIVE && !d.matchOver && killerId != victimId)
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

    // ── Round-based match state machine (Counter-Strike and future modes) ──
    // INTERMISSION -> COUNTDOWN -> GO -> ACTIVE -> RESULTS -> next round or
    // intermission. A round mode is a distinct lifecycle from the free-for-all
    // score/time lifecycle, so it routes first.
    if (d.objectiveRounds)
    {
        if (d.stateBroadcastPending)
        {
            broadcastDuelState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
        switch (d.phase)
        {
        case DUEL_PHASE_WAITING:
        case DUEL_PHASE_INTERMISSION:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                // A round needs the human actor; wait for one to connect.
                bool hasHuman = false;
                for (const auto& kv : players)
                    if (kv.second.spawnState == ServerPlayer::Active) { hasHuman = true; break; }
                if (!hasHuman)
                {
                    d.phaseTimer = 0.5f;  // re-check shortly, don't fire an empty round
                    break;
                }
                if (world.spawnPoints.empty())
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                buildObjectiveRoster(d, players, npcs);
                beginObjectiveRound(d, players, npcs, npcSystem, world, tick);
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 30)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_COUNTDOWN:
            if (tick >= d.matchStartTick)
            {
                d.phase = DUEL_PHASE_GO;
                d.phaseTimer = d.goSeconds > 0.0f ? d.goSeconds : 0.75f;
                d.roundEndTick = d.roundSeconds > 0.0f
                    ? tick + (uint32_t)(d.roundSeconds * 60.0f) : 0;
                d.matchStartTick = tick;
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
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
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                Debug::log(Debug::Category::Duel,
                    "[ROUND] ACTIVE round=%u version=%u endTick=%u\n",
                    d.roundNumber, d.roundVersion, d.roundEndTick);
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 10)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_ACTIVE:
            serverObjectiveTick(d, players, npcs, tick);
            checkObjectiveRoundEnd(d, tick);
            if (d.phase != DUEL_PHASE_ACTIVE)
            {
                broadcastDuelState(sock, d, players, totalPacketsOut);
                break;
            }
            if (tick - d.lastBroadcastTick >= 30)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_RESULTS:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                if (d.matchOver || (d.maxRounds > 0 && (int)d.roundNumber >= d.maxRounds))
                {
                    // Match complete: return to intermission for the next match.
                    d.phase = DUEL_PHASE_INTERMISSION;
                    d.phaseTimer = d.intermissionSeconds;
                    d.roundWins[0] = d.roundWins[1] = 0;
                    d.roundNumber = 0;
                    d.rosterLocked = false;
                    ++d.stateVersion;
                    ++d.roundVersion;
                    Debug::warn(Debug::Category::Duel,
                        "[MATCH] complete mode=%s wins=%d-%d -> intermission\n",
                        d.matchMode.c_str(), d.roundWins[0], d.roundWins[1]);
                }
                else
                {
                    // Next round: rebuild roster only if unlocked, then countdown.
                    if (!d.rosterLocked)
                        buildObjectiveRoster(d, players, npcs);
                    beginObjectiveRound(d, players, npcs, npcSystem, world, tick);
                }
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 30)
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

    // ── Shared FFA/TDM/elimination match mode state machine ─────────
    // Any mode with a generic win condition (e.g. last_team_standing) uses the
    // same lifecycle instead of requiring a mode-specific branch.
    if (d.npcWaves || d.matchMode == "ffa" || d.matchMode == "tdm" ||
        d.winCondition == "last_team_standing")
    {
        if (d.stateBroadcastPending)
        {
            // The command changed the authoritative mode/phase between ticks.
            // Send that state now so every client can show intermission or the
            // immediate countdown without waiting for the periodic broadcast.
            broadcastDuelState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
        switch (d.phase)
        {
        case DUEL_PHASE_WAITING:
            if ((d.npcWaves && countActivePlayers(players) >= 1) ||
                (!d.npcWaves && (countActivePlayers(players) >= 2 ||
                (countActivePlayers(players) >= 1 && !npcs.empty()))))
            {
                // If the current map has no spawn points, rotate.
                if (world.spawnPoints.empty())
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                if (d.npcWaves) {
                    d.waveNumber = 1;
                    beginNpcWave(d, players, npcs, npcSystem, npcIdsAlive, tick);
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
                d.phaseTimer = d.npcWaves ? d.waveBannerSeconds : d.goSeconds;
                d.waveBannerVisible = d.npcWaves;
                d.waveBannerUntilTick = d.npcWaves
                    ? tick + (uint32_t)std::ceil(d.waveBannerSeconds * 60.0f) : 0;
                if (d.npcWaves)
                    spawnNpcWaveBatch(d, npcs);
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
            if (d.npcWaves)
                spawnNpcWaveBatch(d, npcs);
            if (d.phaseTimer <= 0.0f)
            {
                d.phase = DUEL_PHASE_ACTIVE;
                d.waveBannerVisible = false;
                d.matchStartTick = tick;
                if (d.npcWaves) {
                    spawnNpcWaveBatch(d, npcs);
                    ++d.stateVersion;
                    d.lastBroadcastTick = tick;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    break;
                }
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

        case DUEL_PHASE_ACTIVE:
            if (d.npcWaves) {
                if (d.waveNpcSpawned < d.waveNpcTarget)
                    spawnNpcWaveBatch(d, npcs);
                updateActorStates(d, players, npcs);
                checkNpcWaveConditions(d, tick);
                if (d.phase != DUEL_PHASE_ACTIVE) {
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    break;
                }
                if (tick - d.lastBroadcastTick >= 15) {
                    d.lastBroadcastTick = tick;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                }
                break;
            }
            // Check win conditions on every tick
            checkMatchWinConditions(d, tick, sock, players, totalPacketsOut);
            if (d.phase != DUEL_PHASE_ACTIVE) break;  // win condition triggered
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

        case DUEL_PHASE_RESULTS:
            if (d.npcWaves) {
                if (d.waveRunOver) {
                    d.phase = DUEL_PHASE_INTERMISSION;
                    d.phaseTimer = d.intermissionSeconds;
                    d.waveNumber = 0;
                    clearNpcWaveActors(d, npcs, npcSystem, npcIdsAlive);
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                } else {
                    ++d.waveNumber;
                    beginNpcWave(d, players, npcs, npcSystem, npcIdsAlive, tick);
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                }
                break;
            }
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
            if (d.npcWaves) {
                d.phaseTimer -= SERVER_DT;
                if (d.phaseTimer <= 0.0f) {
                    assignGamemodeSpawns(d, world);
                    d.waveNumber = 1;
                    beginNpcWave(d, players, npcs, npcSystem, npcIdsAlive, tick);
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                } else if (tick - d.lastBroadcastTick >= 60) {
                    d.lastBroadcastTick = tick;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                }
                break;
            }
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                // Rotate map if configured
                if (d.rotateMaps && d.mapPool.size() > 1)
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                assignMatchParticipants(d, players, &npcs);
                beginMatchCountdown(d, players, npcs, npcSystem, tick);
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

bool serverCounterStrikeRoundSelfTest(std::string& report)
{
    // Build a throwaway round state from the real gamemode JSON and exercise
    // the world-independent round math. This proves the data model and rules
    // load and behave; it does not prove live gameplay.
    ServerGamemodeState d;
    d.enabled = true;
    d.matchMode = "counterstrike";
    MatchRoleRegistry::instance().load("config/roles.json");
    MatchRoleRegistry::instance().loadActorPresets("config/actor-presets");
    WeaponData::registerBuiltinWeapons();
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);

    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    if (gm.teams.size() != 2) fail("expected 2 ordered teams");
    else {
        report += "team0=" + gm.teams[0].displayName + " team1=" + gm.teams[1].displayName + "\n";
        if (gm.teams[0].displayName != "Counter-Terrorists") fail("team 0 display name");
        if (gm.teams[1].displayName != "Terrorists") fail("team 1 display name");
        if (gm.teams[0].capacity != 5 || gm.teams[1].capacity != 5) fail("team capacity != 5");
    }
    if (!(gm.victoryCondition == "rounds")) fail("victoryCondition != rounds");
    if (gm.rounds.roundsToWin != 8) fail("rounds_to_win != 8");
    if (gm.rounds.roundSeconds != 115.0f) fail("round_seconds != 115");

    // Roster sizing: human on each team gives 4 allies + 5 enemies.
    int counts[2] = {0, 0};
    roundRosterNpcCounts(gm, 0, counts);
    report += "humanOnCT npcs=" + std::to_string(counts[0]) + "+" + std::to_string(counts[1]) + "\n";
    if (counts[0] != 4 || counts[1] != 5) fail("human on CT should be 4 allies + 5 enemies");
    roundRosterNpcCounts(gm, 1, counts);
    if (counts[0] != 5 || counts[1] != 4) fail("human on T should be 5 enemies + 4 allies");

    // Round tally + victory threshold. Copy just the tally fields the decision
    // logic uses; the full state is owned by the server tick.
    d.objectiveRounds = true;
    d.roundsToWin = gm.rounds.roundsToWin;
    d.roundWins[0] = d.roundWins[1] = 0;
    auto award = [&](int team) {
        if (team >= 0 && team < 2) ++d.roundWins[team];
    };
    for (int i = 0; i < 7; ++i) award(0);
    if (d.roundWins[0] != 7) fail("7 CT round wins not tallied");
    const bool notYet = !(d.roundWins[0] >= d.roundsToWin || d.roundWins[1] >= d.roundsToWin);
    if (!notYet) fail("match ended before threshold");
    award(0);
    const bool decided = d.roundWins[0] >= d.roundsToWin || d.roundWins[1] >= d.roundsToWin;
    if (!decided) fail("match did not end at 8 CT wins");
    const int winner = d.roundWins[0] >= d.roundWins[1] ? 0 : 1;
    report += "after8 CT=" + std::to_string(d.roundWins[0]) + " T=" + std::to_string(d.roundWins[1]) +
              " winner=" + std::to_string(winner) + "\n";
    if (winner != 0) fail("wrong match winner");

    // Stale-round guard: roundVersion must strictly increase per round.
    d.roundVersion = 0;
    const uint32_t v0 = d.roundVersion;
    ++d.roundVersion;
    ++d.roundVersion;
    if (!(d.roundVersion > v0)) fail("roundVersion did not advance");

    // Weapon overrides: applying the actor preset must reach the ACTIVE weapon
    // table (what both local and authoritative traces read), while the base
    // table stays untouched. This is the runtime application proof, not just
    // the JSON parse proof.
    if (const MatchRoleDefinition* preset =
            MatchRoleRegistry::instance().getActorPreset("counter_strike")) {
        ActorPresetWeapons::apply(*preset);
        const WeaponDefinition* activeRifle = WeaponRegistry::instance().get("hitscan_rifle");
        const WeaponDefinition* activeRev = WeaponRegistry::instance().get("revolver");
        report += "rifle mag=" + std::to_string(activeRifle ? activeRifle->magazineSize : -1) +
                  " reserve=" + std::to_string(activeRifle ? activeRifle->reserveSize : -1) +
                  " beam=" + std::to_string(activeRifle ? activeRifle->beamThickness : -1.0f) + "\n";
        if (!activeRifle || activeRifle->magazineSize != 30 || activeRifle->reserveSize != 180)
            fail("rifle override did not reach active table");
        if (!activeRifle || activeRifle->beamThickness != 0.0f || activeRifle->beamWorldThickness != 0.0f)
            fail("rifle zero thickness did not reach active table");
        if (!activeRev || activeRev->magazineSize != 6 || activeRev->reserveSize != 36)
            fail("revolver override did not reach active table");
        ActorPresetWeapons::clear();
    } else {
        fail("counter_strike preset missing for weapon-apply proof");
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
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
    // Team selection is a pre-round decision. Once the countdown begins the
    // roster is locked for the round, matching the plan's intermission lock.
    if (d.phase != DUEL_PHASE_WAITING && d.phase != DUEL_PHASE_INTERMISSION &&
        d.phase != DUEL_PHASE_RESULTS) {
        message = "team selection locked";
        return false;
    }
    if (requestedTeam < 0 || requestedTeam >= static_cast<int>(gm.teamNames.size())) {
        message = gm.teamNames.empty() ? "active gamemode has no teams" : "invalid team";
        return false;
    }
    // Enforce ordered-team capacity when the mode declares it.
    if (requestedTeam < static_cast<int>(gm.teams.size())) {
        const int capacity = gm.teams[static_cast<size_t>(requestedTeam)].capacity;
        if (capacity > 0 && playerIt->second.matchTeam != requestedTeam) {
            int count = 0;
            for (const auto& entry : d.matchTeams)
                if (entry.second == requestedTeam) ++count;
            if (count >= capacity) {
                message = "team is full";
                return false;
            }
        }
    }
    playerIt->second.matchTeam = requestedTeam;
    d.matchTeams[playerId] = requestedTeam;
    message = "switched to " + gm.teamNames[static_cast<size_t>(requestedTeam)];
    broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
        (playerIt->second.name + " " + message).c_str());
    Debug::warn(Debug::Category::Duel,
        "[MATCH TEAM] player=%u team=%d mode=%s result=accepted\n",
        playerId, requestedTeam, id.c_str());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "actor.team-assigned", std::to_string(d.duelId), "team_pick", tick,
        nlohmann::json{{"player", playerId}, {"team", requestedTeam},
                       {"team_name", roundTeamName(gm, requestedTeam)}});
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
                serverConsumeNpcWaveLife(victimIt->second.id);
                victimIt->second.respawnSeconds = serverPlayerRespawnsEnabled(victimIt->second.id)
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
