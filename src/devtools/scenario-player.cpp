#include "devtools/scenario-player.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "camera.h"
#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "input/scenario-input.h"
#include "network/multiplayer-context.h"
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"

extern FramePacer gFramePacer;

namespace {

constexpr float kDivergenceFailM = 2.0f;
constexpr uint64_t kReadinessTimeoutMs = 5000u;

uint64_t nowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

glm::vec3 jsonVec3(const nlohmann::json& value)
{
    if (!value.is_array() || value.size() < 3)
        return glm::vec3(0.0f);
    return glm::vec3(value[0].get<float>(),
                     value[1].get<float>(),
                     value[2].get<float>());
}

} // namespace

ScenarioPlayer& ScenarioPlayer::instance()
{
    static ScenarioPlayer player;
    return player;
}

bool ScenarioPlayer::load(const std::string& path, std::string* error)
{
    if (mActive)
    {
        if (error) *error = "playback_already_active";
        return false;
    }
    if (path.find("..") != std::string::npos)
    {
        if (error) *error = "path_traversal_rejected";
        return false;
    }
    if (path.size() < 6 || path.substr(path.size() - 6) != ".jsonl")
    {
        if (error) *error = "expected_jsonl_scenario";
        return false;
    }
    if (!loadLines(path, error))
        return false;
    if (mTicks.empty())
    {
        if (error) *error = "scenario_has_no_ticks";
        return false;
    }
    return true;
}

