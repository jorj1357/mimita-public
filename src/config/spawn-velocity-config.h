// 2026-09-08 12:01 EST
/* purpose
* hot-reloadable JSON config for spawn velocity impulse on respawn
* controls direction mode, speed, and vertical component of the impulse
* applied to both players and NPCs on respawn
* does NOT modify knockback, dash, jump, or other movement abilities
* does NOT change death animation or ragdoll behavior
*/

#pragma once

#include <filesystem>
#include <string>
#include <glm/glm.hpp>

struct SpawnVelocityConfigData {
    bool enabled = false;
    std::string mode = "random";
    float speed = 40.0f;
    glm::vec3 direction{1.0f, 0.0f, 0.0f};
    bool applyVertical = false;
    glm::vec2 randomRange{-180.0f, 180.0f};
};

class SpawnVelocityConfig {
public:
    static SpawnVelocityConfig& instance();

    bool load(const std::string& path = "config/spawnvelocity.json");
    bool pollReload();

    bool enabled() const { return mData.enabled; }
    const std::string& mode() const { return mData.mode; }
    float speed() const { return mData.speed; }
    const SpawnVelocityConfigData& data() const { return mData; }

    glm::vec3 computeSpawnImpulse(float playerYaw) const;

private:
    SpawnVelocityConfig() = default;

    SpawnVelocityConfigData mData;
    std::string mPath = "config/spawnvelocity.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
