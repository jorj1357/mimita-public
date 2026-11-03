#include "devtools/scenario-recorder.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include <windows.h>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "camera.h"
#include "combat/melee-client.h"
#include "combat/weapon-attachment.h"
#include "combat/weapon-melee-config.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-system.h"
#include "debug/structured-log.h"
#include "devtools/scenario-tracks.h"
#include "entities/player.h"
#include "input/input-frame.h"
#include "network/multiplayer-context.h"
#include "terminal/terminal-state.h"
#include "world/world.h"
#include "npc/npc.h"

namespace {

constexpr uint32_t kMaxFrames = 60u * 60u * 10u;
constexpr uint32_t kFlushEvery = 6u;
constexpr uint64_t kMergeWaitMs = 5000u;

std::string utcTimestamp(bool compact)
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer),
                  compact ? "%Y%m%d_%H%M%S" : "%Y-%m-%dT%H:%M:%SZ",
                  &tm);
    return buffer;
}

std::string sanitizeName(const std::string& name)
{
    std::string out;
    for (char c : name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            out.push_back(c);
        else if (c == ' ')
            out.push_back('_');
        if (out.size() >= 48)
            break;
    }
    while (!out.empty() && (out.back() == '_' || out.back() == '-'))
        out.pop_back();
    return out;
}

uint64_t nowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string buildIdentity()
{
    std::ifstream in("version.txt");
    std::string value;
    if (in.is_open())
        std::getline(in, value);
    return value;
}

bool journalHasServerInvalid(const std::string& journalPath,
                             const std::string& scenarioId)
{
    if (journalPath.empty() || scenarioId.empty())
        return false;
    const std::filesystem::path active(journalPath);
    std::vector<std::filesystem::path> segments;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(active.parent_path(), ec), end;
         it != end; it.increment(ec))
    {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
        const std::string name = it->path().filename().string();
        if (name.rfind("events-", 0) == 0 &&
            name.size() > 12 && name.substr(name.size() - 6) == ".jsonl")
            segments.push_back(it->path());
    }
    segments.push_back(active);
    for (const auto& segment : segments)
    {
        std::ifstream in(segment);
        if (!in.is_open())
            continue;
        std::string line;
        while (std::getline(in, line))
        {
            if (line.find(scenarioId) == std::string::npos)
                continue;
            try
            {
                const auto value = nlohmann::json::parse(line);
                const std::string event = value.value("event", "");
                if (event != "scenario.record.invalid" &&
                    event != "scenario.server.record.invalid")
                    continue;
                const auto& fields = value["fields"];
                if (fields.value("scenario_id", "") == scenarioId &&
                    value.value("process", "") == "server")
                    return true;
            }
            catch (const std::exception&)
            {
                continue;
            }
        }
    }
    return false;
}


void appendBodyParts(const Player& player,
                     std::vector<ScenarioTracks::BodyPartBox>& out)
{
    out.clear();
    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
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
        ScenarioTracks::BodyPartBox box;
        box.name = part.name;
        box.center = (low + high) * 0.5f;
        box.half = (high - low) * 0.5f;
        out.push_back(std::move(box));
    }
}

const WeaponData::MeleeAction* resolveRecordedAction(
    const Player& player,
    bool localPlayer,
    std::string& actionId,
    bool& active,
    uint32_t& startTick,
    int& phaseTick)
{
    std::string definitionId;
    int actionIndex = -1;
    if (localPlayer)
    {
        active = MimitaMelee::currentActionActive();
        actionId = MimitaMelee::currentActionId();
        definitionId = MimitaMelee::currentActionDefinitionId();
        startTick = MimitaMelee::currentActionStartTick();
        phaseTick = MimitaMelee::currentActionPhaseTick();
    }
    else
    {
        definitionId = player.networkMeleeAudioDefinitionId;
        actionIndex = static_cast<int>(player.networkMeleeAudioActionIndex);
        active = !definitionId.empty() && actionIndex >= 0 && actionIndex != 255;
        startTick = player.networkMeleeAudioStartTick;
        phaseTick = static_cast<int>(player.networkMeleeAudioLastElapsed);
    }
    const WeaponData::MeleeMoveSet* set =
        definitionId.empty() ? nullptr
                             : WeaponData::MeleeMoveSetRegistry::instance().get(definitionId);
    if (!set)
        return nullptr;
    const WeaponData::MeleeAction* action = nullptr;
    if (!actionId.empty())
        action = set->find(actionId);
    if (!action && actionIndex >= 0 && actionIndex < static_cast<int>(set->actions.size()))
        action = &set->actions[static_cast<size_t>(actionIndex)];
    if (action)
        actionId = action->id;
    return action;
}

