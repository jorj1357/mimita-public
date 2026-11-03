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

nlohmann::json inputStateJson(const InputState& input)
{
    return {
        {"wish", {input.wishMoveXY.x, input.wishMoveXY.y}},
        {"jump_held", input.jumpHeld},
        {"jump_pressed", input.jumpPressed},
        {"dash_pressed", input.dashPressed},
        {"down_dash_pressed", input.downDashPressed},
        {"freeze_held", input.freezeHeld},
        {"freeze_pressed", input.freezePressed},
        {"crouch_held", input.crouchHeld},
        {"interact_held", input.interactHeld},
        {"movement_pressed", input.movementPressed},
        {"movement_just_pressed", input.movementJustPressed},
        {"movement_held_duration", input.movementHeldDuration},
        {"cam_forward", ScenarioTracks::vec3Json(input.camForward)}
    };
}

nlohmann::json contactJson(const MovementContact& contact)
{
    return {
        {"contact_id", contact.contactId},
        {"source_event_id", contact.sourceEventId},
        {"source_entity_id", contact.sourceEntityId},
        {"target_entity_id", contact.targetEntityId},
        {"surface_id", contact.surfaceId},
        {"world_triangle", contact.surfaceId},
        {"solid_id", contact.surfaceId},
        {"material_id", contact.materialId},
        {"kind", static_cast<int>(contact.kind)},
        {"source", static_cast<int>(contact.source)},
        {"shape", static_cast<int>(contact.shapeKind)},
        {"subshape", static_cast<int>(contact.subshape)},
        {"point", ScenarioTracks::vec3Json(contact.point)},
        {"normal", ScenarioTracks::vec3Json(contact.normal)},
        {"sweep_velocity", ScenarioTracks::vec3Json(contact.sweepVelocity)},
        {"surface_velocity", ScenarioTracks::vec3Json(contact.surfaceVelocity)},
        {"penetration_m", contact.penetrationDepth}
    };
}

nlohmann::json projectileJson(const MimitaNet::ServerProjectile& projectile)
{
    return {
        {"track", "object:" + std::to_string(projectile.id)},
        {"runtime_id", projectile.id},
        {"kind", "projectile"},
        {"weapon_type", projectile.weaponType},
        {"owner_player_id", projectile.ownerPlayerId},
        {"owner_npc_id", projectile.ownerNpcId},
        {"fire_serial", projectile.fireSerial},
        {"position", ScenarioTracks::vec3Json(projectile.position)},
        {"previous_position", ScenarioTracks::vec3Json(projectile.previousPosition)},
        {"velocity", ScenarioTracks::vec3Json(projectile.velocity)},
        {"rotation", {projectile.rotation.w, projectile.rotation.x,
                      projectile.rotation.y, projectile.rotation.z}},
        {"angular_velocity", ScenarioTracks::vec3Json(projectile.angularVelocity)},
        {"age", projectile.age},
        {"lifetime", projectile.lifetime},
        {"radius", projectile.radius},
        {"splash_radius", projectile.splashRadius},
        {"bounce_count", projectile.bounceCount},
        {"max_bounce_count", projectile.maxBounceCount},
        {"penetrations_remaining", projectile.penetrationsRemaining},
        {"exploded", projectile.exploded},
        {"world_touched", projectile.worldTouched},
        {"spawn_tick", projectile.spawnTick},
        {"simulation_tick", projectile.simulationTick}
    };
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
    mCanonical = false;
    mCaptureInvalid = false;
    mCaptureFailure.clear();
    mScenarioId = scenarioPath;
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

bool ScenarioServerRecorder::startCanonical(const std::string& scenarioId,
                                            const std::string& scenarioPath,
                                            std::string* error)
{
    if (mActive)
    {
        if (error) *error = "server_recording_already_active";
        return false;
    }
    if (scenarioId.empty())
    {
        if (error) *error = "missing_scenario_id";
        return false;
    }
    mScenarioId = scenarioId;
    mScenarioPath = scenarioPath;
    mSidecarPath.clear();
    mCanonical = true;
    mActive = true;
    mCaptureInvalid = false;
    mCaptureFailure.clear();
    mFrames = 0;
    mStartTick = 0;
    mLastActors.clear();
    mKnownIdentities.clear();
    if (!writeScenarioEvent(
            "scenario.server.record.started",
            "authoritative canonical scenario capture armed",
            0,
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"scenario_path", mScenarioPath},
                           {"journal_path", StructuredLogger::instance().eventsPath()},
                           {"process_role", "server"},
                           {"tick_rate", 60}}))
    {
        markCaptureFailure("scenario.server.record.started", 0);
        mActive = false;
        if (error) *error = "journal_refused_server_record_start";
        return false;
    }
    return true;
}

