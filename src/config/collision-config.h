// 08 15 2026, 12 00
/* purpose
* Live-tunable collision response settings (bounce).
* Reloads config/collision.json on change so bounce strength, min speed,
* and cooldown tune at runtime without restarting.
* Does NOT build collision meshes, own the world, or apply physics.
*/
#pragma once

#include <chrono>
#include <filesystem>
#include <string>

class CollisionConfig {
public:
    static CollisionConfig& instance();

    bool load(const std::string& path = "config/collision.json");
    // Returns true when the file changed and settings were re-loaded.
    bool pollHotReload();

    bool bounceEnabled() const { return mBounceEnabled; }
    float bounceStrength() const { return mBounceStrength; }
    float bounceFriction() const { return mBounceFriction; }
    float bounceMinSpeed() const { return mBounceMinSpeed; }
    float bounceMaxSpeed() const { return mBounceMaxSpeed; }
    float bounceCooldown() const { return mBounceCooldown; }

private:
    CollisionConfig();

    bool mBounceEnabled = false;
    float mBounceStrength = 0.0f;
    float mBounceFriction = 0.5f;
    float mBounceMinSpeed = 7.0f;
    float mBounceMaxSpeed = 45.0f;
    float mBounceCooldown = 0.05f;

    std::string mPath;
    std::filesystem::file_time_type mLastWrite{};
    std::chrono::steady_clock::time_point mLastCheck{};
};