void resolveActorShape(const Player& player,
                       const WeaponDefinition* def,
                       const WeaponData::MeleeAction* action,
                       ScenarioTracks::WeaponShape& shape)
{
    shape = ScenarioTracks::WeaponShape{};
    if (!def || def->id == "nothing")
        return;
    glm::vec3 actionRotation(0.0f);
    glm::vec3 actionOffset(0.0f);
    if (action && action->hasWeaponPose)
    {
        actionRotation = action->weaponPoseRotation;
        actionOffset = action->weaponPoseOffset;
    }
    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        if (part.name != "rightArm")
            continue;
        WeaponAttachmentPose attachment;
        if (!resolveWeaponAttachment(*def, part, player.sizeScale, attachment,
                                     actionRotation, actionOffset))
            continue;
        shape.valid = true;
        shape.previousA = attachment.bladeBase;
        shape.previousB = attachment.bladeTip;
        shape.currentA = attachment.bladeBase;
        shape.currentB = attachment.bladeTip;
        shape.radius = attachment.bladeRadius;
        return;
    }
}

ScenarioTracks::ActorState captureActor(const Player& player,
                                        const std::string& trackId,
                                        const std::string& kind,
                                        bool localPlayer,
                                        WeaponSystem& weapons)
{
    ScenarioTracks::ActorState state;
    state.trackId = trackId;
    state.kind = kind;
    state.position = player.pos;
    state.velocity = player.vel;
    state.aim = glm::vec3(std::cos(glm::radians(player.yaw)),
                          std::sin(glm::radians(player.yaw)), 0.0f);
    state.yaw = player.yaw;
    state.sizeScale = player.sizeScale;
    state.health = player.currentHp;
    state.maxHealth = player.maxHp;
    state.team = player.matchTeam;
    state.dead = player.dead;
    state.grounded = player.ground.onGround;
    state.spawnGeneration = player.spawnGeneration;
    state.transformEpoch = player.networkTransformEpoch;
    state.weaponId = player.equippedWeaponId;
    state.equippedSlot = player.equippedSlot;
    state.weaponState = player.networkWeaponState;

    std::string actionId;
    bool actionActive = false;
    uint32_t actionStartTick = 0;
    int actionPhaseTick = 0;
    const WeaponData::MeleeAction* action = resolveRecordedAction(
        player, localPlayer, actionId, actionActive, actionStartTick, actionPhaseTick);
    state.actionActive = actionActive;
    state.actionId = actionId;
    state.actionStartTick = actionStartTick;
    state.actionPhaseTick = actionPhaseTick;

    appendBodyParts(player, state.bodyParts);
    const WeaponDefinition* def = weapons.getCurrentDef(player);
    if (!def && !state.weaponId.empty())
        def = WeaponRegistry::instance().get(state.weaponId);
    resolveActorShape(player, def, action, state.shape);
    return state;
}

} // namespace

ScenarioRecorder& ScenarioRecorder::instance()
{
    static ScenarioRecorder recorder;
    return recorder;
}

