#include "devtools/scenario-server-recorder.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <limits>

#include <nlohmann/json.hpp>

#include "combat/weapon-melee-config.h"
#include "debug/structured-log.h"
#include "devtools/scenario-tracks.h"
#include "network/server-gamemode.h"
#include "network/server.h"
#include "npc/npc.h"
#include "ragdoll/ragdoll-replication.h"

namespace {

std::string utcTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

std::string buildIdentity()
{
    std::ifstream in("version.txt");
    std::string value;
    if (in.is_open())
        std::getline(in, value);
    return value;
}

ScenarioTracks::WeaponShape shapeFrom(const WeaponExecution::PhysicalContactShape& shape,
                                      bool hasShape)
{
    ScenarioTracks::WeaponShape out;
    if (!hasShape)
        return out;
    out.valid = true;
    out.previousA = shape.previousA;
    out.previousB = shape.previousB;
    out.currentA = shape.currentA;
    out.currentB = shape.currentB;
    out.radius = shape.radius;
    return out;
}

void appendReplayBodyParts(const MimitaNet::ServerNpc& npc,
                           std::vector<ScenarioTracks::BodyPartBox>& out)
{
    out.clear();
    if (npc.replayControlled && !npc.bodyParts.empty() && npc.bodyPartCount > 0)
    {
        for (uint8_t i = 0; i < npc.bodyPartCount; ++i)
        {
            const auto& part = npc.bodyParts[i];
            ScenarioTracks::BodyPartBox box;
            box.name = "part";
            box.center = glm::vec3(part.cx, part.cy, part.cz);
            box.half = glm::vec3(part.hx, part.hy, part.hz);
            out.push_back(std::move(box));
        }
    }
}

ScenarioTracks::ActorState capturePlayer(const MimitaNet::ServerPlayer& player)
{
    ScenarioTracks::ActorState state;
    state.trackId = "player:" + std::to_string(player.id);
    state.kind = "player";
    state.position = player.pos;
    state.velocity = player.vel;
    state.aim = player.input.camForward;
    state.yaw = player.yaw;
    state.sizeScale = player.sizeScale;
    state.health = player.health;
    state.maxHealth = player.maxHealth;
    state.team = player.matchTeam;
    state.dead = player.dead;
    state.grounded = player.onGround;
    state.spawnGeneration = player.spawnGeneration;
    state.transformEpoch = player.transformEpoch;
    state.weaponId = player.equippedWeaponId;
    state.equippedSlot = player.equippedSlot;
    state.weaponState = player.weaponState;
    state.actionActive = player.meleeActionActive;
    state.actionId = player.meleeActionId;
    state.actionStartTick = player.meleeActionStartTick;
    state.actionPhaseTick = player.meleePhaseTick;
    state.nextAllowedTick = player.meleeNextAllowedTick;
    state.shape = shapeFrom(player.lastPhysicalWeaponShape,
                            player.hasLastPhysicalWeaponShape);
    if (player.hasRagdollPose && !player.ragdollHistory.empty())
    {
        const auto& pose = player.ragdollHistory.back().pose;
        for (int i = 0; i < pose.count && i < kRagdollLimbCount; ++i)
        {
            if (glm::length(pose.limbs[i].hitHalf) < 0.00001f)
                continue;
            ScenarioTracks::BodyPartBox box;
            box.name = "part";
            box.center = pose.limbs[i].hitCenter;
            box.half = pose.limbs[i].hitHalf;
            state.bodyParts.push_back(std::move(box));
        }
    }
    return state;
}

ScenarioTracks::ActorState captureNpc(const MimitaNet::ServerNpc& npc)
{
    ScenarioTracks::ActorState state;
    state.trackId = "npc:" + std::to_string(npc.entityId);
    state.kind = "npc";
    state.position = npc.pos;
    state.velocity = npc.vel;
    state.aim = npc.aim;
    state.yaw = npc.yaw;
    state.sizeScale = npc.sizeScale;
    state.health = npc.health;
    state.maxHealth = npc.health;
    state.team = npc.matchTeam;
    state.dead = npc.health <= 0;
    state.grounded = npc.onGround;
    state.spawnGeneration = 0;
    state.transformEpoch = npc.transformEpoch;
    state.weaponId = npc.equippedWeaponId;
    state.equippedSlot = npc.equippedSlot;
    state.weaponState = npc.weaponState;
    state.actionActive = npc.meleeActionActive;
    state.actionId = npc.meleeActionId;
    state.actionStartTick = npc.meleeActionStartTick;
    state.nextAllowedTick = npc.meleeNextAttackTick;
    state.shape = shapeFrom(npc.lastPhysicalWeaponShape,
                            npc.hasLastPhysicalWeaponShape);
    appendReplayBodyParts(npc, state.bodyParts);
    if (state.bodyParts.empty() && npc.bodyPartCount > 0)
    {
        for (uint8_t i = 0; i < npc.bodyPartCount; ++i)
        {
            const auto& part = npc.bodyParts[i];
            ScenarioTracks::BodyPartBox box;
            box.name = "part";
            box.center = glm::vec3(part.cx, part.cy, part.cz);
            box.half = glm::vec3(part.hx, part.hy, part.hz);
            state.bodyParts.push_back(std::move(box));
        }
    }
    return state;
}

} // namespace