bool ScenarioServerRecorder::writeScenarioEvent(const std::string& event,
                                                const std::string& reason,
                                                uint32_t tick,
                                                const nlohmann::json& fields)
{
    return StructuredLogger::instance().writeEventChecked(
        StructuredCategory::General, StructuredLevel::Important,
        event, mScenarioId, reason, tick, fields,
        __FILE__, __LINE__, __FUNCTION__);
}

void ScenarioServerRecorder::markCaptureFailure(const std::string& reason,
                                                uint32_t tick)
{
    if (mCaptureInvalid)
        return;
    mCaptureInvalid = true;
    mCaptureFailure = reason;
    writeScenarioEvent(
        "scenario.record.invalid",
        "required authoritative scenario evidence could not be written",
        tick,
        nlohmann::json{{"scenario_id", mScenarioId},
                       {"frame", mFrames},
                       {"last_complete_frame", mFrames},
                       {"reason", reason},
                       {"journal_path", StructuredLogger::instance().eventsPath()},
                       {"process_role", "server"}});
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
    const std::unordered_map<uint32_t, MimitaNet::ServerProjectile>& projectiles,
    uint32_t serverTick)
{
    if (!mActive)
        return;

    std::unordered_map<uint32_t, const Npc*> simulated;
    for (const Npc& npc : npcSystem.all())
        simulated[npc.id] = &npc;

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
        state.hasInput = true;
        state.input.wishMoveXY = player.input.wish;
        state.input.jumpHeld = player.input.jumpHeld;
        state.input.dashPressed = player.input.dashPressed;
        state.input.downDashPressed = player.input.downDashPressed;
        state.input.freezeHeld = player.input.freezeHeld;
        state.input.crouchHeld = player.input.crouchHeld;
        state.input.interactHeld = player.input.interactHeld;
        state.input.camForward = player.input.camForward;
        state.stableGrounded = player.onGround;
        state.hasWorldContact = player.onGround;
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
        auto simIt = simulated.find(npc.entityId);
        if (simIt != simulated.end() && simIt->second)
        {
            const Npc& sim = *simIt->second;
            if (sim.hasLastAcceptedInput)
            {
                state.hasInput = true;
                state.input = sim.lastAcceptedInput;
            }
            state.previousPosition = sim.body.actorCollisionPreviousPos;
            state.stableGrounded = sim.body.ground.stableOnGround;
            state.hasWorldContact = sim.body.ground.hasWorldContact;
            state.rootContainment = sim.lastRootInsideClosedSolid ? 1 : 2;
            if (!sim.lastContainmentKnown)
                state.rootContainment = 0;
            state.maxPenetration = sim.lastFinalPenetration;
            state.containmentStage = sim.lastContainmentStage;
            state.npcState = static_cast<int>(sim.stateMachine.currentState);
            state.npcRngState = sim.rngState;
            state.npcDashCooldown = sim.dashCooldown;
            state.npcDownDashCooldown = sim.downDashCooldown;
            state.npcJumpCooldown = sim.jumpCooldown;
            state.npcAttackCooldown = sim.attackCooldown;
            const size_t maxContacts =
                std::min<size_t>(sim.body.movementContacts.size(), 32);
            for (size_t i = 0; i < maxContacts; ++i)
            {
                const MovementContact& contact = sim.body.movementContacts[i];
                ScenarioTracks::ContactEvidence evidence;
                evidence.contactId = contact.contactId;
                evidence.sourceEventId = contact.sourceEventId;
                evidence.sourceEntityId = contact.sourceEntityId;
                evidence.targetEntityId = contact.targetEntityId;
                evidence.surfaceId = contact.surfaceId;
                evidence.worldTriangle = contact.surfaceId;
                evidence.solidId = contact.surfaceId;
                evidence.materialId = contact.materialId;
                evidence.kind = static_cast<int>(contact.kind);
                evidence.source = static_cast<int>(contact.source);
                evidence.shape = static_cast<int>(contact.shapeKind);
                evidence.subshape = static_cast<int>(contact.subshape);
                evidence.point = contact.point;
                evidence.normal = contact.normal;
                evidence.sweepVelocity = contact.sweepVelocity;
                evidence.surfaceVelocity = contact.surfaceVelocity;
                evidence.penetration = contact.penetrationDepth;
                state.contacts.push_back(evidence);
            }
        }
        actors.push_back(ScenarioTracks::actorStateJson(state));
        current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
    }

    nlohmann::json objects = nlohmann::json::array();
    for (const auto& entry : projectiles)
        objects.push_back(projectileJson(entry.second));

    if (mCanonical)
    {
        if (!writeScenarioEvent(
                "scenario.record.tick",
                "canonical authoritative actor, contact, and object evidence for one fixed tick",
                serverTick,
                nlohmann::json{{"scenario_id", mScenarioId},
                               {"frame", mFrames},
                               {"server_tick", serverTick},
                               {"journal_path", StructuredLogger::instance().eventsPath()},
                               {"process_role", "server"},
                               {"actors", actors},
                               {"objects", objects}}))
        {
            markCaptureFailure("server_tick_write_refused", serverTick);
            stop("capture_failed");
            return;
        }
    }

    if (mOut.is_open())
    {
        if (mFrames == 0)
            writeHeader(serverTick);
        updateLifecycle(current, serverTick);
        mOut << nlohmann::json{
            {"kind", "server_tick"},
            {"tick", serverTick},
            {"server_tick", serverTick},
            {"actors", std::move(actors)}
        }.dump() << '\n';
        if (mFrames % 6 == 0)
            mOut.flush();
    }
    ++mFrames;
}