bool ScenarioRecorder::start(const std::string& name,
                             const std::string& map,
                             const std::string& mode,
                             int weaponSet,
                             const Player& player,
                             const Camera& camera,
                             std::string* error)
{
    if (mActive)
    {
        if (error) *error = "recording_already_active";
        return false;
    }
    if (MP_CONTEXT.active &&
        (!MP_CONTEXT.gameplayActive ||
         MP_CONTEXT.pendingSpawnAckGeneration != 0 ||
         MP_CONTEXT.lastKnownSpawnGeneration == 0 ||
         player.spawnGeneration == 0 ||
         player.dead))
    {
        if (error) *error = "spawn_not_ready";
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Errors,
            "scenario.record.rejected", "scenario-recorder",
            "recording rejected because client/server spawn state is not ready", 0,
            nlohmann::json{{"reason", "spawn_not_ready"},
                           {"gameplay_active", MP_CONTEXT.gameplayActive},
                           {"pending_spawn_ack_generation", MP_CONTEXT.pendingSpawnAckGeneration},
                           {"last_known_spawn_generation", MP_CONTEXT.lastKnownSpawnGeneration},
                           {"local_server_epoch", MP_CONTEXT.localServerEpoch},
                           {"player_spawn_generation", player.spawnGeneration},
                           {"player_dead", player.dead}},
            __FILE__, __LINE__, __FUNCTION__);
        return false;
    }

    std::error_code ec;
    const std::filesystem::path root = "devscripts/scenarios/user";
    std::filesystem::create_directories(root, ec);
    if (ec)
    {
        if (error) *error = "cannot_create_scenario_directory";
        return false;
    }

    const std::string stamp = utcTimestamp(true);
    const std::string cleanName = sanitizeName(name);
    const std::string file =
        cleanName.empty()
            ? "scenario-" + stamp + ".jsonl"
            : "scenario-" + stamp + "-" + cleanName + ".jsonl";
    const std::filesystem::path path = root / file;
    mScenarioId = "scn-" + stamp + "-" + std::to_string(GetCurrentProcessId());
    if (!cleanName.empty())
        mScenarioId += "-" + cleanName;

    mOut.open(path, std::ios::out | std::ios::trunc);
    if (!mOut.is_open())
    {
        if (error) *error = "cannot_open_recording_file";
        return false;
    }

    mPath = path.generic_string();
    mServerPath = mPath + ".server.jsonl";
    mMap = map;
    mMode = mode;
    mWeaponSet = weaponSet;
    mFrame = 0;
    mStartPending = true;
    mActive = true;
    mCanonical = true;
    mLocalPlayerId = MP_CONTEXT.active ? MP_CONTEXT.localPlayerId : 0;
    mNetworked = MP_CONTEXT.active && mLocalPlayerId != 0;
    mServerTrackStartTick = 0;
    mMerged = false;
    mMergeError.clear();
    mCaptureInvalid = false;
    mCaptureFailure.clear();
    mProbePending = true;
    mProbeDone = false;
    mLastPose.clear();
    mLastGrounded.clear();
    mLastAnomalyTick.clear();
    mLastActors.clear();
    mKnownIdentities.clear();
    mStartMs = nowMs();

    if (mNetworked)
    {
        // The server owns the authoritative actor sidecar. It is merged into
        // this file when recording stops. The scenario id links the canonical
        // client/server events in the shared journal.
        MimitaNet::mpSendServerCommand(MP_CONTEXT,
            "scenario_server_record_start " + mPath + " " + mScenarioId);
    }

    StructuredLogger::instance().beginScenarioCapture();
    if (!writeScenarioEvent(
            "scenario.record.started",
            "scenario recording armed; canonical client/server capture begins on the next fixed tick",
            0,
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"path", mPath},
                           {"schema", 4},
                           {"map", mMap},
                           {"mode", mMode},
                           {"weapon_set", mWeaponSet},
                           {"networked", mNetworked},
                           {"local_player_id", mLocalPlayerId},
                           {"tick_rate", 60},
                           {"capture_profile", "scenario_debug_v1"},
                           {"npc_geometry_coverage", true},
                           {"required_evidence", {"input", "contacts", "containment", "npc_state", "rng", "grounding", "desired_vs_actual"}},
                           {"journal_path", StructuredLogger::instance().eventsPath()},
                           {"process_role", "client"},
                           {"start", {
                               {"position", ScenarioTracks::vec3Json(player.pos)},
                               {"velocity", ScenarioTracks::vec3Json(player.vel)},
                               {"yaw", camera.yaw},
                               {"pitch", camera.pitch},
                               {"cam_forward", ScenarioTracks::vec3Json(camera.front)},
                               {"grounded", player.ground.onGround},
                               {"health", player.currentHp},
                               {"equipped_weapon", player.equippedWeaponId},
                               {"equipped_slot", player.equippedSlot},
                               {"spawn_generation", player.spawnGeneration},
                               {"transform_epoch", player.networkTransformEpoch}}}}))
    {
        markCaptureFailure("scenario.record.started", 0);
        mActive = false;
        if (mOut.is_open())
            mOut.close();
        if (error) *error = "journal_refused_record_start";
        return false;
    }
    return true;
}