bool ScenarioPlayer::loadLines(const std::string& path, std::string* error)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        if (error) *error = "cannot_open_scenario";
        return false;
    }

    mTicks.clear();
    mRevisions.clear();
    mMap.clear();
    mMode.clear();
    mWeaponSet = 0;
    mSchemaVersion = 1;
    mStart = StartState{};
    mIndex = 0;
    mMaxPositionError = 0.0f;
    mFirstDivergenceFrame = 0xFFFFFFFFu;
    mFirstDivergenceError = 0.0f;
    mFirstDivergenceExpected = glm::vec3(0.0f);
    mFirstDivergenceActual = glm::vec3(0.0f);
    mFailureReason.clear();
    mServerPrepared = false;
    mServerBeginAck = false;
    mServerRejected = false;
    mServerCommandSent = false;
    mBeginSent = false;
    mServerError.clear();
    mPath = path;

    bool sawHeader = false;
    bool sawFooter = false;
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
            // A crash can leave one partial trailing line; ignore only the
            // final malformed line, never a malformed line in the middle.
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
            if (sawHeader)
            {
                if (error) *error = "duplicate_header";
                return false;
            }
            sawHeader = true;
            mSchemaVersion = value.value("schema", 0);
            if (mSchemaVersion != 1 && mSchemaVersion != ScenarioTracks::kSchemaV2)
            {
                if (error) *error = "unsupported_schema";
                return false;
            }
            if (value.value("tick_rate", 0) != 60)
            {
                if (error) *error = "unsupported_tick_rate";
                return false;
            }
            if (mSchemaVersion == ScenarioTracks::kSchemaV2)
            {
                if (!value.contains("tracks") || !value["tracks"].is_array() ||
                    std::find(value["tracks"].begin(), value["tracks"].end(),
                              "server_actors") == value["tracks"].end())
                {
                    if (error) *error = "missing_server_actor_track";
                    return false;
                }
                if (value.contains("melee_revisions"))
                    mRevisions = ScenarioTracks::parseMeleeRevisions(value["melee_revisions"]);
            }
            mMap = value.value("map", "");
            mMode = value.value("mode", "");
            mWeaponSet = value.value("weapon_set", 0);
            if (value.contains("start") && value["start"].is_object())
            {
                const auto& start = value["start"];
                mStart.position = jsonVec3(start.value("position", nlohmann::json::array()));
                mStart.velocity = jsonVec3(start.value("velocity", nlohmann::json::array()));
                mStart.camForward = jsonVec3(start.value("cam_forward", nlohmann::json::array()));
                mStart.yaw = start.value("yaw", -90.0f);
                mStart.pitch = start.value("pitch", 0.0f);
                mStart.grounded = start.value("grounded", false);
                mStart.health = start.value("health", 100);
                mStart.weapon = start.value("equipped_weapon", "");
                mStart.slot = start.value("equipped_slot", 0);
                mStart.spawnGeneration = start.value("spawn_generation", 0u);
                mStart.transformEpoch = static_cast<uint16_t>(
                    start.value("transform_epoch", 0u));
            }
        }
        else if (kind == "tick")
        {
            if (!sawHeader)
            {
                if (error) *error = "tick_before_header";
                return false;
            }
            Tick tick;
            tick.input.moveX = value.value("move_world", nlohmann::json::array()).size() >= 2
                ? value["move_world"][0].get<float>() : 0.0f;
            tick.input.moveY = value.value("move_world", nlohmann::json::array()).size() >= 2
                ? value["move_world"][1].get<float>() : 0.0f;
            if (value.contains("wish") && value["wish"].is_array() && value["wish"].size() >= 2)
            {
                tick.input.wishSide = value["wish"][0].get<float>();
                tick.input.wishForward = value["wish"][1].get<float>();
            }
            tick.input.jump = value.value("jump", false);
            tick.input.jumpPressed = value.value("jump_pressed", false);
            tick.input.dashPressed = value.value("dash_pressed", false);
            tick.input.downDashPressed = value.value("down_dash_pressed", false);
            tick.input.freezeHeld = value.value("freeze_held", false);
            tick.input.freezePressed = value.value("freeze_pressed", false);
            tick.input.crouchHeld = value.value("crouch_held", false);
            tick.input.reloadPressed = value.value("reload_pressed", false);
            tick.input.primaryHeld = value.value("primary_held", false);
            tick.input.secondaryHeld = value.value("secondary_held", false);
            tick.input.shieldHeld = value.value("shield_held", false);
            tick.input.grabHeld = value.value("grab_held", false);
            tick.input.lookYaw = value.value("look_yaw", -90.0f);
            tick.input.lookPitch = value.value("look_pitch", 0.0f);
            tick.input.camForward = jsonVec3(value.value("cam_forward", nlohmann::json::array()));
            tick.equippedWeapon = value.value("equipped_weapon", "");
            tick.equippedSlot = value.value("equipped_slot", 0);
            tick.input.movementPressed =
                std::abs(tick.input.moveX) > 0.0001f ||
                std::abs(tick.input.moveY) > 0.0001f;
            tick.input.movementJustPressed = tick.input.movementPressed;
            tick.position = jsonVec3(value.value("position", nlohmann::json::array()));
            tick.velocity = jsonVec3(value.value("velocity", nlohmann::json::array()));
            tick.health = value.value("health", 100);
            tick.grounded = value.value("grounded", false);
            tick.serverTick = value.value("server_tick", 0u);
            mTicks.push_back(tick);
        }
        else if (kind == "footer")
        {
            sawFooter = true;
            break;
        }
        // actor_identity, actor_lifecycle, and server_tick records are consumed
        // by the server playback owner; the client input player ignores them.
    }

    if (!sawHeader)
    {
        if (error) *error = "missing_header";
        return false;
    }
    if (mSchemaVersion == ScenarioTracks::kSchemaV2 && !sawFooter)
    {
        if (error) *error = "incomplete_scenario_track";
        return false;
    }
    (void)sawFooter;
    return true;
}

