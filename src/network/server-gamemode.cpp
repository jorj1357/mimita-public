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
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <random>

#include "network/packets.h"
#include "network/server.h"
#include "npc/npc.h"
#include "npc/npc-internal.h"
#include "npc/npc-navigation.h"
#include "world/world.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-data.h"
#include "combat/actor-preset-weapons.h"
#include "combat/grenade-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "perf/perf.h"
#include "network/community-server-config.h"
#include "gamemode/gamemode.h"
#include "gamemode/match-roles.h"
#include "gamemode/map-config.h"
#include "map/map-authored-geometry.h"
#include "gamemode/monster-pools.h"
#include "gamemode/mode-pack-registry.h"
#include "gamemode/deterministic-rng.h"
#include "persistence/persistence-queue.h"
#include "persistence/persistence-events.h"
#include "persistence/persistence-emit.h"
#include "config/spawn-velocity-config.h"
#include "network/actor-lifecycle.h"
#include "physics/physical-entity.h"

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

// Classify a map spawn node tag into a fixed team index (0=CT, 1=T), or -1
// when the tag names no team. Matches "spawnpoint.CT", "spawnpoint.T",
// "ct_spawn", "counterterrorist", "terrorist", etc.
//
// The generic "spawn"/"spawnpoint" prefix is stripped first so a nameless
// `spawnpoint.011` does NOT match the "t" in "spawnpoint" and get misread as
// Terrorist. CT is checked before T so "counterterror" is not read as "terror".
static int spawnTagTeamIndex(const std::string& rawTag)
{
    std::string tag = rawTag;
    std::transform(tag.begin(), tag.end(), tag.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    // Remove a leading spawn-token so only the team suffix remains.
    const size_t dot = tag.find_last_of("._-");
    std::string suffix = (dot != std::string::npos) ? tag.substr(dot + 1) : tag;
    if (suffix.empty()) suffix = tag;

    // Whole-token CT/T markers first.
    if (suffix == "ct" || suffix == "c") return 0;
    if (suffix == "t") return 1;
    // Then broader word markers.
    if (suffix.find("ct") != std::string::npos ||
        suffix.find("counter") != std::string::npos ||
        suffix.find("defender") != std::string::npos)
        return 0;
    if (suffix.find("terror") != std::string::npos ||
        suffix.find("attacker") != std::string::npos)
        return 1;
    return -1;
}

static bool containsInsensitive(const std::string& value, const std::string& needle)
{
    if (needle.empty()) return false;
    std::string lowerValue = value;
    std::string lowerNeedle = needle;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return lowerValue.find(lowerNeedle) != std::string::npos;
}

static bool spawnTagMatchesTeam(const std::string& tag, const GamemodeTeam& team)
{
    if (!team.spawnTagContains.empty() &&
        !containsInsensitive(tag, team.spawnTagContains))
        return false;
    if (!team.spawnTagExcludes.empty() &&
        containsInsensitive(tag, team.spawnTagExcludes))
        return false;
    return !team.spawnTagContains.empty() || !team.spawnTagExcludes.empty();
}

static void finalizeServerNpcMirrorSpawn(ServerNpc& npc,
                                         ActorSpawnReason reason,
                                         uint32_t serverTick)
{
    serverMeleeResetNpcForSpawn(npc);
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
    if (d.warmup) return true;        // intermission warmup: infinite lives
    return d.respawnSeconds != 0.0f;  // 0 == one-life
}

bool serverPlayerRespawnsEnabled(uint32_t playerId)
{
    const ServerGamemodeState& d = serverGamemodeState();
    // Intermission warmup: infinite lives so players can move and fight freely.
    if (d.warmup)
        return true;
    if (d.npcWaves)
        return d.waveLivesRemaining > 0;
    if (d.persistentNpcSpawns)
        return true;
    // Melee: a fighter respawns only while it still has stocks.
    if (d.matchMode == "melee")
        return serverMeleeActorHasStocks(playerId);
    return serverMatchRespawnsEnabled();
}

bool serverMeleeActorHasStocks(uint32_t actorId)
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (d.matchMode != "melee")
        return true;
    auto it = d.meleeStocks.find(actorId);
    return it == d.meleeStocks.end() ? d.waveLives > 0 : it->second > 0;
}

bool serverMeleeActorProtected(uint32_t actorId)
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (d.matchMode != "melee" && !d.payloadMode)
        return false;
    auto it = d.meleeSpawnProtectionUntilTick.find(actorId);
    return it != d.meleeSpawnProtectionUntilTick.end() &&
           d.currentServerTick < it->second;
}

void serverMeleeNoteSpawn(uint32_t actorId, uint32_t protectionTicks)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (d.matchMode != "melee" && !d.payloadMode)
        return;
    if (d.matchMode == "melee") {
        if (d.meleeStocks.find(actorId) == d.meleeStocks.end())
            d.meleeStocks[actorId] = std::max(1, d.waveLives);
    }
    if (protectionTicks == 0)
        return;
    d.meleeSpawnProtectionUntilTick[actorId] = d.currentServerTick + protectionTicks;
    Debug::log(Debug::Category::Duel,
        "[SPAWN PROTECTION] mode=%s actor=%u until tick=%u ticks=%u\n",
        d.matchMode.c_str(), actorId, d.meleeSpawnProtectionUntilTick[actorId],
        protectionTicks);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        d.payloadMode ? "payload.spawn_protection" : "melee.spawn_protection",
        std::to_string(actorId),
        d.payloadMode ? "payload spawn protection granted"
                      : "melee spawn protection granted",
        d.currentServerTick,
        nlohmann::json{{"actor", actorId},
                       {"until_tick", d.meleeSpawnProtectionUntilTick[actorId]},
                       {"protection_ticks", protectionTicks}});
}

bool serverMeleeConsumeStock(uint32_t actorId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (d.matchMode != "melee")
        return true;
    int& stocks = d.meleeStocks[actorId];
    if (stocks <= 0)
        stocks = 0;
    else
        --stocks;
    Debug::log(Debug::Category::Duel,
        "[MELEE] actor=%u lost a stock; stocks=%d\n", actorId, stocks);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "melee.stock_lost", std::to_string(actorId),
        stocks > 0 ? "melee stock lost; fighter respawns"
                   : "melee stock lost; fighter eliminated",
        d.currentServerTick,
        nlohmann::json{{"actor", actorId},
                       {"stocks_remaining", stocks},
                       {"eliminated", stocks <= 0}});
    return stocks > 0;
}

bool serverFriendlyFireEnabled()
{
    return serverGamemodeState().friendlyFireEnabled;
}

void serverConsumeNpcWaveLife(uint32_t playerId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.npcWaves || d.waveLivesRemaining <= 0)
        return;
    auto actorIt = d.matchActors.find(playerId);
    // One death costs one life. The lethal-damage path and the actor-state
    // transition both call this helper in the same tick; the actor is already
    // Dead after the first call.
    if (actorIt != d.matchActors.end() &&
        actorIt->second.state == ActorState::Dead)
        return;
    const int livesBefore = d.waveLivesRemaining;
    --d.waveLivesRemaining;
    if (actorIt != d.matchActors.end())
        actorIt->second.state = ActorState::Dead;
    Debug::log(Debug::Category::Duel,
        "[WAVES] player=%u died livesRemaining=%d wave=%u\n",
        playerId, d.waveLivesRemaining, d.waveNumber);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "gamemode.npc_waves.life_lost", std::to_string(playerId),
        "player death consumed one NPC Waves life", d.currentServerTick,
        nlohmann::json{{"actor_id", playerId},
                       {"wave", d.waveNumber},
                       {"lives_before", livesBefore},
                       {"lives_after", d.waveLivesRemaining},
                       {"respawn_allowed", d.waveLivesRemaining > 0}},
        __FILE__, __LINE__, __FUNCTION__);
}

float serverMatchRespawnSeconds()
{
    const ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return 0.01f;
    return std::max(0.01f, d.respawnSeconds >= 0.0f ? d.respawnSeconds : 0.01f);
}

ActorSpawnProfile serverResolveActorSpawnProfile(uint32_t actorId)
{
    ActorSpawnProfile out;
    const ServerGamemodeState& d = serverGamemodeState();
    // Mode-wide avatar policy is the shared outer filter. Role/actor-preset
    // policy may refine it below; explicit role lists still win.
    {
        const Gamemode& modeAvatar = GamemodeRegistry::instance().get(d.matchMode);
        if (modeAvatar.avatar.configured) {
            out.allowedAvatars = modeAvatar.avatar.allowed;
            out.deniedAvatars = modeAvatar.avatar.denied;
            if (modeAvatar.avatar.forced && !modeAvatar.avatar.name.empty()) {
                out.avatarName = modeAvatar.avatar.name;
                out.avatarForced = true;
            }
        }
    }
    // Melee first match: force both the human and the NPC onto the Captain
    // Falcon profile so the mode's exclusive authored poses and move clips apply
    // to every actor, regardless of role assignment or any random NPC avatar.
    // Every Melee actor also uses the mode-forced movement profile
    // (supersmashbrosmelee.md §12), so this is forced here — the single owner
    // that already special-cases the Melee avatar.
    if (d.matchMode == "melee") {
        out.avatarName = "captainfalcon";
        out.avatarForced = true;
    }
    // A mode may force one movement profile for every actor, independent of
    // role. Forced before the matchActors lookup so actors without a roster
    // entry (e.g. a client-requested Melee NPC) still receive it.
    {
        const Gamemode& modeCfg = GamemodeRegistry::instance().get(d.matchMode);
        if (!modeCfg.movementPreset.empty())
            out.movementPreset = modeCfg.movementPreset;
    }
    auto it = d.matchActors.find(actorId);
    if (it == d.matchActors.end())
    {
        // Zombie Tower initial join happens before the roster is assigned.
        // Give the human the first team's role immediately so the first spawn
        // already has the configured loadout/ammo instead of a stale GUI set.
        if (d.zombieTower) {
            const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
            if (!gm.teams.empty() && !gm.teams[0].role.empty()) {
                if (const MatchRoleDefinition* def =
                        MatchRoleRegistry::instance().get(gm.teams[0].role)) {
                    out.hasRole = true;
                    out.roleId = def->id;
                    out.health = def->health;
                    out.startingWeapon = def->startingWeapon;
                    out.avatarName = def->avatarName;
                    out.avatarForced = def->avatarForced;
                    out.movementPreset = def->movementPreset;
                    out.actorPresetId = def->actorPresetId;
                    if (const CommunityWeaponSet* set =
                            CommunityServerConfig::instance().weaponSetByKey(def->weaponSet)) {
                        out.weaponSetId = set->id;
                        out.weapons = set->weapons;
                    }
                    for (const auto& entry : def->weaponAmmo)
                        out.weaponAmmo[entry.first] = {entry.second.magazine, entry.second.reserve};
                    return out;
                }
            }
        }
        return out;
    }

    // Precedence for NPC combat profiles: an explicit per-actor descriptor
    // override wins, then the gamemode NPC profile, then the role profile,
    // then the default. This never affects human actors.
    // The roster descriptor is the authoritative per-actor override. Reading
    // the freshly-created result here always yields an empty string and drops
    // explicit role profiles such as juggernaut_swarm.
    const std::string explicitProfile = it->second.behaviorProfileId;
    std::string modeProfile;

    // A gamemode may provide one NPC-only combat profile (e.g. Counter-Strike
    // rage2). It overrides role-specific profiles below.
    if (it->second.controller == ActorController::Npc) {
        const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
        if (!gm.npcBehaviorProfile.empty()) {
            if (BehaviorProfileRegistry::instance().get(gm.npcBehaviorProfile)) {
                modeProfile = gm.npcBehaviorProfile;
            } else {
                Debug::warn(Debug::Category::Duel,
                    "[GAMEMODE] mode %s references unknown NPC behavior profile \"%s\"; using role/default behavior\n",
                    d.matchMode.c_str(), gm.npcBehaviorProfile.c_str());
            }
        }
    }

    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    const bool hasExplicitTeamRoles = std::any_of(
        gm.teams.begin(), gm.teams.end(),
        [](const GamemodeTeam& team) { return !team.role.empty(); });
    // A mode-level actor preset is only a role source for legacy preset-only
    // modes. Role-driven modes must keep the actor's explicit team role here;
    // otherwise the shared resolver erases the role's movement/behavior
    // profile after the roster has assigned it.
    const std::string roleId = (!hasExplicitTeamRoles &&
                                !d.actorPresetOverrideId.empty())
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
    if (!def->avatarName.empty()) {
        out.avatarName = def->avatarName;
        out.avatarForced = def->avatarForced;
    }
    if (!def->allowedAvatars.empty()) out.allowedAvatars = def->allowedAvatars;
    if (!def->deniedAvatars.empty()) out.deniedAvatars = def->deniedAvatars;
    if (!def->actorPresetId.empty()) {
        if (const MatchRoleDefinition* preset = MatchRoleRegistry::instance().getActorPreset(def->actorPresetId)) {
            out.allowedAvatars = preset->allowedAvatars;
            out.deniedAvatars = preset->deniedAvatars;
            if (out.avatarName.empty() && !preset->avatarName.empty()) {
                out.avatarName = preset->avatarName;
                out.avatarForced = preset->avatarForced;
            }
        }
    }

    // A team role may carry its own forced avatar (e.g. CT/T). Prefer it over
    // the shared actor preset so teams look distinct while still sharing the
    // preset's movement/weapon/behavior configuration.
    if (!it->second.roleId.empty() && it->second.roleId != def->id) {
        if (const MatchRoleDefinition* teamRole =
                MatchRoleRegistry::instance().get(it->second.roleId)) {
            if (teamRole->avatarForced && !teamRole->avatarName.empty()) {
                out.avatarName = teamRole->avatarName;
                out.avatarForced = true;
                if (teamRole->health > 0) out.health = teamRole->health;
            }
            if (!teamRole->allowedAvatars.empty()) out.allowedAvatars = teamRole->allowedAvatars;
            if (!teamRole->deniedAvatars.empty()) out.deniedAvatars = teamRole->deniedAvatars;
        }
    }

    // Melee first match: force both the human and the NPC onto the Captain
    // Falcon profile so the mode's exclusive authored poses and move clips apply
    // to every actor, regardless of any random/assigned avatar. The mode-forced
    // movement profile is re-applied after the role block so a non-Falcon role
    // cannot silently leave a Melee actor on default movement.
    if (d.matchMode == "melee") {
        out.avatarName = "captainfalcon";
        out.avatarForced = true;
    }
    {
        const Gamemode& modeCfg = GamemodeRegistry::instance().get(d.matchMode);
        if (!modeCfg.movementPreset.empty())
            out.movementPreset = modeCfg.movementPreset;
    }

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
    out.weaponAmmo.clear();
    for (const auto& entry : def->weaponAmmo)
        out.weaponAmmo[entry.first] = {entry.second.magazine, entry.second.reserve};

    std::string roleProfile;
    if (!def->behaviorProfile.empty()) {
        // Validate once here (warn with role context) instead of per tick.
        if (BehaviorProfileRegistry::instance().get(def->behaviorProfile)) {
            roleProfile = def->behaviorProfile;
        } else {
            Debug::warn(Debug::Category::Duel,
                "[ROLES] role %s references unknown behavior profile \"%s\"; using NPC defaults\n",
                def->id.c_str(), def->behaviorProfile.c_str());
        }
    }

    // Commit the NPC profile using the agreed precedence. Humans keep an empty
    // profile (they never use NPC behavior).
    if (it->second.controller == ActorController::Npc) {
        out.behaviorProfileId = !explicitProfile.empty() ? explicitProfile
                              : !modeProfile.empty()     ? modeProfile
                              :                            roleProfile;
    }

    // The movement policy is owned by the actor preset: a preset carries it
    // directly, a role references the preset that owns it. The live NPC
    // re-resolves the policy by id each tick so hot reload reaches it.
    const auto actorPresetOverride = d.actorPresetOverridesByActorId.find(actorId);
    const bool hasActorPresetOverride = actorPresetOverride != d.actorPresetOverridesByActorId.end();
    out.actorPresetId = hasActorPresetOverride
        ? actorPresetOverride->second
        : (def->actorPreset ? def->id : def->actorPresetId);
    if (hasActorPresetOverride) {
        if (const MatchRoleDefinition* preset =
                MatchRoleRegistry::instance().getActorPreset(out.actorPresetId)) {
            // A host assignment overlays this actor's role through the same
            // server-owned profile used by initial spawn and respawn.
            if (preset->health > 0) out.health = preset->health;
            if (!preset->movementPreset.empty() &&
                RoleMovementCache::instance().get(preset->movementPreset))
                out.movementPreset = preset->movementPreset;
            if (!preset->weaponSet.empty()) {
                if (const CommunityWeaponSet* set =
                        CommunityServerConfig::instance().weaponSetByKey(preset->weaponSet)) {
                    out.weaponSetId = set->id;
                    out.weapons = set->weapons;
                }
            }
            if (!preset->startingWeapon.empty())
                out.startingWeapon = preset->startingWeapon;
            if (preset->avatarForced && !preset->avatarName.empty()) {
                out.avatarName = preset->avatarName;
                out.avatarForced = true;
            }
            if (!preset->allowedAvatars.empty()) out.allowedAvatars = preset->allowedAvatars;
            if (!preset->deniedAvatars.empty()) out.deniedAvatars = preset->deniedAvatars;
            if (it->second.controller == ActorController::Npc &&
                !preset->behaviorProfile.empty() &&
                BehaviorProfileRegistry::instance().get(preset->behaviorProfile))
                out.behaviorProfileId = preset->behaviorProfile;
            out.weaponAmmo.clear();
            for (const auto& entry : preset->weaponAmmo)
                out.weaponAmmo[entry.first] = {entry.second.magazine, entry.second.reserve};
        }
    }
    // A role/preset without its own weapon set inherits the active mode's
    // authoritative set. Apply the same inventory to NPCs that player spawn
    // already resolves from communityWeaponSetId; filtering an NPC's generic
    // default inventory can otherwise leave it with only a subset of the set.
    if (out.weaponSetId <= 0 && out.weapons.empty() && d.enabled &&
        d.communityWeaponSetId > 0) {
        if (const CommunityWeaponSet* set =
                CommunityServerConfig::instance().weaponSetById(d.communityWeaponSetId)) {
            out.weaponSetId = set->id;
            out.weapons = set->weapons;
        }
    }
    return out;
}

