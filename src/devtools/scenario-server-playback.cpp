#include "devtools/scenario-server-playback.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include <nlohmann/json.hpp>

#include "combat/weapon-melee-config.h"
#include "combat/weapon-registry.h"
#include "debug/structured-log.h"
#include "network/net_common.h"
#include "network/server-gamemode.h"
#include "network/server.h"
#include "npc/npc.h"

namespace {

Npc* findNpc(NpcSystem& npcSystem, uint32_t id)
{
    for (Npc& npc : npcSystem.all())
        if (npc.id == id)
            return &npc;
    return nullptr;
}

} // namespace

ScenarioServerPlayback& ScenarioServerPlayback::instance()
{
    static ScenarioServerPlayback playback;
    return playback;
}

bool ScenarioServerPlayback::load(const std::string& path, std::string* error)
{
    mServerTicks.clear();
    mIdentities.clear();
    mRevisions.clear();
    mLocalPlayerTrack.clear();
    mLocalPlayerId = 0;
    mFirstTrackTick = 0;
    mLastTrackTick = 0;

    std::ifstream in(path);
    if (!in.is_open())
    {
        if (error) *error = "cannot_open_scenario";
        return false;
    }

    bool sawHeader = false;
    uint32_t firstClientServerTick = 0;
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;
        nlohmann::json value;
        try
        {
            value = nlohmann::json::parse(line);
        }
        catch (const std::exception&)
        {
            std::string rest;
            (void)std::getline(in, rest);
            if (!rest.empty())
            {
                if (error) *error = "invalid_json_line";
                return false;
            }
            break;
        }
        const std::string kind = value.value("kind", "");
        if (kind == "header")
        {
            sawHeader = true;
            if (value.value("schema", 0) != ScenarioTracks::kSchemaV2)
            {
                if (error) *error = "server_track_requires_schema_2";
                return false;
            }
            mMap = value.value("map", "");
            mMode = value.value("mode", "");
            mWeaponSet = value.value("weapon_set", 0);
            mLocalPlayerId = value.value("local_player_id", 0u);
            if (mLocalPlayerId != 0)
                mLocalPlayerTrack = "player:" + std::to_string(mLocalPlayerId);
            if (value.contains("melee_revisions"))
                mRevisions = ScenarioTracks::parseMeleeRevisions(value["melee_revisions"]);
            if (value.contains("start") && value["start"].is_object())
            {
                mStartSpawnGeneration = value["start"].value("spawn_generation", 0u);
                mStartTransformEpoch = static_cast<uint16_t>(
                    value["start"].value("transform_epoch", 0u));
            }
            if (!value.contains("tracks") || !value["tracks"].is_array() ||
                std::find(value["tracks"].begin(), value["tracks"].end(),
                          "server_actors") == value["tracks"].end())
            {
                if (error) *error = "missing_server_actor_track";
                return false;
            }
        }
        else if (kind == "actor_identity")
        {
            mIdentities.push_back(ScenarioTracks::parseIdentity(value));
        }
        else if (kind == "tick")
        {
            if (firstClientServerTick == 0)
                firstClientServerTick = value.value("server_tick", 0u);
        }
        else if (kind == "server_tick")
        {
            const uint32_t tick = value.value("tick", value.value("server_tick", 0u));
            if (tick == 0)
                continue;
            if (mFirstTrackTick == 0 || tick < mFirstTrackTick)
                mFirstTrackTick = tick;
            mLastTrackTick = std::max(mLastTrackTick, tick);
            std::vector<ScenarioTracks::ActorState> states;
            if (value.contains("actors") && value["actors"].is_array())
                for (const auto& actor : value["actors"])
                    states.push_back(ScenarioTracks::parseActorState(actor));
            mServerTicks[tick] = std::move(states);
        }
    }
    if (!sawHeader)
    {
        if (error) *error = "missing_header";
        return false;
    }
    if (mServerTicks.empty())
    {
        if (error) *error = "server_track_empty";
        return false;
    }
    if (firstClientServerTick != 0 && firstClientServerTick < mFirstTrackTick)
        mFirstTrackTick = firstClientServerTick;
    if (mLocalPlayerTrack.empty())
    {
        for (const auto& entry : mServerTicks)
            for (const auto& actor : entry.second)
                if (actor.kind == "player")
                {
                    mLocalPlayerTrack = actor.trackId;
                    if (mLocalPlayerId == 0)
                    {
                        const size_t colon = actor.trackId.find(':');
                        if (colon != std::string::npos)
                            mLocalPlayerId = static_cast<uint32_t>(
                                std::stoul(actor.trackId.substr(colon + 1)));
                    }
                    break;
                }
    }
    if (mLocalPlayerTrack.empty())
    {
        if (error) *error = "missing_local_player_track";
        return false;
    }
    return true;
}

