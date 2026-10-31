// 08 15 2026, 12 00
/* purpose
* Live-tunable collision response settings (bounce).
* Reloads config/collision.json on change so bounce strength, min speed,
* and cooldown tune at runtime without restarting.
* Does NOT build collision meshes, own the world, or apply physics.
*/

#include "config/collision-config.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "debug/structured-log.h"

using json = nlohmann::json;

namespace {

float requireNumber(const json& object, const char* key, const std::string& path)
{
    if (!object.contains(key))
        throw std::runtime_error(path + "." + key + " is required");
    const json& value = object.at(key);
    if (!value.is_number())
        throw std::runtime_error(path + "." + key + " must be a number");
    const double number = value.get<double>();
    if (!std::isfinite(number))
        throw std::runtime_error(path + "." + key + " must be finite");
    return static_cast<float>(number);
}

bool requireBool(const json& object, const char* key, const std::string& path)
{
    if (!object.contains(key))
        throw std::runtime_error(path + "." + key + " is required");
    const json& value = object.at(key);
    if (!value.is_boolean())
        throw std::runtime_error(path + "." + key + " must be a boolean");
    return value.get<bool>();
}

WeaponVsWeaponSettings readWeaponVsWeapon(const json& root)
{
    WeaponVsWeaponSettings out;
    if (!root.contains("weapon_vs_weapon"))
        return out;

    const json& block = root.at("weapon_vs_weapon");
    if (!block.is_object())
        throw std::runtime_error("weapon_vs_weapon must be an object");

    out.distanceToleranceM = requireNumber(
        block, "distance_tolerance_m", "weapon_vs_weapon");
    if (out.distanceToleranceM < 0.0f || out.distanceToleranceM > 2.0f)
        throw std::runtime_error(
            "weapon_vs_weapon.distance_tolerance_m must be in [0, 2]");

    if (!block.contains("cooldown_ticks") ||
        !block.at("cooldown_ticks").is_number_integer())
        throw std::runtime_error(
            "weapon_vs_weapon.cooldown_ticks must be an integer");
    out.cooldownTicks = block.at("cooldown_ticks").get<int>();
    if (out.cooldownTicks < 0 || out.cooldownTicks > 600)
        throw std::runtime_error(
            "weapon_vs_weapon.cooldown_ticks must be in [0, 600]");

    if (!block.contains("effects") || !block.at("effects").is_object())
        throw std::runtime_error(
            "weapon_vs_weapon.effects must be an object");
    const json& effects = block.at("effects");
    out.sparkEnabled = requireBool(effects, "spark", "weapon_vs_weapon.effects");
    out.ricochetEnabled = requireBool(
        effects, "ricochet", "weapon_vs_weapon.effects");

    if (!block.contains("knockback") || !block.at("knockback").is_object())
        throw std::runtime_error(
            "weapon_vs_weapon.knockback must be an object");
    const json& knockback = block.at("knockback");
    out.knockbackMinSpeedMps = requireNumber(
        knockback, "min_speed_mps", "weapon_vs_weapon.knockback");
    out.knockbackMaxSpeedMps = requireNumber(
        knockback, "max_speed_mps", "weapon_vs_weapon.knockback");
    out.knockbackReferenceSpeedMps = requireNumber(
        knockback, "reference_speed_mps", "weapon_vs_weapon.knockback");

    if (out.knockbackMinSpeedMps < 0.0f ||
        out.knockbackMinSpeedMps > 500.0f)
        throw std::runtime_error(
            "weapon_vs_weapon.knockback.min_speed_mps must be in [0, 500]");
    if (out.knockbackMaxSpeedMps < out.knockbackMinSpeedMps ||
        out.knockbackMaxSpeedMps > 1000.0f)
        throw std::runtime_error(
            "weapon_vs_weapon.knockback.max_speed_mps must be >= min and <= 1000");
    if (out.knockbackReferenceSpeedMps < 0.1f ||
        out.knockbackReferenceSpeedMps > 1000.0f)
        throw std::runtime_error(
            "weapon_vs_weapon.knockback.reference_speed_mps must be in [0.1, 1000]");

    return out;
}

} // namespace

float WeaponVsWeaponSettings::impulseForRelativeSpeed(float relativeSpeedMps) const
{
    if (!std::isfinite(relativeSpeedMps) || knockbackReferenceSpeedMps <= 0.0f)
        return knockbackMinSpeedMps;
    const float force = std::clamp(
        relativeSpeedMps / knockbackReferenceSpeedMps, 0.0f, 1.0f);
    return knockbackMinSpeedMps +
        force * (knockbackMaxSpeedMps - knockbackMinSpeedMps);
}

CollisionConfig& CollisionConfig::instance()
{
    static CollisionConfig cfg;
    return cfg;
}

CollisionConfig::CollisionConfig()
{
    load();
}

WeaponVsWeaponSettings CollisionConfig::weaponVsWeapon() const
{
    std::lock_guard<std::mutex> lock(mWeaponVsWeaponMutex);
    return mWeaponVsWeapon;
}