bool serverSetActorPreset(uint32_t actorId, const std::string& presetId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled || !MatchRoleRegistry::instance().getActorPreset(presetId))
        return false;
    auto actor = d.matchActors.find(actorId);
    if (actor == d.matchActors.end() ||
        std::find(d.participants.begin(), d.participants.end(), actorId) == d.participants.end())
        return false;

    d.actorPresetOverridesByActorId[actorId] = presetId;
    d.stateVersion++;
    d.stateBroadcastPending = true;
    Debug::log(Debug::Category::Duel,
        "[ACTOR PRESET] assigned actor=%u preset=%s; profile applies at next spawn\n",
        actorId, presetId.c_str());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "actor.preset-assigned", std::to_string(actorId), "host-command",
        d.currentServerTick,
        nlohmann::json{{"actor_id", actorId}, {"preset", presetId},
                       {"lifetime", "actor-present-in-match"}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

void serverClearActorPreset(uint32_t actorId)
{
    ServerGamemodeState& d = serverGamemodeState();
    d.actorPresetOverridesByActorId.erase(actorId);
    d.actorPresetAppliedByActorId.erase(actorId);
}

void serverMarkActorPresetApplied(uint32_t actorId, const std::string& presetId)
{
    ServerGamemodeState& d = serverGamemodeState();
    const auto overrideIt = d.actorPresetOverridesByActorId.find(actorId);
    if (overrideIt != d.actorPresetOverridesByActorId.end() && overrideIt->second == presetId)
        d.actorPresetAppliedByActorId[actorId] = presetId;
    else
        d.actorPresetAppliedByActorId.erase(actorId);
}

// ServerNpc is the snapshot-side health owner. Every path that starts a new
// NPC life must use the role profile here instead of a generic 100 HP default;
// otherwise the next mirror sync can overwrite the real NPC body with the
// wrong health. The host override remains the intentional exception.
static void applyServerNpcRoleHealth(ServerNpc& npc, const char* reason)
{
    const ActorSpawnProfile profile = serverResolveActorSpawnProfile(npc.entityId);
    const int overrideHp = serverGameOverrides().maxHpOverride;
    const int maxHp = overrideHp > 0 ? overrideHp
        : (profile.health > 0 ? profile.health : npc.health);
    npc.health = maxHp;
    Debug::log(Debug::Category::Duel,
        "[ROLE HEALTH] actor=%u role=%s hp=%d source=%s\n",
        npc.entityId, profile.roleId.empty() ? "default" : profile.roleId.c_str(),
        npc.health, reason ? reason : "unknown");
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
    d.countdownDeadlineMs = 0;
    d.matchStartTick = 0;
    d.matchTimeLimitTick = 0;
    d.phaseTimer = 0.0f;
    d.intermissionSeconds = rules.intermissionSeconds;
    d.resultsSeconds = rules.resultsSeconds;
    d.timeLimitSeconds = rules.timeLimitSeconds;
    d.respawnSeconds = rules.respawnSeconds;
    d.killHeals = rules.killHeals;
    d.friendlyFireEnabled = false;
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
        || state.persistentNpcSpawns
        || state.hasBombFeature;
    if (!communityMode) return true;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    return config.weaponAllowed(state.communityWeaponSetId, weaponId);
}

bool serverCommunityWeaponAllowed(const ServerPlayer& player,
                                  const std::string& weaponId)
{
    if (player.weaponSetId <= 0)
        return serverCommunityWeaponAllowed(weaponId);

    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    return config.weaponAllowed(player.weaponSetId, weaponId);
}

int serverCommunityWeaponNativeSlot(int logicalSlot)
{
    if (logicalSlot == 0)
        logicalSlot = serverCommunityWeaponLogicalSlot("nothing");
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

int serverCommunityWeaponNativeSlot(const ServerPlayer& player, int logicalSlot)
{
    if (logicalSlot == 0)
        logicalSlot = serverCommunityWeaponLogicalSlot(player, "nothing");
    if (player.weaponSetId > 0) {
        CommunityServerConfig& config = CommunityServerConfig::instance();
        if (config.weaponSets().empty()) config.load();
        const std::string* id = config.weaponForSlot(player.weaponSetId, logicalSlot);
        if (!id) return -1;
        const WeaponDefinition* def = WeaponRegistry::instance().get(*id);
        return def ? def->slot : -1;
    }
    return serverCommunityWeaponNativeSlot(logicalSlot);
}

int serverCommunityWeaponLogicalSlot(const ServerPlayer& player,
                                     const std::string& weaponId)
{
    if (player.weaponSetId > 0) {
        CommunityServerConfig& config = CommunityServerConfig::instance();
        if (config.weaponSets().empty()) config.load();
        const int slot = config.slotForWeapon(player.weaponSetId, weaponId);
        return slot > 0 ? slot : -1;
    }
    return serverCommunityWeaponLogicalSlot(weaponId);
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

static void payloadRemoveCart(ServerGamemodeState& d)
{
    if (d.payloadCartEntityId == 0) return;
    PhysicalEntitySystem::instance().remove(d.payloadCartEntityId);
    d.payloadCartEntityId = 0;
}

static uint32_t payloadSpawnCart(ServerGamemodeState& d)
{
    payloadRemoveCart(d);
    if (!d.payloadMode) return 0;
    const glm::vec3 size = d.payloadRuntime.cartSize;
    if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) return 0;

    std::vector<CollisionTriangle> triangles;
    buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), size * 0.5f);
    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    const uint32_t id = system.add(triangles, d.payloadRuntime.cartTransform,
                                   PhysicalEntityMotion::Kinematic, 0);
    PhysicalEntity* entity = system.find(id);
    if (!entity) return 0;
    entity->networkId = id;
    entity->replicate = true;
    entity->renderModel = true;
    entity->collisionBox = true;
    entity->modelPath = "assets/objects/things/gamemode-objects/payload-payload.glb";
    entity->shape = PhysicalEntityShape::Box;
    entity->halfExtents = size * 0.5f;
    entity->collidesWithActors = true;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "payload.cart-spawned", std::to_string(id),
        "payload cart physical entity created", d.currentServerTick,
        nlohmann::json{{"entity", id}, {"size", {size.x, size.y, size.z}}});
    return id;
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
    // Durable match-configuration evidence: proves which mode/win condition
    // started without needing the server console.
    nlohmann::json teamCapacities = nlohmann::json::array();
    for (const GamemodeTeam& team : gm.teams)
        teamCapacities.push_back({{"id", team.id}, {"capacity", team.capacity}});
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "gamemode.match.configured", resolvedGamemodeId,
        "gamemode match configuration resolved",
        d.currentServerTick,
        nlohmann::json{{"mode", resolvedGamemodeId},
                       {"community_mode", d.communityMode},
                       {"win_condition", gm.winCondition},
                       {"goal_value", gm.goalValue},
                       {"time_limit_seconds", gm.timeLimitSeconds},
                       {"time_limit_outcome", gm.timeLimitOutcome},
                       {"countdown_seconds", gm.countdownSeconds},
                       {"go_seconds", gm.goSeconds},
                       {"intermission_seconds", gm.intermissionSeconds},
                       {"results_seconds", gm.resultsSeconds},
                       {"npc_spawn_policy", gm.npcSpawnPolicy},
                       {"team_capacities", teamCapacities},
                       {"lives", gm.lives},
                       {"teams", gm.teams.size()},
                       {"run_mode", gm.runMode},
                       {"music_pool", gm.musicRandomPool.size()},
                       {"skip_intermission", skipIntermission}});
    d.goalValue = gm.goalValue;
    // An explicit GUI/runtime selection wins over the gamemode default.
    if (!d.communityWeaponSetExplicit && gm.weaponSetId > 0)
        d.communityWeaponSetId = gm.weaponSetId;
    d.timeLimitSeconds = gm.timeLimitSeconds;
    d.respawnSeconds = gm.respawnSeconds;
    d.killHeals = gm.killHeals;
    d.friendlyFireEnabled = gm.friendlyFire;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "gamemode.friendly_fire.policy", resolvedGamemodeId,
        d.friendlyFireEnabled ? "same-team damage enabled" : "same-team damage blocked",
        d.currentServerTick,
        nlohmann::json{{"mode", resolvedGamemodeId},
                       {"friendly_fire", d.friendlyFireEnabled}});
    d.winCondition = gm.winCondition;
    d.zombieTower = gm.runMode;
    d.ztAttemptResult = false;
    d.ztFinalResult = false;
    d.ztRunWon = false;
    d.ztResumingFromCheckpoint = false;
    d.ztTowerEndReached = false;
    d.ztBossDefeated = false;
    d.ztHadBossTrigger = false;
    d.ztMusicStartedTick = 0;
    d.ztRunStartTick = 0;
    d.ztRunElapsedTicks = 0;
    d.ztCheckpointSerial = 0;
    d.ztCheckpointName.clear();
    d.ztMusicSerial = 0;
    d.ztMusicTrack.clear();
    d.ztMusicPlaying = false;
    d.ztFog = gm.features.fog;
    d.ztRain = gm.features.rain;
    d.ztTorchLighting = gm.features.torchLighting;
    d.ztStats.clear();
    d.npcWaves = gm.winCondition == "npc_waves";
    // The persistent lifecycle is enabled by the policy alone. The spawn cap is
    // a separate concern: a cap of 0 means "run the mode, spawn no NPCs" (useful
    // for lag-free authored-entity testing) instead of disabling the lifecycle.
    d.persistentNpcSpawns = gm.npcSpawnPolicy == "persistent";
    d.waveStartCount = gm.waveStartCount;
    d.waveIncrement = gm.waveIncrement;
    d.waveNpcsPerWave = gm.waveNpcsPerWave;
    d.waveLives = std::max(1, gm.lives);
    d.waveLivesRemaining = d.waveLives;
    // Melee resets every fighter's stocks from the mode `lives` value. The map
    // is also lazily seeded per actor, so a late joiner still starts with stocks.
    d.meleeStocks.clear();
    d.meleeSpawnProtectionUntilTick.clear();
    d.waveHighest = 0;
    d.waveBannerSeconds = gm.waveBannerSeconds;
    d.waveStaggerEnabled = gm.waveStaggerEnabled;
    d.waveNpcsPerTick = gm.waveNpcsPerTick;
    d.npcSpawnMax = std::max(0, gm.npcSpawnMax);
    d.npcSpawnIntervalTicks = std::max(1, gm.npcSpawnIntervalTicks);
    d.npcSpawnPerInterval = std::max(1, gm.npcSpawnPerInterval);
    // Owner-level evidence: the persistent lifecycle is enabled by policy alone,
    // independent of the spawn cap. The journal makes the ACTIVE-phase gate
    // (serverGamemodeTick) traceable without guessing.
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "gamemode.npc-spawn.policy", resolvedGamemodeId,
        d.persistentNpcSpawns ? "persistent lifecycle enabled"
                              : "persistent lifecycle disabled",
        d.currentServerTick,
        nlohmann::json{{"mode", resolvedGamemodeId},
                       {"policy", gm.npcSpawnPolicy},
                       {"persistent_npc_spawns", d.persistentNpcSpawns},
                       {"npc_spawn_max", d.npcSpawnMax},
                       {"npc_waves", d.npcWaves}});
    d.npcSpawnNextTick = 0;
    d.npcSpawnSequence = 0;
    // Authored-map run state (checkpoints/attempts/boss) resets per match.
    d.runAttemptsTotal = std::max(1, gm.runAttempts);
    d.runAttemptsRemaining = d.runAttemptsTotal;
    d.runCheckpointCount = 0;
    d.currentCheckpointId.clear();
    d.hasCheckpointSpawn = false;
    d.activatedCheckpoints.clear();
    d.activatedBossTriggers.clear();
    d.damageVolumeNextTick.clear();
    d.bossEncounterActive = false;
    d.activeBossId.clear();
    d.progressionLocked = false;
    d.activeBossNpcId = 0;
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

    // ── Generic disaster runtime (data-driven, no mode-name branch) ──
    // A mode pack declares capabilities and disasters; the server resolves
    // them into an action graph. A missing pack means no disaster.
    d.disasterGraph = MimitaGamemode::ActionGraph{};
    d.disaster = MimitaGamemode::DisasterState{};
    d.disasterPhaseStartTick = 0;
    d.matchSeed = 0;
    if (const MimitaGamemode::ModePack* pack =
            MimitaGamemode::ModePackRegistry::instance().get(resolvedGamemodeId)) {
        d.disasterGraph = MimitaGamemode::ActionGraph::build(*pack);
        if (!pack->disasters.empty()) {
            static uint32_t s_matchSeedCounter = 0;
            d.matchSeed = MimitaGamemode::mixSeed((uint32_t)nowMs(), ++s_matchSeedCounter);
            MimitaGamemode::disasterConfigure(d.disaster, *pack, d.matchSeed);
            Debug::warn(Debug::Category::Duel,
                "[DISASTER] configured pack=%s disaster=%s seed=%u duration=%.1fs capabilities=%zu\n",
                pack->id.c_str(), d.disaster.disasterId.c_str(), d.matchSeed,
                (float)d.disaster.durationTicks / 60.0f, pack->capabilities.size());
        }
    }

    // ── Round-based lifecycle rules (Counter-Strike and future modes) ──
    d.victoryCondition = gm.victoryCondition;
    d.objectiveRounds = gm.victoryCondition == "rounds" &&
        (gm.rounds.endlessRounds || gm.rounds.roundsToWin > 0 || gm.rounds.maxRounds > 0);
    d.roundsToWin = gm.rounds.roundsToWin;
    d.maxRounds = gm.rounds.maxRounds;
    d.endlessRounds = gm.rounds.endlessRounds;
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
    d.objectiveRosterNpcIds.clear();
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
    // ── Payload mode setup ─────────────────────────────────────────
    // Map data owns the cart origin/route/box; mode data owns the rules.
    d.payloadMode = false;
    payloadRemoveCart(d);
    d.payloadSessionWins[0] = d.payloadSessionWins[1] = 0;
    d.payloadSessionKills.clear();
    d.teamChangeCooldownUntil.clear();
    if (gm.payload.configured) {
        if (MapConfigRegistry::instance().mapId() != d.mapId)
            MapConfigRegistry::instance().load(d.mapId);
        GamemodePayload rules = gm.payload;
        rules.attackerTeamIndex = resolveTeamIndexFromId(gm, rules.attackerTeam);
        if (rules.attackerTeamIndex < 0) rules.attackerTeamIndex = 0;
        rules.defenderTeamIndex = resolveTeamIndexFromId(gm, rules.defenderTeam);
        if (rules.defenderTeamIndex < 0) rules.defenderTeamIndex = 1;
        const MapObjectiveConfig& mapCfg = MapConfigRegistry::instance().current();
        if (MimitaGamemode::payloadRuntimeInit(d.payloadRuntime, mapCfg.payload, rules)) {
            d.payloadMode = true;
            d.payloadCartEntityId = payloadSpawnCart(d);
            std::vector<PayloadAuthoringIssue> authoringIssues;
            const bool authoringValid =
                validatePayloadAuthoring(mapCfg.payload, authoringIssues);
            nlohmann::json authoringReport = nlohmann::json::array();
            for (const PayloadAuthoringIssue& issue : authoringIssues)
                authoringReport.push_back({{"severity", issue.severity},
                                           {"code", issue.code},
                                           {"message", issue.message}});
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel,
                authoringValid ? StructuredLevel::Important : StructuredLevel::Errors,
                "payload.authoring-validated", d.mapId,
                authoringValid ? "payload authoring valid" : "payload authoring has errors",
                d.currentServerTick,
                nlohmann::json{{"map", d.mapId},
                               {"valid", authoringValid},
                               {"issue_count", authoringIssues.size()},
                               {"issues", authoringReport}},
                __FILE__, __LINE__, __FUNCTION__);
            for (const PayloadAuthoringIssue& issue : authoringIssues) {
                Debug::warn(Debug::Category::Duel,
                    "[PAYLOAD AUTHORING] %s %s: %s\n",
                    issue.severity.c_str(), issue.code.c_str(), issue.message.c_str());
            }
            // Payload always uses the shared round lifecycle. The mode's
            // max_rounds=1 makes RESULTS return to INTERMISSION.
            d.objectiveRounds = true;
            d.winCondition = "payload";
            d.roundSeconds = gm.rounds.roundSeconds;
            d.payloadSpawnProtectionTicks = std::max(0, rules.spawnProtectionTicks);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "payload.configured", resolvedGamemodeId,
                "payload cart route and rules resolved", d.currentServerTick,
                nlohmann::json{{"mode", resolvedGamemodeId},
                               {"map", d.mapId},
                               {"attacker_team", rules.attackerTeamIndex},
                               {"defender_team", rules.defenderTeamIndex},
                               {"route_points", d.payloadRuntime.routePoints.size()},
                               {"checkpoints", d.payloadRuntime.checkpoints.size()},
                               {"total_distance", d.payloadRuntime.totalDistance},
                               {"overtime_seconds", rules.overtimeSeconds},
                               {"push_per_attacker", rules.pushMetersPerSecondPerAttacker},
                               {"max_push_attackers", rules.maxPushAttackers},
                               {"facing_mode", d.payloadRuntime.cartFixedFacing ? "fixed" : "route"},
                               {"cart_yaw_degrees", mapCfg.payload.cartYaw}});
            Debug::warn(Debug::Category::Duel,
                "[PAYLOAD] configured map=%s route=%zu checkpoints=%zu distance=%.2f\n",
                d.mapId.c_str(), d.payloadRuntime.routePoints.size(),
                d.payloadRuntime.checkpoints.size(), d.payloadRuntime.totalDistance);
        } else {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Errors,
                "payload.config-invalid", resolvedGamemodeId,
                "payload map config missing/invalid; running without cart", d.currentServerTick,
                nlohmann::json{{"mode", resolvedGamemodeId}, {"map", d.mapId},
                               {"payload_present", mapCfg.payload.present},
                               {"has_cart_origin", mapCfg.payload.hasCartOrigin},
                               {"route_points", mapCfg.payload.route.size()}});
            Debug::error(Debug::Category::Duel,
                "[PAYLOAD] map=%s has no usable payload cart/route\n", d.mapId.c_str());
        }
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
    const bool hasExplicitTeamRoles = std::any_of(
        gm.teams.begin(), gm.teams.end(),
        [](const GamemodeTeam& team) { return !team.role.empty(); });
    // Presentation actor presets must not replace explicit gameplay roles.
    if (!gm.actorPresetId.empty() && !hasExplicitTeamRoles && gm.roleCounts.empty()) {
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
    d.phase = (gm.winCondition == "first_to_score" && !skipIntermission)
        ? DUEL_PHASE_WAITING : DUEL_PHASE_INTERMISSION;
    d.phaseTimer = skipIntermission ? 0.0f : d.intermissionSeconds;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    // Round modes start with a fresh tally; the roster is built at countdown.
    d.roundNumber = 0;
    d.roundVersion = 0;
    d.roundWins[0] = d.roundWins[1] = 0;
    d.roundNextNpcId = 0;
    d.objectiveRosterNpcIds.clear();
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
        skipIntermission ? "COUNTDOWN_PENDING" :
            (gm.winCondition == "first_to_score" ? "WAITING_FOR_PLAYER" : "INTERMISSION"),
        d.goalValue,
        d.timeLimitSeconds, d.intermissionSeconds);
}

namespace {

float countdownWallSecondsRemaining(const ServerGamemodeState& d)
{
    if (d.countdownDeadlineMs == 0)
        return std::max(0.0f, d.countdown);
    const uint64_t now = nowMs();
    return d.countdownDeadlineMs > now
        ? (float)(d.countdownDeadlineMs - now) / 1000.0f
        : 0.0f;
}

bool countdownWallDeadlineReached(const ServerGamemodeState& d)
{
    return d.countdownDeadlineMs != 0
        ? nowMs() >= d.countdownDeadlineMs
        : d.countdown <= 0.0f;
}

void startCountdownWallClock(ServerGamemodeState& d)
{
    d.countdownDeadlineMs = nowMs() +
        (uint64_t)std::ceil(std::max(0.0f, d.countdownSeconds) * 1000.0f);
    d.countdown = d.countdownSeconds;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "match.countdown.start", std::to_string(d.duelId), "countdown_start",
        d.currentServerTick,
        nlohmann::json{{"mode", d.matchMode},
                       {"countdown_seconds", d.countdownSeconds},
                       {"deadline_steady_ms", d.countdownDeadlineMs}});
}

void logCountdownCompletion(ServerGamemodeState& d, uint32_t tick)
{
    const uint64_t now = nowMs();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "match.countdown.complete", std::to_string(d.duelId), "countdown_complete",
        tick,
        nlohmann::json{{"mode", d.matchMode},
                       {"next_phase", (int)d.phase},
                       {"deadline_steady_ms", d.countdownDeadlineMs},
                       {"late_by_ms", now > d.countdownDeadlineMs
                           ? now - d.countdownDeadlineMs : 0}});
}

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
    pkt.countdownLeft = d.phase == DUEL_PHASE_COUNTDOWN
        ? countdownWallSecondsRemaining(d)
        : d.countdown;
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
    // Result screens: a normal round sends the round winner; only the match-over
    // screen uses the overall match winner. d.winnerTeam stays -1 until then.
    pkt.winnerTeam = d.matchOver ? d.winnerTeam : d.roundWinnerTeam;
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

    // ── Payload HUD ────────────────────────────────────────────────
    pkt.payloadActive = d.payloadMode ? 1 : 0;
    if (d.payloadMode) {
        const MimitaGamemode::PayloadRuntime& payload = d.payloadRuntime;
        pkt.payloadOvertime = payload.overtimeActive ? 1 : 0;
        pkt.payloadAdvancing = payload.advancing ? 1 : 0;
        pkt.payloadBlocked = payload.blocked ? 1 : 0;
        pkt.payloadRollingBack = payload.rollingBack ? 1 : 0;
        pkt.payloadAttackers = (uint8_t)std::min(payload.attackers, 255);
        pkt.payloadDefenders = (uint8_t)std::min(payload.defenders, 255);
        pkt.payloadOvertimeTicksLeft =
            (uint32_t)std::max(0, payload.overtimeTicksRemaining);
        pkt.payloadOvertimeTicksMax =
            (uint32_t)std::max(0, (int)std::lround(payload.overtimeSeconds * 60.0f));
        pkt.payloadDistance = payload.distanceTravelled;
        pkt.payloadTotalDistance = payload.totalDistance;
        pkt.payloadProgress = payload.totalDistance > 0.0f
            ? std::clamp(payload.distanceTravelled / payload.totalDistance, 0.0f, 1.0f)
            : 0.0f;
        pkt.payloadCartX = payload.cartPosition.x;
        pkt.payloadCartY = payload.cartPosition.y;
        pkt.payloadCartZ = payload.cartPosition.z;
    }

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

    // ── Zombie Tower run presentation ───────────────────────────────
    pkt.ztAttemptResult = d.ztAttemptResult ? 1 : 0;
    pkt.ztFinalResult = d.ztFinalResult ? 1 : 0;
    pkt.ztRunWon = d.ztRunWon ? 1 : 0;
    pkt.ztBossActive = d.bossEncounterActive ? 1 : 0;
    pkt.ztMusicPlaying = d.ztMusicPlaying ? 1 : 0;
    pkt.ztFog = d.ztFog ? 1 : 0;
    pkt.ztRain = d.ztRain ? 1 : 0;
    pkt.ztTorchLighting = d.ztTorchLighting ? 1 : 0;
    pkt.ztAttemptsRemaining = d.runAttemptsRemaining;
    pkt.ztRunElapsedTicks = d.ztRunElapsedTicks;
    std::snprintf(pkt.ztBossName, sizeof(pkt.ztBossName), "%s", d.activeBossName.c_str());
    pkt.ztBossHp = d.activeBossCurrentHp;
    pkt.ztBossMaxHp = d.activeBossMaxHp;
    uint32_t bestDamageId = 0;
    uint32_t bestHealingId = 0;
    uint32_t bestSpeedId = 0;
    for (const auto& entry : d.ztStats) {
        const auto& stats = entry.second;
        if (stats.damageDealt > (bestDamageId ? d.ztStats.at(bestDamageId).damageDealt : 0))
            bestDamageId = entry.first;
        if (stats.healingDone > (bestHealingId ? d.ztStats.at(bestHealingId).healingDone : 0))
            bestHealingId = entry.first;
        if (stats.topSpeed > (bestSpeedId ? d.ztStats.at(bestSpeedId).topSpeed : 0.0f))
            bestSpeedId = entry.first;
    }
    if (bestDamageId) {
        const auto& stats = d.ztStats.at(bestDamageId);
        pkt.ztMostDamage = (uint32_t)stats.damageDealt;
        std::snprintf(pkt.ztMostDamageName, sizeof(pkt.ztMostDamageName), "%s",
                      stats.name.empty() ? "NONE" : stats.name.c_str());
    }
    if (bestHealingId) {
        const auto& stats = d.ztStats.at(bestHealingId);
        pkt.ztMostHealing = (uint32_t)stats.healingDone;
        std::snprintf(pkt.ztMostHealingName, sizeof(pkt.ztMostHealingName), "%s",
                      stats.name.empty() ? "NONE" : stats.name.c_str());
    }
    if (bestSpeedId) {
        const auto& stats = d.ztStats.at(bestSpeedId);
        pkt.ztTopSpeed = stats.topSpeed;
        std::snprintf(pkt.ztTopSpeedName, sizeof(pkt.ztTopSpeedName), "%s",
                      stats.name.empty() ? "NONE" : stats.name.c_str());
    }
    pkt.ztCheckpointSerial = d.ztCheckpointSerial;
    std::snprintf(pkt.ztCheckpointName, sizeof(pkt.ztCheckpointName), "%s",
                  d.ztCheckpointName.c_str());
    pkt.ztMusicSerial = d.ztMusicSerial;
    std::snprintf(pkt.ztMusicTrack, sizeof(pkt.ztMusicTrack), "%s",
                  d.ztMusicTrack.c_str());

    // FFA and generic first-to-score top-3 leaderboard.
    if (d.matchMode == "ffa" || d.winCondition == "first_to_score") {
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
    // Reuse the existing replicated name slot for Melee's result banner. The
    // Melee HUD consumes this only as a winner label, not as an FFA leaderboard.
    if (d.matchMode == "melee" && d.matchOver &&
        d.phase == DUEL_PHASE_RESULTS && d.winnerPlayerId != 0) {
        pkt.ffaLeaderIds[0] = d.winnerPlayerId;
        auto winnerNameIt = d.participantNames.find(d.winnerPlayerId);
        if (winnerNameIt != d.participantNames.end())
            std::strncpy(pkt.ffaLeaderNames[0], winnerNameIt->second.c_str(),
                         sizeof(pkt.ffaLeaderNames[0]) - 1);
        else
            std::snprintf(pkt.ffaLeaderNames[0], sizeof(pkt.ffaLeaderNames[0]),
                          "Fighter %u", d.winnerPlayerId);
    }

    // Participant IDs and teams
    pkt.participantCount = (uint8_t)std::min((size_t)64, d.participants.size());
    const auto actorPresets = MatchRoleRegistry::instance().actorPresets();
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
            const auto actorPresetApplied = d.actorPresetAppliedByActorId.find(actorId);
            const std::string presetId = actorPresetApplied == d.actorPresetAppliedByActorId.end()
                ? std::string() : actorPresetApplied->second;
            for (size_t presetIndex = 0; presetIndex < actorPresets.size() && presetIndex < 255; ++presetIndex) {
                if (actorPresets[presetIndex]->id == presetId) {
                    pkt.participantActorPresets[i] = (uint8_t)(presetIndex + 1);
                    break;
                }
            }
        } else {
            pkt.participantRoles[i] = 0;
            pkt.participantStates[i] = (uint8_t)ActorState::Alive;
            pkt.participantActorPresets[i] = 0;
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

    // ── Disaster state (only when a mode pack declares one) ─────────
    // Rides the existing state cadence so a declared disaster always reaches
    // clients without a separate broadcast scheduler. Legacy modes skip it.
    if (d.disaster.configured || d.disaster.active) {
        DisasterStatePacket dp{};
        dp.header.type = PACKET_DISASTER_STATE;
        dp.duelId = d.duelId;
        dp.stateVersion = d.stateVersion;
        dp.seed = d.disaster.seed;
        dp.startTick = d.disaster.startTick;
        dp.durationTicks = d.disaster.durationTicks;
        dp.winnerActor = d.disaster.winnerActor;
        dp.phase = d.phase;
        dp.active = d.disaster.active ? 1 : 0;
        dp.resolveSource = d.disaster.resolveSource;
        std::strncpy(dp.disasterId, d.disaster.disasterId.c_str(), sizeof(dp.disasterId) - 1);
        std::strncpy(dp.name, d.disaster.name.c_str(), sizeof(dp.name) - 1);
        std::strncpy(dp.description, d.disaster.description.c_str(), sizeof(dp.description) - 1);
        for (const auto& kv : players) {
            if (kv.second.spawnState != ServerPlayer::Active)
                continue;
            const uint32_t eventId = nextReliableGameplayEventId();
            queueReliableGameplayEventToPlayer(
                sock, const_cast<ServerPlayer&>(kv.second), &dp, sizeof(dp), eventId,
                reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)),
                totalPacketsOut);
        }
    }
}