bool ScenarioRecorder::writeScenarioEvent(const std::string& event,
                                          const std::string& reason,
                                          uint32_t tick,
                                          const nlohmann::json& fields)
{
    return StructuredLogger::instance().writeEventChecked(
        StructuredCategory::General, StructuredLevel::Important,
        event, mScenarioId, reason, tick, fields,
        __FILE__, __LINE__, __FUNCTION__);
}

void ScenarioRecorder::markCaptureFailure(const std::string& reason,
                                          uint32_t tick)
{
    if (mCaptureInvalid)
        return;
    mCaptureInvalid = true;
    mCaptureFailure = reason;
    // A compact failure record is attempted immediately. If the journal is
    // completely exhausted, scenario_status/dev-loop status still exposes the
    // failure; the run never emits a completed marker.
    writeScenarioEvent(
        "scenario.record.invalid",
        "required scenario evidence could not be written; capture stopped",
        tick,
        nlohmann::json{{"scenario_id", mScenarioId},
                       {"path", mPath},
                       {"frame", mFrame},
                       {"reason", reason},
                       {"last_complete_frame", mFrame},
                       {"journal_path", StructuredLogger::instance().eventsPath()},
                       {"process_role", "client"}});
}

nlohmann::json ScenarioRecorder::playerEvidenceJson(const Player& player,
                                                    const Camera& camera,
                                                    const World& world) const
{
    nlohmann::json contacts = nlohmann::json::array();
    const size_t maxContacts = std::min<size_t>(player.movementContacts.size(), 32);
    for (size_t i = 0; i < maxContacts; ++i)
    {
        const MovementContact& contact = player.movementContacts[i];
        contacts.push_back({
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
        });
    }
    const int rootContainment = player.finalPose.hasClearPose ? 2 : 0;
    return nlohmann::json{
        {"track", "player:" + std::to_string(mLocalPlayerId)},
        {"runtime_id", mLocalPlayerId},
        {"kind", "player"},
        {"name", player.username},
        {"avatar", player.avatarName()},
        {"spawn_generation", player.spawnGeneration},
        {"transform_epoch", player.networkTransformEpoch},
        {"position", ScenarioTracks::vec3Json(player.pos)},
        {"velocity", ScenarioTracks::vec3Json(player.vel)},
        {"yaw", player.yaw},
        {"aim", ScenarioTracks::vec3Json(camera.front)},
        {"health", player.currentHp},
        {"max_health", player.maxHp},
        {"dead", player.dead},
        {"grounded", player.ground.onGround},
        {"stable_grounded", player.ground.stableOnGround},
        {"world_contact", player.ground.hasWorldContact},
        {"collision_backend", "player_movement"},
        {"root_containment", rootContainment},
        {"containment_stage", player.finalPose.hasClearPose ? "last_clear" : "unknown"},
        {"body_parts", [&]() {
            std::vector<ScenarioTracks::BodyPartBox> parts;
            appendBodyParts(player, parts);
            nlohmann::json out = nlohmann::json::array();
            for (const auto& part : parts)
                out.push_back({{"name", part.name},
                               {"center", ScenarioTracks::vec3Json(part.center)},
                               {"half", ScenarioTracks::vec3Json(part.half)}});
            return out;
        }()},
        {"contacts", std::move(contacts)},
        {"contact_count", player.movementContacts.size()},
        {"duplicate_contact_count", player.movementContacts.duplicateCount},
        {"overflow_contact_count", player.movementContacts.overflowCount},
        {"equipped_weapon", player.equippedWeaponId},
        {"equipped_slot", player.equippedSlot}
    };
}