ScenarioServerRecorder& ScenarioServerRecorder::instance()
{
    static ScenarioServerRecorder recorder;
    return recorder;
}

bool ScenarioServerRecorder::start(const std::string& scenarioPath,
                                   std::string* error)
{
    if (mActive)
    {
        if (error) *error = "server_recording_already_active";
        return false;
    }
    if (!ScenarioTracks::validateScenarioPath(scenarioPath, error))
        return false;
    mScenarioPath = scenarioPath;
    mSidecarPath = scenarioPath + ".server.jsonl";
    std::error_code ec;
    const std::filesystem::path parent =
        std::filesystem::path(mSidecarPath).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent, ec);
    mOut.open(mSidecarPath, std::ios::out | std::ios::trunc);
    if (!mOut.is_open())
    {
        if (error) *error = "cannot_open_server_scenario_track";
        return false;
    }
    mActive = true;
    mFrames = 0;
    mStartTick = 0;
    mLastActors.clear();
    mKnownIdentities.clear();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.server.record.started", "scenario-server-recorder",
        "authoritative actor track armed for scenario capture", 0,
        nlohmann::json{{"scenario_path", mScenarioPath},
                       {"sidecar_path", mSidecarPath}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

void ScenarioServerRecorder::writeHeader(uint32_t serverTick)
{
    nlohmann::json revisions = nlohmann::json::array();
    for (const auto& entry : WeaponData::MeleeMoveSetRegistry::instance().all())
    {
        revisions.push_back({
            {"weapon", entry.first},
            {"revision", entry.second.revision},
            {"hash", entry.second.hash}
        });
    }
    const auto& mode = MimitaNet::serverGamemodeState();
    mStartTick = serverTick;
    nlohmann::json tracks = nlohmann::json::array();
    tracks.push_back("server_actors");
    mOut << nlohmann::json{
        {"kind", "header"},
        {"schema", ScenarioTracks::kSchemaV2},
        {"created_utc", utcTimestamp()},
        {"tick_rate", 60},
        {"map", mode.mapId},
        {"mode", mode.matchMode},
        {"weapon_set", mode.communityWeaponSetId},
        {"build_id", buildIdentity()},
        {"start_server_tick", serverTick},
        {"tracks", tracks},
        {"melee_revisions", revisions},
        {"actors", nlohmann::json::array()}
    }.dump() << '\n';
}

void ScenarioServerRecorder::writeIdentity(const std::string& trackId,
                                           const std::string& kind,
                                           uint32_t entityId,
                                           const std::string& name,
                                           const std::string& avatar,
                                           uint32_t spawnGeneration,
                                           uint16_t transformEpoch)
{
    if (trackId.empty() || mKnownIdentities.count(trackId))
        return;
    mKnownIdentities[trackId] = true;
    ScenarioTracks::ActorIdentity identity;
    identity.trackId = trackId;
    identity.kind = kind;
    identity.entityId = entityId;
    identity.name = name;
    identity.avatar = avatar;
    identity.spawnGeneration = spawnGeneration;
    identity.transformEpoch = transformEpoch;
    nlohmann::json line = ScenarioTracks::identityJson(identity);
    line["kind"] = "actor_identity";
    mOut << line.dump() << '\n';
}

void ScenarioServerRecorder::updateLifecycle(
    const std::unordered_map<std::string, ActorSnapshot>& current,
    uint32_t serverTick)
{
    for (const auto& entry : current)
    {
        const ActorSnapshot& now = entry.second;
        auto previous = mLastActors.find(entry.first);
        const bool isNew = previous == mLastActors.end();
        const bool respawned = !isNew &&
            (previous->second.spawnGeneration != now.spawnGeneration ||
             previous->second.transformEpoch != now.transformEpoch);
        const bool died = !isNew && previous->second.health > 0 && now.health <= 0;
        if (!isNew && !respawned && !died)
            continue;
        ScenarioTracks::LifecycleEvent event;
        event.trackId = entry.first;
        event.event = isNew ? "spawn" : (died ? "death" : "respawn");
        event.serverTick = serverTick;
        event.spawnGeneration = now.spawnGeneration;
        event.transformEpoch = now.transformEpoch;
        event.health = now.health;
        mOut << ScenarioTracks::lifecycleJson(event).dump() << '\n';
    }
    for (const auto& entry : mLastActors)
    {
        if (current.find(entry.first) != current.end())
            continue;
        ScenarioTracks::LifecycleEvent event;
        event.trackId = entry.first;
        event.event = "despawn";
        event.serverTick = serverTick;
        event.spawnGeneration = entry.second.spawnGeneration;
        event.transformEpoch = entry.second.transformEpoch;
        event.health = entry.second.health;
        mOut << ScenarioTracks::lifecycleJson(event).dump() << '\n';
    }
    mLastActors.clear();
    for (const auto& entry : current)
        mLastActors[entry.first] = entry.second;
}

void ScenarioServerRecorder::captureTick(
    const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
    const std::unordered_map<uint32_t, MimitaNet::ServerNpc>& npcs,
    const NpcSystem& npcSystem,
    uint32_t serverTick)
{
    (void)npcSystem;
    if (!mActive || !mOut.is_open())
        return;
    if (mFrames == 0)
        writeHeader(serverTick);

    std::unordered_map<std::string, ActorSnapshot> current;
    nlohmann::json actors = nlohmann::json::array();
    for (const auto& entry : players)
    {
        const auto& player = entry.second;
        if (!player.spawned)
            continue;
        const std::string track = "player:" + std::to_string(player.id);
        writeIdentity(track, "player", player.id, player.name, player.avatarName,
                      player.spawnGeneration, player.transformEpoch);
        ScenarioTracks::ActorState state = capturePlayer(player);
        actors.push_back(ScenarioTracks::actorStateJson(state));
        current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
    }
    for (const auto& entry : npcs)
    {
        const auto& npc = entry.second;
        const std::string track = "npc:" + std::to_string(npc.entityId);
        writeIdentity(track, "npc", npc.entityId, npc.name, npc.avatarName,
                      0, npc.transformEpoch);
        ScenarioTracks::ActorState state = captureNpc(npc);
        actors.push_back(ScenarioTracks::actorStateJson(state));
        current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
    }

    updateLifecycle(current, serverTick);
    mOut << nlohmann::json{
        {"kind", "server_tick"},
        {"tick", serverTick},
        {"server_tick", serverTick},
        {"actors", std::move(actors)}
    }.dump() << '\n';
    ++mFrames;
    if (mFrames % 6 == 0)
        mOut.flush();
}

void ScenarioServerRecorder::writeFooter(const char* reason)
{
    if (!mOut.is_open())
        return;
    mOut << nlohmann::json{
        {"kind", "footer"},
        {"ended_utc", utcTimestamp()},
        {"frames", mFrames},
        {"start_server_tick", mStartTick},
        {"stop_reason", reason ? reason : "unknown"}
    }.dump() << '\n';
    mOut.flush();
    mOut.close();
}

void ScenarioServerRecorder::stop(const char* reason)
{
    if (!mActive)
        return;
    mActive = false;
    writeFooter(reason);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.server.record.stopped", "scenario-server-recorder",
        "authoritative actor track finalized", mFrames,
        nlohmann::json{{"scenario_path", mScenarioPath},
                       {"sidecar_path", mSidecarPath},
                       {"frames", mFrames},
                       {"stop_reason", reason ? reason : "unknown"}},
        __FILE__, __LINE__, __FUNCTION__);
}