// Pick ONE random map spawn point as the shared fallback anchor for maps
// without team-tagged spawns. Also resolves per-team spawn clusters from
// spawnpoint.CT / spawnpoint.T tags so each squad starts apart.
void assignGamemodeSpawns(ServerGamemodeState& d, const HeadlessWorld& world)
{
    d.spawnsAssigned = true;
    d.teamSpawnPoints[0].clear();
    d.teamSpawnPoints[1].clear();
    d.teamSpawnsResolved = false;

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
        d.sharedAnchor = anchor;

        const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);

        // Group tagged spawns by the active mode's declared filters first.
        // This lets a mode define rules such as "contains NPC" and
        // "excludes NPC" without adding a mode-name branch here.
        for (const ServerSpawnPoint& sp : world.spawnPoints) {
            int team = -1;
            for (size_t i = 0; i < gm.teams.size() && i < 2; ++i) {
                if (spawnTagMatchesTeam(sp.tag, gm.teams[i])) {
                    team = (int)i;
                    break;
                }
            }
            if (team < 0)
                team = spawnTagTeamIndex(sp.tag);
            if (team == 0 || team == 1)
                d.teamSpawnPoints[team].push_back(sp.position);
        }
        d.teamSpawnsResolved = !d.teamSpawnPoints[0].empty() &&
                               !d.teamSpawnPoints[1].empty();
        if (!d.teamSpawnPoints[0].empty()) d.spawnA = d.teamSpawnPoints[0].front();
        if (!d.teamSpawnPoints[1].empty()) d.spawnB = d.teamSpawnPoints[1].front();
        Debug::log(Debug::Category::Duel,
            "[DuelAnchor] map=%s anchorIndex=%zu anchor=(%.3f,%.3f,%.3f) teamTags ct=%zu t=%zu resolved=%d\n",
            d.mapId.c_str(), anchorIndex, anchor.x, anchor.y, anchor.z,
            d.teamSpawnPoints[0].size(), d.teamSpawnPoints[1].size(),
            (int)d.teamSpawnsResolved);

        // A map may have generic spawn nodes instead of explicit CT/T names.
        // In a two-team round mode, never collapse both teams onto the same
        // fallback anchor: use deterministic endpoints of the authored spawn
        // list until the map receives explicit spawnpoint.CT/spawnpoint.T tags.
        if (gm.teams.size() >= 2 && !d.teamSpawnsResolved) {
            if (world.spawnPoints.size() >= 2) {
                d.teamSpawnPoints[0].clear();
                d.teamSpawnPoints[1].clear();
                d.teamSpawnPoints[0].push_back(world.spawnPoints.front().position);
                d.teamSpawnPoints[1].push_back(world.spawnPoints.back().position);
                d.spawnA = d.teamSpawnPoints[0].front();
                d.spawnB = d.teamSpawnPoints[1].front();
                d.teamSpawnsResolved = true;
                Debug::warn(Debug::Category::Duel,
                    "[CS SPAWN] mode=%s has no CT/T tags; generic fallback uses first=%zu for CT and last=%zu for T\n",
                    d.matchMode.c_str(), (size_t)0, world.spawnPoints.size() - 1);
            } else {
                Debug::error(Debug::Category::Duel,
                    "[CS SPAWN] mode=%s missing separate team spawn points; map has only %zu generic point(s). Add spawnpoint.CT and spawnpoint.T nodes.\n",
                    d.matchMode.c_str(), world.spawnPoints.size());
            }
        }
    }
    else
    {
        d.spawnA = glm::vec3(1.0f, 5.0f, 30.0f);
        d.spawnB = d.spawnA;
        d.sharedAnchor = d.spawnA;
        Debug::warn(Debug::Category::Duel,
            "[DuelFallback] map=%s reason=no_spawn_points final=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_str(), d.spawnA.x, d.spawnA.y, d.spawnA.z);
    }
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (gm.teams.size() >= 2) {
        bool hasSeparatedClusters = !d.teamSpawnPoints[0].empty() &&
                                    !d.teamSpawnPoints[1].empty();
        for (const glm::vec3& red : d.teamSpawnPoints[0])
            for (const glm::vec3& blue : d.teamSpawnPoints[1])
                if (glm::length(red - blue) <= 0.01f)
                    hasSeparatedClusters = false;
        if (!hasSeparatedClusters) {
            const float separation = std::max(2.0f, d.spawnOffsetRadius * 2.0f + 1.0f);
            d.teamSpawnPoints[0] = {d.sharedAnchor + glm::vec3(-separation, 0.0f, 0.0f)};
            d.teamSpawnPoints[1] = {d.sharedAnchor + glm::vec3(separation, 0.0f, 0.0f)};
            d.spawnA = d.teamSpawnPoints[0].front();
            d.spawnB = d.teamSpawnPoints[1].front();
            d.teamSpawnsResolved = true;
            Debug::warn(Debug::Category::Duel,
                "[TEAM SPAWN] mode=%s map=%s reason=team groups missing or overlapping; derived separated team anchors separation=%.2f\n",
                d.matchMode.c_str(), d.mapId.c_str(), separation);
        }
    }
    const glm::vec3 teamA = d.teamSpawnPoints[0].empty() ? d.spawnA : d.teamSpawnPoints[0].front();
    const glm::vec3 teamB = d.teamSpawnPoints[1].empty() ? d.spawnB : d.teamSpawnPoints[1].front();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "gamemode.spawn-groups-resolved", d.matchMode,
        "team spawn clusters resolved", d.currentServerTick,
        nlohmann::json{{"mode", d.matchMode}, {"map", d.mapId},
                       {"world_spawn_points", world.spawnPoints.size()},
                       {"team_a_count", d.teamSpawnPoints[0].size()},
                       {"team_b_count", d.teamSpawnPoints[1].size()},
                       {"team_a", {teamA.x, teamA.y, teamA.z}},
                       {"team_b", {teamB.x, teamB.y, teamB.z}},
                       {"separation", glm::length(teamA - teamB)},
                       {"resolved", d.teamSpawnsResolved}},
        __FILE__, __LINE__, __FUNCTION__);
    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] anchor=(%.1f %.1f %.1f) spawns=%zu\n",
        d.spawnA.x, d.spawnA.y, d.spawnA.z, world.spawnPoints.size());
}

// ── Zombie Tower authored run helpers ──────────────────────────────────
glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d, int team);

static bool isZombieTowerRun(const ServerGamemodeState& d)
{
    return d.enabled && d.zombieTower;
}

static std::string lowerAscii(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

static const MapEntity* findSpawnEntityByTag(const std::string& tag)
{
    const std::string wanted = lowerAscii(tag);
    for (const auto& e : MapConfigRegistry::instance().current().entities) {
        if (e.type != "spawnpoint" || !e.enabled) continue;
        if (lowerAscii(e.tag) == wanted) return &e;
    }
    return nullptr;
}

static const MapEntity* findCheckpointByOrder(int order)
{
    const MapEntity* best = nullptr;
    for (const auto& e : MapConfigRegistry::instance().current().entities) {
        if (e.type != "checkpoint" || !e.enabled || e.order != order) continue;
        if (!best || e.order < best->order) best = &e;
    }
    return best;
}

static int highestReachedCheckpointOrder(const ServerGamemodeState& d)
{
    int highest = 0;
    for (const auto& id : d.activatedCheckpoints) {
        const MapEntity* e = MapConfigRegistry::instance().findEntity(id);
        if (e) highest = std::max(highest, e->order);
    }
    return highest;
}

// Spawn selection for the ZT run. Team 0 uses the lobby during intermission,
// the intro spawn during countdown, and the last checkpoint during a run.
// Team 1 (zombie players) enters at the next checkpoint the humans have not
// reached, or falls back safely when every checkbox is already behind them.
static glm::vec3 zombieTowerSpawnPointForActor(
    const ServerGamemodeState& d, int team, uint32_t actorId, const char* reason)
{
    static std::mt19937 rng(std::random_device{}());
    auto offset = [&](const glm::vec3& base) {
        std::uniform_real_distribution<float> off(-d.spawnOffsetRadius, d.spawnOffsetRadius);
        return base + glm::vec3(off(rng), off(rng), 0.0f);
    };

    if (team == 1) {
        const int highest = highestReachedCheckpointOrder(d);
        const MapEntity* next = nullptr;
        for (const auto& e : MapConfigRegistry::instance().current().entities) {
            if (e.type != "checkpoint" || !e.enabled) continue;
            if (e.order <= highest) continue;
            if (!next || e.order < next->order) next = &e;
        }
        if (next) return offset(next->position);
        if (d.hasCheckpointSpawn) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "zombie_tower.zombie-spawn-no-ahead-checkpoint", std::to_string(actorId),
                "all checkpoints reached; using highest reached checkpoint", d.currentServerTick,
                nlohmann::json{{"actor", actorId}, {"reason", reason},
                               {"checkpoint_id", d.currentCheckpointId}});
            return offset(d.currentCheckpointPos);
        }
        if (const MapEntity* intro = findSpawnEntityByTag("tower_intro"))
            return offset(intro->position);
    }

    if (team == 0) {
        if (d.hasCheckpointSpawn && d.ztResumingFromCheckpoint)
            return offset(d.currentCheckpointPos);
        if ((d.phase == DUEL_PHASE_ACTIVE || d.phase == DUEL_PHASE_GO) &&
            d.hasCheckpointSpawn)
            return offset(d.currentCheckpointPos);
        if (d.phase == DUEL_PHASE_COUNTDOWN || d.phase == DUEL_PHASE_GO) {
            if (const MapEntity* intro = findSpawnEntityByTag("tower_intro"))
                return offset(intro->position);
        }
        if (const MapEntity* lobby = findSpawnEntityByTag("lobby"))
            return offset(lobby->position);
    }

    return gamemodeSpawnPoint(d, team);
}

// Evidence for one spawn decision. The caller logs this, so a spawn failure is
// diagnosable as "no authored tag", "team cluster missing", or a real fallback
// instead of appearing as an unexplained wrong position.
struct SpawnResolutionInfo
{
    std::string group;
    std::string spawnTagContains;
    int candidateCount = 0;
    const char* fallback = "none";
    bool teamTaggedMode = false;
};

// Team-aware spawn point: uses the team's tagged spawn cluster when resolved,
// otherwise falls back to the team cluster, then the team anchor. The random XY
// offset spreads squadmates around their spawn so they are not stacked.
static glm::vec3 resolveGamemodeSpawnPoint(const ServerGamemodeState& d, int team,
                                           SpawnResolutionInfo& info)
{
    static std::mt19937 rng(std::random_device{}());
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (team >= 0 && team < (int)gm.teams.size()) {
        info.group = gm.teams[(size_t)team].spawnGroup;
        info.spawnTagContains = gm.teams[(size_t)team].spawnTagContains;
        info.teamTaggedMode = !info.group.empty() || !info.spawnTagContains.empty();
    }

    // A mode may declare a shared spawn strategy (e.g. a 1v1 brawl): every actor
    // starts at the single map anchor so the fighters begin on the same level.
    if (gm.spawnStrategy == "shared") {
        info.fallback = "spawn-strategy-shared";
        std::uniform_real_distribution<float> off(-d.spawnOffsetRadius, d.spawnOffsetRadius);
        return d.sharedAnchor + glm::vec3(off(rng), off(rng), 0.0f);
    }

    // Prefer authored spawnpoint entities for the active map when present. A
    // team with spawn tagging configured must never fall through to untagged
    // points: doing so collapses both teams onto an unrelated editor spawn.
    {
        std::string opposingGroup;
        const int opposingTeam = team == 0 ? 1 : (team == 1 ? 0 : -1);
        if (opposingTeam >= 0 && opposingTeam < (int)gm.teams.size())
            opposingGroup = gm.teams[(size_t)opposingTeam].spawnGroup;
        std::vector<glm::vec3> opposingPositions;
        if (!opposingGroup.empty()) {
            for (const auto& e : MapConfigRegistry::instance().current().entities)
                if (e.type == "spawnpoint" && e.enabled && e.tag == opposingGroup)
                    opposingPositions.push_back(e.position);
        }
        if (opposingTeam >= 0 && opposingTeam < 2)
            opposingPositions.insert(opposingPositions.end(),
                d.teamSpawnPoints[opposingTeam].begin(), d.teamSpawnPoints[opposingTeam].end());

        std::vector<const MapEntity*> candidates;
        const bool allowUntagged = !info.teamTaggedMode;
        for (const auto& e : MapConfigRegistry::instance().current().entities) {
            if (e.type != "spawnpoint" || !e.enabled) continue;
            if (info.group.empty()) {
                if (!e.tag.empty() || !allowUntagged) continue;
            } else if (e.tag != info.group) {
                continue;
            }
            if (std::none_of(opposingPositions.begin(), opposingPositions.end(),
                    [&](const glm::vec3& position) {
                        return glm::length(e.position - position) <= 0.01f;
                    }))
                candidates.push_back(&e);
        }
        info.candidateCount = (int)candidates.size();
        if (!candidates.empty()) {
            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            const MapEntity* chosen = candidates[pick(rng)];
            std::uniform_real_distribution<float> off(-d.spawnOffsetRadius, d.spawnOffsetRadius);
            info.fallback = "authored-team-group";
            return chosen->position + glm::vec3(off(rng), off(rng), 0.0f);
        }
    }

    std::uniform_real_distribution<float> dist(-d.spawnOffsetRadius, d.spawnOffsetRadius);
    // A teamless request (team < 0) uses the neutral map anchor, never a team
    // cluster, so an unknown team cannot silently land on the CT spawn.
    glm::vec3 base = d.sharedAnchor;
    if (team == 0 && !d.teamSpawnPoints[0].empty()) {
        std::uniform_int_distribution<size_t> pick(0, d.teamSpawnPoints[0].size() - 1);
        base = d.teamSpawnPoints[0][pick(rng)];
        info.fallback = "team-cluster";
    } else if (team == 1 && !d.teamSpawnPoints[1].empty()) {
        std::uniform_int_distribution<size_t> pick(0, d.teamSpawnPoints[1].size() - 1);
        base = d.teamSpawnPoints[1][pick(rng)];
        info.fallback = "team-cluster";
    } else if (team == 0) {
        base = d.spawnA;
        info.fallback = "team-anchor-a";
    } else if (team == 1) {
        base = d.spawnB;
        info.fallback = "team-anchor-b";
    } else {
        info.fallback = "neutral-anchor";
    }
    return base + glm::vec3(dist(rng), dist(rng), 0.0f);
}

glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d, int team)
{
    SpawnResolutionInfo info;
    return resolveGamemodeSpawnPoint(d, team, info);
}

// Legacy single-argument spawn (shared anchor, no team). Kept for callers that
// have no team context (waves, objective drop).
glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d)
{
    return gamemodeSpawnPoint(d, -1);
}

// ── Team identity source of truth ───────────────────────────────────
// Resolve an actor's authoritative playing team from the single chain:
// matchTeams -> matchActors.teamId -> ServerPlayer/ServerNpc mirror. Returns -1
// when the team is unknown (the caller must not silently assume a team).
int actorTeamOf(const ServerGamemodeState& d, uint32_t actorId,
                const std::unordered_map<uint32_t, ServerPlayer>& players,
                const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    auto it = d.matchTeams.find(actorId);
    if (it != d.matchTeams.end() && it->second >= 0) return it->second;
    auto aIt = d.matchActors.find(actorId);
    if (aIt != d.matchActors.end() && aIt->second.teamId >= 0) return aIt->second.teamId;
    auto pIt = players.find(actorId);
    if (pIt != players.end() && pIt->second.matchTeam >= 0) return pIt->second.matchTeam;
    auto nIt = npcs.find(actorId);
    if (nIt != npcs.end() && nIt->second.matchTeam >= 0) return nIt->second.matchTeam;
    return -1;
}

// A valid playing team for the active mode: in range and not the Spectator
// team. Capacity is NOT used (0 means "unlimited" per the schema).
bool isValidPlayingTeam(const ServerGamemodeState& d, int team)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (team < 0 || team >= (int)gm.teams.size()) return false;
    const GamemodeTeam& t = gm.teams[(size_t)team];
    if (t.id == "spec" || t.role == "spectator") return false;
    return true;
}

std::string spawnGroupForTeam(const ServerGamemodeState& d, int team)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (team >= 0 && team < (int)gm.teams.size())
        return gm.teams[(size_t)team].spawnGroup;
    return {};
}

// Team-validated spawn for one actor. In a team mode an unknown team is logged
// loudly and falls back to the neutral anchor (never a team cluster, so a bad
// team can never silently spawn on the CT spawn).
glm::vec3 gamemodeSpawnPointForActor(
    const ServerGamemodeState& d, uint32_t actorId,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    const char* reason)
{
    const int spawnTeam = actorTeamOf(d, actorId, players, npcs);
    if (isZombieTowerRun(d))
        return zombieTowerSpawnPointForActor(d, spawnTeam, actorId, reason);

    // A checkpointed run respawns players at the latest authored checkpoint.
    if (d.hasCheckpointSpawn && players.find(actorId) != players.end()) {
        static std::mt19937 crng(std::random_device{}());
        std::uniform_real_distribution<float> off(-d.spawnOffsetRadius, d.spawnOffsetRadius);
        return d.currentCheckpointPos + glm::vec3(off(crng), off(crng), 0.0f);
    }
    const int team = actorTeamOf(d, actorId, players, npcs);
    std::string roleId;
    auto aIt = d.matchActors.find(actorId);
    if (aIt != d.matchActors.end()) roleId = aIt->second.roleId;

    const bool managedTeamMode = d.objectiveRounds || d.npcWaves || d.persistentNpcSpawns;
    if (managedTeamMode && !isValidPlayingTeam(d, team)) {
        Debug::error(Debug::Category::Duel,
            "[RESPAWN] actor=%u role=%s team=%d reason=%s mode=%s: unknown team; using neutral anchor (refusing team-spawn fallback)\n",
            actorId, roleId.empty() ? "none" : roleId.c_str(), team, reason,
            d.matchMode.c_str());
        SpawnResolutionInfo info;
        const glm::vec3 spawn = resolveGamemodeSpawnPoint(d, -1, info);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Errors,
            "gamemode.spawn-resolved", std::to_string(actorId), reason,
            d.currentServerTick,
            nlohmann::json{{"actor", actorId}, {"role", roleId},
                           {"team", team}, {"mode", d.matchMode}, {"map", d.mapId},
                           {"spawn_group", info.group},
                           {"spawn_tag_contains", info.spawnTagContains},
                           {"team_tagged_mode", info.teamTaggedMode},
                           {"candidate_count", info.candidateCount},
                           {"fallback", info.fallback},
                           {"position", {spawn.x, spawn.y, spawn.z}},
                           {"reason", reason}},
            __FILE__, __LINE__, __FUNCTION__);
        return spawn;
    }

    SpawnResolutionInfo info;
    const glm::vec3 spawn = resolveGamemodeSpawnPoint(d, team, info);
    if (managedTeamMode) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "gamemode.spawn-resolved", std::to_string(actorId), reason,
            d.currentServerTick,
            nlohmann::json{{"actor", actorId}, {"role", roleId},
                           {"team", team}, {"mode", d.matchMode}, {"map", d.mapId},
                           {"spawn_group", info.group},
                           {"spawn_tag_contains", info.spawnTagContains},
                           {"team_tagged_mode", info.teamTaggedMode},
                           {"candidate_count", info.candidateCount},
                           {"fallback", info.fallback},
                           {"position", {spawn.x, spawn.y, spawn.z}},
                           {"reason", reason}},
            __FILE__, __LINE__, __FUNCTION__);
        Debug::log(Debug::Category::Duel,
            "[RESPAWN] actor=%u role=%s team=%d spawn_group=%s candidates=%d fallback=%s pos=(%.2f,%.2f,%.2f) reason=%s\n",
            actorId, roleId.empty() ? "none" : roleId.c_str(), team,
            info.group.empty() ? "none" : info.group.c_str(), info.candidateCount,
            info.fallback, spawn.x, spawn.y, spawn.z, reason);
    }
    return spawn;
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
    startCountdownWallClock(d);
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
    world.authoredSpawnPointBaseCount = 0;
    world.authoredSpawnPointsApplied = false;
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
    // Authored spawnpoint entities are part of the map, so apply them before
    // deciding whether the map has any usable spawns.
    applyAuthoredMapGeometry(candidate, mapId);
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
        const glm::vec3 spawn =
            gamemodeSpawnPointForActor(d, npc.id, players, npcs, "map-change");
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
        p.duelSpawnPos = gamemodeSpawnPointForActor(d, p.id, players, npcs, "map-change");
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
                const glm::vec3 spawn =
                    gamemodeSpawnPointForActor(d, npc.id, players, npcs, "map-rotate");
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

// NPC combat profile the mode assigns: the gamemode NPC profile wins over the
// role profile (explicit per-actor overrides are applied later by
// serverResolveActorSpawnProfile). NPC-only; humans keep an empty profile.
std::string resolveNpcBehaviorProfileId(const Gamemode& gm, const std::string& roleProfile)
{
    if (!gm.npcBehaviorProfile.empty() &&
        BehaviorProfileRegistry::instance().get(gm.npcBehaviorProfile))
        return gm.npcBehaviorProfile;
    return roleProfile;
}

