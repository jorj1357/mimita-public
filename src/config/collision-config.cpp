// 08 15 2026, 12 00
/* purpose
* Live-tunable collision response settings (bounce).
* Reloads config/collision.json on change so bounce strength, min speed,
* and cooldown tune at runtime without restarting.
* Does NOT build collision meshes, own the world, or apply physics.
*/

#include "config/collision-config.h"

#include <algorithm>
#include <exception>
#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

CollisionConfig& CollisionConfig::instance()
{
    static CollisionConfig cfg;
    return cfg;
}

CollisionConfig::CollisionConfig()
{
    load();
}

bool CollisionConfig::load(const std::string& path)
{
    mPath = path;

    std::ifstream file(path);
    if (!file.is_open())
    {
        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG] No config at %s (bounce disabled)\n", path.c_str());
        return false;
    }

    try
    {
        json j;
        file >> j;

        if (j.contains("bounce"))
        {
            const json& b = j["bounce"];
            mBounceEnabled = b.value("enabled", false);
            mBounceStrength = std::max(0.0f, b.value("strength", 0.0f));
            mBounceFriction = std::clamp(b.value("friction", 0.5f), 0.0f, 1.0f);
            mBounceMinSpeed = std::max(0.0f, b.value("minSpeed", 7.0f));
            mBounceMaxSpeed = std::max(mBounceMinSpeed, b.value("maxSpeed", 45.0f));
            mBounceCooldown = std::max(0.0f, b.value("cooldown", 0.05f));
        }
        else
        {
            mBounceEnabled = false;
        }

        std::error_code ec;
        mLastWrite = std::filesystem::last_write_time(path, ec);
        mLastCheck = std::chrono::steady_clock::now();

        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG] bounce enabled=%d strength=%.3f friction=%.2f "
            "minSpeed=%.2f maxSpeed=%.2f cooldown=%.3f\n",
            (int)mBounceEnabled, mBounceStrength, mBounceFriction,
            mBounceMinSpeed, mBounceMaxSpeed, mBounceCooldown);
        return true;
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG ERROR] Failed to parse %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

bool CollisionConfig::pollHotReload()
{
    const auto now = std::chrono::steady_clock::now();
    if (mLastCheck.time_since_epoch().count() != 0 &&
        now - mLastCheck < std::chrono::milliseconds(250))
        return false;
    mLastCheck = now;

    std::error_code ec;
    if (!std::filesystem::exists(mPath, ec) || ec)
        return false;
    const auto writeTime = std::filesystem::last_write_time(mPath, ec);
    if (ec || writeTime == mLastWrite)
        return false;

    mLastWrite = writeTime;
    return load(mPath);
}
