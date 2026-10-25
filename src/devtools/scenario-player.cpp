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
#include "terminal/terminal-state.h"
#include "video/frame-pacer.h"

extern FramePacer gFramePacer;

namespace {

// Input playback may drift because the live server/world state differs. When
// the local character is this far from the recorded server-side position,
// snap to the next recorded valid position so the rest of the input sequence
// is applied at the correct place on the map.
constexpr float kMaxPlaybackDriftM = 5.0f;

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
    mMap.clear();
    mMode.clear();
    mWeaponSet = 0;
    mStart = StartState{};
    mIndex = 0;
    mMaxPositionError = 0.0f;
    mFirstDivergenceFrame = 0xFFFFFFFFu;
    mFirstDivergenceError = 0.0f;
    mFirstDivergenceExpected = glm::vec3(0.0f);
    mFirstDivergenceActual = glm::vec3(0.0f);
    mSnapCount = 0;
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
            if (value.value("schema", 0) != 1)
            {
                if (error) *error = "unsupported_schema";
                return false;
            }
            if (value.value("tick_rate", 0) != 60)
            {
                if (error) *error = "unsupported_tick_rate";
                return false;
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
            mTicks.push_back(tick);
        }
        else if (kind == "footer")
        {
            sawFooter = true;
            break;
        }
    }

    if (!sawHeader)
    {
        if (error) *error = "missing_header";
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
    mIndex = 0;
    mMaxPositionError = 0.0f;
    mLastAppliedWeapon = mStart.weapon;
    mLastAppliedSlot = mStart.slot;
    mStartMs = nowMs();
    mSavedMaxFrames = gFramePacer.maxFrames();
    gFramePacer.setMaxFrames(60);
    ScenarioInput::setActive(true);
    ScenarioInput::setFrame(&mNeutral);

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.playback.started", "scenario-player",
        "input playback started; waiting for recorded start state", 0,
        nlohmann::json{{"path", mPath},
                       {"map", mMap},
                       {"mode", mMode},
                       {"weapon_set", mWeaponSet},
                       {"frame_count", mTicks.size()},
                       {"start_position", {mStart.position.x, mStart.position.y, mStart.position.z}}},
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

    if (mWaitingForStart)
    {
        const bool networked = MP_CONTEXT.active;
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
        const bool ready = positionReady && serverPositionReady &&
                           teleportSettled && reconciled;
        if (ready || nowMs() - mStartMs > 5000)
        {
            mWaitingForStart = false;
            // Start motion is restored only when frames begin, so the actor
            // does not drift during the teleport/reconciliation wait.
            player.vel = mStart.velocity;
            player.externalImpulse = glm::vec3(0.0f);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "scenario.playback.start_ready", "scenario-player",
                ready
                    ? "recorded start state reached; playback frames begin"
                    : "recorded start state was not fully restored before the timeout; playback begins anyway",
                0,
                nlohmann::json{{"path", mPath},
                               {"start_restored", ready},
                               {"position_ready", positionReady},
                               {"server_position_ready", serverPositionReady},
                               {"teleport_settled", teleportSettled},
                               {"reconciled", reconciled},
                               {"waited_ms", nowMs() - mStartMs}},
                __FILE__, __LINE__, __FUNCTION__);
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
            if (error > 1.0f && mFirstDivergenceFrame == 0xFFFFFFFFu)
            {
                mFirstDivergenceFrame = mIndex;
                mFirstDivergenceError = error;
                mFirstDivergenceExpected = next.position;
                mFirstDivergenceActual = player.pos;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "scenario.playback.divergence", "scenario-player",
                    "recorded input playback diverged from the recorded pose", mIndex,
                    nlohmann::json{{"path", mPath},
                                   {"frame", mIndex},
                                   {"expected_position", {mFirstDivergenceExpected.x,
                                                          mFirstDivergenceExpected.y,
                                                          mFirstDivergenceExpected.z}},
                                   {"actual_position", {mFirstDivergenceActual.x,
                                                        mFirstDivergenceActual.y,
                                                        mFirstDivergenceActual.z}},
                                   {"error_m", error}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            if (error >= kMaxPlaybackDriftM)
            {
                const glm::vec3 before = player.pos;
                player.pos = next.position;
                player.vel = next.velocity;
                player.externalImpulse = glm::vec3(0.0f);
                player.lastValidWorldCollisionPosition = next.position;
                player.hasLastValidWorldCollisionPosition = true;
                player.updateModelWorldTransforms();
                ++mSnapCount;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "scenario.playback.snap", "scenario-player",
                    "snapped local playback actor to the next recorded valid position", mIndex,
                    nlohmann::json{{"path", mPath},
                                   {"frame", mIndex},
                                   {"drift_m", error},
                                   {"max_drift_m", kMaxPlaybackDriftM},
                                   {"actual_before", {before.x, before.y, before.z}},
                                   {"snapped_to", {next.position.x, next.position.y,
                                                   next.position.z}}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
        }
        ++mIndex;
    }
    if (mIndex >= mTicks.size())
        finish("complete");
}

void ScenarioPlayer::stop(const char* reason)
{
    if (!mActive)
        return;
    finish(reason ? reason : "stop");
}

void ScenarioPlayer::finish(const char* reason)
{
    if (!mActive)
        return;
    mActive = false;
    mWaitingForStart = false;
    ScenarioInput::clear();
    gFramePacer.setMaxFrames(mSavedMaxFrames > 0 ? mSavedMaxFrames : 999);
    const bool completed = std::string(reason ? reason : "") == "complete";
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        completed ? "scenario.playback.completed" : "scenario.playback.stopped",
        "scenario-player",
        completed ? "input playback finished" : "input playback stopped", mIndex,
        nlohmann::json{{"path", mPath},
                       {"completed", completed},
                       {"end_reason", reason ? reason : "unknown"},
                       {"frames_played", mIndex},
                       {"frame_count", mTicks.size()},
                       {"max_position_error_m", mMaxPositionError},
                       {"snap_count", mSnapCount},
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
// force scenario-player recompile: per-tick equipment and playback drift snap.
