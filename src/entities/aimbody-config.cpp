// 08 15 2026, 22 40
/* purpose
* Implements the hot-reloadable aimbody.json config: per-limb pitch/yaw/roll
* gains that the animation system applies when the camera looks up/down.
* Watches the file mtime on a 250ms throttle so edits take effect live.
* Does NOT own the animation math, the pose pipeline, or firing logic.
*/
#include "aimbody-config.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace {

int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int64_t modifiedTimeNs(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        time.time_since_epoch()).count();
}

bool readLimb(const json& j, LimbAim& out)
{
    if (!j.is_object())
        return false;
    if (j.contains("pitch") && j["pitch"].is_number())
        out.pitch = j["pitch"].get<float>();
    if (j.contains("yaw") && j["yaw"].is_number())
        out.yaw = j["yaw"].get<float>();
    if (j.contains("roll") && j["roll"].is_number())
        out.roll = j["roll"].get<float>();
    return true;
}

} // namespace

AimBodyConfig& AimBodyConfig::instance()
{
    static AimBodyConfig config;
    return config;
}

bool AimBodyConfig::load(const std::string& path)
{
    mPath = path;
    mEnabled = true;
    mLimbs.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Animation,
            "[AIMBODY] missing %s; defaults active (enabled=1, pitch-only)\n", path.c_str());
        save();
        return false;
    }

    try {
        json j;
        file >> j;
        if (j.contains("enabled"))
            mEnabled = j.value("enabled", true);
        if (j.contains("limbs") && j["limbs"].is_object()) {
            for (auto it = j["limbs"].begin(); it != j["limbs"].end(); ++it) {
                LimbAim limb;
                if (readLimb(it.value(), limb))
                    mLimbs[it.key()] = limb;
            }
        }
        mLastModified = modifiedTimeNs(path);
        Debug::log(Debug::Category::Animation,
            "[AIMBODY] loaded %s enabled=%d limbs=%zu\n",
            path.c_str(), (int)mEnabled, mLimbs.size());
        return true;
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Animation,
            "[AIMBODY] parse failed: %s\n", e.what());
        return false;
    }
}

bool AimBodyConfig::save()
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(mPath).parent_path(), ec);
    json j;
    j["enabled"] = mEnabled;
    json limbs = json::object();
    for (const auto& [name, limb] : mLimbs) {
        limbs[name] = {
            {"pitch", limb.pitch},
            {"yaw", limb.yaw},
            {"roll", limb.roll}
        };
    }
    j["limbs"] = limbs;
    std::ofstream file(mPath);
    if (!file.is_open()) return false;
    file << j.dump(2) << '\n';
    file.close();
    mLastModified = modifiedTimeNs(mPath);
    return true;
}

bool AimBodyConfig::reload()
{
    return load(mPath);
}

bool AimBodyConfig::pollReload()
{
    const int64_t now = nowMs();
    if (now - mLastPollMs < 250)
        return false;
    mLastPollMs = now;
    const int64_t current = modifiedTimeNs(mPath);
    if (current != 0 && current != mLastModified)
        return reload();
    return false;
}

const LimbAim* AimBodyConfig::limb(const std::string& name) const
{
    auto it = mLimbs.find(name);
    return it != mLimbs.end() ? &it->second : nullptr;
}