bool ScenarioServerPlayback::prepare(
    const std::string& path,
    const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
    NpcSystem& npcSystem,
    std::string* error,
    std::string* status)
{
    stop("prepare_replaced", npcSystem);
    mPath = path;
    if (!load(path, error))
        return false;

    const auto& mode = MimitaNet::serverGamemodeState();
    if (!mMap.empty() && !mode.mapId.empty() &&
        !MimitaNet::mapIdsReferToSameMap(mMap, mode.mapId))
    {
        if (error) *error = "map_mismatch";
        return false;
    }
    if (mWeaponSet > 0 && mode.communityWeaponSetId > 0 &&
        mWeaponSet != mode.communityWeaponSetId)
    {
        if (error) *error = "weapon_set_mismatch";
        return false;
    }
    for (const ScenarioTracks::MeleeRevision& revision : mRevisions)
    {
        const WeaponData::MeleeMoveSet* current =
            WeaponData::MeleeMoveSetRegistry::instance().get(revision.weaponId);
        if (!current)
        {
            if (error) *error = "melee_moveset_missing:" + revision.weaponId;
            return false;
        }
        if (revision.hash != 0 && current->hash != revision.hash)
        {
            if (error) *error = "melee_moveset_hash_mismatch:" + revision.weaponId;
            return false;
        }
        if (revision.revision != 0 && current->revision != revision.revision)
        {
            if (error) *error = "melee_moveset_revision_mismatch:" + revision.weaponId;
            return false;
        }
    }

    auto local = players.find(mLocalPlayerId);
    if (local == players.end())
    {
        if (error) *error = "local_player_missing";
        return false;
    }
    const MimitaNet::ServerPlayer& localPlayer = local->second;
    if (localPlayer.spawnState != MimitaNet::ServerPlayer::Active || localPlayer.dead)
    {
        if (error) *error = "spawn_not_ready";
        return false;
    }
    if (mStartSpawnGeneration != 0 &&
        localPlayer.spawnGeneration != mStartSpawnGeneration)
    {
        if (error) *error = "spawn_generation_mismatch";
        return false;
    }
    if (mStartTransformEpoch != 0 &&
        localPlayer.transformEpoch != mStartTransformEpoch)
    {
        if (error) *error = "transform_epoch_mismatch";
        return false;
    }

    // A recorded human that is still connected must never be turned into a
    // proxy. Fail rather than silently producing a misleading target track.
    for (const auto& identity : mIdentities)
    {
        if (identity.kind != "player" || identity.entityId == mLocalPlayerId)
            continue;
        if (players.find(identity.entityId) != players.end())
        {
            if (error) *error = "live_target_conflict:" + std::to_string(identity.entityId);
            return false;
        }
    }

    auto firstIt = mServerTicks.lower_bound(mFirstTrackTick);
    if (firstIt == mServerTicks.end())
        firstIt = mServerTicks.begin();
    mPlaybackStartServerTick = mFirstTrackTick;
    mNextProxyId = 0x40000000u;
    mProxies.clear();
    for (const auto& state : firstIt->second)
    {
        if (state.trackId == mLocalPlayerTrack)
            continue;
        std::string name = state.trackId;
        std::string avatar;
        for (const auto& identity : mIdentities)
            if (identity.trackId == state.trackId)
            {
                name = identity.name.empty() ? name : identity.name;
                avatar = identity.avatar;
                break;
            }
        Proxy proxy;
        proxy.trackId = state.trackId;
        proxy.name = name;
        proxy.avatar = avatar;
        proxy.proxyId = mNextProxyId++;
        mProxies[state.trackId] = proxy;
        mPlaybackStartServerTick = mFirstTrackTick;
        applyActor(mProxies[state.trackId], state, npcSystem,
                   firstIt->first, true);
    }
    mPrepared = true;
    mActive = false;
    if (status)
    {
        std::ostringstream out;
        out << "scenario server prepared proxies=" << mProxies.size()
            << " start_tick=" << mFirstTrackTick
            << " last_tick=" << mLastTrackTick
            << " local_track=" << mLocalPlayerTrack;
        *status = out.str();
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.server_ready", "scenario-server-playback",
        "authoritative target proxies prepared from the recorded track",
        mFirstTrackTick,
        nlohmann::json{{"path", mPath},
                       {"proxy_count", mProxies.size()},
                       {"first_track_tick", mFirstTrackTick},
                       {"last_track_tick", mLastTrackTick},
                       {"local_player_track", mLocalPlayerTrack},
                       {"map", mMap},
                       {"mode", mMode},
                       {"weapon_set", mWeaponSet}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

bool ScenarioServerPlayback::begin(
    uint32_t serverTick,
    const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
    std::string* error,
    std::string* status)
{
    if (!mPrepared)
    {
        if (error) *error = "scenario_not_prepared";
        return false;
    }
    auto local = players.find(mLocalPlayerId);
    if (local == players.end() ||
        local->second.spawnState != MimitaNet::ServerPlayer::Active ||
        local->second.dead)
    {
        if (error) *error = "spawn_not_ready";
        return false;
    }
    if (mStartSpawnGeneration != 0 &&
        local->second.spawnGeneration != mStartSpawnGeneration)
    {
        if (error) *error = "spawn_generation_mismatch";
        return false;
    }
    if (mStartTransformEpoch != 0 &&
        local->second.transformEpoch != mStartTransformEpoch)
    {
        if (error) *error = "transform_epoch_mismatch";
        return false;
    }
    mPlaybackStartServerTick = serverTick;
    mActive = true;
    if (status)
        *status = "scenario server playback began at tick " +
            std::to_string(serverTick);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.server_began", "scenario-server-playback",
        "authoritative proxy track started", serverTick,
        nlohmann::json{{"path", mPath},
                       {"server_tick", serverTick},
                       {"first_track_tick", mFirstTrackTick},
                       {"proxy_count", mProxies.size()}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

uint32_t ScenarioServerPlayback::mapTrackTick(uint32_t serverTick) const
{
    if (serverTick <= mPlaybackStartServerTick)
        return mFirstTrackTick;
    return mFirstTrackTick + (serverTick - mPlaybackStartServerTick);
}

const ScenarioTracks::ActorState* ScenarioServerPlayback::stateFor(
    const std::string& trackId, uint32_t trackTick) const
{
    auto it = mServerTicks.upper_bound(trackTick);
    if (it == mServerTicks.begin())
        return nullptr;
    do
    {
        --it;
        for (const auto& state : it->second)
            if (state.trackId == trackId)
                return &state;
    } while (it != mServerTicks.begin());
    return nullptr;
}

bool ScenarioServerPlayback::applyActor(
    Proxy& proxy,
    const ScenarioTracks::ActorState& state,
    NpcSystem& npcSystem,
    uint32_t serverTick,
    bool initial)
{
    Npc* npc = findNpc(npcSystem, proxy.proxyId);
    if (!npc)
    {
        npcSystem.spawnNpc(proxy.proxyId, 1.0f, state.position,
                           proxy.name.empty() ? state.trackId : proxy.name);
        npc = findNpc(npcSystem, proxy.proxyId);
        if (!npc)
            return false;
        npc->transformEpoch = state.transformEpoch;
        proxy.hasLife = false;
    }

    const bool lifeChanged = !proxy.hasLife ||
        proxy.lastGeneration != state.spawnGeneration ||
        proxy.lastEpoch != state.transformEpoch;
    const bool firstApply = initial || !proxy.present || lifeChanged;

    npc->replayControlled = true;
    npc->replayTrackId = proxy.proxyId;
    if (!proxy.avatar.empty())
        npc->avatarName = proxy.avatar;
    npc->body.username = proxy.name.empty() ? state.trackId : proxy.name;
    npc->body.isNpcActor = true;
    npc->body.pos = state.position;
    npc->body.vel = state.velocity;
    npc->body.yaw = state.yaw;
    npc->body.ground.onGround = state.grounded;
    npc->body.ground.stableOnGround = state.grounded;
    npc->body.sizeScale = state.sizeScale;
    npc->body.matchTeam = state.team;
    npc->transformEpoch = state.transformEpoch != 0
        ? state.transformEpoch : npc->transformEpoch;
    npc->body.equippedWeaponId = state.weaponId;
    npc->body.equippedSlot = state.equippedSlot;
    npc->body.hasValidWeapon = !state.weaponId.empty() && state.weaponId != "nothing";
    npc->body.networkWeaponState = static_cast<uint8_t>(state.weaponState);

    const uint32_t actionOffset =
        state.actionStartTick >= mFirstTrackTick
            ? state.actionStartTick - mFirstTrackTick : 0;
    const uint32_t actionStart =
        (mActive ? mPlaybackStartServerTick : mFirstTrackTick) + actionOffset;
    (void)actionStart;
    npc->replayBodyParts.clear();
    for (const auto& part : state.bodyParts)
    {
        Npc::ReplayBodyPart replay;
        replay.name = part.name;
        replay.center = part.center;
        replay.half = part.half;
        npc->replayBodyParts.push_back(std::move(replay));
    }
    npc->hasReplayWeaponShape = state.shape.valid;
    npc->replayBladePreviousA = state.shape.previousA;
    npc->replayBladePreviousB = state.shape.previousB;
    npc->replayBladeCurrentA = state.shape.currentA;
    npc->replayBladeCurrentB = state.shape.currentB;
    npc->replayBladeRadius = state.shape.radius;

    if (firstApply)
    {
        npc->body.maxHp = std::max(1, state.maxHealth);
        npc->body.currentHp = state.health;
        npc->body.dead = state.dead;
    }
    npc->body.updateModelWorldTransforms();

    proxy.lastGeneration = state.spawnGeneration;
    proxy.lastEpoch = state.transformEpoch;
    proxy.hasLife = true;
    proxy.present = true;
    if (lifeChanged)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "scenario.playback.proxy_lifecycle", "scenario-server-playback",
            "target proxy life applied from recorded actor track",
            serverTick,
            nlohmann::json{{"track", state.trackId},
                           {"proxy_id", proxy.proxyId},
                           {"kind", state.kind},
                           {"spawn_generation", state.spawnGeneration},
                           {"transform_epoch", state.transformEpoch},
                           {"health", state.health},
                           {"position", {state.position.x, state.position.y,
                                         state.position.z}}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    return true;
}

void ScenarioServerPlayback::removeProxy(Proxy& proxy,
                                         NpcSystem& npcSystem,
                                         uint32_t serverTick)
{
    if (!proxy.present)
        return;
    npcSystem.destroySelected({proxy.proxyId});
    proxy.present = false;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.proxy_lifecycle", "scenario-server-playback",
        "target proxy despawned because the recorded actor left the track",
        serverTick,
        nlohmann::json{{"track", proxy.trackId},
                       {"proxy_id", proxy.proxyId},
                       {"event", "despawn"}},
        __FILE__, __LINE__, __FUNCTION__);
}

void ScenarioServerPlayback::tick(
    uint32_t serverTick,
    const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
    NpcSystem& npcSystem)
{
    (void)players;
    if (!mActive)
        return;
    const uint32_t trackTick = mapTrackTick(serverTick);
    if (trackTick > mLastTrackTick + 2)
    {
        stop("complete", npcSystem);
        return;
    }
    // Actors can enter the recording after frame 0. Create their proxies when
    // they first appear so spawn/despawn lifecycle is preserved.
    auto latest = mServerTicks.upper_bound(trackTick);
    if (latest != mServerTicks.begin())
    {
        --latest;
        for (const auto& state : latest->second)
        {
            if (state.trackId == mLocalPlayerTrack ||
                mProxies.find(state.trackId) != mProxies.end())
                continue;
            Proxy proxy;
            proxy.trackId = state.trackId;
            proxy.name = state.trackId;
            for (const auto& identity : mIdentities)
                if (identity.trackId == state.trackId)
                {
                    if (!identity.name.empty())
                        proxy.name = identity.name;
                    proxy.avatar = identity.avatar;
                    break;
                }
            proxy.proxyId = mNextProxyId++;
            mProxies[state.trackId] = proxy;
        }
    }
    for (auto& entry : mProxies)
    {
        Proxy& proxy = entry.second;
        const ScenarioTracks::ActorState* state = stateFor(proxy.trackId, trackTick);
        if (!state)
        {
            removeProxy(proxy, npcSystem, serverTick);
            continue;
        }
        applyActor(proxy, *state, npcSystem, serverTick, false);
        const glm::vec3 expected = state->position;
        Npc* npc = findNpc(npcSystem, proxy.proxyId);
        if (npc && glm::distance(npc->body.pos, expected) > 0.01f)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Errors,
                "scenario.playback.proxy_divergence", "scenario-server-playback",
                "target proxy did not follow the recorded authoritative transform",
                serverTick,
                nlohmann::json{{"track", proxy.trackId},
                               {"proxy_id", proxy.proxyId},
                               {"track_tick", trackTick},
                               {"expected", {expected.x, expected.y, expected.z}},
                               {"actual", {npc->body.pos.x, npc->body.pos.y,
                                           npc->body.pos.z}},
                               {"error_m", glm::distance(npc->body.pos, expected)}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
}

void ScenarioServerPlayback::stop(const char* reason, NpcSystem& npcSystem)
{
    if (!mPrepared && !mActive && mProxies.empty())
        return;
    for (auto& entry : mProxies)
        removeProxy(entry.second, npcSystem, mPlaybackStartServerTick);
    mProxies.clear();
    const bool wasActive = mActive;
    const bool wasPrepared = mPrepared;
    mActive = false;
    mPrepared = false;
    if (wasActive || wasPrepared)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            wasActive ? "scenario.playback.server_completed"
                      : "scenario.playback.server_stopped",
            "scenario-server-playback",
            "authoritative scenario proxy track stopped", 0,
            nlohmann::json{{"path", mPath},
                           {"reason", reason ? reason : "unknown"},
                           {"was_active", wasActive}},
            __FILE__, __LINE__, __FUNCTION__);
    }
}