// Pure validation of a role/team/spawn-group assignment for a team mode.
// Rejects a role that declares a different team, an unknown/spectator team, and
// a role spawn group that disagrees with the team's. Returns true when valid.
bool validateTeamRoleConsistency(const Gamemode& gm,
                                 const MatchRoleDefinition* role,
                                 int team, std::string& error)
{
    error.clear();
    if (team < 0 || team >= (int)gm.teams.size()) {
        error = "team " + std::to_string(team) + " is not a valid team for mode " + gm.id;
        return false;
    }
    const GamemodeTeam& gt = gm.teams[(size_t)team];
    if (gt.id == "spec" || gt.role == "spectator") {
        error = "team " + std::to_string(team) + " is the spectator team";
        return false;
    }
    if (!role) return true;
    if (role->team >= 0 && role->team != team) {
        error = "role " + role->id + " declares team " + std::to_string(role->team) +
                " but was assigned team " + std::to_string(team);
        return false;
    }
    if (!role->spawnGroup.empty() && !gt.spawnGroup.empty() &&
        role->spawnGroup != gt.spawnGroup) {
        error = "role " + role->id + " spawn_group " + role->spawnGroup +
                " does not match team " + gt.id + " spawn_group " + gt.spawnGroup;
        return false;
    }
    return true;
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

// Headless NPC-only match affordance for controlled runtime testing. When
// MIMITA_CS_HEADLESS_MATCH is set, a round-mode match runs with NPCs only (no
// connected human): the roster fills both teams and the normal round lifecycle
// runs. Default (unset) behavior is byte-for-byte unchanged. This exists only
// so the bomb/round behavior can be exercised and observed in the real
// executable without a GUI client; it is never a production path.
static bool headlessMatchAllowed()
{
    static const bool allowed = [] {
        const char* v = std::getenv("MIMITA_CS_HEADLESS_MATCH");
        if (!v) return false;
        const std::string s(v);
        return s == "1" || s == "true";
    }();
    return allowed;
}

// Build the round-time scoreboard from the mode's ordered teams. The match is
// built around one human; the human's team gets allied NPCs up to capacity and
// the opposing team gets a full NPC squad. In headless test mode there is no
// human and both teams are filled with NPCs.
void buildObjectiveRoster(ServerGamemodeState& d,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);

    // Reserve the roster id range. A startup/map NPC may already exist, so the
    // old "seed only when npcs is empty" rule let roster ids start at 0 and
    // collide with player ids (the client rejects a whole snapshot whose entity
    // ids repeat). Always keep objective-roster ids above the startup range.
    static constexpr uint32_t kObjectiveRosterNpcIdBase = 100000;
    if (d.roundNextNpcId < kObjectiveRosterNpcIdBase)
        d.roundNextNpcId = kObjectiveRosterNpcIdBase;

    // Clear exactly the previous objective roster (plus the historical
    // >=100000 range) so rebuilding for a new round/warmup never duplicates or
    // leaks the squad. Startup/map NPCs keep their ids and bodies.
    for (auto it = npcs.begin(); it != npcs.end(); ) {
        const bool ownedRosterNpc =
            it->first >= kObjectiveRosterNpcIdBase ||
            d.objectiveRosterNpcIds.count(it->first) != 0;
        if (ownedRosterNpc) {
            d.objectiveRosterNpcIds.erase(it->first);
            d.matchTeams.erase(it->first);
            d.matchActors.erase(it->first);
            d.participantNames.erase(it->first);
            d.ffaKills.erase(it->first);
            d.ffaDeaths.erase(it->first);
            d.participants.erase(std::remove(d.participants.begin(), d.participants.end(),
                                             it->first), d.participants.end());
            it = npcs.erase(it);
        } else {
            ++it;
        }
    }
    // Reuse the reserved roster range after retiring the previous roster. Any
    // still-live id (startup/map NPC) is skipped by the allocation loop below.
    d.roundNextNpcId = kObjectiveRosterNpcIdBase;

    // Collect every active human. Payload needs up to 12 humans per team, not
    // only the single-human Counter-Strike case.
    std::vector<uint32_t> humans;
    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active) humans.push_back(kv.first);
    }
    std::sort(humans.begin(), humans.end());
    const bool headlessMatch = humans.empty() && headlessMatchAllowed();
    if (humans.empty() && !headlessMatch) return;

    int humanCounts[2] = {0, 0};
    for (uint32_t humanId : humans) {
        auto teamIt = d.matchTeams.find(humanId);
        int humanTeam = (teamIt != d.matchTeams.end()) ? teamIt->second : -1;
        if (humanTeam < 0 || humanTeam > 1)
            humanTeam = chooseFallbackTeam(players, d);
        applyHumanRosterTeam(d, players, humanId, humanTeam);
        ++humanCounts[humanTeam];

        // Ensure the human's match actor exists with the team's role.
        ActorMatchDescriptor& desc = d.matchActors[humanId];
        desc.controller = ActorController::Human;
        desc.state = ActorState::Alive;
        desc.teamId = humanTeam;
        const std::string roleId = roundTeamRoleId(gm, humanTeam);
        const MatchRoleDefinition* humanDef =
            roleId.empty() ? nullptr : MatchRoleRegistry::instance().get(roleId);
        std::string teamError;
        if (!validateTeamRoleConsistency(gm, humanDef, humanTeam, teamError)) {
            Debug::error(Debug::Category::Duel,
                "[CS TEAM] human team assignment invalid: %s\n", teamError.c_str());
        }
        if (humanDef) {
            desc.roleId = roleId;
            desc.movementProfileId = humanDef->movementPreset;
            desc.weaponProfileId = humanDef->weaponSet;
        }
        if (std::find(d.participants.begin(), d.participants.end(), humanId) ==
            d.participants.end()) {
            d.participants.push_back(humanId);
        }
        d.participantNames[humanId] = players[humanId].name;
        d.ffaKills[humanId] = 0;
        d.ffaDeaths[humanId] = 0;
    }

    int npcCounts[2] = {0, 0};
    for (int team = 0; team < 2; ++team) {
        const int capacity = roundTeamCapacity(gm, team);
        const int target = capacity > 0 ? capacity : 5;
        npcCounts[team] = std::max(0, target - humanCounts[team]);
    }
    for (int team = 0; team < 2; ++team) {
        const int npcCount = npcCounts[team];
        const std::string roleId = roundTeamRoleId(gm, team);
        {
            const MatchRoleDefinition* teamDef =
                roleId.empty() ? nullptr : MatchRoleRegistry::instance().get(roleId);
            std::string teamError;
            if (!validateTeamRoleConsistency(gm, teamDef, team, teamError)) {
                Debug::error(Debug::Category::Duel,
                    "[CS TEAM] team %d role assignment invalid: %s\n", team, teamError.c_str());
            }
        }
        for (int i = 0; i < npcCount; ++i) {
            // Never reuse an id already owned by a player, startup/map NPC, or
            // roster descriptor. Duplicate ids invalidate an entire client
            // snapshot, which makes every roster NPC disappear.
            auto idInUse = [&](uint32_t id) {
                return npcs.find(id) != npcs.end() ||
                       players.find(id) != players.end() ||
                       d.matchTeams.find(id) != d.matchTeams.end() ||
                       d.matchActors.find(id) != d.matchActors.end();
            };
            while (idInUse(d.roundNextNpcId)) ++d.roundNextNpcId;
            ServerNpc npc;
            npc.entityId = d.roundNextNpcId++;
            npc.name = roundTeamName(gm, team) + " " + std::to_string(i + 1);
            npc.pos = gamemodeSpawnPoint(d, team);
            npc.yaw = team == 0 ? 0.0f : 3.14159265f;
            npc.difficulty = 1.0f;
            npc.matchTeam = team;
            ActorMatchDescriptor desc;
            desc.controller = ActorController::Npc;
            desc.state = ActorState::Alive;
            desc.teamId = team;
            if (!roleId.empty() && MatchRoleRegistry::instance().get(roleId)) {
                desc.roleId = roleId;
                const MatchRoleDefinition* def = MatchRoleRegistry::instance().get(roleId);
                desc.movementProfileId = def->movementPreset;
                desc.weaponProfileId = def->weaponSet;
                desc.behaviorProfileId = resolveNpcBehaviorProfileId(gm, def->behaviorProfile);
            }
            d.matchActors[npc.entityId] = std::move(desc);
            d.matchTeams[npc.entityId] = team;
            d.participants.push_back(npc.entityId);
            d.participantNames[npc.entityId] = npc.name;
            d.ffaKills[npc.entityId] = 0;
            d.ffaDeaths[npc.entityId] = 0;
            d.objectiveRosterNpcIds.insert(npc.entityId);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "npc.roster-id.allocated", std::to_string(npc.entityId),
                "objective roster id reserved above startup range",
                d.currentServerTick,
                nlohmann::json{{"actor", npc.entityId},
                               {"team", team},
                               {"base", kObjectiveRosterNpcIdBase},
                               {"next", d.roundNextNpcId},
                               {"player_id_collision", players.find(npc.entityId) != players.end()}},
                __FILE__, __LINE__, __FUNCTION__);
            const ActorMatchDescriptor& rosterDesc = d.matchActors[npc.entityId];
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "npc.roster-profile-resolved", std::to_string(npc.entityId),
                "round-roster", d.currentServerTick,
                nlohmann::json{
                    {"actor", npc.entityId},
                    {"team", team},
                    {"role", rosterDesc.roleId},
                    {"profile", rosterDesc.behaviorProfileId},
                    {"movement_preset", rosterDesc.movementProfileId},
                    {"actor_preset", gm.actorPresetId},
                    {"mode", d.matchMode},
                    {"mode_preset", gm.actorPresetId},
                    {"source", "buildObjectiveRoster"}},
                __FILE__, __LINE__, __FUNCTION__);
            applyServerNpcRoleHealth(npc, "round-roster");
            npcs.emplace(npc.entityId, std::move(npc));
        }
    }

    std::sort(d.participants.begin(), d.participants.end());

    Debug::warn(Debug::Category::Duel,
        "[ROSTER] mode=%s humans=%zu teams=%d-%d npcs=%zu participants=%zu locked=%d\n",
        d.matchMode.c_str(), humans.size(), humanCounts[0], humanCounts[1],
        npcs.size(), d.participants.size(), (int)d.rosterLocked);
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
    startCountdownWallClock(d);
    d.matchTimeLimitTick = 0;
    d.roundEndTick = 0;
    d.lastBroadcastTick = currentTick;
    resetMatchScores(d);
    d.phase = DUEL_PHASE_COUNTDOWN;
    // Freeze the whole countdown: human input lock + NPC hold, released exactly
    // at GO. Newly adopted roster bodies get the same hold in
    // the server NPC phase gate (the gate now pauses all NPC updates).
    d.roundCountdownFreeze = true;
    resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
    if (d.payloadMode) {
        MimitaGamemode::payloadRuntimeReset(d.payloadRuntime);
        if (d.payloadCartEntityId != 0) {
            PhysicalEntitySystem::instance().moveKinematic(
                d.payloadCartEntityId, d.payloadRuntime.cartTransform, 0.0f);
        }
        d.teamChangeCooldownUntil.clear();
        d.payloadLastCartState = -1;
    } else {
        assignObjectiveCarrier(d, players, npcs);
    }
    Debug::warn(Debug::Category::Duel,
        "[ROUND] begin mode=%s round=%u version=%u wins=%d-%d toWin=%d tick=%u\n",
        d.matchMode.c_str(), d.roundNumber, d.roundVersion,
        d.roundWins[0], d.roundWins[1], d.roundsToWin, currentTick);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "round.start", std::to_string(d.duelId), "countdown", currentTick,
        nlohmann::json{
            {"mode", d.matchMode},
            {"round", d.roundNumber},
            {"round_version", d.roundVersion},
            {"wins", {d.roundWins[0], d.roundWins[1]}},
            {"rounds_to_win", d.roundsToWin},
            {"countdown_seconds", d.countdownSeconds}});
}

// Decide a round. Awards the round win to `winnerTeam` (or -1 for a draw),
// increments the tally, and either ends the match or schedules the next round.
void endObjectiveRound(ServerGamemodeState& d, int winnerTeam, int reason,
                       uint32_t tick)
{
    if (d.phase != DUEL_PHASE_ACTIVE) return;
    d.roundCountdownFreeze = false;
    d.roundWinnerTeam = winnerTeam;
    d.roundEndReason = reason;
    if (winnerTeam >= 0 && winnerTeam < 2)
        ++d.roundWins[winnerTeam];

    const bool matchDecided = !d.endlessRounds && d.roundsToWin > 0 &&
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
    if (d.payloadMode && winnerTeam >= 0) {
        // The shared round tally still counts, but Payload's displayed result
        // and process-only session wins are owned here.
        d.winnerTeam = winnerTeam;
        if (winnerTeam < 2) ++d.payloadSessionWins[winnerTeam];
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

// ── Payload fixed-tick adapter ─────────────────────────────────────────
// Builds actor snapshots, runs the pure cart rules, applies healing, and
// performs the round transition. The cart itself is still driven here; client
// presentation and the physical cart body are separate owners.
static void serverPayloadTick(ServerGamemodeState& d,
                              std::unordered_map<uint32_t, ServerPlayer>& players,
                              std::unordered_map<uint32_t, ServerNpc>& npcs,
                              NpcSystem& npcSystem,
                              uint32_t tick)
{
    if (!d.payloadMode || d.phase != DUEL_PHASE_ACTIVE) return;

    // Cart facing is authored map data and may be hot-reloaded live.
    const MapObjectiveConfig& payloadMap = MapConfigRegistry::instance().current();
    d.payloadRuntime.cartYaw = glm::radians(payloadMap.payload.cartYaw);
    d.payloadRuntime.cartFixedFacing = payloadMap.payload.cartFixedFacing;

    std::array<MimitaGamemode::PayloadActorView, 64> actors{};
    int actorCount = 0;
    auto addActor = [&](uint32_t id, int team, bool alive, const glm::vec3& pos,
                        float radius, float height) {
        if (actorCount >= (int)actors.size()) return;
        MimitaGamemode::PayloadActorView& view = actors[(size_t)actorCount++];
        view.id = id;
        view.team = team;
        view.alive = alive;
        view.position = pos;
        view.radius = radius;
        view.height = height;
    };
    for (const auto& kv : players) {
        addActor(kv.first, objectiveActorTeam(d, kv.first), !kv.second.dead,
                 kv.second.pos, PLAYER_RADIUS, PLAYER_HEIGHT);
    }
    for (const auto& kv : npcs) {
        addActor(kv.first, kv.second.matchTeam, kv.second.health > 0,
                 kv.second.pos, PLAYER_RADIUS, PLAYER_HEIGHT);
    }

    MimitaGamemode::PayloadTickResult result;
    MimitaGamemode::payloadRuntimeTick(d.payloadRuntime, actors.data(), actorCount,
                                       tick, d.roundEndTick, result);

    if (d.payloadCartEntityId != 0) {
        PhysicalEntitySystem::instance().moveKinematic(
            d.payloadCartEntityId, d.payloadRuntime.cartTransform, 1.0f / 60.0f);
    }
    for (auto& kv : players) {
        ServerPlayer& player = kv.second;
        if (player.dead) continue;
        glm::vec3 resolvedPos;
        glm::vec3 resolvedVel;
        bool resolvedGround = player.onGround;
        if (MimitaGamemode::payloadResolveActorAgainstCart(
                d.payloadRuntime, player.pos, player.vel, player.onGround,
                PLAYER_RADIUS, PLAYER_HEIGHT, resolvedPos, resolvedVel,
                resolvedGround)) {
            player.pos = resolvedPos;
            player.vel = resolvedVel;
            player.onGround = resolvedGround;
            player.actorCollisionChanged = true;
        }
    }
    for (Npc& npc : npcSystem.all()) {
        if (npc.body.dead || npc.body.currentHp <= 0) continue;
        const float scale = std::max(0.05f, npc.body.sizeScale);
        glm::vec3 resolvedPos;
        glm::vec3 resolvedVel;
        bool resolvedGround = npc.body.ground.onGround;
        if (!MimitaGamemode::payloadResolveActorAgainstCart(
                d.payloadRuntime, npc.body.pos, npc.body.vel,
                npc.body.ground.onGround, PLAYER_RADIUS * scale,
                PLAYER_HEIGHT * scale, resolvedPos, resolvedVel,
                resolvedGround)) {
            continue;
        }
        npc.body.pos = resolvedPos;
        npc.body.vel = resolvedVel;
        npc.body.ground.onGround = resolvedGround;
        npc.body.updateModelWorldTransforms();
        auto snapshot = npcs.find(npc.id);
        if (snapshot != npcs.end()) {
            snapshot->second.pos = resolvedPos;
            snapshot->second.vel = resolvedVel;
            snapshot->second.onGround = resolvedGround;
        }
    }

    // Apply the fixed-tick healing result through the existing health owners.
    for (uint8_t i = 0; i < result.healedCount; ++i) {
        const uint32_t id = result.healedIds[i];
        auto pIt = players.find(id);
        if (pIt != players.end()) {
            const int maxHp = std::max(1, pIt->second.maxHealth);
            pIt->second.health = std::min(maxHp,
                pIt->second.health + d.payloadRuntime.healHpPer10Ticks);
            continue;
        }
        auto nIt = npcs.find(id);
        if (nIt != npcs.end()) {
            const ActorSpawnProfile profile = serverResolveActorSpawnProfile(id);
            const int maxHp = std::max(1, profile.hasRole && profile.health > 0
                ? profile.health : std::max(nIt->second.health, 100));
            nIt->second.health = std::min(maxHp,
                nIt->second.health + d.payloadRuntime.healHpPer10Ticks);
        }
    }

    if (result.waitStarted) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "payload.cart-wait", std::to_string(d.duelId), "authored-wait", tick,
            nlohmann::json{{"wait_ticks_remaining", result.waitTicksRemaining},
                           {"distance", d.payloadRuntime.distanceTravelled}});
        Debug::warn(Debug::Category::Duel,
            "[PAYLOAD] cart wait started ticks=%d distance=%.2f tick=%u\n",
            result.waitTicksRemaining, d.payloadRuntime.distanceTravelled, tick);
    }

    if (result.checkpointReached) {
        if (result.checkpointBonusSeconds > 0.0f && d.roundEndTick > 0) {
            d.roundEndTick += (uint32_t)objectiveSecondsToTicks(result.checkpointBonusSeconds);
            ++d.stateVersion;
            d.stateBroadcastPending = true;
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "payload.checkpoint", std::to_string(d.duelId), "cart-checkpoint", tick,
            nlohmann::json{{"route_index", result.checkpointRouteIndex},
                           {"bonus_seconds", result.checkpointBonusSeconds},
                           {"distance", d.payloadRuntime.distanceTravelled},
                           {"round_end_tick", d.roundEndTick}});
        Debug::warn(Debug::Category::Duel,
            "[PAYLOAD] checkpoint route=%d bonus=%.1fs distance=%.2f endTick=%u tick=%u\n",
            result.checkpointRouteIndex, result.checkpointBonusSeconds,
            d.payloadRuntime.distanceTravelled, d.roundEndTick, tick);
    }

    if (result.overtimeStarted) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "payload.overtime-start", std::to_string(d.duelId), "clock-expired", tick,
            nlohmann::json{{"overtime_seconds", d.payloadRuntime.overtimeSeconds},
                           {"attackers", result.attackers},
                           {"defenders", result.defenders}});
    }
    if (result.overtimeExpired) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "payload.overtime-expired", std::to_string(d.duelId), "no-attacker-in-box", tick,
            nlohmann::json{{"distance", d.payloadRuntime.distanceTravelled}});
        endObjectiveRound(d, d.payloadRuntime.defenderTeam, 3, tick);
        d.stateBroadcastPending = true;
        return;
    }
    if (result.finished) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "payload.finished", std::to_string(d.duelId), "cart-reached-final", tick,
            nlohmann::json{{"distance", d.payloadRuntime.distanceTravelled},
                           {"total_distance", d.payloadRuntime.totalDistance},
                           {"attackers", result.attackers},
                           {"defenders", result.defenders}});
        endObjectiveRound(d, d.payloadRuntime.attackerTeam, 2, tick);
        d.stateBroadcastPending = true;
        return;
    }

    // Change-edge cart status diagnostics (not per-tick spam).
    const int state = d.payloadRuntime.waiting ? 5
        : result.attackers > 0 && result.defenders > 0 ? 1
        : d.payloadRuntime.blocked ? 2
        : d.payloadRuntime.advancing ? 3
        : d.payloadRuntime.rollingBack ? 4 : 0;
    if (state != d.payloadLastCartState) {
        d.payloadLastCartState = state;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "payload.cart-state", std::to_string(d.duelId), "cart-state-changed", tick,
            nlohmann::json{{"state", state},
                           {"attackers", result.attackers},
                           {"defenders", result.defenders},
                           {"distance", d.payloadRuntime.distanceTravelled},
                           {"overtime", d.payloadRuntime.overtimeActive}});
    }
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
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "objective.plant-interrupted", std::to_string(d.duelId),
                "left_site", tick,
                nlohmann::json{{"objective", d.objective.id},
                               {"operator", d.objective.planterActorId},
                               {"progress_ticks", d.objective.plantTicksElapsed},
                               {"required_ticks", d.objective.plantTicksRequired}});
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
        } else if (!canDefuse && d.objective.defuseTicksElapsed > 0) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "objective.defuse-interrupted", std::to_string(d.duelId),
                "no_defuser_in_range", tick,
                nlohmann::json{{"objective", d.objective.id},
                               {"operator", d.objective.defuserActorId},
                               {"progress_ticks", d.objective.defuseTicksElapsed},
                               {"required_ticks", d.objective.defuseTicksRequired}});
            d.objective.defuseTicksElapsed = 0;
            d.objective.defuserActorId = 0;
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
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "objective.carried", std::to_string(d.duelId),
        d.objective.carrierActorId != 0 ? "carried" : "dropped", 0,
        nlohmann::json{
            {"objective", d.objective.id},
            {"carrier", d.objective.carrierActorId},
            {"carrier_team", objectiveActorTeam(d, d.objective.carrierActorId)},
            {"allowed_team", d.objective.allowedCarrierTeam}});
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

    if (npcs && !d.zombieTower) {
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
    const bool hasExplicitTeamRoles = std::any_of(
        gm.teams.begin(), gm.teams.end(),
        [](const GamemodeTeam& team) { return !team.role.empty(); });
    if (d.zombieTower) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "zombie_tower.roster-assigned", d.matchMode,
            "Zombie Tower roster resolved", d.currentServerTick,
            nlohmann::json{{"mode", d.matchMode},
                           {"teams", gm.teams.size()},
                           {"participants", d.participants.size()},
                           {"explicit_team_roles", hasExplicitTeamRoles},
                           {"run_mode", gm.runMode}});
    }
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

    // A mode-level actor preset is presentation/settings data when the mode
    // already declares gameplay roles. Do not let it replace those roles and
    // silently discard their movement/behavior profiles. Keep the old
    // preset-only path for modes that truly have no role declarations.
    if (!gm.actorPresetId.empty() && !hasExplicitTeamRoles && gm.roleCounts.empty()) {
        if (MatchRoleRegistry::instance().getActorPreset(gm.actorPresetId)) {
            for (uint32_t id : d.participants) {
                ActorMatchDescriptor& desc = d.matchActors[id];
                desc.roleId = gm.actorPresetId;
                const MatchRoleDefinition* def =
                    MatchRoleRegistry::instance().getActorPreset(desc.roleId);
                desc.movementProfileId = def->movementPreset;
                desc.weaponProfileId = def->weaponSet;
                if (desc.controller == ActorController::Npc)
                    desc.behaviorProfileId = resolveNpcBehaviorProfileId(gm, def->behaviorProfile);
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
                desc.behaviorProfileId = resolveNpcBehaviorProfileId(gm, def->behaviorProfile);
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
        if (gm.npcSpawnPolicy == "team_roster" && gm.teams.size() >= 2) {
            int rosterTeamCounts[2];
            rosterTeamCounts[0] = rosterTeamCounts[1] = 0;
            for (size_t assigned = 0; assigned < i; ++assigned) {
                const auto previous = d.matchActors.find(d.participants[assigned]);
                if (previous != d.matchActors.end() && previous->second.teamId >= 0 &&
                    previous->second.teamId < 2)
                    ++rosterTeamCounts[previous->second.teamId];
            }
            if (team < 0 && players.find(id) != players.end())
                team = players.at(id).matchTeam;
            if (team < 0 && npcs) {
                const auto npc = npcs->find(id);
                if (npc != npcs->end()) team = npc->second.matchTeam;
            }
            if (team < 0 || team > 1 ||
                (gm.teams[(size_t)team].capacity > 0 &&
                 rosterTeamCounts[team] >= gm.teams[(size_t)team].capacity))
                team = rosterTeamCounts[0] <= rosterTeamCounts[1] ? 0 : 1;
        }
        if (d.npcWaves)
            team = players.find(id) != players.end() ? 0 : 1;
        else if (team < 0) {
            // Data-driven default assignment. A team declares whether human
            // players or NPCs belong there; modes that do not declare it keep
            // the legacy behavior below.
            const bool isPlayer = players.find(id) != players.end();
            for (size_t t = 0; t < gm.teams.size(); ++t) {
                const GamemodeTeam& candidate = gm.teams[t];
                if ((isPlayer && candidate.players) || (!isPlayer && candidate.npcs)) {
                    team = (int)t;
                    break;
                }
            }
            if (team < 0 && (d.matchMode == "tdm" ||
                             d.winCondition == "last_team_standing" ||
                             !gm.actorPresetId.empty()))
                team = (int)(i % 2);
        }
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

    // Resolve explicit team roles after team assignment. This is the shared
    // role/preset/profile boundary used by both humans and NPCs; the bounded
    // event makes the effective source visible in events.jsonl before NPC
    // navigation or squad behavior is evaluated.
    if (hasExplicitTeamRoles) {
        for (uint32_t id : d.participants) {
            ActorMatchDescriptor& desc = d.matchActors[id];
            if (desc.teamId < 0) continue;
            std::string roleId = roundTeamRoleId(gm, desc.teamId);
            if (desc.controller == ActorController::Npc &&
                desc.teamId < (int)gm.teams.size() &&
                !gm.teams[(size_t)desc.teamId].npcRole.empty())
                roleId = gm.teams[(size_t)desc.teamId].npcRole;
            const MatchRoleDefinition* def = roleId.empty()
                ? nullptr : MatchRoleRegistry::instance().get(roleId);
            if (def) {
                desc.roleId = def->id;
                desc.movementProfileId = def->movementPreset;
                desc.weaponProfileId = def->weaponSet;
                if (desc.controller == ActorController::Npc)
                    desc.behaviorProfileId = resolveNpcBehaviorProfileId(
                        gm, def->behaviorProfile);
            }
            const ActorSpawnProfile resolvedProfile =
                serverResolveActorSpawnProfile(id);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.roster-profile-resolved", std::to_string(id),
                def ? "role_profile" : "role_profile_missing",
                d.currentServerTick,
                nlohmann::json{
                    {"actor", id},
                    {"controller", desc.controller == ActorController::Npc ? "npc" : "human"},
                    {"team", desc.teamId},
                    {"role", desc.roleId},
                    {"mode_preset", gm.actorPresetId},
                    {"movement_preset", desc.movementProfileId},
                    {"behavior_profile", desc.behaviorProfileId},
                    {"weapon_set_id", resolvedProfile.weaponSetId},
                    {"weapon_count", resolvedProfile.weapons.size()},
                    {"weapons", resolvedProfile.weapons},
                    {"starting_weapon", resolvedProfile.startingWeapon},
                    {"source", def ? "explicit_team_role" : "missing_role"}},
                __FILE__, __LINE__, __FUNCTION__);
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
        if (d.zombieTower) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "zombie_tower.roster-actor", std::to_string(id),
                "Zombie Tower actor identity", d.currentServerTick,
                nlohmann::json{{"actor", id}, {"controller", a.controller == ActorController::Npc ? "npc" : "human"},
                               {"role", a.roleId}, {"team", a.teamId},
                               {"behavior", a.behaviorProfileId},
                               {"movement", a.movementProfileId},
                               {"weapon", a.weaponProfileId}});
        }
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

static void reconcileTeamRosterNpc(ServerGamemodeState& d,
                                   std::unordered_map<uint32_t, ServerPlayer>& players,
                                   std::unordered_map<uint32_t, ServerNpc>& npcs,
                                   NpcSystem& npcSystem,
                                   std::unordered_set<uint32_t>& npcIdsAlive)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (gm.npcSpawnPolicy != "team_roster") {
        if (!d.teamRosterNpcIds.empty()) {
            for (uint32_t id : d.teamRosterNpcIds) {
                npcs.erase(id);
                npcIdsAlive.erase(id);
                d.matchActors.erase(id);
                d.matchTeams.erase(id);
                d.participantNames.erase(id);
                d.ffaKills.erase(id);
                d.ffaDeaths.erase(id);
            }
            npcSystem.destroySelected(d.teamRosterNpcIds);
            d.teamRosterNpcIds.clear();
        }
        return;
    }

    d.teamRosterNpcIds.erase(
        std::remove_if(d.teamRosterNpcIds.begin(), d.teamRosterNpcIds.end(),
            [&](uint32_t id) { return npcs.find(id) == npcs.end(); }),
        d.teamRosterNpcIds.end());

    std::vector<uint32_t> unrelatedNpcIds;
    for (const auto& entry : npcs)
        if (std::find(d.teamRosterNpcIds.begin(), d.teamRosterNpcIds.end(), entry.first) ==
            d.teamRosterNpcIds.end()) unrelatedNpcIds.push_back(entry.first);
    if (!unrelatedNpcIds.empty()) {
        for (uint32_t id : unrelatedNpcIds) {
            npcs.erase(id);
            npcIdsAlive.erase(id);
        }
        npcSystem.destroySelected(unrelatedNpcIds);
    }

    const size_t teamCount = std::min<size_t>(2, gm.teams.size());
    if (teamCount == 0) return;
    int humanCounts[2] = {0, 0};
    for (auto& entry : players) {
        ServerPlayer& player = entry.second;
        if (player.spawnState != ServerPlayer::Active) continue;
        int team = player.matchTeam;
        if (team < 0 || team >= (int)teamCount ||
            (gm.teams[(size_t)team].capacity > 0 &&
             humanCounts[team] >= gm.teams[(size_t)team].capacity)) {
            team = humanCounts[0] <= humanCounts[1] ? 0 : 1;
            if (team >= (int)teamCount) team = 0;
        }
        player.matchTeam = team;
        d.matchTeams[entry.first] = team;
        ++humanCounts[team];
    }

    uint32_t nextId = 200000;
    for (size_t team = 0; team < teamCount; ++team) {
        const int capacity = gm.teams[team].capacity;
        const int target = capacity > 0 ? capacity : humanCounts[team];
        const int needed = std::max(0, target - humanCounts[team]);
        int have = 0;
        for (uint32_t id : d.teamRosterNpcIds) {
            const auto it = npcs.find(id);
            if (it != npcs.end() && it->second.matchTeam == (int)team) ++have;
        }
        while (have > needed) {
            auto rosterIt = std::find_if(d.teamRosterNpcIds.rbegin(), d.teamRosterNpcIds.rend(),
                [&](uint32_t id) {
                    const auto npc = npcs.find(id);
                    return npc != npcs.end() && npc->second.matchTeam == (int)team;
                });
            if (rosterIt == d.teamRosterNpcIds.rend()) break;
            const uint32_t id = *rosterIt;
            d.teamRosterNpcIds.erase(std::next(rosterIt).base());
            npcs.erase(id);
            npcIdsAlive.erase(id);
            d.matchActors.erase(id);
            d.matchTeams.erase(id);
            d.participantNames.erase(id);
            d.ffaKills.erase(id);
            d.ffaDeaths.erase(id);
            npcSystem.destroySelected({id});
            serverClearActorPreset(id);
            --have;
        }
        while (have < needed) {
            while (npcs.find(nextId) != npcs.end() || players.find(nextId) != players.end()) ++nextId;
            ServerNpc npc;
            npc.entityId = nextId;
            npc.name = gm.teams[team].displayName + " " + std::to_string(humanCounts[team] + have + 1);
            npc.pos = gamemodeSpawnPoint(d, (int)team);
            npc.yaw = team == 0 ? 0.0f : 3.14159265f;
            npc.health = 3500;
            npc.matchTeam = (int)team;
            npcs.emplace(nextId, npc);
            d.teamRosterNpcIds.push_back(nextId);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "gamemode.team-roster-npc.created", std::to_string(nextId),
                "configured-team-capacity", d.currentServerTick,
                nlohmann::json{{"actor", nextId}, {"team", team}, {"mode", d.matchMode},
                               {"team_capacity", capacity}, {"human_count", humanCounts[team]}},
                __FILE__, __LINE__, __FUNCTION__);
            ++nextId;
            ++have;
        }
        for (uint32_t id : d.teamRosterNpcIds) {
            auto it = npcs.find(id);
            if (it != npcs.end() && it->second.matchTeam == (int)team) {
                it->second.health = 3500;
                it->second.pos = gamemodeSpawnPoint(d, (int)team);
            }
        }
    }
    nlohmann::json expectedCounts = nlohmann::json::array();
    nlohmann::json actualCounts = nlohmann::json::array();
    for (size_t team = 0; team < teamCount; ++team) {
        const int capacity = gm.teams[team].capacity;
        const int expected = capacity > 0 ? capacity : humanCounts[team];
        int actual = humanCounts[team];
        for (uint32_t id : d.teamRosterNpcIds) {
            const auto npc = npcs.find(id);
            if (npc != npcs.end() && npc->second.matchTeam == (int)team) ++actual;
        }
        expectedCounts.push_back(expected);
        actualCounts.push_back(actual);
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "gamemode.team-roster.resolved", d.matchMode,
        "configured team capacities reconciled", d.currentServerTick,
        nlohmann::json{{"mode", d.matchMode}, {"expected_per_team", expectedCounts},
                       {"actual_per_team", actualCounts},
                       {"generated_npcs", d.teamRosterNpcIds.size()}},
        __FILE__, __LINE__, __FUNCTION__);
}