bool ScenarioPlayer::begin()
{
    if (mActive || mTicks.empty())
        return false;
    mActive = true;
    mWaitingForStart = true;
    mValid = false;
    mIndex = 0;
    mMaxPositionError = 0.0f;
    mFailureReason.clear();
    mLastAppliedWeapon = mStart.weapon;
    mLastAppliedSlot = mStart.slot;
    mStartMs = nowMs();
    mSavedMaxFrames = gFramePacer.maxFrames();
    gFramePacer.setMaxFrames(60);
    ScenarioInput::setActive(true);
    ScenarioInput::setFrame(&mNeutral);

    mServerPrepared = !schema2() || !MP_CONTEXT.active;
    mServerBeginAck = !schema2() || !MP_CONTEXT.active;
    mServerRejected = false;
    mServerCommandSent = false;
    mBeginSent = false;
    mServerError.clear();

    if (schema2() && MP_CONTEXT.active)
    {
        MimitaNet::mpSendServerCommand(
            MP_CONTEXT, "scenario_server_play " + mPath);
        mServerCommandSent = true;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.started", "scenario-player",
        "scenario playback started; waiting for recorded start state", 0,
        nlohmann::json{{"path", mPath},
                       {"schema", mSchemaVersion},
                       {"map", mMap},
                       {"mode", mMode},
                       {"weapon_set", mWeaponSet},
                       {"frame_count", mTicks.size()},
                       {"start_position", {mStart.position.x, mStart.position.y,
                                           mStart.position.z}},
                       {"spawn_generation", mStart.spawnGeneration},
                       {"transform_epoch", mStart.transformEpoch}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

void ScenarioPlayer::applyRecordedEquipment(const Tick& tick)
{
    if (tick.equippedWeapon == mLastAppliedWeapon &&
        tick.equippedSlot == mLastAppliedSlot)
        return;

    const std::string weapon =
        tick.equippedWeapon.empty() ? "nothing" : tick.equippedWeapon;
    Terminal::instance().execute("equipweapon " + weapon);
    mLastAppliedWeapon = tick.equippedWeapon;
    mLastAppliedSlot = tick.equippedSlot;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.equipment", "scenario-player",
        "applied recorded weapon change during playback", mIndex,
        nlohmann::json{{"path", mPath},
                       {"frame", mIndex},
                       {"weapon", weapon},
                       {"equipped_slot", tick.equippedSlot}},
        __FILE__, __LINE__, __FUNCTION__);
}

const InputFrame& ScenarioPlayer::currentFrame()
{
    if (!mActive)
        return mNeutral;
    if (mWaitingForStart)
    {
        mCurrent = mNeutral;
    }
    else
    {
        const Tick& tick = mTicks[mIndex];
        applyRecordedEquipment(tick);
        mCurrent = tick.input;
        // Weapon changes are replayed from the recorded result, not from the
        // raw slot edges, so combat cannot re-equip a different slot.
        mCurrent.slotHeldMask = 0;
    }
    ScenarioInput::setFrame(&mCurrent);
    return mCurrent;
}

void ScenarioPlayer::advance(Player& player, Camera& camera)
{
    (void)camera;
    if (!mActive)
        return;

    if (mServerRejected)
    {
        fail(mServerError.empty() ? "server_rejected" : mServerError);
        return;
    }

    if (mWaitingForStart)
    {
        const bool networked = MP_CONTEXT.active;
        const bool v2 = schema2();
        if (v2 && !networked)
        {
            fail("server_required_for_schema_2");
            return;
        }
        const bool positionReady =
            glm::distance(player.pos, mStart.position) <= 0.75f;
        const bool serverPositionReady =
            !networked ||
            glm::distance(MP_CONTEXT.localServerPosition, mStart.position) <= 1.0f;
        const bool teleportSettled =
            !networked || !MP_CONTEXT.awaitingTeleportAck;
        const bool reconciled =
            !networked || (MP_CONTEXT.localPlayerReconciled &&
                           MP_CONTEXT.hasLocalServerPosition);
        const bool spawnReady =
            !networked || (MP_CONTEXT.gameplayActive &&
                           MP_CONTEXT.pendingSpawnAckGeneration == 0);
        const bool generationReady =
            !networked || mStart.spawnGeneration == 0 ||
            MP_CONTEXT.lastKnownSpawnGeneration == mStart.spawnGeneration;
        const bool epochReady =
            !networked || mStart.transformEpoch == 0 ||
            MP_CONTEXT.localServerEpoch == mStart.transformEpoch;
        const bool equipmentReady =
            mStart.weapon.empty() || player.equippedWeaponId == mStart.weapon;
        const bool localReady = positionReady && serverPositionReady &&
            teleportSettled && reconciled && spawnReady && generationReady &&
            epochReady && equipmentReady;
        const bool serverReady = !v2 || !networked || mServerPrepared;

        if (localReady && serverReady && !mBeginSent)
        {
            if (v2 && networked)
            {
                MimitaNet::mpSendServerCommand(
                    MP_CONTEXT,
                    "scenario_server_play_begin " +
                        std::to_string(MP_CONTEXT.latestServerTick));
            }
            mBeginSent = true;
            return;
        }
        if (mBeginSent && (mServerBeginAck || !v2 || !networked))
        {
            mWaitingForStart = false;
            // Restore the complete recorded start state only when frames begin,
            // so the actor does not drift during the teleport/reconciliation
            // wait. This is start restoration, not path snapping.
            player.pos = mStart.position;
            player.vel = mStart.velocity;
            player.externalImpulse = glm::vec3(0.0f);
            player.ground.onGround = mStart.grounded;
            player.ground.stableOnGround = mStart.grounded;
            player.ground.hasWorldContact = mStart.grounded;
            player.lastValidWorldCollisionPosition = mStart.position;
            player.hasLastValidWorldCollisionPosition = true;
            player.updateModelWorldTransforms();
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "scenario.playback.start_ready", "scenario-player",
                "recorded start state and server proxies reached; playback frames begin",
                0,
                nlohmann::json{{"path", mPath},
                               {"start_restored", true},
                               {"position_ready", positionReady},
                               {"server_position_ready", serverPositionReady},
                               {"teleport_settled", teleportSettled},
                               {"reconciled", reconciled},
                               {"spawn_ready", spawnReady},
                               {"generation_ready", generationReady},
                               {"epoch_ready", epochReady},
                               {"equipment_ready", equipmentReady},
                               {"server_prepared", mServerPrepared},
                               {"server_begin_ack", mServerBeginAck},
                               {"waited_ms", nowMs() - mStartMs}},
                __FILE__, __LINE__, __FUNCTION__);
            return;
        }

        if (nowMs() - mStartMs > kReadinessTimeoutMs)
        {
            std::string reason = "readiness_timeout";
            if (!localReady)
                reason = "start_readiness_timeout";
            else if (!serverReady)
                reason = "server_readiness_timeout";
            else if (mBeginSent && !mServerBeginAck)
                reason = "server_begin_timeout";
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Errors,
                "scenario.playback.start_ready", "scenario-player",
                "recorded start state was not ready before timeout", 0,
                nlohmann::json{{"path", mPath},
                               {"start_restored", false},
                               {"reason", reason},
                               {"position_ready", positionReady},
                               {"server_position_ready", serverPositionReady},
                               {"teleport_settled", teleportSettled},
                               {"reconciled", reconciled},
                               {"spawn_ready", spawnReady},
                               {"generation_ready", generationReady},
                               {"epoch_ready", epochReady},
                               {"equipment_ready", equipmentReady},
                               {"server_prepared", mServerPrepared},
                               {"server_begin_ack", mServerBeginAck},
                               {"begin_sent", mBeginSent},
                               {"waited_ms", nowMs() - mStartMs}},
                __FILE__, __LINE__, __FUNCTION__);
            fail(reason);
        }
        return;
    }

    if (mIndex < mTicks.size())
    {
        // The recorded position is the pre-tick state. After simulating frame
        // N, compare against the recorded start of frame N+1.
        const uint32_t compareIndex = mIndex + 1;
        if (compareIndex < mTicks.size())
        {
            const Tick& next = mTicks[compareIndex];
            const float error = glm::distance(player.pos, next.position);
            mMaxPositionError = std::max(mMaxPositionError, error);
            if (error > kDivergenceFailM)
            {
                if (mFirstDivergenceFrame == 0xFFFFFFFFu)
                {
                    mFirstDivergenceFrame = mIndex;
                    mFirstDivergenceError = error;
                    mFirstDivergenceExpected = next.position;
                    mFirstDivergenceActual = player.pos;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::General, StructuredLevel::Errors,
                        "scenario.playback.divergence", "scenario-player",
                        "recorded input playback diverged from the recorded pose",
                        mIndex,
                        nlohmann::json{{"path", mPath},
                                       {"frame", mIndex},
                                       {"server_tick", next.serverTick},
                                       {"expected_position", {mFirstDivergenceExpected.x,
                                                              mFirstDivergenceExpected.y,
                                                              mFirstDivergenceExpected.z}},
                                       {"actual_position", {mFirstDivergenceActual.x,
                                                            mFirstDivergenceActual.y,
                                                            mFirstDivergenceActual.z}},
                                       {"error_m", error},
                                       {"threshold_m", kDivergenceFailM}},
                        __FILE__, __LINE__, __FUNCTION__);
                }
                fail("position_divergence");
                return;
            }
        }
        ++mIndex;
    }
    if (mIndex >= mTicks.size())
        finish("complete");
}

