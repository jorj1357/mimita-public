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
    // Minimum outward push for a valid body/weapon contact at very low speed,
    // so a touching limb/tool still nudges the whole body.
    float bounceMinPush() const { return mBounceMinPush; }

    // When true, limb collision uses each body part's real mesh triangles
    // (triangle-vs-triangle against the world) instead of one AABB sphere per
    // part. Toggle off to fall back to the sphere approximation.
    bool bodyMeshCollision() const { return mBodyMeshCollision; }

    // When true, the local player's GLB collision uses the single
    // actor-triangle solver instead of the legacy capsule/body/emergency
    // pipeline. Hot-reloadable so it can be enabled for testing and reverted
    // without a rebuild.
    bool actorTriangleSolver() const { return mActorTriangleSolver; }

private:
    CollisionConfig();

    bool mBounceEnabled = false;
    float mBounceStrength = 0.0f;
    float mBounceFriction = 0.5f;
    float mBounceMinSpeed = 7.0f;
    float mBounceMaxSpeed = 45.0f;
    float mBounceCooldown = 0.05f;
    float mBounceMinPush = 0.1f;
    bool mBodyMeshCollision = true;
    bool mActorTriangleSolver = false;

    std::string mPath;
    std::filesystem::file_time_type mLastWrite{};
    std::chrono::steady_clock::time_point mLastCheck{};
};