void ScenarioServerRecorder::captureCheckpoint(
    uint32_t bookmarkNumber,
    uint32_t clientTick,
    const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
    const std::unordered_map<uint32_t, MimitaNet::ServerNpc>& npcs,
    const NpcSystem& npcSystem,
    const std::unordered_map<uint32_t, MimitaNet::ServerProjectile>& projectiles,
    uint32_t serverTick)
{
    if (!mActive || !mCanonical)
        return;

    std::unordered_map<uint32_t, const Npc*> simulated;
    for (const Npc& npc : npcSystem.all())
        simulated[npc.id] = &npc;

    nlohmann::json actors = nlohmann::json::array();
    for (const auto& entry : players)
    {
        const auto& player = entry.second;
        if (!player.spawned)
            continue;
        ScenarioTracks::ActorState state = capturePlayer(player);
        state.hasInput = true;
        state.input.wishMoveXY = player.input.wish;
        state.input.jumpHeld = player.input.jumpHeld;
        state.input.dashPressed = player.input.dashPressed;
        state.input.downDashPressed = player.input.downDashPressed;
        state.input.freezeHeld = player.input.freezeHeld;
        state.input.crouchHeld = player.input.crouchHeld;
        state.input.interactHeld = player.input.interactHeld;
        state.input.camForward = player.input.camForward;
        actors.push_back(ScenarioTracks::actorStateJson(state));
    }
    for (const auto& entry : npcs)
    {
        ScenarioTracks::ActorState state = captureNpc(entry.second);
        auto simIt = simulated.find(entry.second.entityId);
        if (simIt != simulated.end() && simIt->second)
        {
            const Npc& sim = *simIt->second;
            if (sim.hasLastAcceptedInput)
            {
                state.hasInput = true;
                state.input = sim.lastAcceptedInput;
            }
            state.previousPosition = sim.body.actorCollisionPreviousPos;
            state.stableGrounded = sim.body.ground.stableOnGround;
            state.hasWorldContact = sim.body.ground.hasWorldContact;
            state.rootContainment = sim.lastContainmentKnown
                ? (sim.lastRootInsideClosedSolid ? 1 : 2) : 0;
            state.maxPenetration = sim.lastFinalPenetration;
            state.containmentStage = sim.lastContainmentStage;
            state.npcState = static_cast<int>(sim.stateMachine.currentState);
            state.npcRngState = sim.rngState;
            state.npcDashCooldown = sim.dashCooldown;
            state.npcDownDashCooldown = sim.downDashCooldown;
            state.npcJumpCooldown = sim.jumpCooldown;
            state.npcAttackCooldown = sim.attackCooldown;
        }
        actors.push_back(ScenarioTracks::actorStateJson(state));
    }
    nlohmann::json objects = nlohmann::json::array();
    for (const auto& entry : projectiles)
        objects.push_back(projectileJson(entry.second));

    nlohmann::json fields{
        {"scenario_id", mScenarioId},
        {"bookmark_number", bookmarkNumber},
        {"frame", mFrames},
        {"client_tick", clientTick},
        {"server_tick", serverTick},
        {"journal_path", StructuredLogger::instance().eventsPath()},
        {"process_role", "server"},
        {"actors", std::move(actors)},
        {"objects", std::move(objects)},
        {"rng", nlohmann::json::array()},
        {"world_mutable", nlohmann::json{{"policy", "static_collision_cases"},
                                          {"restored", false}}},
        {"complete", true}
    };
    for (const auto& entry : npcs)
    {
        auto simIt = simulated.find(entry.second.entityId);
        if (simIt == simulated.end() || !simIt->second)
            continue;
        fields["rng"].push_back({{"track", "npc:" + std::to_string(entry.second.entityId)},
                                 {"rng_state", simIt->second->rngState}});
    }
    if (!writeScenarioEvent(
            "scenario.record.checkpoint",
            "complete authoritative simulation checkpoint at bookmark",
            serverTick, fields))
    {
        markCaptureFailure("checkpoint_write_refused", serverTick);
        stop("capture_failed");
    }
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
    if (mCanonical)
    {
        writeScenarioEvent(
            mCaptureInvalid ? "scenario.record.invalid"
                            : "scenario.server.record.completed",
            mCaptureInvalid ? "authoritative recording stopped without complete evidence"
                            : "authoritative scenario recording stopped with complete evidence",
            static_cast<uint32_t>(mFrames),
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"scenario_path", mScenarioPath},
                           {"frames", mFrames},
                           {"valid", !mCaptureInvalid},
                           {"capture_failure", mCaptureFailure},
                           {"journal_path", StructuredLogger::instance().eventsPath()},
                           {"process_role", "server"},
                           {"stop_reason", reason ? reason : "unknown"}});
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.server.record.stopped", mScenarioId,
        "authoritative actor track finalized", mFrames,
        nlohmann::json{{"scenario_id", mScenarioId},
                       {"scenario_path", mScenarioPath},
                       {"sidecar_path", mSidecarPath},
                       {"frames", mFrames},
                       {"stop_reason", reason ? reason : "unknown"}},
        __FILE__, __LINE__, __FUNCTION__);
}