void ScenarioPlayer::onServerCommandResult(bool accepted,
                                           const std::string& status)
{
    if (!mActive && !mWaitingForStart)
        return;
    if (accepted)
    {
        if (status.find("scenario server prepared") != std::string::npos)
            mServerPrepared = true;
        else if (status.find("scenario server playback began") != std::string::npos)
            mServerBeginAck = true;
    }
    else if (status.find("scenario playback rejected") != std::string::npos ||
             status.find("scenario server") != std::string::npos)
    {
        mServerRejected = true;
        mServerError = status;
    }
}

void ScenarioPlayer::stop(const char* reason)
{
    if (!mActive)
        return;
    finish(reason ? reason : "stop");
}

void ScenarioPlayer::fail(const std::string& reason)
{
    if (!mActive)
        return;
    mFailureReason = reason;
    mValid = false;
    mActive = false;
    mWaitingForStart = false;
    ScenarioInput::clear();
    gFramePacer.setMaxFrames(mSavedMaxFrames > 0 ? mSavedMaxFrames : 999);
    if (schema2() && MP_CONTEXT.active)
        MimitaNet::mpSendServerCommand(MP_CONTEXT, "scenario_server_stop");
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Errors,
        "scenario.playback.failed", "scenario-player",
        "scenario playback invalidated without snapping to the recorded path",
        mIndex,
        nlohmann::json{{"path", mPath},
                       {"valid", false},
                       {"reason", reason},
                       {"frames_played", mIndex},
                       {"frame_count", mTicks.size()},
                       {"max_position_error_m", mMaxPositionError},
                       {"first_divergence_frame", mFirstDivergenceFrame == 0xFFFFFFFFu
                            ? nlohmann::json(nullptr) : nlohmann::json(mFirstDivergenceFrame)},
                       {"first_divergence_error_m", mFirstDivergenceError},
                       {"first_divergence_expected", {mFirstDivergenceExpected.x,
                                                       mFirstDivergenceExpected.y,
                                                       mFirstDivergenceExpected.z}},
                       {"first_divergence_actual", {mFirstDivergenceActual.x,
                                                     mFirstDivergenceActual.y,
                                                     mFirstDivergenceActual.z}}},
        __FILE__, __LINE__, __FUNCTION__);
}

