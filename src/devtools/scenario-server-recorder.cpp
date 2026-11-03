#include "devtools/scenario-server-recorder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
#include "world/world.h"
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

void computeMotionEvidence(
    ScenarioTracks::ActorState& state,
    const std::string& track,
    const InputState& input,
    float dt,
    std::unordered_map<std::string, std::pair<glm::vec3, glm::vec3>>& lastPose,
    std::unordered_map<std::string, bool>& lastGrounded)
{
    auto poseIt = lastPose.find(track);
    if (poseIt != lastPose.end())
    {
        state.previousPosition = poseIt->second.first;
        state.previousVelocity = poseIt->second.second;
    }
    else
    {
        state.previousPosition = state.position;
        state.previousVelocity = state.velocity;
    }
    auto groundIt = lastGrounded.find(track);
    state.expectedGrounded = groundIt != lastGrounded.end()
        ? groundIt->second : state.grounded;

    state.actualDisplacement = state.position - state.previousPosition;
    state.previousSpeed = glm::length(state.previousVelocity);
    state.actualSpeed = glm::length(state.velocity);
    state.speedRatio = state.previousSpeed > 0.01f
        ? state.actualSpeed / state.previousSpeed : 1.0f;

    glm::vec3 forward = input.camForward;
    forward.z = 0.0f;
    if (glm::length(forward) > 0.001f)
        forward = glm::normalize(forward);
    const glm::vec3 right(-forward.y, forward.x, 0.0f);
    const glm::vec2 wish = input.wishMoveXY;
    if (glm::length(wish) > 0.001f)
        state.desiredDirection = glm::normalize(right * wish.x + forward * wish.y);
    state.desiredVelocity = state.desiredDirection * state.previousSpeed;

    if (glm::length(wish) > 0.001f && state.previousSpeed > 0.5f)
    {
        const float intendedDistance = state.previousSpeed * dt;
        const float progress = glm::dot(state.actualDisplacement,
                                        state.desiredDirection);
        state.progressRatio = intendedDistance > 0.0001f
            ? progress / intendedDistance : 1.0f;
        const glm::vec2 actualDir(state.velocity.x, state.velocity.y);
        if (glm::length(actualDir) > 0.01f &&
            glm::length(glm::vec2(state.desiredDirection)) > 0.01f)
        {
            const glm::vec2 desiredDir(state.desiredDirection.x,
                                       state.desiredDirection.y);
            const float dot = glm::clamp(
                glm::dot(glm::normalize(actualDir), glm::normalize(desiredDir)),
                -1.0f, 1.0f);
            state.deflectionDegrees = glm::degrees(std::acos(dot));
        }
    }

    state.expectedMaxPenetration = 0.02f;
    lastPose[track] = {state.position, state.velocity};
    lastGrounded[track] = state.grounded;
}

void appendSimulatedBodyParts(const Npc& npc,
                              std::vector<ScenarioTracks::BodyPartBox>& out)
{
    out.clear();
    auto append = [&](const std::vector<PhysicalBodyPart>& parts) {
        for (const PhysicalBodyPart& part : parts)
        {
            glm::vec3 low(std::numeric_limits<float>::max());
            glm::vec3 high = -low;
            for (int corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 local(
                    (corner & 1) ? part.collider.localMax.x : part.collider.localMin.x,
                    (corner & 2) ? part.collider.localMax.y : part.collider.localMin.y,
                    (corner & 4) ? part.collider.localMax.z : part.collider.localMin.z);
                const glm::vec3 world =
                    glm::vec3(part.worldTransform * glm::vec4(local, 1.0f));
                low = glm::min(low, world);
                high = glm::max(high, world);
            }
            if (glm::any(glm::greaterThan(low, high)))
                continue;
            ScenarioTracks::BodyPartBox box;
            box.name = part.name;
            box.center = (low + high) * 0.5f;
            box.half = (high - low) * 0.5f;
            out.push_back(std::move(box));
        }
    };
    append(npc.body.physicalBody.parts);
    append(npc.body.physicalBody.extraParts);
}