void ScenarioRecorder::writeHeader(const Player& player, const Camera& camera)
{
    nlohmann::json revisions = nlohmann::json::array();
    nlohmann::json tracks = nlohmann::json::array();
    tracks.push_back("client_input");
    if (mNetworked)
        tracks.push_back("client_actors");
    tracks.push_back("client_collision");
    if (mNetworked)
    {
        for (const auto& entry : WeaponData::MeleeMoveSetRegistry::instance().all())
        {
            revisions.push_back({
                {"weapon", entry.first},
                {"revision", entry.second.revision},
                {"hash", entry.second.hash}
            });
        }
    }
    nlohmann::json header{
        {"kind", "header"},
        {"schema", mNetworked ? ScenarioTracks::kSchemaV2 : 1},
        {"created_utc", utcTimestamp(false)},
        {"tick_rate", 60},
        {"map", mMap},
        {"mode", mMode},
        {"weapon_set", mWeaponSet},
        {"player", player.username},
        {"avatar", player.avatarName()},
        {"source", "human"},
        {"build_id", buildIdentity()},
        {"local_player_id", mLocalPlayerId},
        {"networked", mNetworked},
        {"tracks", tracks},
        {"melee_revisions", revisions},
        {"start", {
            {"position", ScenarioTracks::vec3Json(player.pos)},
            {"velocity", ScenarioTracks::vec3Json(player.vel)},
            {"yaw", camera.yaw},
            {"pitch", camera.pitch},
            {"cam_forward", ScenarioTracks::vec3Json(camera.front)},
            {"grounded", player.ground.onGround},
            {"health", player.currentHp},
            {"equipped_weapon", player.equippedWeaponId},
            {"equipped_slot", player.equippedSlot},
            {"spawn_generation", player.spawnGeneration},
            {"transform_epoch", player.networkTransformEpoch},
            {"movement_simulation_tick", player.movementSimulationTick}
        }}
    };
    mOut << header.dump() << '\n';
    mStartPending = false;
}

void ScenarioRecorder::writeIdentity(const std::string& trackId,
                                     const std::string& kind,
                                     uint32_t entityId,
                                     const std::string& name,
                                     const std::string& avatar,
                                     uint32_t spawnGeneration,
                                     uint16_t transformEpoch)
{
    if (!mNetworked || trackId.empty() || mKnownIdentities.count(trackId))
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

void ScenarioRecorder::updateLifecycle(
    const std::unordered_map<std::string, ActorSnapshot>& current,
    uint32_t serverTick,
    uint64_t clientTick)
{
    if (!mNetworked)
        return;
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
        event.clientTick = static_cast<uint32_t>(clientTick);
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
        event.clientTick = static_cast<uint32_t>(clientTick);
        event.spawnGeneration = entry.second.spawnGeneration;
        event.transformEpoch = entry.second.transformEpoch;
        event.health = entry.second.health;
        mOut << ScenarioTracks::lifecycleJson(event).dump() << '\n';
    }
    mLastActors.clear();
    for (const auto& entry : current)
        mLastActors[entry.first] = entry.second;
}

