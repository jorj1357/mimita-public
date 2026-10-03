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

    // When true, the actor narrowphase prunes candidate pairs with a per-solve
    // AABB tree instead of scanning the candidate list. Hot-reloadable.
    bool actorCollisionAccelerated() const { return mActorCollisionAccelerated; }
    // When true (debug only), the actor narrowphase runs the linear scan and the
    // AABB-tree path on the same input and logs a structured diff. Hot-reloadable.
    bool actorCollisionComparison() const { return mActorCollisionComparison; }

    // Shared actor/world contact margin. Hot-reloadable from collision.json.
    float collisionSkin() const { return mCollisionSkin; }
    // Static triangle touches below this depth are treated as seam/edge
    // contact, not as a penetrating blocking surface.
    float edgeTouchTolerance() const { return mEdgeTouchTolerance; }

    // Anti-tunneling: the local player's fixed tick is split into enough
    // collision sub-steps that each sub-step moves at most this many world
    // units. Keeps the swept actor narrowphase bounded at extreme speeds.
    // 0 disables the extra sub-stepping. Hot-reloadable from collision.json.
    float maxSubStepDistance() const { return mMaxSubStepDistance; }
    // Hard ceiling on the speed-derived collision sub-step count so a garbage
    // velocity cannot explode the per-tick solve.
    int maxSubSteps() const { return mMaxSubSteps; }

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
    bool mActorCollisionAccelerated = true;
    bool mActorCollisionComparison = false;
    float mCollisionSkin = 0.05f;
    float mEdgeTouchTolerance = 0.002f;
    float mMaxSubStepDistance = 0.35f;
    int mMaxSubSteps = 128;

    std::string mPath;
    std::filesystem::file_time_type mLastWrite{};
    std::chrono::steady_clock::time_point mLastCheck{};
};