bool CollisionConfig::load(const std::string& path, std::uint32_t tick)
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
        j = nlohmann::json::parse(file, nullptr, true, true);

        // Validate the shared clash block first so a malformed or out-of-range
        // edit cannot partially publish new values.
        const WeaponVsWeaponSettings weaponVsWeapon = readWeaponVsWeapon(j);

        mBodyMeshCollision = j.value("bodyMeshCollision", true);
        mActorTriangleSolver = j.value("actorTriangleSolver", false);
        mActorCollisionAccelerated = j.value("actorCollisionAccelerated", true);
        mActorCollisionComparison = j.value("actorCollisionComparison", false);
        mCollisionSkin = std::clamp(j.value("collisionSkin", 0.05f), 0.0f, 0.25f);
        mEdgeTouchTolerance = std::clamp(
            j.value("edgeTouchTolerance", 0.002f), 0.0f, 0.05f);
        mSmoothSeamDot = std::clamp(
            j.value("smoothSeamDot", 0.9999f), 0.0f, 1.0f);
        mMaxSubStepDistance = std::clamp(
            j.value("maxSubStepDistance", 0.35f), 0.0f, 5.0f);
        mMaxSubSteps = std::clamp(
            j.value("maxSubSteps", 128), 1, 512);

        if (j.contains("bounce"))
        {
            const json& b = j["bounce"];
            mBounceEnabled = b.value("enabled", false);
            mBounceStrength = std::max(0.0f, b.value("strength", 0.0f));
            mBounceFriction = std::clamp(b.value("friction", 0.5f), 0.0f, 1.0f);
            mBounceMinSpeed = std::max(0.0f, b.value("minSpeed", 7.0f));
            mBounceMaxSpeed = std::max(mBounceMinSpeed, b.value("maxSpeed", 45.0f));
            mBounceCooldown = std::max(0.0f, b.value("cooldown", 0.05f));
            mBounceMinPush = std::max(0.0f, b.value("minPush", 0.1f));
        }
        else
        {
            mBounceEnabled = false;
        }
        mLimbBounceMode = std::clamp(j.value("limbBounceMode", 2), 1, 2);

        {
            std::lock_guard<std::mutex> lock(mWeaponVsWeaponMutex);
            mWeaponVsWeapon = weaponVsWeapon;
        }

        std::error_code ec;
        mLastWrite = std::filesystem::last_write_time(path, ec);
        mLastCheck = std::chrono::steady_clock::now();

        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG] skin=%.3f edgeTouch=%.4f smoothSeamDot=%.4f bounce enabled=%d strength=%.3f friction=%.2f "
            "minSpeed=%.2f maxSpeed=%.2f cooldown=%.3f minPush=%.3f limbBounceMode=%d\n",
            mCollisionSkin, mEdgeTouchTolerance, mSmoothSeamDot, (int)mBounceEnabled,
            mBounceStrength, mBounceFriction,
            mBounceMinSpeed, mBounceMaxSpeed, mBounceCooldown, mBounceMinPush,
            mLimbBounceMode);
        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG] maxSubStepDistance=%.3f maxSubSteps=%d\n",
            mMaxSubStepDistance, mMaxSubSteps);
        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG] weapon_vs_weapon tolerance=%.3f cooldown=%d "
            "knockback=[%.2f..%.2f]@%.2f effects=%d/%d\n",
            weaponVsWeapon.distanceToleranceM, weaponVsWeapon.cooldownTicks,
            weaponVsWeapon.knockbackMinSpeedMps,
            weaponVsWeapon.knockbackMaxSpeedMps,
            weaponVsWeapon.knockbackReferenceSpeedMps,
            (int)weaponVsWeapon.sparkEnabled, (int)weaponVsWeapon.ricochetEnabled);

        StructuredLogger::instance().writeEvent(
            StructuredCategory::Collision, StructuredLevel::Important,
            "collision.config.reload", path,
            "collision config applied at fixed-tick boundary", tick,
            {{"path", path},
             {"applied_tick", tick},
             {"weapon_vs_weapon",
              {{"distance_tolerance_m", weaponVsWeapon.distanceToleranceM},
               {"cooldown_ticks", weaponVsWeapon.cooldownTicks},
               {"effects",
                {{"spark", weaponVsWeapon.sparkEnabled},
                 {"ricochet", weaponVsWeapon.ricochetEnabled}}},
               {"knockback",
                {{"min_speed_mps", weaponVsWeapon.knockbackMinSpeedMps},
                 {"max_speed_mps", weaponVsWeapon.knockbackMaxSpeedMps},
                 {"reference_speed_mps",
                  weaponVsWeapon.knockbackReferenceSpeedMps}}}}}});
        return true;
    }
    catch (const std::exception& e)
    {
        const WeaponVsWeaponSettings retained = weaponVsWeapon();
        Debug::log(Debug::Category::Collision,
            "[COLLISION CONFIG ERROR] Failed to parse %s: %s; keeping previous valid settings.\n",
            path.c_str(), e.what());
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Collision, StructuredLevel::Errors,
            "collision.config.reload_rejected", path,
            "invalid collision config retained previous values", tick,
            {{"path", path},
             {"reason", e.what()},
             {"applied_tick", tick},
             {"retained_weapon_vs_weapon",
              {{"distance_tolerance_m", retained.distanceToleranceM},
               {"cooldown_ticks", retained.cooldownTicks},
               {"effects",
                {{"spark", retained.sparkEnabled},
                 {"ricochet", retained.ricochetEnabled}}},
               {"knockback",
                {{"min_speed_mps", retained.knockbackMinSpeedMps},
                 {"max_speed_mps", retained.knockbackMaxSpeedMps},
                 {"reference_speed_mps",
                  retained.knockbackReferenceSpeedMps}}}}}});
        return false;
    }
}

bool CollisionConfig::pollHotReload(std::uint32_t tick)
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
    return load(mPath, tick);
}