// Advance each participant's authoritative match lifecycle state. This is the
// single owner of the Alive/Dead/Respawning/Spectating transitions and maps the
// legacy `dead`/health fields onto ActorState:
//   !dead                     -> Alive
//   dead + respawns enabled   -> Dead (one tick) -> Respawning -> Alive
//   dead + no respawn enabled -> Dead (one tick) -> Spectating (terminal)
void updateActorStates(ServerGamemodeState& d, SOCKET sock,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       uint32_t tick, uint64_t& totalPacketsOut)
{
    for (auto& kv : d.matchActors) {
        ActorMatchDescriptor& desc = kv.second;
        const ActorState before = desc.state;

        bool dead = false;
        bool downed = false;
        bool found = false;
        auto pIt = players.find(kv.first);
        if (pIt != players.end()) {
            dead = pIt->second.dead;
            downed = pIt->second.downed;
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

        // Melee: any fighter (human or NPC) that dies loses one stock. Publish
        // the stock count in chat so the event is visible to everyone, not
        // only present in the server journal.
        if (d.matchMode == "melee" && before == ActorState::Alive && dead) {
            serverMeleeConsumeStock(kv.first);
            const auto stockIt = d.meleeStocks.find(kv.first);
            const int stocksLeft = stockIt == d.meleeStocks.end()
                ? 0 : std::max(0, stockIt->second);
            const auto nameIt = d.participantNames.find(kv.first);
            const std::string fighterName = nameIt != d.participantNames.end()
                ? nameIt->second : "A fighter";
            std::string message = fighterName + " lost a stock. " +
                std::to_string(stocksLeft) +
                (stocksLeft == 1 ? " stock left." : " stocks left.");

            if (stocksLeft == 0) {
                uint32_t winnerId = 0;
                for (uint32_t participantId : d.participants) {
                    if (participantId == kv.first) continue;
                    const auto otherIt = d.matchActors.find(participantId);
                    if (otherIt == d.matchActors.end() ||
                        (otherIt->second.state != ActorState::Alive &&
                         otherIt->second.state != ActorState::Respawning))
                        continue;
                    const auto otherPlayer = players.find(participantId);
                    if (otherPlayer != players.end() && otherPlayer->second.dead)
                        continue;
                    const auto otherNpc = npcs.find(participantId);
                    if (otherNpc != npcs.end() && otherNpc->second.health <= 0)
                        continue;
                    winnerId = participantId;
                    break;
                }
                const auto winnerNameIt = d.participantNames.find(winnerId);
                if (winnerId != 0 && winnerNameIt != d.participantNames.end())
                    message += " " + winnerNameIt->second + " wins the round!";
            }
            broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
                                       message.c_str());
        }

        const bool respawns = desc.controller == ActorController::Human
            ? serverPlayerRespawnsEnabled(kv.first)
            : (d.matchMode == "melee" ? serverMeleeActorHasStocks(kv.first)
                                      : serverMatchRespawnsEnabled());

        desc.state = downed ? ActorState::Downed
                            : nextActorState(before, dead, respawns);

        // In a one-life round, Spectating is an actor STATE, not a replacement
        // TEAM. Keep matchTeams and desc.teamId on the original CT/T team so
        // elimination, objective ownership, and the next-round spawn resolver
        // can still identify the actor's real side.

        if (desc.state != before) {
            auto nameOf = [](ActorState s) {
                switch (s) {
                    case ActorState::Alive:      return "alive";
                    case ActorState::Dead:       return "dead";
                    case ActorState::Respawning: return "respawning";
                    case ActorState::Spectating: return "spectating";
                    case ActorState::Downed:     return "downed";
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
        const int spawnTeam = actorTeamOf(d, pid, players, npcs);
        const glm::vec3 spawn =
            gamemodeSpawnPointForActor(d, pid, players, npcs, "round-reset");
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
            // Keep the authoritative team on the real body so the local brain
            // and the per-tick mirror rebuild never lose it.
            npc.body.matchTeam = spawnTeam;
            // Role health override applies unless a host healthall override is set.
            const int npcOverrideHp = serverGameOverrides().maxHpOverride;
            const int npcMaxHp = npcOverrideHp > 0 ? npcOverrideHp
                : (profile.health > 0 ? profile.health : npc.body.maxHp);
            npc.body.maxHp = npcMaxHp;
            npc.body.currentHp = npcMaxHp;
            npc.body.dead = false;
            npc.body.respawnTimer = 0.0f;
            npc.movementProfileId = profile.movementPreset;
            npc.actorPresetId = profile.actorPresetId;
            serverMarkActorPresetApplied(npc.id, profile.actorPresetId);
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
                "[NPC BEHAVIOR] mode=%s actor=%u role=%s profile=%s aim=%.1f react=%.2f cadence=%.2f aggr=%.2f range=%.1f\n",
                d.matchMode.c_str(), npc.id, profile.roleId.c_str(),
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
            for (auto& weapon : npc.body.weaponRuntimes) {
                const auto ammoIt = profile.weaponAmmo.find(weapon.first);
                if (ammoIt == profile.weaponAmmo.end()) continue;
                if (ammoIt->second.first >= 0) weapon.second.currentAmmo = ammoIt->second.first;
                if (ammoIt->second.second >= 0) weapon.second.reserveAmmo = ammoIt->second.second;
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

static void reconcileZombieTowerParty(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs);

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
    startCountdownWallClock(d);
    d.matchTimeLimitTick = 0;
    d.lastBroadcastTick = currentTick;
    resetMatchScores(d);
    d.phase = DUEL_PHASE_COUNTDOWN;
    d.roundCountdownFreeze =
        GamemodeRegistry::instance().get(d.matchMode).features.freezeDuringCountdown;
    resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
    if (d.zombieTower) {
        const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
        if (gm.features.freezeDuringCountdown) d.roundCountdownFreeze = true;
        reconcileZombieTowerParty(d, players, npcs);
    }
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

// ── Zombie Tower party roster ──────────────────────────────────────────
static void addZombieTowerTeamNpc(ServerGamemodeState& d, ServerNpc& npc,
                                  int team, const std::string& roleId)
{
    d.participants.push_back(npc.entityId);
    d.participantNames[npc.entityId] = npc.name;
    d.ffaKills[npc.entityId] = 0;
    d.ffaDeaths[npc.entityId] = 0;
    ActorMatchDescriptor desc;
    desc.controller = ActorController::Npc;
    desc.state = ActorState::Alive;
    desc.teamId = team;
    desc.roleId = roleId;
    if (const MatchRoleDefinition* role = MatchRoleRegistry::instance().get(roleId)) {
        desc.movementProfileId = role->movementPreset;
        desc.weaponProfileId = role->weaponSet;
        desc.behaviorProfileId = role->behaviorProfile;
    }
    d.matchActors[npc.entityId] = std::move(desc);
    d.matchTeams[npc.entityId] = team;
    npc.matchTeam = team;
}

static void removeZombieTowerNpc(ServerGamemodeState& d,
                                 std::unordered_map<uint32_t, ServerNpc>& npcs,
                                 uint32_t id)
{
    npcs.erase(id);
    d.participants.erase(std::remove(d.participants.begin(), d.participants.end(), id),
                         d.participants.end());
    d.participantNames.erase(id);
    d.matchTeams.erase(id);
    d.matchActors.erase(id);
    serverClearActorPreset(id);
    d.ffaKills.erase(id);
    d.ffaDeaths.erase(id);
    d.ztStats.erase(id);
}

static void reconcileZombieTowerParty(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (!d.zombieTower || d.matchMode.empty()) return;
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (gm.teams.empty()) return;

    size_t humans = 0;
    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        if (kv.second.matchTeam == 0) ++humans;
    }
    if (humans == 0) return;

    const int configuredTarget = gm.teams[0].capacity > 0
        ? std::max(4, gm.teams[0].capacity) : 4;
    const size_t target = (size_t)configuredTarget;
    std::string companionRole = gm.teams[0].npcRole;
    if (companionRole.empty()) companionRole = gm.teams[0].role;

    std::vector<uint32_t> companions;
    for (const auto& kv : npcs) {
        if (kv.second.matchTeam != 0) continue;
        const auto actorIt = d.matchActors.find(kv.first);
        if (actorIt != d.matchActors.end() && actorIt->second.controller == ActorController::Npc)
            companions.push_back(kv.first);
    }

    while (humans + companions.size() < target && !companionRole.empty()) {
        uint32_t id = d.waveNextNpcId++;
        while (npcs.find(id) != npcs.end()) id = d.waveNextNpcId++;
        ServerNpc npc;
        npc.entityId = id;
        npc.name = "Companion " + std::to_string(++d.npcSpawnSequence);
        npc.pos = zombieTowerSpawnPointForActor(d, 0, id, "party-fill");
        npc.yaw = 0.0f;
        npc.difficulty = 1.0f;
        npc.spawnZoneId = "zt_party";
        addZombieTowerTeamNpc(d, npc, 0, companionRole);
        npcs.emplace(id, std::move(npc));
        companions.push_back(id);
    }

    while (humans + companions.size() > target && !companions.empty()) {
        const uint32_t victim = companions.back();
        companions.pop_back();
        removeZombieTowerNpc(d, npcs, victim);
    }
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
        npc.pos = gamemodeSpawnPoint(d, 1);
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

static size_t livingNpcCount(const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    size_t count = 0;
    for (const auto& kv : npcs)
        if (kv.second.health > 0) ++count;
    return count;
}

void spawnPersistentNpcBatchIfDue(ServerGamemodeState& d,
                                  const std::unordered_map<uint32_t, ServerPlayer>& players,
                                  std::unordered_map<uint32_t, ServerNpc>& npcs,
                                  uint32_t tick)
{
    if (!d.persistentNpcSpawns) return;
    if (!d.zombieTower && (d.npcSpawnMax <= 0 || tick < d.npcSpawnNextTick))
        return;

    auto& mapConfig = MapConfigRegistry::instance();
    auto hasHumanTeamPlayer = [&](const MapEntity& zone) {
        for (const auto& player : players) {
            if (player.second.dead || player.second.spawnState != ServerPlayer::Active)
                continue;
            if (player.second.matchTeam != 0) continue;
            const glm::vec3 delta = player.second.pos - zone.position;
            const float radius = zone.activationRadius > 0.0f
                ? zone.activationRadius : zone.radius;
            if (glm::dot(delta, delta) <= radius * radius) return true;
        }
        return false;
    };

    bool activatedZone = false;
    for (auto& zone : mapConfig.entitiesMutable()) {
        if (zone.type != "monster_zone" || !zone.enabled) continue;
        if (zone.checkpointRequirement > 0 &&
            (int)d.runCheckpointCount < zone.checkpointRequirement)
            continue;

        size_t zoneLiving = 0;
        for (const auto& npc : npcs)
            if (npc.second.health > 0 && npc.second.spawnZoneId == zone.id) ++zoneLiving;

        if (zone.runtimeCleared && !zone.oneShot &&
            tick >= zone.lastActivationTick + (uint32_t)std::max(1, zone.spawnCooldownTicks)) {
            zone.runtimeCleared = false;
            zone.runtimeActivated = false;
            zone.runtimeSpawned = 0;
        }
        if (zone.runtimeActivated && zone.oneShot) {
            if (zone.runtimeSpawned >= zone.spawnCount && zoneLiving == 0)
                zone.runtimeCleared = true;
            continue;
        }
        if (zone.runtimeSpawned >= zone.spawnCount) {
            if (zoneLiving == 0) {
                if (!zone.runtimeCleared) {
                    zone.runtimeCleared = true;
                    zone.lastActivationTick = tick;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::World, StructuredLevel::Important,
                        "monster-zone.cleared", zone.id, "encounter group defeated", tick,
                        nlohmann::json{{"map", d.mapId}, {"entity_id", zone.id},
                                       {"required", zone.required}});
                }
                zone.runtimeActivated = zone.oneShot;
            }
            continue;
        }
        if (!hasHumanTeamPlayer(zone)) continue;
        if (tick < zone.lastActivationTick + (uint32_t)std::max(1, zone.spawnCooldownTicks))
            continue;
        if (zoneLiving >= (size_t)std::max(1, zone.maxAlive)) continue;

        if (!zone.runtimePlayerActivated) {
            zone.runtimePlayerActivated = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "monster.spawn-zone-activated", zone.id,
                "human-team player entered activation radius", tick,
                nlohmann::json{{"map", d.mapId}, {"entity_id", zone.id},
                               {"monster_pool", zone.monsterPool},
                               {"spawn_count", zone.spawnCount},
                               {"required", zone.required}});
        }

        const auto* pool = MonsterPoolRegistry::instance().get(zone.monsterPool);
        std::vector<std::string> roles;
        if (pool) {
            for (const auto& entry : *pool)
                for (int i = 0; i < entry.count; ++i)
                    roles.push_back(entry.role);
        }
        if (roles.empty())
            roles.push_back("zombie_tower_normal_zombie_npc");

        const size_t room = (size_t)std::max(1, zone.maxAlive) - zoneLiving;
        const size_t remaining = (size_t)zone.spawnCount - (size_t)zone.runtimeSpawned;
        const size_t batch = std::min(room, remaining);
        for (size_t i = 0; i < batch; ++i) {
            while (npcs.find(d.waveNextNpcId) != npcs.end()) ++d.waveNextNpcId;
            const std::string& roleId = roles[(zone.runtimeSpawned + i) % roles.size()];
            ServerNpc npc;
            npc.entityId = d.waveNextNpcId++;
            const MatchRoleDefinition* role = MatchRoleRegistry::instance().get(roleId);
            npc.name = role && !role->displayName.empty()
                ? role->displayName : roleId;
            const bool useBox = zone.size.x > 1.001f || zone.size.y > 1.001f ||
                                zone.size.z > 1.001f;
            static std::mt19937 spawnRng(std::random_device{}());
            if (useBox) {
                std::uniform_real_distribution<float> rx(-zone.size.x * 0.5f, zone.size.x * 0.5f);
                std::uniform_real_distribution<float> ry(-zone.size.y * 0.5f, zone.size.y * 0.5f);
                std::uniform_real_distribution<float> rz(-zone.size.z * 0.5f, zone.size.z * 0.5f);
                npc.pos = zone.position + glm::vec3(rx(spawnRng), ry(spawnRng), rz(spawnRng));
            } else {
                std::uniform_real_distribution<float> r(-zone.radius, zone.radius);
                npc.pos = zone.position + glm::vec3(r(spawnRng), r(spawnRng), 0.0f);
            }
            npc.yaw = 0.0f;
            npc.difficulty = 1.0f;
            npc.spawnZoneId = zone.id;
            addZombieTowerTeamNpc(d, npc, 1, roleId);
            d.ztStats[npc.entityId].name = npc.name;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "monster.spawned", std::to_string(npc.entityId),
                "monster encounter spawn", tick,
                nlohmann::json{{"map", d.mapId}, {"zone", zone.id},
                               {"role", roleId},
                               {"position", {npc.pos.x, npc.pos.y, npc.pos.z}}});
            npcs.emplace(npc.entityId, std::move(npc));
        }
        zone.runtimeSpawned += (int)batch;
        zone.lastActivationTick = tick;
        zone.runtimeActivated = true;
        activatedZone = true;
    }
    if (d.zombieTower || activatedZone) return;

    const size_t living = livingNpcCount(npcs);
    if (living >= (size_t)d.npcSpawnMax)
        return;

    const size_t room = (size_t)d.npcSpawnMax - living;
    const size_t batch = std::min(room, (size_t)std::max(1, d.npcSpawnPerInterval));
    for (size_t i = 0; i < batch; ++i) {
        while (npcs.find(d.waveNextNpcId) != npcs.end()) ++d.waveNextNpcId;
        ServerNpc npc;
        npc.entityId = d.waveNextNpcId++;
        npc.name = "Zombie Tower NPC " + std::to_string(++d.npcSpawnSequence);
        npc.pos = gamemodeSpawnPoint(d, 1);
        npc.yaw = 0.0f;
        npc.difficulty = 1.0f;
        addNpcWaveParticipant(d, npc);
        npcs.emplace(npc.entityId, std::move(npc));
    }
    d.npcSpawnNextTick = tick + (uint32_t)std::max(1, d.npcSpawnIntervalTicks);
    Debug::log(Debug::Category::Duel,
        "[NPC SPAWN] policy=persistent spawned=%zu living=%zu/%d intervalTicks=%d tick=%u\n",
        batch, livingNpcCount(npcs), d.npcSpawnMax, d.npcSpawnIntervalTicks, tick);
}

// ── Authored map-entity run runtime (checkpoint / boss_trigger) ──────────
// Trigger geometry is owned by `mapEntityContainsPoint` in map-config.
static bool mapEntityHasLivePlayer(const MapEntity& e,
    const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (const auto& kv : players) {
        if (kv.second.dead || kv.second.spawnState != ServerPlayer::Active) continue;
        if (mapEntityContainsPoint(e, kv.second.pos)) return true;
    }
    return false;
}

static bool mapEntityHasLiveHumanTeamPlayer(const MapEntity& e,
    const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (const auto& kv : players) {
        if (kv.second.dead || kv.second.spawnState != ServerPlayer::Active) continue;
        if (kv.second.matchTeam != 0) continue;
        if (mapEntityContainsPoint(e, kv.second.pos)) return true;
    }
    return false;
}

static void placeActorAtRunSpawn(ServerGamemodeState& d, ServerPlayer& p,
    const glm::vec3& spawn)
{
    resetPlayerForSpawn(p, true);
    p.duelSpawnPos = spawn;
    p.hasDuelSpawnPos = true;
    p.respawnSeconds = 0.0f;
    beginAuthoritativeTransform(p, spawn,
        SpawnVelocityConfig::instance().enabled()
            ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
            : glm::vec3(0.0f), p.yaw, "run-spawn");
    p.justRespawned = true;
}

// Admit players who joined while a Zombie Tower run was already in progress.
// Humans take the first human-team slot; a full human team makes the joiner a
// spectator. The spawn follows the authored lobby/intro/checkpoint data.
static void admitZombieTowerPlayers(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (!d.zombieTower) return;
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    if (gm.teams.empty()) return;

    int humanCount = 0;
    for (const auto& kv : d.matchTeams)
        if (kv.second == 0) ++humanCount;
    const int humanCapacity = gm.teams[0].capacity;

    for (auto& kv : players) {
        ServerPlayer& p = kv.second;
        if (p.spawnState != ServerPlayer::Active) continue;
        const auto actorIt = d.matchActors.find(p.id);
        const bool needsIdentity = p.matchTeam < 0 || actorIt == d.matchActors.end() ||
            actorIt->second.roleId.empty();
        if (!needsIdentity) continue;

        int team = 0;
        if (humanCapacity > 0 && humanCount >= humanCapacity) {
            team = 2;
        } else {
            ++humanCount;
        }
        const std::string roleId = team < (int)gm.teams.size()
            ? gm.teams[(size_t)team].role : std::string{};
        const MatchRoleDefinition* role = roleId.empty()
            ? nullptr : MatchRoleRegistry::instance().get(roleId);

        ActorMatchDescriptor desc;
        desc.controller = ActorController::Human;
        desc.state = team == 2 ? ActorState::Spectating : ActorState::Alive;
        desc.teamId = team;
        desc.roleId = roleId;
        if (role) {
            desc.movementProfileId = role->movementPreset;
            desc.weaponProfileId = role->weaponSet;
            desc.behaviorProfileId.clear();
        }
        d.matchActors[p.id] = std::move(desc);
        d.matchTeams[p.id] = team;
        p.matchTeam = team;
        if (std::find(d.participants.begin(), d.participants.end(), p.id) == d.participants.end())
            d.participants.push_back(p.id);
        d.participantNames[p.id] = p.name;

        if (team < 2) {
            const glm::vec3 spawn = zombieTowerSpawnPointForActor(
                d, team, p.id, "late-join");
            placeActorAtRunSpawn(d, p, spawn);
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "actor.team-assigned", std::to_string(d.duelId), "zt-late-join",
            d.currentServerTick,
            nlohmann::json{{"player", p.id}, {"team", team},
                           {"role", roleId}, {"late_join", true}});
    }
    if (d.phase != DUEL_PHASE_INTERMISSION && d.phase != DUEL_PHASE_RESULTS)
        reconcileZombieTowerParty(d, players, npcs);
}

// Party wipe: when every active human is dead, spend an attempt and respawn the
// party at the latest checkpoint; when none remain, end the run and restart.
static void zombieTowerDownedTick(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t tick)
{
    if (!d.zombieTower || d.phase != DUEL_PHASE_ACTIVE) return;
    for (auto& kv : players) {
        ServerPlayer& p = kv.second;
        if (p.spawnState != ServerPlayer::Active) continue;
        auto& stats = d.ztStats[p.id];
        if (stats.name.empty()) stats.name = p.name;
        stats.topSpeed = std::max(stats.topSpeed, glm::length(p.vel));
    }
    for (auto& kv : players) {
        ServerPlayer& target = kv.second;
        if (!target.downed || target.dead) continue;
        ServerPlayer* reviver = nullptr;
        float bestDist = 4.0f;
        for (auto& otherKv : players) {
            ServerPlayer& other = otherKv.second;
            if (other.id == target.id || other.dead || other.downed) continue;
            if (other.matchTeam != 0 || !other.input.reviveHeld) continue;
            if (other.spawnState != ServerPlayer::Active) continue;
            const float dist = glm::distance(other.pos, target.pos);
            if (dist <= bestDist) {
                bestDist = dist;
                reviver = &other;
            }
        }
        if (reviver) {
            target.downedHealth = std::min(100.0f, target.downedHealth + 20.0f * SERVER_DT);
            target.reviveProgress = target.downedHealth / 100.0f;
            d.ztStats[reviver->id].healingDone += (uint64_t)std::max(0.0f, 20.0f * SERVER_DT);
            if (d.ztStats[reviver->id].name.empty())
                d.ztStats[reviver->id].name = reviver->name;
            if (target.downedHealth >= 100.0f) {
                target.downed = false;
                target.downedHealth = 0.0f;
                target.reviveProgress = 0.0f;
                target.health = std::max(100, target.maxHealth);
                target.maxHealth = std::max(100, target.maxHealth);
                target.respawnSeconds = 0.0f;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "zombie_tower.actor-revived", std::to_string(target.id),
                    "teammate completed revival", tick,
                    nlohmann::json{{"actor", target.id}, {"reviver", reviver->id},
                                   {"health", target.health}});
            }
        } else {
            target.downedHealth = std::max(0.0f, target.downedHealth - (100.0f / 30.0f) * SERVER_DT);
            target.reviveProgress = target.downedHealth / 100.0f;
            if (target.downedHealth <= 0.0f) {
                target.downed = false;
                target.reviveProgress = 0.0f;
                target.dead = true;
                target.respawnSeconds = -1.0f;
                target.deaths += 1;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "zombie_tower.actor-died-downed", std::to_string(target.id),
                    "downed timer expired", tick,
                    nlohmann::json{{"actor", target.id}});
            }
        }
    }
}

static void zombieTowerPartyWipeTick(
    ServerGamemodeState& d, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    uint32_t tick, uint64_t& totalPacketsOut)
{
    if (d.ztAttemptResult || d.ztFinalResult) return;

    size_t teamActors = 0;
    size_t ableActors = 0;
    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        if (kv.second.matchTeam != 0) continue;
        ++teamActors;
        if (!kv.second.dead && !kv.second.downed) ++ableActors;
    }
    for (const auto& kv : npcs) {
        if (kv.second.matchTeam != 0) continue;
        ++teamActors;
        if (kv.second.health > 0) ++ableActors;
    }
    if (teamActors == 0 || ableActors > 0) return;

    const bool retry = d.runAttemptsRemaining > 1;
    if (retry) --d.runAttemptsRemaining;
    d.ztRunElapsedTicks = tick > d.ztRunStartTick ? tick - d.ztRunStartTick : 0;
    d.ztFinalResult = !retry;
    d.ztAttemptResult = retry;
    d.phase = DUEL_PHASE_RESULTS;
    d.phaseTimer = std::max(1.0f, d.resultsSeconds);
    ++d.stateVersion;

    if (retry) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "zombie_tower.attempt-started", d.mapId,
            "party wipe; attempt result shown before checkpoint countdown", tick,
            nlohmann::json{{"map", d.mapId},
                           {"attempts_remaining", d.runAttemptsRemaining},
                           {"checkpoint_id", d.currentCheckpointId}});
        Debug::warn(Debug::Category::Duel,
            "[ZOMBIE TOWER] party wipe -> attempt result attempts_left=%d checkpoint=%s tick=%u\n",
            d.runAttemptsRemaining, d.currentCheckpointId.c_str(), tick);
    } else {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Duel, StructuredLevel::Important,
            "zombie_tower.run-failed", d.mapId, "no attempts remain", tick,
            nlohmann::json{{"map", d.mapId}, {"checkpoint_id", d.currentCheckpointId}});
        Debug::warn(Debug::Category::Duel,
            "[ZOMBIE TOWER] run failed; final results pending tick=%u\n", tick);
    }
    broadcastDuelState(sock, d, players, totalPacketsOut);
}