void ScenarioRecorder::captureTick(const InputFrame& frame,
                                   const Player& player,
                                   const Camera& camera,
                                   const NpcSystem& npcSystem,
                                   const MimitaNet::MultiplayerContext& mpContext,
                                   WeaponSystem& weapons,
                                   const World& world)
{
    if (!mActive || !mOut.is_open())
        return;

    const auto captureStart = std::chrono::steady_clock::now();
    if (mStartPending)
        writeHeader(player, camera);

    const uint32_t serverTick = mpContext.active
        ? static_cast<uint32_t>(mpContext.latestServerTick) : 0;
    const uint64_t clientTick = static_cast<uint64_t>(player.movementSimulationTick);

    nlohmann::json tick{
        {"kind", "tick"},
        {"frame", mFrame},
        {"client_tick", clientTick},
        {"server_tick", serverTick},
        {"wish", {frame.wishSide, frame.wishForward}},
        {"move_world", {frame.moveX, frame.moveY}},
        {"jump", frame.jump},
        {"jump_pressed", frame.jumpPressed},
        {"dash_pressed", frame.dashPressed},
        {"down_dash_pressed", frame.downDashPressed},
        {"freeze_held", frame.freezeHeld},
        {"freeze_pressed", frame.freezePressed},
        {"crouch_held", frame.crouchHeld},
        {"interact_held", frame.interactHeld},
        {"reload_pressed", frame.reloadPressed},
        {"primary_held", frame.primaryHeld},
        {"secondary_held", frame.secondaryHeld},
        {"shield_held", frame.shieldHeld},
        {"grab_held", frame.grabHeld},
        {"look_yaw", frame.lookYaw},
        {"look_pitch", frame.lookPitch},
        {"cam_forward", ScenarioTracks::vec3Json(frame.camForward)},
        {"equipped_weapon", player.equippedWeaponId},
        {"equipped_slot", player.equippedSlot},
        {"position", ScenarioTracks::vec3Json(player.pos)},
        {"velocity", ScenarioTracks::vec3Json(player.vel)},
        {"health", player.currentHp},
        {"grounded", player.ground.onGround},
        {"spawn_generation", player.spawnGeneration},
        {"transform_epoch", player.networkTransformEpoch},
        {"cam_yaw", camera.yaw},
        {"cam_pitch", camera.pitch}
    };

    nlohmann::json collisionContacts = nlohmann::json::array();
    const size_t maxCollisionContacts =
        std::min<size_t>(player.movementContacts.size(), 8);
    for (size_t i = 0; i < maxCollisionContacts; ++i)
    {
        const MovementContact& contact = player.movementContacts[i];
        collisionContacts.push_back({
            {"kind", static_cast<int>(contact.kind)},
            {"source", static_cast<int>(contact.source)},
            {"shape", static_cast<int>(contact.shapeKind)},
            {"subshape", static_cast<int>(contact.subshape)},
            {"point", ScenarioTracks::vec3Json(contact.point)},
            {"normal", ScenarioTracks::vec3Json(contact.normal)},
            {"sweep_velocity", ScenarioTracks::vec3Json(contact.sweepVelocity)},
            {"penetration_m", contact.penetrationDepth},
            {"surface_id", contact.surfaceId},
            {"source_entity_id", contact.sourceEntityId}
        });
    }
    tick["collision"] = {
        {"on_ground", player.ground.onGround},
        {"stable_on_ground", player.ground.stableOnGround},
        {"has_world_contact", player.ground.hasWorldContact},
        {"contact_count", player.movementContacts.size()},
        {"duplicate_contact_count", player.movementContacts.duplicateCount},
        {"overflow_contact_count", player.movementContacts.overflowCount},
        {"contacts", std::move(collisionContacts)}
    };

    if (mNetworked)
    {
        std::unordered_map<std::string, ActorSnapshot> current;
        nlohmann::json actors = nlohmann::json::array();

        const std::string localTrack = "player:" + std::to_string(mLocalPlayerId);
        writeIdentity(localTrack, "player", mLocalPlayerId, player.username,
                      player.avatarName(), player.spawnGeneration,
                      player.networkTransformEpoch);
        ScenarioTracks::ActorState local = captureActor(
            player, localTrack, "player", true, weapons);
        actors.push_back(ScenarioTracks::actorStateJson(local));
        current[localTrack] = {local.spawnGeneration, local.transformEpoch, local.health};

        for (const Npc& npc : npcSystem.all())
        {
            const std::string track = "npc:" + std::to_string(npc.id);
            writeIdentity(track, "npc", npc.id, npc.body.username,
                          npc.avatarName, 0, npc.transformEpoch);
            ScenarioTracks::ActorState state = captureActor(
                npc.body, track, "npc", false, weapons);
            state.transformEpoch = npc.transformEpoch;
            actors.push_back(ScenarioTracks::actorStateJson(state));
            current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
        }
        for (const auto& entry : mpContext.remotePlayers)
        {
            const Player& remote = entry.second;
            const std::string track = "player:" + std::to_string(entry.first);
            writeIdentity(track, "player", entry.first, remote.username,
                          remote.avatarName(), remote.spawnGeneration,
                          remote.networkTransformEpoch);
            ScenarioTracks::ActorState state = captureActor(
                remote, track, "player", false, weapons);
            actors.push_back(ScenarioTracks::actorStateJson(state));
            current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
        }
        for (const auto& entry : mpContext.remoteNpcs)
        {
            const Player& remote = entry.second;
            const std::string track = "npc:" + std::to_string(entry.first);
            writeIdentity(track, "npc", entry.first, remote.username,
                          remote.avatarName(), 0, remote.networkTransformEpoch);
            ScenarioTracks::ActorState state = captureActor(
                remote, track, "npc", false, weapons);
            state.transformEpoch = remote.networkTransformEpoch;
            actors.push_back(ScenarioTracks::actorStateJson(state));
            current[track] = {state.spawnGeneration, state.transformEpoch, state.health};
        }
        tick["actors"] = std::move(actors);
        updateLifecycle(current, serverTick, clientTick);
        if (mServerTrackStartTick == 0 && serverTick != 0)
            mServerTrackStartTick = serverTick;
    }

    const nlohmann::json inputEvidence{
        {"wish", {frame.wishSide, frame.wishForward}},
        {"move_world", {frame.moveX, frame.moveY}},
        {"jump_held", frame.jump},
        {"jump_pressed", frame.jumpPressed},
        {"dash_pressed", frame.dashPressed},
        {"down_dash_pressed", frame.downDashPressed},
        {"freeze_held", frame.freezeHeld},
        {"freeze_pressed", frame.freezePressed},
        {"crouch_held", frame.crouchHeld},
        {"interact_held", frame.interactHeld},
        {"reload_pressed", frame.reloadPressed},
        {"primary_held", frame.primaryHeld},
        {"secondary_held", frame.secondaryHeld},
        {"shield_held", frame.shieldHeld},
        {"grab_held", frame.grabHeld},
        {"look_yaw", frame.lookYaw},
        {"look_pitch", frame.lookPitch},
        {"cam_forward", ScenarioTracks::vec3Json(frame.camForward)}
    };
    nlohmann::json actorEvidence = playerEvidenceJson(player, camera, world);
    const uint64_t captureBuildUs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - captureStart).count());

    if (mProbePending && !mProbeDone)
    {
        const auto& parts = actorEvidence["body_parts"];
        const bool finiteVelocity =
            std::isfinite(player.vel.x) && std::isfinite(player.vel.y) &&
            std::isfinite(player.vel.z);
        const bool contactsPresent =
            !player.ground.hasWorldContact || !player.movementContacts.empty();
        nlohmann::json missing = nlohmann::json::array();
        if (!actorEvidence["body_parts"].is_array() || parts.empty())
            missing.push_back("body_parts");
        if (!finiteVelocity)
            missing.push_back("finite_velocity");
        if (!contactsPresent)
            missing.push_back("world_contact_contacts");
        if (player.dead || player.currentHp <= 0)
            missing.push_back("alive_player");
        const bool ok = missing.empty();
        const bool probeWritten = writeScenarioEvent(
            "scenario.record.capture_probe",
            ok ? "client scenario capture probe passed"
               : "client scenario capture probe failed; recording cannot prove complete evidence",
            static_cast<uint32_t>(clientTick),
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"process_role", "client"},
                           {"ok", ok},
                           {"frame", mFrame},
                           {"client_tick", clientTick},
                           {"server_tick", serverTick},
                           {"body_part_count", parts.is_array() ? parts.size() : 0},
                           {"contact_count", player.movementContacts.size()},
                           {"has_world_contact", player.ground.hasWorldContact},
                           {"grounded", player.ground.onGround},
                           {"position", ScenarioTracks::vec3Json(player.pos)},
                           {"velocity", ScenarioTracks::vec3Json(player.vel)},
                           {"missing", missing}});
        mProbeDone = true;
        if (!probeWritten || !ok)
        {
            markCaptureFailure(probeWritten ? "capture_probe_failed"
                                            : "capture_probe_write_refused",
                               static_cast<uint32_t>(clientTick));
            stop("capture_failed");
            return;
        }
    }

    if (!writeScenarioEvent(
            "scenario.record.client_tick",
            "canonical client input and collision evidence for one fixed tick",
            static_cast<uint32_t>(clientTick),
            nlohmann::json{{"scenario_id", mScenarioId},
                           {"frame", mFrame},
                           {"client_tick", clientTick},
                           {"server_tick", serverTick},
                           {"journal_path", StructuredLogger::instance().eventsPath()},
                           {"process_role", "client"},
                           {"input", inputEvidence},
                           {"actor", actorEvidence},
                           {"capture", {{"build_us", captureBuildUs}}}}))
    {
        markCaptureFailure("client_tick_write_refused", static_cast<uint32_t>(clientTick));
        stop("capture_failed");
        return;
    }

    mOut << tick.dump() << '\n';
    ++mFrame;
    flushIfNeeded();

    if (mFrame >= kMaxFrames)
        stop("limit");
}

