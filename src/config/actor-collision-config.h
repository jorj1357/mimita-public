// 2026-10-20
/* purpose
* Own config/actor-collision.json: per-actor-preset capsule dimensions and
* impact/support tuning for the shared actor-pair collision path.
* Validate the whole candidate and keep the last valid settings on error.
* Poll hot reload only from the fixed tick; never from the render loop.
* Does NOT simulate collision, mutate actors, render, or send packets.
*/
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

struct ActorCollisionPushTuning
{
    float weakHitDistanceM = 0.1f;
    float weakHitMotionSeconds = 0.1f;
    float minImpactSpeedMps = 0.5f;
    float maxImpactSpeedMps = 100.0f;
    float maxKnockbackSpeedMps = 5.0f;
    float impactSpeedCurveExponent = 1.0f;
    float impactAngleMinScale = 0.15f;
    float impactAngleCurveExponent = 1.0f;
    float supportMaxImpactSpeedMps = 6.0f;
    float supportTopToleranceM = 0.08f;
    float supportSnapMaxM = 0.35f;
};

struct ActorCollisionProfile
{
    std::string id = "default";
    float capsuleRadiusM = 0.7f;
    float capsuleHeightM = 3.6f;
    float deadCapsuleLengthM = 2.6f;
    ActorCollisionPushTuning push;
};

struct ActorCollisionData
{
    std::uint64_t revision = 0;
    float broadphaseCellSizeM = 4.0f;
    int maxCandidatePairs = 8192;
    std::unordered_map<std::string, ActorCollisionProfile> profiles;
};

class ActorCollisionConfig
{
public:
    static ActorCollisionConfig& instance();

    bool load(const std::string& path = "config/actor-collision.json");
    // Call once at the start of a fixed tick. Installs only a fully valid
    // candidate and returns true when the active settings changed.
    bool pollHotReload();

    const ActorCollisionData& data() const { return mData; }
    const ActorCollisionProfile& profileFor(const std::string& actorPresetId) const;
    std::uint64_t revision() const { return mData.revision; }

private:
    ActorCollisionConfig();

    static ActorCollisionData defaults();
    static bool validate(ActorCollisionData& candidate, std::string& error);

    ActorCollisionData mData;
    std::string mPath = "config/actor-collision.json";
    std::filesystem::file_time_type mLastWrite{};
    std::chrono::steady_clock::time_point mLastCheck{};
    bool mWatchLogged = false;
};
