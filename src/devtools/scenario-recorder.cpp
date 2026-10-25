#include "devtools/scenario-recorder.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "camera.h"
#include "debug/structured-log.h"
#include "entities/player.h"
#include "input/input-frame.h"

namespace {

constexpr uint32_t kMaxFrames = 60u * 60u * 10u;
constexpr uint32_t kFlushEvery = 6u;

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

nlohmann::json vec3Json(const glm::vec3& v)
{
    return nlohmann::json::array({v.x, v.y, v.z});
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
    (void)player;
    (void)camera;
    if (mActive)
    {
        if (error) *error = "recording_already_active";
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

    mOut.open(path, std::ios::out | std::ios::trunc);
    if (!mOut.is_open())
    {
        if (error) *error = "cannot_open_recording_file";
        return false;
    }

    mPath = path.generic_string();
    mMap = map;
    mMode = mode;
    mWeaponSet = weaponSet;
    mFrame = 0;
    mStartPending = true;
    mActive = true;
    mStartMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.record.started", "scenario-recorder",
        "input recording armed; header is written on the next fixed tick", 0,
        nlohmann::json{{"path", mPath},
                       {"map", mMap},
                       {"mode", mMode},
                       {"weapon_set", mWeaponSet},
                       {"tick_rate", 60}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
}

void ScenarioRecorder::writeHeader(const Player& player, const Camera& camera)
{
    const nlohmann::json header{
        {"kind", "header"},
        {"schema", 1},
        {"created_utc", utcTimestamp(false)},
        {"tick_rate", 60},
        {"map", mMap},
        {"mode", mMode},
        {"weapon_set", mWeaponSet},
        {"player", player.username},
        {"avatar", player.avatarName()},
        {"source", "human"},
        {"start", {
            {"position", vec3Json(player.pos)},
            {"velocity", vec3Json(player.vel)},
            {"yaw", camera.yaw},
            {"pitch", camera.pitch},
            {"cam_forward", vec3Json(camera.front)},
            {"grounded", player.ground.onGround},
            {"health", player.currentHp},
            {"equipped_weapon", player.equippedWeaponId},
            {"equipped_slot", player.equippedSlot},
            {"movement_simulation_tick", player.movementSimulationTick}
        }}
    };
    mOut << header.dump() << '\n';
    mStartPending = false;
}

void ScenarioRecorder::captureTick(const InputFrame& frame,
                                   const Player& player,
                                   const Camera& camera)
{
    if (!mActive || !mOut.is_open())
        return;

    if (mStartPending)
        writeHeader(player, camera);

    const nlohmann::json tick{
        {"kind", "tick"},
        {"frame", mFrame},
        {"client_tick", static_cast<uint64_t>(player.movementSimulationTick)},
        {"wish", {frame.wishSide, frame.wishForward}},
        {"move_world", {frame.moveX, frame.moveY}},
        {"jump", frame.jump},
        {"jump_pressed", frame.jumpPressed},
        {"dash_pressed", frame.dashPressed},
        {"down_dash_pressed", frame.downDashPressed},
        {"freeze_held", frame.freezeHeld},
        {"freeze_pressed", frame.freezePressed},
        {"crouch_held", frame.crouchHeld},
        {"reload_pressed", frame.reloadPressed},
        {"primary_held", frame.primaryHeld},
        {"secondary_held", frame.secondaryHeld},
        {"shield_held", frame.shieldHeld},
        {"grab_held", frame.grabHeld},
        {"look_yaw", frame.lookYaw},
        {"look_pitch", frame.lookPitch},
        {"cam_forward", vec3Json(frame.camForward)},
        {"equipped_weapon", player.equippedWeaponId},
        {"equipped_slot", player.equippedSlot},
        {"position", vec3Json(player.pos)},
        {"velocity", vec3Json(player.vel)},
        {"health", player.currentHp},
        {"grounded", player.ground.onGround},
        {"cam_yaw", camera.yaw},
        {"cam_pitch", camera.pitch}
    };
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
    const uint64_t nowMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const double duration = mStartMs && nowMs > mStartMs
        ? static_cast<double>(nowMs - mStartMs) / 1000.0 : 0.0;
    const nlohmann::json footer{
        {"kind", "footer"},
        {"ended_utc", utcTimestamp(false)},
        {"frames", mFrame},
        {"duration_seconds", duration},
        {"stop_reason", reason ? reason : "unknown"}
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

    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "scenario.record.stopped", "scenario-recorder",
        "input recording stopped", mFrame,
        nlohmann::json{{"path", mPath},
                       {"frames", mFrame},
                       {"stop_reason", reason ? reason : "unknown"}},
        __FILE__, __LINE__, __FUNCTION__);
}