void ScenarioPlayer::finish(const char* reason)
{
    if (!mActive)
        return;
    mActive = false;
    mWaitingForStart = false;
    ScenarioInput::clear();
    gFramePacer.setMaxFrames(mSavedMaxFrames > 0 ? mSavedMaxFrames : 999);
    if (schema2() && MP_CONTEXT.active)
        MimitaNet::mpSendServerCommand(MP_CONTEXT, "scenario_server_stop");
    const bool completed = std::string(reason ? reason : "") == "complete" &&
        mFailureReason.empty();
    mValid = completed;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General,
        completed ? StructuredLevel::Important : StructuredLevel::Errors,
        completed ? "scenario.playback.completed" : "scenario.playback.stopped",
        "scenario-player",
        completed ? "scenario playback finished" : "scenario playback stopped",
        mIndex,
        nlohmann::json{{"path", mPath},
                       {"completed", completed},
                       {"valid", mValid},
                       {"end_reason", reason ? reason : "unknown"},
                       {"failure_reason", mFailureReason},
                       {"frames_played", mIndex},
                       {"frame_count", mTicks.size()},
                       {"max_position_error_m", mMaxPositionError},
                       {"first_divergence_frame", mFirstDivergenceFrame == 0xFFFFFFFFu
                            ? nlohmann::json(nullptr) : nlohmann::json(mFirstDivergenceFrame)},
                       {"first_divergence_error_m", mFirstDivergenceError},
                       {"first_divergence_expected", {mFirstDivergenceExpected.x,
                                                       mFirstDivergenceExpected.y,
                                                       mFirstDivergenceExpected.z}},
                       {"first_divergence_actual", {mFirstDivergenceActual.x,
                                                     mFirstDivergenceActual.y,
                                                     mFirstDivergenceActual.z}}},
        __FILE__, __LINE__, __FUNCTION__);
}