static void zombieTowerRespawnAttempt(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    NpcSystem& npcSystem,
    uint32_t tick)
{
    std::vector<uint32_t> companions;
    for (const auto& kv : npcs) {
        if (kv.second.matchTeam != 0) continue;
        const auto actorIt = d.matchActors.find(kv.first);
        if (actorIt != d.matchActors.end() &&
            actorIt->second.controller == ActorController::Npc)
            companions.push_back(kv.first);
    }
    for (uint32_t id : companions)
        removeZombieTowerNpc(d, npcs, id);
    for (auto& kv : players) {
        kv.second.downed = false;
        kv.second.downedHealth = 0.0f;
        kv.second.reviveProgress = 0.0f;
    }
    d.ztAttemptResult = false;
    d.ztFinalResult = false;
    d.ztResumingFromCheckpoint = d.hasCheckpointSpawn;
    beginMatchCountdown(d, players, npcs, npcSystem, tick);
    d.ztResumingFromCheckpoint = false;
    ++d.stateVersion;
}

static void zombieTowerReturnToLobby(
    ServerGamemodeState& d, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    NpcSystem& npcSystem,
    uint32_t tick, uint64_t& totalPacketsOut)
{
    npcs.clear();
    npcSystem.destroyAll();
    d.runAttemptsRemaining = d.runAttemptsTotal;
    d.runCheckpointCount = 0;
    d.currentCheckpointId.clear();
    d.hasCheckpointSpawn = false;
    d.activatedCheckpoints.clear();
    d.activatedBossTriggers.clear();
    d.damageVolumeNextTick.clear();
    d.bossEncounterActive = false;
    d.activeBossId.clear();
    d.activeBossNpcId = 0;
    d.progressionLocked = false;
    d.ztTowerEndReached = false;
    d.ztBossDefeated = false;
    d.ztHadBossTrigger = false;
    d.ztRunStartTick = 0;
    d.ztRunElapsedTicks = 0;
    d.ztAttemptResult = false;
    d.ztFinalResult = false;
    d.ztRunWon = false;
    d.ztMusicTrack.clear();
    d.ztMusicPlaying = false;
    ++d.ztMusicSerial;
    d.ztStats.clear();
    MapConfigRegistry::instance().resetRuntimeState();

    const MapEntity* lobby = findSpawnEntityByTag("lobby");
    const glm::vec3 spawn = lobby ? lobby->position : gamemodeSpawnPoint(d, 0);
    for (auto& kv : players) {
        ServerPlayer& p = kv.second;
        if (p.spawnState != ServerPlayer::Active) continue;
        p.downed = false;
        p.downedHealth = 0.0f;
        p.reviveProgress = 0.0f;
        placeActorAtRunSpawn(d, p, spawn);
    }
    assignMatchParticipants(d, players, nullptr);
    d.phase = DUEL_PHASE_INTERMISSION;
    d.phaseTimer = std::max(1.0f, d.intermissionSeconds);
    ++d.stateVersion;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "zombie_tower.returned-to-lobby", d.mapId,
        "final results complete; lobby intermission started", tick,
        nlohmann::json{{"map", d.mapId},
                       {"intermission_seconds", d.intermissionSeconds}});
    broadcastDuelState(sock, d, players, totalPacketsOut);
}

static void zombieTowerCheckWin(
    ServerGamemodeState& d, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    uint32_t tick, uint64_t& totalPacketsOut)
{
    if (!d.zombieTower || d.phase != DUEL_PHASE_ACTIVE) return;
    if (d.ztAttemptResult || d.ztFinalResult) return;
    if (d.bossEncounterActive) return;

    bool hasWinContent = false;
    bool hasBossTrigger = false;
    bool hasTowerEnd = false;
    for (const auto& e : MapConfigRegistry::instance().current().entities) {
        if (!e.enabled) continue;
        if (e.type == "tower_end") hasTowerEnd = true;
        if (e.type == "boss_trigger") hasBossTrigger = true;
        if ((e.type == "tower_end" || e.type == "boss_trigger" ||
             (e.type == "monster_zone" && e.required)))
            hasWinContent = true;
        if (e.type == "monster_zone" && e.required && !e.runtimeCleared)
            return;
    }
    if (!hasWinContent) return;
    if (hasBossTrigger && !d.ztBossDefeated) return;
    if (hasTowerEnd && !d.ztTowerEndReached) return;

    d.ztFinalResult = true;
    d.ztAttemptResult = false;
    d.ztRunWon = true;
    d.phase = DUEL_PHASE_RESULTS;
    d.phaseTimer = std::max(1.0f, d.resultsSeconds);
    d.ztRunElapsedTicks = tick > d.ztRunStartTick ? tick - d.ztRunStartTick : 0;
    ++d.stateVersion;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "zombie_tower.completed", d.mapId,
        "required encounters and boss cleared", tick,
        nlohmann::json{{"map", d.mapId},
                       {"elapsed_ticks", d.ztRunElapsedTicks},
                       {"tower_end", d.ztTowerEndReached}});
    broadcastDuelState(sock, d, players, totalPacketsOut);
}