void ScenarioRecorder::flushIfNeeded()
{
    if (!mOut.is_open())
        return;
    if (mFrame % kFlushEvery == 0)
        mOut.flush();
}

void ScenarioRecorder::writeFooter(const char* reason)
{
    if (!mOut.is_open())
        return;
    const uint64_t now = nowMs();
    const double duration = mStartMs && now > mStartMs
        ? static_cast<double>(now - mStartMs) / 1000.0 : 0.0;
    const nlohmann::json footer{
        {"kind", "footer"},
        {"ended_utc", utcTimestamp(false)},
        {"frames", mFrame},
        {"duration_seconds", duration},
        {"stop_reason", reason ? reason : "unknown"},
        {"tracks_complete", !mNetworked},
        {"schema", mNetworked ? ScenarioTracks::kSchemaV2 : 1}
    };
    mOut << footer.dump() << '\n';
    mOut.flush();
    mOut.close();
}

void ScenarioRecorder::stop(const char* reason)
{
    if (!mActive)
        return;
    mActive = false;
    if (mStartPending)
    {
        mOut.close();
        mStartPending = false;
    }
    else
        writeFooter(reason);
    // Canonical recordings are complete in the shared journal; there is no
    // server sidecar to merge.
    if (mCanonical)
        mMerged = true;

    if (mNetworked)
        MimitaNet::mpSendServerCommand(MP_CONTEXT,
            "scenario_server_record_stop " + mPath + " " + mScenarioId);

    if (!mCaptureInvalid &&
        journalHasServerInvalid(StructuredLogger::instance().eventsPath(),
                                mScenarioId))
    {
        mCaptureInvalid = true;
        mCaptureFailure = "server_capture_invalid";
    }
    const nlohmann::json fields{
        {"scenario_id", mScenarioId},
        {"path", mPath},
        {"frames", mFrame},
        {"networked", mNetworked},
        {"stop_reason", reason ? reason : "unknown"},
        {"valid", !mCaptureInvalid},
        {"capture_failure", mCaptureFailure},
        {"journal_path", StructuredLogger::instance().eventsPath()},
        {"process_role", "client"}
    };
    writeScenarioEvent(
        mCaptureInvalid ? "scenario.record.invalid" : "scenario.record.completed",
        mCaptureInvalid ? "scenario recording stopped without complete evidence"
                        : "scenario recording stopped with complete client evidence",
        mFrame, fields);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.record.stopped", mScenarioId,
        "scenario recording stopped", mFrame,
        fields,
        __FILE__, __LINE__, __FUNCTION__);
    StructuredLogger::instance().endScenarioCapture();
}

