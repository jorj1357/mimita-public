// 2026-10-01
/* purpose
* Own the destruction budgets and fracture tuning loaded from
* config/destructible-world.json. Mirrors the MaterialConfig singleton pattern
* (load + pollReload; malformed JSON keeps the previous valid values).
* Gameplay reads these values; it does NOT hardcode per-cut budgets elsewhere.
* Does NOT own cut sizing, geometry, damage authority, or rendering.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "impact/destructible-geometry.h"

namespace MimitaImpact {

class DestructibleWorldConfig
{
public:
    static DestructibleWorldConfig& instance();

    bool load(const std::string& path = "config/destructible-world.json");
    bool pollReload();
    void reloadNow();

    // Destruction work is aggregated: cuts are collected and applied in one
    // batch, and an entity is only reworked when this many fixed ticks have
    // passed since its last batch (or its queue reaches cutBatchMax first).
    uint32_t cutBatchIntervalTicks() const { return mCutBatchIntervalTicks; }
    uint32_t cutBatchMax() const { return mCutBatchMax; }
    float cutBudgetMsPerTick() const { return mCutBudgetMsPerTick; }

    // Safety cap on generated triangles per entity.
    size_t maxTrianglesPerEntity() const { return mMaxTrianglesPerEntity; }

    // > 0 combines near-coplanar triangles after each cut (bounded growth).
    float meshSimplifyTolerance() const { return mMeshSimplifyTolerance; }

    // Fracture trigger + budget.
    const FractureTuning& fractureTuning() const { return mFractureTuning; }

    // Physical-object motion tuning (crates/objects; players keep PHYS.gravity).
    float objectGravity() const { return mObjectGravity; }
    float objectLinearDamping() const { return mObjectLinearDamping; }
    float objectAngularDamping() const { return mObjectAngularDamping; }
    float supportedFrictionRetain() const { return mSupportedFrictionRetain; }

    // Settling: an object that stays within sleepMoveThresholdMeters of its
    // anchor for sleepRequiredTicks fixed ticks freezes there until disturbed.
    float sleepMoveThresholdMeters() const { return mSleepMoveThresholdMeters; }
    uint32_t sleepRequiredTicks() const { return mSleepRequiredTicks; }

    // Default object rigid-body material response (dynamic objects).
    float objectRestitution() const { return mObjectRestitution; }
    float objectFriction() const { return mObjectFriction; }
    // Below this relative normal speed an object does not bounce (it projects),
    // so it settles instead of jittering forever. `objectMaxSpeed` caps the
    // linear speed so a physics glitch cannot fling an object across the map.
    float objectMinBounceSpeed() const { return mObjectMinBounceSpeed; }
    float objectMaxSpeed() const { return mObjectMaxSpeed; }
    // Rounded-shell radius for the deep-depenetration recovery pass: a falling
    // body with no contacts uses this to find and push out of geometry it has
    // already sunk into (larger = catches deeper penetration).
    float recoveryFeatureRadius() const { return mRecoveryFeatureRadius; }

    // Fragment lifecycle. A detached piece smaller than minFragmentVolume is
    // removed, and any fragment older than fragmentLifetimeSeconds is removed;
    // maxTotalFragments caps how many can exist at once.
    float minFragmentVolume() const { return mMinFragmentVolume; }
    float fragmentLifetimeSeconds() const { return mFragmentLifetimeSeconds; }
    uint32_t maxTotalFragments() const { return mMaxTotalFragments; }
    // A fragment whose largest AABB dimension is below these is deleted:
    // instantly below `instant`, or below `delete` once it has not been
    // interacted with (impulse) for `idleSeconds`. A long thin shard has a large
    // max dimension and is kept; a small compact chunk is removed.
    float fragmentInstantDeleteMaxDimMeters() const
    {
        return mFragmentInstantDeleteMaxDimMeters;
    }
    float fragmentDeleteMaxDimMeters() const { return mFragmentDeleteMaxDimMeters; }
    float fragmentIdleDeleteSeconds() const { return mFragmentIdleDeleteSeconds; }

    uint32_t revision() const { return mRevision; }

private:
    DestructibleWorldConfig() = default;

    uint32_t mCutBatchIntervalTicks = 8;
    uint32_t mCutBatchMax = 64;
    float mCutBudgetMsPerTick = 2.0f;
    size_t mMaxTrianglesPerEntity = 4096;
    // Off by default: Simplify applied per-batch makes the final mesh depend on
    // how cuts were batched, so the server and a client that applies cuts one
    // at a time would diverge. The coarse cutter already bounds growth.
    float mMeshSimplifyTolerance = 0.0f;
    FractureTuning mFractureTuning;
    // Built-in defaults preserve the long-standing crate feel so the automated
    // self-tests are stable; gameplay loads the tuned values from
    // config/destructible-world.json (physics section) on the first tick.
    float mObjectGravity = -58.0f;
    float mObjectLinearDamping = 0.15f;
    float mObjectAngularDamping = 2.0f;
    float mSupportedFrictionRetain = 0.65f;
    float mSleepMoveThresholdMeters = 0.1f;
    uint32_t mSleepRequiredTicks = 20;
    float mObjectRestitution = 0.1f;
    float mObjectFriction = 0.6f;
    float mObjectMinBounceSpeed = 1.0f;
    float mObjectMaxSpeed = 60.0f;
    float mRecoveryFeatureRadius = 0.5f;
    float mMinFragmentVolume = 0.001f;
    float mFragmentLifetimeSeconds = 12.0f;
    uint32_t mMaxTotalFragments = 64;
    float mFragmentInstantDeleteMaxDimMeters = 0.15f;
    float mFragmentDeleteMaxDimMeters = 0.3f;
    float mFragmentIdleDeleteSeconds = 8.0f;
    std::string mPath = "config/destructible-world.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mHasWriteTime = false;
    uint32_t mRevision = 0;
};

} // namespace MimitaImpact