void appendTemplateBodyParts(const Npc& npc,
                             std::vector<ScenarioTracks::BodyPartBox>& out)
{
    const auto* tpl = MimitaNet::standardPlayerBodyTemplate();
    if (!tpl)
        return;
    const float yaw = glm::radians(npc.body.yaw);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    for (const auto& part : *tpl)
    {
        const glm::vec3 offset(part.offset.x * c - part.offset.y * s,
                               part.offset.x * s + part.offset.y * c,
                               part.offset.z);
        ScenarioTracks::BodyPartBox box;
        box.name = part.name;
        box.center = npc.body.pos + offset * npc.body.sizeScale;
        box.half = part.half * npc.body.sizeScale;
        out.push_back(std::move(box));
    }
}

std::string movementSubshapeName(int subshape)
{
    switch (static_cast<MovementSubshape>(subshape))
    {
        case MovementSubshape::RootCapsule: return "root";
        case MovementSubshape::Head: return "head";
        case MovementSubshape::Torso: return "torso";
        case MovementSubshape::LeftArm: return "leftArm";
        case MovementSubshape::RightArm: return "rightArm";
        case MovementSubshape::LeftLeg: return "leftLeg";
        case MovementSubshape::RightLeg: return "rightLeg";
        case MovementSubshape::Weapon: return "weapon";
        case MovementSubshape::World: return "world";
        default: return "";
    }
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
    mLastPose.clear();
    mLastGrounded.clear();
    mLastAnomalyTick.clear();
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
    mProbePending = true;
    mProbeDone = false;
    mFrames = 0;
    mStartTick = 0;
    mLastActors.clear();
    mLastPose.clear();
    mLastGrounded.clear();
    mLastAnomalyTick.clear();
    mKnownIdentities.clear();
    StructuredLogger::instance().beginScenarioCapture();
    if (!writeScenarioEvent(
            "scenario.server.record.started",
            "authoritative canonical scenario capture armed",
            0,
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"scenario_path", mScenarioPath},
                           {"journal_path", StructuredLogger::instance().eventsPath()},
                           {"process_role", "server"},
                           {"tick_rate", 60},
                           {"capture_profile", "scenario_debug_v1"},
                           {"npc_geometry_coverage", true},
                           {"required_evidence", {"input", "contacts", "containment", "npc_state", "rng", "grounding", "desired_vs_actual"}}}))
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
    const MimitaNet::HeadlessWorld& world,
    uint32_t serverTick)
{
    if (!mActive)
        return;

    const auto captureStart = std::chrono::steady_clock::now();
    std::unordered_map<uint32_t, const Npc*> simulated;
    for (const Npc& npc : npcSystem.all())
        simulated[npc.id] = &npc;

    std::unordered_map<std::string, ActorSnapshot> current;
    nlohmann::json actors = nlohmann::json::array();
    uint32_t playerActorCount = 0;
    uint32_t liveNpcCount = 0;
    uint32_t simulatedNpcCount = 0;
    bool eligibleNpcSeen = false;
    std::string probeNpcTrack;
    nlohmann::json probeMissing = nlohmann::json::array();
    auto emitMotionAnomaly = [&](const ScenarioTracks::ActorState& state,
                                 const std::string& track) {
        std::string type;
        if (state.speedRatio < 0.15f && state.previousSpeed > 8.0f)
            type = "speed_collapse";
        else if (state.expectedGrounded && !state.grounded &&
                 state.velocity.z < -1.0f)
            type = "support_loss_descending";
        else if (state.hasInput &&
                 glm::length(state.input.wishMoveXY) > 0.1f &&
                 state.expectedGrounded && state.progressRatio < 0.1f)
            type = "no_progress";
        else if (state.maxPenetration > state.expectedMaxPenetration)
            type = "penetration";
        else if (state.rootContainment == 1)
            type = "root_inside";
        if (type.empty())
            return;
        const std::string key = track + ":" + type;
        auto previous = mLastAnomalyTick.find(key);
        if (previous != mLastAnomalyTick.end() &&
            serverTick - previous->second < 60u)
            return;
        mLastAnomalyTick[key] = serverTick;
        writeScenarioEvent(
            "scenario.record.anomaly",
            "scenario capture detected a movement or geometry divergence",
            serverTick,
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"process_role", "server"},
                           {"track", track},
                           {"kind", type},
                           {"frame", mFrames},
                           {"server_tick", serverTick},
                           {"position", ScenarioTracks::vec3Json(state.position)},
                           {"previous_position", ScenarioTracks::vec3Json(state.previousPosition)},
                           {"velocity", ScenarioTracks::vec3Json(state.velocity)},
                           {"previous_velocity", ScenarioTracks::vec3Json(state.previousVelocity)},
                           {"desired_direction", ScenarioTracks::vec3Json(state.desiredDirection)},
                           {"actual_displacement", ScenarioTracks::vec3Json(state.actualDisplacement)},
                           {"previous_speed", state.previousSpeed},
                           {"actual_speed", state.actualSpeed},
                           {"speed_ratio", state.speedRatio},
                           {"progress_ratio", state.progressRatio},
                           {"deflection_degrees", state.deflectionDegrees},
                           {"expected_grounded", state.expectedGrounded},
                           {"actual_grounded", state.grounded},
                           {"expected_max_penetration_m", state.expectedMaxPenetration},
                           {"actual_penetration_m", state.maxPenetration},
                           {"root_containment", state.rootContainment},
                           {"containment_stage", state.containmentStage}});
    };

    for (const auto& entry : players)
    {
        if (entry.second.spawned)
            ++playerActorCount;
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
        computeMotionEvidence(state, track, state.input, 1.0f / 60.0f,
                              mLastPose, mLastGrounded);
        emitMotionAnomaly(state, track);
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
        const bool liveNpc = npc.health > 0;
        if (liveNpc)
            ++liveNpcCount;
        auto simIt = simulated.find(npc.entityId);
        if (simIt != simulated.end() && simIt->second)
        {
            ++simulatedNpcCount;
            const Npc& sim = *simIt->second;
            state.bodyParts.clear();
            appendSimulatedBodyParts(sim, state.bodyParts);
            if (state.bodyParts.empty())
                appendTemplateBodyParts(sim, state.bodyParts);
            const bool sleeping = sim.wakeupTimer > 0.0f || sim.body.dead;
            if (liveNpc && !sleeping)
            {
                bool evidenceOk = true;
                nlohmann::json missing = nlohmann::json::array();
                if (!sim.hasLastAcceptedInput)
                {
                    evidenceOk = false;
                    missing.push_back("accepted_input");
                }
                if (!sim.lastContainmentKnown)
                {
                    evidenceOk = false;
                    missing.push_back("containment");
                }
                if (state.bodyParts.empty())
                {
                    evidenceOk = false;
                    missing.push_back("body_parts");
                }
                if (!std::isfinite(state.position.x) ||
                    !std::isfinite(state.position.y) ||
                    !std::isfinite(state.position.z) ||
                    !std::isfinite(state.velocity.x) ||
                    !std::isfinite(state.velocity.y) ||
                    !std::isfinite(state.velocity.z))
                {
                    evidenceOk = false;
                    missing.push_back("finite_pose");
                }
                if (evidenceOk && !eligibleNpcSeen)
                {
                    eligibleNpcSeen = true;
                    probeNpcTrack = track;
                }
                for (const auto& item : missing)
                    probeMissing.push_back({{"track", track}, {"missing", item}});
            }
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
            computeMotionEvidence(state, track, state.input, 1.0f / 60.0f,
                                  mLastPose, mLastGrounded);
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
                const int triangleIndex = contact.surfaceId > 0
                    ? static_cast<int>(contact.surfaceId - 1u) : -1;
                evidence.worldTriangle = triangleIndex >= 0
                    ? static_cast<uint32_t>(triangleIndex) : 0u;
                evidence.solidId =
                    triangleIndex >= 0 &&
                    static_cast<size_t>(triangleIndex) < world.solidIds.size()
                        ? world.solidIds[static_cast<size_t>(triangleIndex)]
                        : 0u;
                evidence.materialId = contact.materialId;
                evidence.kind = static_cast<int>(contact.kind);
                evidence.source = static_cast<int>(contact.source);
                evidence.shape = static_cast<int>(contact.shapeKind);
                evidence.subshape = static_cast<int>(contact.subshape);
                evidence.actorPart = movementSubshapeName(evidence.subshape);
                evidence.point = contact.point;
                evidence.normal = contact.normal;
                evidence.sweepVelocity = contact.sweepVelocity;
                evidence.surfaceVelocity = contact.surfaceVelocity;
                evidence.penetration = contact.penetrationDepth;
                state.contacts.push_back(evidence);
            }
            for (const Npc::FinalPoseContactSnapshot& contact :
                 sim.lastFinalPoseContacts)
            {
                ScenarioTracks::ContactEvidence evidence;
                evidence.actorPart = contact.actorPart;
                evidence.worldTriangle = contact.worldTriangle;
                evidence.solidId = contact.solidId;
                evidence.point = contact.point;
                evidence.normal = contact.normal;
                evidence.surfaceVelocity = contact.surfaceNormal;
                evidence.penetration = contact.penetration;
                evidence.kind = 0;
                evidence.source = 0;
                evidence.shape = 0;
                evidence.subshape = 0;
                state.finalPoseContacts.push_back(std::move(evidence));
            }
        }
        else if (liveNpc)
        {
            probeMissing.push_back({{"track", track}, {"missing", "simulated_npc"}});
            if (mProbeDone && !mCaptureInvalid)
            {
                writeScenarioEvent(
                    "scenario.record.capture_gap",
                    "live NPC has no simulated body in the authoritative capture; recording cannot prove NPC geometry",
                    serverTick,
                    nlohmann::json{{"scenario_id", mScenarioId},
                                   {"process_role", "server"},
                                   {"frame", mFrames},
                                   {"server_tick", serverTick},
                                   {"track", track},
                                   {"runtime_id", npc.entityId},
                                   {"simulated_ids", [&]() {
                                       nlohmann::json ids = nlohmann::json::array();
                                       for (const auto& item : simulated)
                                           ids.push_back(item.first);
                                       return ids;
                                   }()}});
                markCaptureFailure("capture_gap_simulated_npc", serverTick);
                stop("capture_failed");
                return;
            }
        }
        emitMotionAnomaly(state, track);
        actors.push_back(ScenarioTracks::actorStateJson(state));
        current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
    }

    nlohmann::json objects = nlohmann::json::array();
    for (const auto& entry : projectiles)
        objects.push_back(projectileJson(entry.second));

    if (mProbePending && !mProbeDone)
    {
        const bool haveLiveNpcs = liveNpcCount > 0;
        bool probeOk = playerActorCount > 0 &&
            (!haveLiveNpcs || (eligibleNpcSeen && probeMissing.empty()));
        if (playerActorCount == 0)
            probeMissing.push_back({{"track", "player"}, {"missing", "player_actor"}});
        const bool probeTimeout = haveLiveNpcs && !probeOk && mFrames >= 120;
        if (probeTimeout)
        {
            if (!eligibleNpcSeen)
                probeMissing.push_back({{"track", "npc"}, {"missing", "no_eligible_live_npc"}});
            probeOk = false;
        }
        if (probeOk || probeTimeout || !haveLiveNpcs)
        {
            nlohmann::json simulatedIds = nlohmann::json::array();
            for (const auto& item : simulated)
                simulatedIds.push_back(item.first);
            const bool probeWritten = writeScenarioEvent(
                "scenario.record.capture_probe",
                probeOk ? "authoritative scenario capture probe passed"
                        : "authoritative scenario capture probe failed; NPC geometry evidence is incomplete",
                serverTick,
                nlohmann::json{{"scenario_id", mScenarioId},
                               {"process_role", "server"},
                               {"ok", probeOk},
                               {"frame", mFrames},
                               {"server_tick", serverTick},
                               {"player_actor_count", playerActorCount},
                               {"live_npc_count", liveNpcCount},
                               {"simulated_npc_count", simulatedNpcCount},
                               {"probe_npc_track", probeNpcTrack},
                               {"missing", probeMissing},
                               {"simulated_ids", simulatedIds}});
            mProbeDone = true;
            if (!probeWritten || !probeOk)
            {
                markCaptureFailure(probeWritten ? "capture_probe_failed"
                                                : "capture_probe_write_refused",
                                   serverTick);
                stop("capture_failed");
                return;
            }
        }
    }

    const uint64_t captureBuildUs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - captureStart).count());

    if (mCanonical)
    {
        const auto writeStart = std::chrono::steady_clock::now();
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
                               {"objects", objects},
                               {"capture", {{"build_us", captureBuildUs},
                                             {"last_write_us", mLastCaptureWriteUs},
                                             {"last_total_us", mLastCaptureTotalUs}}}}))
        {
            markCaptureFailure("server_tick_write_refused", serverTick);
            stop("capture_failed");
            return;
        }
        mLastCaptureWriteUs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - writeStart).count());
        mLastCaptureTotalUs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - captureStart).count());
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
    StructuredLogger::instance().endScenarioCapture();
}