// Called by the fixed tick loop while a networked recording is finalizing.
void ScenarioRecorder::poll()
{
    static uint64_t sStopAtMs = 0;
    if (mCanonical || !mNetworked || mMerged || mActive || mStartPending)
    {
        sStopAtMs = 0;
        return;
    }
    if (mPath.empty())
        return;
    if (sStopAtMs == 0)
        sStopAtMs = nowMs();
    std::error_code ec;
    if (!std::filesystem::exists(mServerPath, ec))
    {
        if (nowMs() - sStopAtMs > kMergeWaitMs)
        {
            mMergeError = "server_track_missing";
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Errors,
                "scenario.record.merge_failed", "scenario-recorder",
                "server authoritative track did not appear before timeout", 0,
                nlohmann::json{{"path", mPath},
                               {"server_path", mServerPath},
                               {"reason", mMergeError}},
                __FILE__, __LINE__, __FUNCTION__);
            sStopAtMs = 0;
        }
        return;
    }
    std::string error;
    if (!ScenarioTracks::mergeClientAndServer(mPath, mServerPath, mPath, &error))
    {
        if (error == "server_track_empty" && nowMs() - sStopAtMs <= kMergeWaitMs)
            return;
        mMergeError = error;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Errors,
            "scenario.record.merge_failed", "scenario-recorder",
            "scenario tracks could not be combined", 0,
            nlohmann::json{{"path", mPath},
                           {"server_path", mServerPath},
                           {"reason", error}},
            __FILE__, __LINE__, __FUNCTION__);
        sStopAtMs = 0;
        return;
    }
    mMerged = true;
    mMergeError.clear();
    sStopAtMs = 0;
    std::filesystem::remove(mServerPath, ec);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.record.merged", "scenario-recorder",
        "client and authoritative server tracks merged into one scenario", mFrame,
        nlohmann::json{{"path", mPath},
                       {"server_path", mServerPath},
                       {"frames", mFrame}},
        __FILE__, __LINE__, __FUNCTION__);
}