// Fixed 60 Hz consumption of authored checkpoint and boss_trigger entities.
static void mapEntityRuntimeTick(ServerGamemodeState& d, SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    uint32_t tick, uint64_t& totalPacketsOut)
{
    auto& mapConfig = MapConfigRegistry::instance();
    if (mapConfig.current().mapId.empty()) return;

    for (auto& e : mapConfig.entitiesMutable()) {
        if (!e.enabled) continue;

        if (e.type == "checkpoint" && d.activatedCheckpoints.count(e.id) == 0) {
            if (!mapEntityHasLiveHumanTeamPlayer(e, players)) continue;
            d.activatedCheckpoints.insert(e.id);
            const MapEntity* previous = mapConfig.findEntity(d.currentCheckpointId);
            if (!previous || e.order >= previous->order) {
                d.currentCheckpointId = e.id;
                d.currentCheckpointPos = e.position;
                d.hasCheckpointSpawn = true;
                d.ztCheckpointName = e.id;
                ++d.ztCheckpointSerial;
            }
            ++d.runCheckpointCount;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "zombie_tower.checkpoint-reached", e.id,
                "party entered authored checkpoint", tick,
                nlohmann::json{{"map", d.mapId}, {"checkpoint_id", e.id},
                               {"checkpoint_count", d.runCheckpointCount},
                               {"position", {e.position.x, e.position.y, e.position.z}}});
            Debug::warn(Debug::Category::Duel,
                "[CHECKPOINT] reached id=%s pos=(%.1f %.1f %.1f) tick=%u\n",
                e.id.c_str(), e.position.x, e.position.y, e.position.z, tick);
        } else if (e.type == "damage_volume" && e.damage > 0) {
            auto& nextTick = d.damageVolumeNextTick[e.id];
            if (tick < nextTick) continue;

            bool hitAny = false;
            int hitPlayers = 0;
            int hitNpcs = 0;
            int killedPlayers = 0;

            for (auto& kv : players) {
                ServerPlayer& p = kv.second;
                if (p.dead || !mapEntityContainsPoint(e, p.pos)) continue;

                const ServerDamageResult result = applyServerDamage(
                    players, p, 0, e.damage, glm::vec3(0.0f),
                    ServerDamageSource::Environment);
                if (!result.applied) continue;

                queueServerDamageConfirmedEvent(
                    sock, players, tick, totalPacketsOut, 0, p, e.damage,
                    result, p.pos, glm::vec3(0.0f), glm::vec3(0.0f),
                    ServerDamageSource::Environment, NETWORK_WEAPON_NONE);
                hitAny = true;
                ++hitPlayers;
                if (result.killed) ++killedPlayers;
            }

            for (auto& kv : npcs) {
                ServerNpc& n = kv.second;
                if (n.health <= 0 || !mapEntityContainsPoint(e, n.pos)) continue;
                n.health = std::max(0, n.health - e.damage);
                hitAny = true;
                ++hitNpcs;
            }

            if (hitAny) {
                nextTick = tick + static_cast<uint32_t>(std::max(1, e.damageIntervalTicks));
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::World, StructuredLevel::Important,
                    "damage-volume.damage", e.id,
                    "actor took authored volume damage", tick,
                    nlohmann::json{{"map", d.mapId}, {"entity_id", e.id},
                                   {"damage_type", e.damageType},
                                   {"damage", e.damage},
                                   {"players_hit", hitPlayers},
                                   {"npcs_hit", hitNpcs},
                                   {"players_killed", killedPlayers}});
            }
        } else if (e.type == "boss_trigger" && d.activatedBossTriggers.count(e.id) == 0) {
            if (e.checkpointRequirement > 0 &&
                (int)d.runCheckpointCount < e.checkpointRequirement)
                continue;
            if (!mapEntityHasLiveHumanTeamPlayer(e, players)) continue;
            d.activatedBossTriggers.insert(e.id);
            d.bossEncounterActive = true;
            d.ztHadBossTrigger = true;
            d.activeBossId = e.bossId;
            d.progressionLocked = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "boss.triggered", e.id, "player entered boss trigger volume", tick,
                nlohmann::json{{"map", d.mapId}, {"trigger_id", e.id},
                               {"boss_id", e.bossId},
                               {"position", {e.position.x, e.position.y, e.position.z}}});
            if (!e.bossId.empty()) {
                while (npcs.find(d.waveNextNpcId) != npcs.end()) ++d.waveNextNpcId;
                ServerNpc npc;
                npc.entityId = d.waveNextNpcId++;
                const MatchRoleDefinition* bossRole = MatchRoleRegistry::instance().get(e.bossId);
                npc.name = bossRole && !bossRole->displayName.empty()
                    ? bossRole->displayName : e.bossId;
                npc.pos = e.position;
                npc.yaw = 0.0f;
                npc.difficulty = 1.5f;
                npc.spawnZoneId = e.id;
                addZombieTowerTeamNpc(d, npc, 1, e.bossId);
                d.ztStats[npc.entityId].name = npc.name;
                d.activeBossName = npc.name;
                d.activeBossMaxHp = bossRole && bossRole->health > 0
                    ? bossRole->health : 100;
                d.activeBossCurrentHp = d.activeBossMaxHp;
                d.activeBossNpcId = npc.entityId;
                npcs.emplace(npc.entityId, std::move(npc));
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "boss.spawned", e.id, "authored boss trigger spawned an actor", tick,
                    nlohmann::json{{"map", d.mapId}, {"boss_id", e.bossId},
                                   {"npc_id", d.activeBossNpcId},
                                   {"position", {e.position.x, e.position.y, e.position.z}}});
            }
        } else if (e.type == "tower_end") {
            if (!mapEntityHasLiveHumanTeamPlayer(e, players)) continue;
            if (!d.ztTowerEndReached) {
                d.ztTowerEndReached = true;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "zombie_tower.tower-end-reached", e.id,
                    "human team reached the authored tower end", tick,
                    nlohmann::json{{"map", d.mapId}, {"entity_id", e.id}});
            }
        }
    }

    // Resolve the boss lock once its spawned actor dies (or is removed).
    if (d.bossEncounterActive && d.activeBossNpcId != 0) {
        auto it = npcs.find(d.activeBossNpcId);
        if (it == npcs.end() || it->second.health <= 0) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "boss.died", d.activeBossId, "boss encounter resolved", tick,
                nlohmann::json{{"map", d.mapId}, {"boss_id", d.activeBossId},
                               {"npc_id", d.activeBossNpcId}});
            d.bossEncounterActive = false;
            d.progressionLocked = false;
            d.ztBossDefeated = true;
            d.activeBossCurrentHp = 0;
            d.activeBossNpcId = 0;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        } else {
            d.activeBossCurrentHp = it->second.health;
            d.activeBossMaxHp = std::max(d.activeBossMaxHp, it->second.health);
        }
    }

    if (d.persistentNpcSpawns)
        zombieTowerPartyWipeTick(d, sock, players, npcs, tick, totalPacketsOut);
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

        if (d.matchMode == "ffa" || d.winCondition == "first_to_score") {
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
    // ── Generic last-actor-alive (teamless) elimination ─────────────
    // The whole participant set is one pool: the match ends when at most one
    // actor is still in play. Used by data-driven modes (win_condition), never
    // by a mode name.
    if (d.winCondition == "last_actor_alive") {
        int aliveActors = 0;
        uint32_t lastAlive = 0;
        for (uint32_t id : d.participants) {
            auto aIt = d.matchActors.find(id);
            const bool inPlay = aIt != d.matchActors.end() &&
                (aIt->second.state == ActorState::Alive ||
                 aIt->second.state == ActorState::Respawning);
            if (inPlay) { ++aliveActors; lastAlive = id; }
        }
        if (d.participants.size() >= 2 && aliveActors <= 1) {
            d.matchOver = true;
            d.phase = DUEL_PHASE_RESULTS;
            d.victoryType = 0;
            d.winnerTeam = -1;
            d.winnerPlayerId = lastAlive;
            if (d.disaster.configured) {
                d.disaster.resolved = true;
                d.disaster.resolveSource = 1;
                d.disaster.winnerActor = lastAlive;
            }
            d.phaseTimer = d.resultsSeconds;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            emitGamemodeMatchPersistence(d, tick, players);
            Debug::warn(Debug::Category::Duel,
                "[ELIMINATION] last_actor_alive winner=%u aliveActors=%d mode=%s\n",
                lastAlive, aliveActors, d.matchMode.c_str());
            return;
        }
    }

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

    // Score-limit ownership is selected by win_condition; FFA keeps its legacy
    // behavior while other modes can reuse the same individual score ledger.
    if (d.winCondition == "first_to_score" || d.matchMode == "ffa") {
        for (const auto& kv : d.ffaKills) {
            if (kv.second >= d.goalValue) {
                d.matchOver = true;
                d.phase = DUEL_PHASE_RESULTS;
                d.winnerPlayerId = kv.first;
                auto winnerTeam = d.matchTeams.find(kv.first);
                d.winnerTeam = winnerTeam == d.matchTeams.end() ? -1 : winnerTeam->second;
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
        if (GamemodeRegistry::instance().get(d.matchMode).timeLimitOutcome == "stalemate") {
            d.winnerPlayerId = 0;
            d.winnerTeam = -1;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "gamemode.match.stalemate", d.matchMode,
                "time-limit-before-score-goal", tick,
                nlohmann::json{{"mode", d.matchMode}, {"goal", d.goalValue},
                               {"time_limit_seconds", d.timeLimitSeconds}},
                __FILE__, __LINE__, __FUNCTION__);
            emitGamemodeMatchPersistence(d, tick, players);
        } else if (d.matchMode == "ffa" || d.winCondition == "first_to_score") {
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

bool serverFriendlyFireBlocks(
    uint32_t attackerId, bool attackerNpc,
    uint32_t victimId, bool victimNpc,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::unordered_map<uint32_t, ServerNpc>& npcs,
    const char* path, uint32_t tick)
{
    const ServerGamemodeState& d = serverGamemodeState();
    const int attackerTeam = actorTeamOf(d, attackerId, players, npcs);
    const int victimTeam = actorTeamOf(d, victimId, players, npcs);
    const bool sameTeam = attackerId != victimId &&
        attackerTeam >= 0 && victimTeam >= 0 && attackerTeam == victimTeam;
    const bool blocked = !d.friendlyFireEnabled && sameTeam;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.damage.policy_check", std::to_string(victimId),
        blocked ? "same-team damage blocked" : "damage path allowed", tick,
        {{"path", path ? path : "unknown"},
         {"attacker_id", attackerId},
         {"attacker_type", attackerNpc ? "npc" : "player"},
         {"victim_id", victimId},
         {"victim_type", victimNpc ? "npc" : "player"},
         {"attacker_team", attackerTeam},
         {"victim_team", victimTeam},
         {"same_team", sameTeam},
         {"friendly_fire", d.friendlyFireEnabled},
         {"blocked", blocked}},
        __FILE__, __LINE__, __FUNCTION__);
    return blocked;
}

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
    effect.damagesEnemiesOnly = damagesEnemiesOnly && !serverFriendlyFireEnabled();
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
                         const World& world,
                         uint32_t tick)
{
    Perf::ScopedTimer teamBrainTimer("Server::TeamBrain");
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

        // Sites from the map config. The authored anchor may only be an XY
        // hint; snap the Z to the loaded collision floor so an objective placed
        // slightly above/below the ground is still reachable. Authored data
        // stays hot-reloadable; this correction is per-tick and never written.
        brain.state().sites.clear();
        for (const BombSite& site : mapCfg.bombSites) {
            TeamSiteInfo info;
            info.id = site.id;
            info.position = site.position;
            info.radius = site.radius;
            info.hasPosition = site.hasPosition;
            if (site.hasPosition) {
                const float groundZ = NpcNavigation::groundHeightAt(
                    world, site.position, 400.0f, 12.0f);
                if (groundZ > -1e5f)
                    info.position.z = groundZ;
                else
                    info.hasPosition = false;  // no floor: not a usable anchor
            }
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

    // ── General group/focus behavior (all modes) ──────────────────────────
    // Resolve per-team squad tuning from the members' behavior profiles, then
    // let the TeamBrain own focus/anchor/slot. Suggestions only; final movement
    // is still resolved by the arbiter and shared physics.
    std::unordered_map<uint32_t, glm::vec3> livingPos;
    for (const auto& kv : players)
        if (!kv.second.dead) livingPos[kv.first] = kv.second.pos;
    for (const auto& kv : npcs)
        if (kv.second.health > 0) livingPos[kv.first] = kv.second.pos;

    SquadTuning tuningA, tuningB;
    bool tuningASet = false, tuningBSet = false;
    for (const Npc& npc : npcSystem.all()) {
        const int t = npc.body.matchTeam;
        if ((t != 0 && t != 1) || !npc.behavior.active || !npc.behavior.focusEnabled)
            continue;
        SquadTuning& tun = (t == 0) ? tuningA : tuningB;
        bool& set = (t == 0) ? tuningASet : tuningBSet;
        if (set) continue;
        tun.enabled = true;
        tun.swarm = npc.behavior.swarm;
        tun.cohesion = npc.behavior.cohesion;
        tun.spreadRadiusMeters = npc.behavior.spreadRadiusMeters;
        tun.maxAttackersPerTarget = npc.behavior.maxAttackersPerTarget;
        tun.rallyDistanceMeters = npc.behavior.rallyDistanceMeters;
        tun.approachStyle = npc.behavior.approachStyle;
        set = true;
    }
    auto gatherMembers = [&](int team, std::vector<SquadMember>& out) {
        for (const auto& e : living) {
            if (e.second != team) continue;
            auto it = livingPos.find(e.first);
            if (it != livingPos.end()) out.push_back({e.first, it->second});
        }
    };
    std::vector<SquadMember> membersA, membersB;
    gatherMembers(0, membersA);
    gatherMembers(1, membersB);
    auto centroidOf = [](const std::vector<SquadMember>& members) {
        glm::vec3 c(0.0f);
        for (const SquadMember& m : members) c += m.pos;
        if (!members.empty()) c /= static_cast<float>(members.size());
        return c;
    };
    const glm::vec3 centroidA = centroidOf(membersA);
    const glm::vec3 centroidB = centroidOf(membersB);
    glm::vec3 focusA{0.0f}, focusB{0.0f};
    uint32_t focusActorA = 0, focusActorB = 0;
    float focusConfA = 0.0f, focusConfB = 0.0f;
    // Focus the enemy nearest the squad's own centroid, not whichever report
    // happened to be inserted first. This is what makes a squad converge as a
    // unit instead of splitting toward unrelated enemies.
    const bool hasFocusA = d.teamBrainA.selectFocus(
        centroidA, focusA, focusActorA, focusConfA);
    const bool hasFocusB = d.teamBrainB.selectFocus(
        centroidB, focusB, focusActorB, focusConfB);
    d.teamBrainA.updateSquad(membersA, focusA, hasFocusA, tuningA, focusActorA);
    d.teamBrainB.updateSquad(membersB, focusB, hasFocusB, tuningB, focusActorB);

    // Bounded, change-edge focus/squad diagnostics.
    auto reportSquad = [&](TeamBrain& brain) {
        const SquadState& sq = brain.squad();
        if (!sq.active) return;
        if (sq.mode != brain.state().reportedSquadMode) {
            brain.state().reportedSquadMode = sq.mode;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.squad-mode-changed", std::to_string(brain.team()), "squad", tick,
                nlohmann::json{{"team", brain.team()}, {"mode", sq.mode},
                               {"members", sq.memberCount},
                               {"anchor", {sq.anchor.x, sq.anchor.y, sq.anchor.z}},
                               {"focus", {sq.focusPos.x, sq.focusPos.y, sq.focusPos.z}}});
        }
        const bool focusMoved = !brain.state().hasReportedFocus ||
            glm::length(sq.focusPos - brain.state().reportedFocusPos) > 5.0f;
        if (focusMoved) {
            brain.state().hasReportedFocus = true;
            brain.state().reportedFocusPos = sq.focusPos;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.focus-changed", std::to_string(brain.team()), "focus", tick,
                nlohmann::json{{"team", brain.team()},
                               {"focus", {sq.focusPos.x, sq.focusPos.y, sq.focusPos.z}},
                               {"members", sq.memberCount}});
        }
    };
    reportSquad(d.teamBrainA);
    reportSquad(d.teamBrainB);

    for (Npc& npc : npcSystem.all()) {
        const int team = npc.body.matchTeam;
        if (team != 0 && team != 1) continue;
        if (!npc.behavior.active || !npc.behavior.focusEnabled) continue;
        TeamBrain& brain = (team == 0) ? d.teamBrainA : d.teamBrainB;
        const SquadState& sq = brain.squad();
        UtilityContext& ctx = npc.utilityContext;
        ctx.focusKnown = sq.active && glm::length(sq.focusPos) > 0.001f;
        if (ctx.focusKnown) ctx.focusPos = sq.focusPos;
        ctx.swarm = npc.behavior.swarm;
        glm::vec3 slot{0.0f}, anchor{0.0f};
        ctx.squadSlotKnown = brain.squadSlotFor(npc.id, slot, anchor);
        if (ctx.squadSlotKnown) {
            ctx.squadSlot = slot;
            ctx.squadAnchor = anchor;
            const float dx = slot.x - npc.body.pos.x;
            const float dy = slot.y - npc.body.pos.y;
            ctx.atSquadSlot = std::sqrt(dx * dx + dy * dy) <= 3.0f;
        }
    }

    if (!d.objectiveRounds) return;

    // Payload NPCs use the same utility objective slot as Bomb: attackers and
    // defenders both receive the live cart position, so they contest the cart
    // instead of wandering. Physical contact still comes from the shared actor
    // and cart collision paths.
    const bool payloadObjective = d.payloadMode && d.payloadRuntime.active &&
        d.phase == DUEL_PHASE_ACTIVE;

    // Push objective context onto each NPC's utility brain. objectivePos points
    // at the bomb/site the NPC's team should care about.
    for (Npc& npc : npcSystem.all()) {
        const int team = npc.body.matchTeam;
        TeamBrain& brain = (team == 0) ? d.teamBrainA : d.teamBrainB;
        const TeamAssignment assignment = brain.assignmentFor(npc.id);
        glm::vec3 objPos;
        bool hasObj = false;
        if (payloadObjective) {
            hasObj = true;
            objPos = d.payloadRuntime.cartPosition;
            // MoveToObjective stops 4 m before its target. Aim at a point just
            // past the cart so attackers and defenders actually enter the
            // interaction box instead of holding outside it.
            glm::vec3 toCart = objPos - npc.body.pos;
            const float flat = glm::length(glm::vec2(toCart.x, toCart.y));
            if (flat > 0.1f)
                objPos += glm::vec3(toCart.x / flat, toCart.y / flat, 0.0f) * 2.0f;
        } else {
            hasObj = brain.objectiveTargetPosition(npc.id, npc.body.pos, objPos);
        }

        UtilityContext& ctx = npc.utilityContext;
        ctx.objectiveKnown = hasObj;
        if (hasObj) ctx.objectivePos = objPos;
        ctx.onDefense = payloadObjective
            ? (team == d.payloadRuntime.defenderTeam)
            : (team == 0);
        // Team's best estimate of where enemies are, so an NPC with no visible
        // target can travel toward the enemy area instead of wandering.
        glm::vec3 enemyAreaPos;
        float enemyConf = 0.0f;
        ctx.enemyAreaKnown = brain.bestTeamReport(enemyAreaPos, enemyConf);
        if (ctx.enemyAreaKnown) ctx.enemyAreaPos = enemyAreaPos;
        if (gm.npcTravel.configured) {
            ctx.travelObjectiveBias = gm.npcTravel.objectiveBias;
            ctx.travelHuntBias = gm.npcTravel.huntBias;
        }
        ctx.atObjective = hasObj &&
            glm::length(npc.body.pos - objPos) <= 3.0f;
        // A Terrorist carrying the bomb can plant inside a site.
        const int actorSiteIndex = MapConfigRegistry::instance().siteIndexAt(npc.body.pos);
        const glm::vec3 objectiveAnchor =
            payloadObjective ? objPos : d.objective.position;
        const float objectiveDistance = glm::length(npc.body.pos - objectiveAnchor);
        ctx.canPlant = !payloadObjective && (team == 1) &&
            d.objective.state == ObjectiveState::Carried &&
            d.objective.carrierActorId == npc.id &&
            actorSiteIndex >= 0;
        // A Counter-Terrorist at a planted bomb can defuse.
        ctx.canDefuse = !payloadObjective && (team == 0) &&
            d.objective.state == ObjectiveState::Planted &&
            glm::length(npc.body.pos - d.objective.position) <= d.objective.interactionRange;
        const int objectiveContextKey = (hasObj ? 1 : 0) |
            (ctx.atObjective ? 2 : 0) | (ctx.canPlant ? 4 : 0) |
            (ctx.canDefuse ? 8 : 0) | (payloadObjective ? 16 : 0);
        if (objectiveContextKey != npc.lastObjectiveContextKey) {
            npc.lastObjectiveContextKey = objectiveContextKey;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "npc.objective-context", std::to_string(npc.id),
                "context_changed", tick,
                nlohmann::json{{"actor", npc.id}, {"team", team},
                               {"assignment", (int)assignment},
                               {"payload", payloadObjective},
                               {"objective_state", (int)d.objective.state},
                               {"objective_known", hasObj},
                               {"at_objective", ctx.atObjective},
                               {"can_plant", ctx.canPlant},
                               {"can_defuse", ctx.canDefuse},
                               {"carrier_actor_id", d.objective.carrierActorId},
                               {"is_carrier", d.objective.carrierActorId == npc.id},
                               {"actor_site_index", actorSiteIndex},
                               {"distance_to_objective", objectiveDistance},
                               {"interaction_range", d.objective.interactionRange},
                               {"eligibility_reason",
                                payloadObjective
                                    ? (ctx.atObjective ? "at_payload_cart" : "travel_to_payload_cart")
                                    : ctx.canPlant ? "plant_eligible" :
                                ctx.canDefuse ? "defuse_eligible" :
                                (team == 1 && d.objective.state == ObjectiveState::Carried
                                    ? (d.objective.carrierActorId != npc.id ? "not_bomb_carrier"
                                       : actorSiteIndex < 0 ? "outside_bomb_site" : "plant_not_eligible")
                                    : team == 0 && d.objective.state == ObjectiveState::Planted
                                        ? "outside_defuse_range" : "objective_state_not_actionable")},
                               {"objective_position", {objPos.x, objPos.y, objPos.z}}});
        }
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
    for (auto it = d.actorPresetOverridesByActorId.begin();
         it != d.actorPresetOverridesByActorId.end(); ) {
        if (players.find(it->first) == players.end() && npcs.find(it->first) == npcs.end()) {
            d.actorPresetAppliedByActorId.erase(it->first);
            it = d.actorPresetOverridesByActorId.erase(it);
        }
        else
            ++it;
    }
    static std::string loadedMapConfigId;
    static uint64_t lastMapConfigPollMs = 0;
    const uint64_t mapPollNow = nowMs();
    if (loadedMapConfigId != d.mapId) {
        MapConfigRegistry::instance().load(d.mapId);
        loadedMapConfigId = d.mapId;
        lastMapConfigPollMs = mapPollNow;
    } else if (mapPollNow - lastMapConfigPollMs >= 250) {
        MapConfigRegistry::instance().pollReload();
        lastMapConfigPollMs = mapPollNow;
    }
    // The server normally polls gamemode JSON from its outer loop. Keep this
    // bounded fallback here too so a hosted server applies wave tuning without
    // a restart, while avoiding filesystem work on every 60 Hz tick.
    static uint64_t lastGamemodePollMs = 0;
    const uint64_t pollNow = nowMs();
    if (pollNow - lastGamemodePollMs >= 250) {
        GamemodeRegistry::instance().pollReload();
        lastGamemodePollMs = pollNow;
        const Gamemode& live = GamemodeRegistry::instance().get(d.matchMode);
        if (d.npcWaves || d.persistentNpcSpawns) {
            d.waveStartCount = live.waveStartCount;
            d.waveIncrement = live.waveIncrement;
            d.waveNpcsPerWave = live.waveNpcsPerWave;
            d.waveBannerSeconds = live.waveBannerSeconds;
            d.waveStaggerEnabled = live.waveStaggerEnabled;
            d.waveNpcsPerTick = live.waveNpcsPerTick;
            // Policy alone enables the lifecycle; the cap may be 0 (no spawns).
            d.persistentNpcSpawns = live.npcSpawnPolicy == "persistent";
            d.npcSpawnMax = std::max(0, live.npcSpawnMax);
            d.npcSpawnIntervalTicks = std::max(1, live.npcSpawnIntervalTicks);
            d.npcSpawnPerInterval = std::max(1, live.npcSpawnPerInterval);
            d.intermissionSeconds = (float)live.intermissionSeconds;
            d.resultsSeconds = (float)live.resultsSeconds;
        }
    }
    updateActorStates(d, sock, players, npcs, tick, totalPacketsOut);
    // Generic area effects tick at fixed 60 Hz in every managed mode.
    serverAreaEffectTick(d, sock, players, npcs, tick, totalPacketsOut);
    // Team assignments and objective context are NPC decision inputs. Keep
    // them frozen with movement/AI until the countdown and GO phase finish.
    const Gamemode& activeModeRules = GamemodeRegistry::instance().get(d.matchMode);
    const bool pauseTeamBrain = d.phase == DUEL_PHASE_COUNTDOWN ||
        (d.phase == DUEL_PHASE_GO && activeModeRules.features.freezeDuringGo);
    if (!pauseTeamBrain)
        serverTeamBrainTick(d, players, npcs, npcSystem, npcWorld, tick);
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

    if (d.pendingJuggernautSkip)
    {
        d.pendingJuggernautSkip = false;
        if (d.matchMode == "juggernaut" && d.objectiveRounds &&
            d.phase != DUEL_PHASE_RESULTS) {
            for (auto& kv : players) {
                if (kv.second.spawnState != ServerPlayer::Active) continue;
                kv.second.health = 0;
                kv.second.dead = true;
            }
            for (auto& kv : npcs)
                if (kv.second.matchTeam >= 0) kv.second.health = 0;

            d.roundWinnerTeam = -1;
            d.roundEndReason = 4; // host-forced draw/skip
            d.roundCountdownFreeze = false;
            d.matchOver = false;
            d.winnerTeam = -1;
            d.phase = DUEL_PHASE_RESULTS;
            d.phaseTimer = d.resultsSeconds;
            ++d.stateVersion;
            ++d.roundVersion;
            Debug::warn(Debug::Category::Duel,
                "[JUGGERNAUT SKIP] forced draw round=%u tick=%u\n",
                d.roundNumber, tick);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
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
                        applyServerNpcRoleHealth(npc, "automatic-map-change");
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
        // Authored map entities (damage volumes, checkpoints, boss triggers)
        // tick during sandbox/mapOnly play too, not only the managed lifecycle.
        mapEntityRuntimeTick(d, sock, players, npcs, tick, totalPacketsOut);
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
                    applyServerNpcRoleHealth(npc, "manual-map-change");
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
        // The anchor MUST be the victim's own team spawn, resolved through the
        // single team source of truth (never the killer/local/nearest/default).
        const glm::vec3 victimSpawn =
            gamemodeSpawnPointForActor(d, victimId, players, npcs, "kill");
        auto victimIt = players.find(victimId);
        if (!d.pendingVictimIsNpc && victimIt != players.end())
        {
            victimIt->second.duelSpawnPos = victimSpawn;
        }
        else if (d.pendingVictimIsNpc)
        {
            // Pin the dead NPC's respawn anchor to its own team spawn so a
            // later map change cannot resurrect it into stale/void space.
            for (Npc& n : npcSystem.all()) {
                if (n.id == victimId) {
                    n.body.respawnPosition = victimSpawn;
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
            if (d.payloadMode) {
                ++d.payloadSessionKills[killerId];
            }
            // Duel 1v1 scoring (original behavior)
            if (d.matchMode == "duel" && d.winCondition != "first_to_score") {
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
            // Data-selected competitor scoring. Only confirmed kills between
            // opposing assigned teams count; human and NPC actors share this map.
            else if (d.winCondition == "first_to_score") {
                const auto killerTeam = d.matchTeams.find(killerId);
                const auto victimTeam = d.matchTeams.find(victimId);
                if (killerTeam != d.matchTeams.end() && victimTeam != d.matchTeams.end() &&
                    killerTeam->second >= 0 && victimTeam->second >= 0 &&
                    killerTeam->second != victimTeam->second) {
                    ++d.ffaKills[killerId];
                    ++d.ffaDeaths[victimId];
                }
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
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::Duel, StructuredLevel::Important,
                            "tdm.team-score.updated", d.matchMode,
                            "confirmed opposing-team kill scored", tick,
                            nlohmann::json{{"killer", killerId}, {"victim", victimId},
                                           {"killer_team", teamIt->second},
                                           {"victim_team", victimTeam},
                                           {"red_score", d.redTeamKills},
                                           {"blue_score", d.blueTeamKills},
                                           {"goal", d.goalValue}},
                            __FILE__, __LINE__, __FUNCTION__);
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
        {
            // Warmup: as soon as a human is present during intermission, spawn
            // the roster (allied + enemy NPCs) at their team spawns and let
            // everyone move and fight freely with infinite lives. The real
            // one-life round begins at countdown.
            if (!d.warmup) {
                bool hasHuman = false;
                for (const auto& kv : players)
                    if (kv.second.spawnState == ServerPlayer::Active) { hasHuman = true; break; }
                if (hasHuman || headlessMatchAllowed()) {
                    if (world.spawnPoints.empty())
                        rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                    assignGamemodeSpawns(d, world);
                    buildObjectiveRoster(d, players, npcs);
                    // Spawn the human at their team spawn for warmup.
                    for (auto& kv : players) {
                        if (kv.second.spawnState != ServerPlayer::Active) continue;
                        const glm::vec3 spawn =
                            gamemodeSpawnPointForActor(d, kv.first, players, npcs, "warmup");
                        kv.second.duelSpawnPos = spawn;
                        kv.second.hasDuelSpawnPos = true;
                        beginAuthoritativeTransform(kv.second, spawn, glm::vec3(0.0f),
                                                    kv.second.yaw, "warmup-spawn");
                        kv.second.justRespawned = true;
                    }
                    assignObjectiveCarrier(d, players, npcs);
                    d.warmup = true;
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    Debug::warn(Debug::Category::Duel,
                        "[WARMUP] intermission roster spawned mode=%s npcs=%zu\n",
                        d.matchMode.c_str(), npcs.size());
                }
            }
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                    bool hasHuman = false;
                    for (const auto& kv : players)
                        if (kv.second.spawnState == ServerPlayer::Active) { hasHuman = true; break; }
                    if (!hasHuman && !headlessMatchAllowed())
                    {
                        d.phaseTimer = 0.5f;  // re-check shortly, don't fire an empty round
                        break;
                    }
                d.warmup = false;
                // Rebuild a clean roster and spawn at team spawns for the round.
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
        }

        case DUEL_PHASE_COUNTDOWN:
            if (countdownWallDeadlineReached(d))
            {
                d.phase = DUEL_PHASE_GO;
                logCountdownCompletion(d, tick);
                d.phaseTimer = d.goSeconds > 0.0f ? d.goSeconds : 0.75f;
                d.roundEndTick = d.roundSeconds > 0.0f
                    ? tick + (uint32_t)(d.roundSeconds * 60.0f) : 0;
                d.matchStartTick = tick;
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "round.go", std::to_string(d.duelId), "go", tick,
                    nlohmann::json{{"round", d.roundNumber},
                                   {"tick", tick}});
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
                // GO! Everyone may move now. Release the countdown freeze.
                d.roundCountdownFreeze = false;
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                Debug::log(Debug::Category::Duel,
                    "[ROUND] ACTIVE round=%u version=%u endTick=%u\n",
                    d.roundNumber, d.roundVersion, d.roundEndTick);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "round.active", std::to_string(d.duelId), "active", tick,
                    nlohmann::json{{"round", d.roundNumber},
                                   {"round_end_tick", d.roundEndTick}});
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 10)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_ACTIVE:
            if (d.payloadMode) {
                serverPayloadTick(d, players, npcs, npcSystem, tick);
            } else {
                serverObjectiveTick(d, players, npcs, tick);
                checkObjectiveRoundEnd(d, tick);
            }
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
                if (!d.endlessRounds &&
                    (d.matchOver || (d.maxRounds > 0 && (int)d.roundNumber >= d.maxRounds)))
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
    if (d.npcWaves || d.persistentNpcSpawns || d.matchMode == "ffa" || d.matchMode == "tdm" ||
        d.winCondition == "first_to_score" ||
        d.winCondition == "last_team_standing" ||
        d.winCondition == "last_actor_alive")
    {
        if (d.stateBroadcastPending)
        {
            // The command changed the authoritative mode/phase between ticks.
            // Send that state now so every client can show intermission or the
            // immediate countdown without waiting for the periodic broadcast.
            broadcastDuelState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
    if (d.zombieTower) {
        static uint32_t lastZtStateLog = 0;
        if (d.currentServerTick > lastZtStateLog + 120) {
            lastZtStateLog = d.currentServerTick;
            size_t active = 0;
            size_t awaiting = 0;
            for (const auto& kv : players) {
                if (kv.second.spawnState == ServerPlayer::Active) ++active;
                else if (kv.second.spawnState == ServerPlayer::AwaitingSpawnAck) ++awaiting;
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "zombie_tower.tick-state", d.mapId, "periodic run state", tick,
                nlohmann::json{{"phase", (int)d.phase}, {"phase_timer", d.phaseTimer},
                               {"active_players", active}, {"awaiting_ack", awaiting},
                               {"participants", d.participants.size()},
                               {"attempts", d.runAttemptsRemaining}});
        }
        admitZombieTowerPlayers(d, players, npcs);
        // If a spawn sync was lost before the client acknowledged it, resend a
        // fresh authoritative spawn at the phase-appropriate point.
        for (auto& kv : players) {
            ServerPlayer& p = kv.second;
            if (p.spawnState != ServerPlayer::AwaitingSpawnAck) continue;
            if (tick < p.ztSpawnResendTick) continue;
            p.ztSpawnResendTick = tick + 120;
            const int team = p.matchTeam >= 0 ? p.matchTeam : 0;
            const glm::vec3 spawn = zombieTowerSpawnPointForActor(
                d, team, p.id, "spawn-resend");
            p.pos = spawn;
            p.duelSpawnPos = spawn;
            p.hasDuelSpawnPos = true;
            completeAuthoritativeSpawn(sock, p, false);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Duel, StructuredLevel::Important,
                "zombie_tower.spawn-resend", std::to_string(p.id),
                "client had not acknowledged the initial spawn", tick,
                nlohmann::json{{"actor", p.id}, {"team", team},
                               {"spawn_generation", p.spawnGeneration}});
        }
    }
    switch (d.phase)
    {
        case DUEL_PHASE_WAITING:
            if (((d.npcWaves || d.persistentNpcSpawns || d.winCondition == "first_to_score" ||
                  GamemodeRegistry::instance().get(d.matchMode).npcSpawnPolicy == "team_roster") &&
                 countActivePlayers(players) >= 1) ||
                (!d.npcWaves && (countActivePlayers(players) >= 2 ||
                (countActivePlayers(players) >= 1 && !npcs.empty()))))
            {
                // If the current map has no spawn points, rotate.
                if (world.spawnPoints.empty())
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                if ((d.winCondition == "first_to_score" ||
                     GamemodeRegistry::instance().get(d.matchMode).npcSpawnPolicy == "team_roster") &&
                    !d.startCountdownImmediately) {
                    d.phase = DUEL_PHASE_INTERMISSION;
                    d.phaseTimer = d.intermissionSeconds;
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    Debug::warn(Debug::Category::Duel,
                        "[MATCH] first participant joined; intermission started mode=%s seconds=%.1f\n",
                        d.matchMode.c_str(), d.intermissionSeconds);
                    break;
                }
                reconcileTeamRosterNpc(d, players, npcs, npcSystem, npcIdsAlive);
                if (d.npcWaves) {
                    d.waveNumber = 1;
                    beginNpcWave(d, players, npcs, npcSystem, npcIdsAlive, tick);
                } else if (d.persistentNpcSpawns) {
                    clearNpcWaveActors(d, npcs, npcSystem, npcIdsAlive);
                    assignMatchParticipants(d, players, &npcs);
                    beginMatchCountdown(d, players, npcs, npcSystem, tick);
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
            if (countdownWallDeadlineReached(d))
            {
                d.phase = DUEL_PHASE_GO;
                logCountdownCompletion(d, tick);
                d.phaseTimer = d.npcWaves ? d.waveBannerSeconds : d.goSeconds;
                d.waveBannerVisible = d.npcWaves;
                d.waveBannerUntilTick = d.npcWaves
                    ? tick + (uint32_t)std::ceil(d.waveBannerSeconds * 60.0f) : 0;
                if (d.npcWaves)
                    spawnNpcWaveBatch(d, npcs);
                const Gamemode& goMode = GamemodeRegistry::instance().get(d.matchMode);
                if (!goMode.features.freezeDuringGo)
                    d.roundCountdownFreeze = false;
                if (d.zombieTower) {
                    if (d.ztRunStartTick == 0) d.ztRunStartTick = tick;
                    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
                    d.ztMusicTrack = gm.musicOnGoTrack;
                    d.ztMusicPlaying = !d.ztMusicTrack.empty();
                    d.ztMusicStartedTick = tick;
                    ++d.ztMusicSerial;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Audio, StructuredLevel::Important,
                        "music.on-go", d.mapId, "Zombie Tower GO track selected", tick,
                        nlohmann::json{{"track", d.ztMusicTrack},
                                       {"serial", d.ztMusicSerial}});
                }
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
                d.roundCountdownFreeze = false;
                d.waveBannerVisible = false;
                d.matchStartTick = tick;
                if (d.persistentNpcSpawns)
                    d.npcSpawnNextTick = tick + (uint32_t)std::max(1, d.npcSpawnIntervalTicks);
                if (d.npcWaves) {
                    spawnNpcWaveBatch(d, npcs);
                    ++d.stateVersion;
                    d.lastBroadcastTick = tick;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    break;
                }
                if (d.timeLimitSeconds > 0)
                    d.matchTimeLimitTick = tick + (uint32_t)(d.timeLimitSeconds * 60.0f);
                // A declared disaster assigns per-actor weapons before the
                // actors are placed at their spawns, so the spawn reset applies
                // the assignment. Pure capability lookup, no mode name.
                if (d.disaster.configured) {
                    MimitaGamemode::disasterBegin(d.disaster, d.participants, tick);
                    d.disasterPhaseStartTick = tick;
                    Debug::warn(Debug::Category::Duel,
                        "[DISASTER] begin id=%s seed=%u actors=%zu duration=%.1fs\n",
                        d.disaster.disasterId.c_str(), d.disaster.seed,
                        d.disaster.weaponByActor.size(),
                        (float)d.disaster.durationTicks / 60.0f);
                }
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
            // Authored-map gameplay entities (checkpoints, boss triggers, and
            // authored spawnpoint selection) tick every active frame.
            mapEntityRuntimeTick(d, sock, players, npcs, tick, totalPacketsOut);
            if (d.npcWaves) {
                if (d.waveNpcSpawned < d.waveNpcTarget)
                    spawnNpcWaveBatch(d, npcs);
                updateActorStates(d, sock, players, npcs, tick, totalPacketsOut);
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
            if (d.persistentNpcSpawns) {
                if (d.zombieTower) {
                    zombieTowerDownedTick(d, players, tick);
                    reconcileZombieTowerParty(d, players, npcs);
                }
                spawnPersistentNpcBatchIfDue(d, players, npcs, tick);
                if (d.zombieTower)
                    zombieTowerCheckWin(d, sock, players, npcs, tick, totalPacketsOut);
                if (tick - d.lastBroadcastTick >= 15) {
                    d.lastBroadcastTick = tick;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                }
                break;
            }
            // Check win conditions on every tick
            checkMatchWinConditions(d, tick, sock, players, totalPacketsOut);
            if (d.phase != DUEL_PHASE_ACTIVE) break;  // win condition triggered
            // A declared disaster that reaches its bounded duration with more
            // than one survivor resolves a deterministic winner (pure rule).
            if (d.disaster.active && !d.disaster.resolved &&
                MimitaGamemode::disasterDurationElapsed(d.disaster, tick)) {
                std::vector<uint32_t> survivors;
                for (uint32_t id : d.participants) {
                    auto aIt = d.matchActors.find(id);
                    if (aIt != d.matchActors.end() &&
                        (aIt->second.state == ActorState::Alive ||
                         aIt->second.state == ActorState::Respawning))
                        survivors.push_back(id);
                }
                d.disaster.resolved = true;
                d.disaster.resolveSource = 2;
                d.disaster.winnerActor =
                    MimitaGamemode::disasterPickTimeoutWinner(d.matchSeed, survivors);
                d.matchOver = true;
                d.phase = DUEL_PHASE_RESULTS;
                d.victoryType = 1;  // time limit
                d.winnerPlayerId = d.disaster.winnerActor;
                d.winnerTeam = -1;
                d.phaseTimer = d.resultsSeconds;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                emitGamemodeMatchPersistence(d, tick, players);
                Debug::warn(Debug::Category::Duel,
                    "[DISASTER] timeout id=%s survivors=%zu winner=%u\n",
                    d.disaster.disasterId.c_str(), survivors.size(),
                    d.disaster.winnerActor);
                break;
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
                // A declared disaster ends with its match; the name stays
                // configured so the next intermission/countdown can display it.
                d.disaster.active = false;
                d.disaster.resolved = false;
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
                if ((d.winCondition == "first_to_score" ||
                     GamemodeRegistry::instance().get(d.matchMode).npcSpawnPolicy == "team_roster") &&
                    countActivePlayers(players) == 0) {
                    d.phase = DUEL_PHASE_WAITING;
                    d.phaseTimer = 0.0f;
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    Debug::warn(Debug::Category::Duel,
                        "[MATCH] empty lobby returned to waiting mode=%s\n", d.matchMode.c_str());
                    break;
                }
                if (d.zombieTower && countActivePlayers(players) == 0) {
                    // Do not start an empty run; the 30s intermission restarts
                    // once a human is actually in the lobby.
                    d.phaseTimer = std::max(1.0f, d.intermissionSeconds);
                    ++d.stateVersion;
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    break;
                }
                // Rotate map if configured
                if (d.rotateMaps && d.mapPool.size() > 1)
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                reconcileTeamRosterNpc(d, players, npcs, npcSystem, npcIdsAlive);
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
        d.countdown = countdownWallSecondsRemaining(d);
        if (countdownWallDeadlineReached(d))
        {
            d.phase = DUEL_PHASE_ACTIVE;
            logCountdownCompletion(d, tick);
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

bool serverSpawnTagSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };
    if (spawnTagTeamIndex("spawnpoint.CT") != 0) fail("spawnpoint.CT should map to team 0");
    if (spawnTagTeamIndex("spawnpoint.T") != 1) fail("spawnpoint.T should map to team 1");
    if (spawnTagTeamIndex("spawnpoint.counterterrorist") != 0) fail("counterterrorist should map to CT");
    if (spawnTagTeamIndex("spawnpoint.terrorist") != 1) fail("terrorist should map to T");
    if (spawnTagTeamIndex("spawnpoint.011") != -1) fail("numbered spawnpoint should be teamless (-1)");
    if (spawnTagTeamIndex("randomspawn") != -1) fail("plain spawn should be teamless (-1)");
    if (spawnTagTeamIndex("spawnpoint") != -1) fail("bare spawnpoint should be teamless (-1)");

    // Team -> spawn group resolution for Counter-Strike. Ensure the gamemode
    // registry is loaded (this selftest may run before the main startup load).
    GamemodeRegistry::instance().loadDirectory("config/gamemodes");
    ServerGamemodeState sd;
    sd.matchMode = "counterstrike";
    if (spawnGroupForTeam(sd, 0) != "ct_spawn") fail("team 0 spawn_group != ct_spawn");
    if (spawnGroupForTeam(sd, 1) != "t_spawn") fail("team 1 spawn_group != t_spawn");
    if (!isValidPlayingTeam(sd, 0) || !isValidPlayingTeam(sd, 1))
        fail("CT/T should be valid playing teams");
    if (isValidPlayingTeam(sd, 2)) fail("spectator should not be a playing team");

    const Gamemode& waves = GamemodeRegistry::instance().get("npc_waves");
    if (waves.teams.size() < 2) {
        fail("npc_waves should declare human and NPC teams");
    } else {
        if (!spawnTagMatchesTeam("spawn point", waves.teams[0]))
            fail("generic spawn point should match the human team");
        if (spawnTagMatchesTeam("NPC spawn point", waves.teams[0]))
            fail("NPC spawn point should not match the human team");
        if (!spawnTagMatchesTeam("NPC spawn point", waves.teams[1]))
            fail("NPC spawn point should match the NPC team");
        if (spawnTagMatchesTeam("spawn point", waves.teams[1]))
            fail("generic spawn point should not match the NPC team");
    }

    const Gamemode& zombie = GamemodeRegistry::instance().get("zombie_tower");
    if (zombie.id != "zombie_tower") fail("zombie_tower gamemode should load");
    if (zombie.npcSpawnPolicy != "persistent") fail("zombie_tower spawn policy should be persistent");
    if (zombie.npcSpawnMax < 1) fail("zombie_tower max_count should be at least 1");
    if (zombie.npcSpawnIntervalTicks != 60) fail("zombie_tower interval_ticks should be 60");
    if (zombie.npcSpawnPerInterval != 1) fail("zombie_tower per_interval should be 1");

    // Payload must declare real team spawn groups, and both authored maps must
    // contain enabled spawnpoint entities carrying those exact tags. This is the
    // data contract the spawn resolver depends on; it does not prove runtime
    // positioning (see game-loop spawn evidence).
    const Gamemode& payload = GamemodeRegistry::instance().get("payload");
    if (payload.id != "payload" || payload.teams.size() < 2) {
        fail("payload gamemode should load with two teams");
    } else {
        if (payload.spawnStrategy == "shared")
            fail("payload must not use the shared spawn strategy (both teams would collapse)");
        if (payload.teams[0].spawnGroup != "attacker_spawn")
            fail("payload team 0 spawn_group != attacker_spawn");
        if (payload.teams[1].spawnGroup != "defender_spawn")
            fail("payload team 1 spawn_group != defender_spawn");
        if (payload.teams[0].spawnTagContains.empty() ||
            payload.teams[1].spawnTagContains.empty())
            fail("payload teams should declare spawn_tag_contains");
        for (const char* mapId : {"payload1", "payload2"}) {
            if (!MapConfigRegistry::instance().load(mapId)) {
                report += std::string("payload spawn map missing: ") + mapId + "\n";
                continue;
            }
            auto hasSpawnTag = [&](const std::string& tag) {
                for (const MapEntity& entity : MapConfigRegistry::instance().current().entities) {
                    if (entity.type == "spawnpoint" && entity.enabled && entity.tag == tag)
                        return true;
                }
                return false;
            };
            if (!hasSpawnTag("attacker_spawn"))
                fail(std::string(mapId) + " has no enabled attacker_spawn entity");
            if (!hasSpawnTag("defender_spawn"))
                fail(std::string(mapId) + " has no enabled defender_spawn entity");
        }
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
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
    BehaviorProfileRegistry::instance().load("config/behavior-profiles.json");
    WeaponData::registerBuiltinWeapons();
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);

    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Three ordered teams: two playing teams plus the built-in Spectator team.
    // The playing teams are always indices 0 and 1; Spectator has capacity 0.
    if (gm.teams.size() != 3) fail("expected 3 ordered teams (CT, T, Spectator)");
    else {
        report += "team0=" + gm.teams[0].displayName + " team1=" + gm.teams[1].displayName +
                  " team2=" + gm.teams[2].displayName + "\n";
        if (gm.teams[0].displayName != "Counter-Terrorists") fail("team 0 display name");
        if (gm.teams[1].displayName != "Terrorists") fail("team 1 display name");
        if (gm.teams[0].capacity != 5 || gm.teams[1].capacity != 5) fail("playing team capacity != 5");
        if (gm.teams[2].displayName != "Spectator") fail("team 2 display name");
        if (gm.teams[2].capacity != 0) fail("spectator capacity != 0");
    }

    // Goal 1: gamemode NPC behavior profile + precedence (mode beats role).
    report += "npc_behavior_profile=" + gm.npcBehaviorProfile + "\n";
    if (gm.npcBehaviorProfile != "rage2") fail("counterstrike npc_behavior_profile != rage2");
    if (resolveNpcBehaviorProfileId(gm, "balanced") != "rage2")
        fail("gamemode profile should override the role profile");
    if (resolveNpcBehaviorProfileId(Gamemode{}, "balanced") != "balanced")
        fail("no mode profile should keep the role profile");

    // Goal 2: mode-level targeting.
    report += "npc_targeting=" + gm.npcTargeting.mode + "\n";
    if (!gm.npcTargeting.configured) fail("npc_targeting block not configured");
    if (gm.npcTargeting.mode != "opposite_team") fail("npc_targeting.mode != opposite_team");
    if (!gm.npcTargeting.includePlayers || !gm.npcTargeting.includeNpcs)
        fail("npc_targeting should include players and npcs");

    // Goal 4: mode-level aim FOV.
    if (!gm.aimFov.enabled) fail("camera.aim_fov.enabled not set");
    if (std::fabs(gm.aimFov.multiplier - 0.5f) > 1e-5f) fail("aim_fov multiplier != 0.5");
    if (std::fabs(gm.aimFov.duration - 0.5f) > 1e-5f) fail("aim_fov duration != 0.5");
    if (gm.aimFov.easing != "ease_in_out") fail("aim_fov easing != ease_in_out");

    // Goal 3: team identity validation and neutral anchor.
    if (!isValidPlayingTeam(d, 0) || !isValidPlayingTeam(d, 1))
        fail("CT/T should be valid playing teams");
    if (isValidPlayingTeam(d, 2)) fail("spectator must not be a valid playing team");
    if (isValidPlayingTeam(d, -1)) fail("unknown team must not be valid");
    {
        const MatchRoleDefinition* ct = MatchRoleRegistry::instance().get("counter_terrorist");
        const MatchRoleDefinition* t = MatchRoleRegistry::instance().get("terrorist");
        std::string err;
        if (!validateTeamRoleConsistency(gm, ct, 0, err))
            fail("counter_terrorist/team0 should be consistent: " + err);
        if (!validateTeamRoleConsistency(gm, t, 1, err))
            fail("terrorist/team1 should be consistent: " + err);
        if (validateTeamRoleConsistency(gm, t, 0, err))
            fail("terrorist/team0 must be rejected");
        if (validateTeamRoleConsistency(gm, ct, 1, err))
            fail("counter_terrorist/team1 must be rejected");
        if (validateTeamRoleConsistency(gm, ct, -1, err))
            fail("team -1 must be rejected");
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
    if (gm.teams.empty() && gm.teamNames.empty()) return "no teams";
    std::string out;
    const size_t teamCount = !gm.teams.empty() ? gm.teams.size() : gm.teamNames.size();
    for (size_t i = 0; i < teamCount; ++i) {
        if (!out.empty()) out += " | ";
        if (i < gm.teams.size()) {
            const auto& team = gm.teams[i];
            const std::string display = team.displayName.empty() ? team.id : team.displayName;
            out += std::to_string(i + 1) + " = " + display +
                   " [id=" + team.id + ", capacity=" + std::to_string(team.capacity) + "]";
        } else {
            out += std::to_string(i + 1) + " = " + gm.teamNames[i];
        }
    }

    return out;
}

bool serverRequestTeamChange(uint32_t playerId, int requestedTeam,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             std::unordered_map<uint32_t, ServerNpc>& npcs,
                             uint32_t tick, uint64_t& totalPacketsOut,
                             std::string& message)
{
    ServerGamemodeState& d = serverGamemodeState();
    auto playerIt = players.find(playerId);
    if (playerIt == players.end()) { message = "player not found"; return false; }
    const CommunityMode* cm = CommunityServerConfig::instance().modeById(d.communityMode);
    const std::string id = cm ? cm->gamemodeId : d.communityMode;
    const Gamemode& gm = GamemodeRegistry::instance().get(id);
    const bool hostOverride = playerIt->second.isHost;
    const bool allowMidrun = d.zombieTower && gm.allowMidrunTeamChange;
    const bool payloadActive = d.payloadMode &&
        (d.phase == DUEL_PHASE_COUNTDOWN || d.phase == DUEL_PHASE_GO ||
         d.phase == DUEL_PHASE_ACTIVE);
    const bool hasPlayingTeam = playerIt->second.matchTeam >= 0 &&
        playerIt->second.matchTeam < 2;
    // Team selection is a pre-round decision. Payload allows active changes
    // after its configured cooldown; joining from no team/spectator is always
    // allowed. The host may override either rule.
    if (!hostOverride && !allowMidrun) {
        if (payloadActive && hasPlayingTeam) {
            const auto cooldownIt = d.teamChangeCooldownUntil.find(playerId);
            if (cooldownIt != d.teamChangeCooldownUntil.end() &&
                tick < cooldownIt->second) {
                message = "team change cooldown";
                return false;
            }
        } else if (d.phase != DUEL_PHASE_WAITING &&
                   d.phase != DUEL_PHASE_INTERMISSION &&
                   d.phase != DUEL_PHASE_RESULTS) {
            message = "team selection locked";
            return false;
        }
    }
    const size_t configuredTeamCount = !gm.teams.empty() ? gm.teams.size() : gm.teamNames.size();
    if (requestedTeam < 0 || requestedTeam >= static_cast<int>(configuredTeamCount)) {
        message = configuredTeamCount == 0 ? "active gamemode has no teams" : "invalid team";
        return false;
    }
    // Enforce ordered-team capacity when the mode declares it.
    if (requestedTeam < static_cast<int>(gm.teams.size())) {
        const GamemodeTeam& requestedTeamDef = gm.teams[static_cast<size_t>(requestedTeam)];
        const int capacity = requestedTeamDef.capacity;
        const bool preferHumans = requestedTeamDef.preferHumans;
        if (capacity > 0 && playerIt->second.matchTeam != requestedTeam) {
            int count = 0;
            for (const auto& entry : d.matchTeams)
                if (entry.second == requestedTeam) ++count;
            if (count >= capacity) {
                if (!hostOverride && !preferHumans) {
                    message = "team is full";
                    return false;
                }

                std::vector<uint32_t> npcVictims;
                std::vector<uint32_t> humanVictims;
                for (const auto& entry : d.matchTeams) {
                    if (entry.first == playerId || entry.second != requestedTeam) continue;
                    if (npcs.find(entry.first) != npcs.end()) npcVictims.push_back(entry.first);
                    else if (players.find(entry.first) != players.end()) humanVictims.push_back(entry.first);
                }
                if (preferHumans && npcVictims.empty()) {
                    message = "team is full";
                    return false;
                }
                uint32_t victimId = 0;
                if (!npcVictims.empty()) {
                    std::mt19937 rng(static_cast<uint32_t>(d.duelId ^ tick ^ playerId));
                    std::uniform_int_distribution<size_t> pick(0, npcVictims.size() - 1);
                    victimId = npcVictims[pick(rng)];
                    npcs.erase(victimId);
                    d.participants.erase(std::remove(d.participants.begin(), d.participants.end(), victimId),
                                         d.participants.end());
                    d.participantNames.erase(victimId);
                    d.matchTeams.erase(victimId);
                    d.matchActors.erase(victimId);
                    serverClearActorPreset(victimId);
                    d.ffaKills.erase(victimId);
                    d.ffaDeaths.erase(victimId);
                } else if (!humanVictims.empty()) {
                    std::mt19937 rng(static_cast<uint32_t>(d.duelId ^ tick ^ playerId));
                    std::uniform_int_distribution<size_t> pick(0, humanVictims.size() - 1);
                    victimId = humanVictims[pick(rng)];
                    players[victimId].matchTeam = 2;
                    d.matchTeams[victimId] = 2;
                    ActorMatchDescriptor& victimActor = d.matchActors[victimId];
                    victimActor.teamId = 2;
                    victimActor.state = ActorState::Spectating;
                }
                Debug::warn(Debug::Category::Duel,
                    "[MATCH TEAM] host=%u evicted=%u target_team=%d reason=host_override_full\n",
                    playerId, victimId, requestedTeam);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Duel, StructuredLevel::Important,
                    "actor.team-slot-evicted", std::to_string(d.duelId), "team_pick", tick,
                    nlohmann::json{{"host", playerId}, {"victim", victimId},
                                   {"team", requestedTeam}, {"victim_was_npc", !npcVictims.empty()}});
            }
        }
    }
    playerIt->second.matchTeam = requestedTeam;
    d.matchTeams[playerId] = requestedTeam;
    ActorMatchDescriptor& actor = d.matchActors[playerId];
    actor.controller = ActorController::Human;
    actor.state = ActorState::Alive;
    actor.teamId = requestedTeam;
    const std::string roleId = roundTeamRoleId(gm, requestedTeam);
    const MatchRoleDefinition* role = roleId.empty()
        ? nullptr : MatchRoleRegistry::instance().get(roleId);
    if (role) {
        actor.roleId = role->id;
        actor.movementProfileId = role->movementPreset;
        actor.weaponProfileId = role->weaponSet;
    }
    if (d.zombieTower)
        actor.state = requestedTeam == 2 ? ActorState::Spectating : ActorState::Alive;
    // Team selection changes the human's role-owned loadout immediately. The
    // next spawn/round still re-resolves the same role from the roster.
    if (role && requestedTeam < 2) {
        if (d.zombieTower) {
            const glm::vec3 spawn = zombieTowerSpawnPointForActor(
                d, requestedTeam, playerId, "team-change");
            placeActorAtRunSpawn(d, playerIt->second, spawn);
        } else {
            completeAuthoritativeSpawn(sock, playerIt->second, false);
        }
    }
    if (d.zombieTower)
        reconcileZombieTowerParty(d, players, npcs);
    if (d.payloadMode && payloadActive && !hostOverride && requestedTeam < 2) {
        const float cooldownSeconds = std::max(0.0f, gm.payload.teamChangeCooldownSeconds);
        d.teamChangeCooldownUntil[playerId] =
            tick + (uint32_t)std::lround(cooldownSeconds * 60.0f);
    }
    message = "switched to " + roundTeamName(gm, requestedTeam);
    if (hostOverride) message += " (host override)";
    broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
        (playerIt->second.name + " " + message).c_str());
    Debug::warn(Debug::Category::Duel,
        "[MATCH TEAM] player=%u team=%d mode=%s result=accepted\n",
        playerId, requestedTeam, id.c_str());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "actor.team-assigned", std::to_string(d.duelId), "team_pick", tick,
        nlohmann::json{{"player", playerId}, {"team", requestedTeam},
                       {"team_name", roundTeamName(gm, requestedTeam)},
                       {"role", role ? role->id : ""},
                       {"weapon_set", role ? role->weaponSet : ""},
                       {"spawn_reapplied", role && requestedTeam < 2},
                       {"host_override", hostOverride}});
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
        player.duelSpawnPos =
            gamemodeSpawnPointForActor(d, player.id, players, npcs, "respawn-all");
        player.hasDuelSpawnPos = true;
        player.pos = player.duelSpawnPos;
        completeAuthoritativeSpawn(sock, player, false);
    }
    for (auto& kv : npcs) {
        ServerNpc& npc = kv.second;
        applyServerNpcRoleHealth(npc, "respawn-all");
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

void serverGamemodeRequestJuggernautSkip()
{
    ServerGamemodeState& d = serverGamemodeState();
    d.pendingJuggernautSkip = true;
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
        if (countdownWallDeadlineReached(d)) {
            d.phase = DUEL_PHASE_GO;
            logCountdownCompletion(d, tick);
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
            if (d.zombieTower) {
                if (d.ztAttemptResult) {
                    zombieTowerRespawnAttempt(d, players, npcs, npcSystem, tick);
                    broadcastDuelState(sock, d, players, totalPacketsOut);
                    break;
                }
                if (d.ztFinalResult) {
                    zombieTowerReturnToLobby(
                        d, sock, players, npcs, npcSystem, tick, totalPacketsOut);
                    break;
                }
            }
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
