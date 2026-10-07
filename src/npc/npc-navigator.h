// 09 10 2026
/* purpose
* Per-NPC navigation layer: turns an abstract NpcGoal into a cached local route
* and returns a steering direction plus traversal requirements.
* Uses a bounded rolling-horizon ground grid around the actor (the maps are far
* too large for a global graph in this slice) and repaths on a timer, on
* significant goal movement, or when stuck.
* Does NOT decide combat, pick goals, or apply physics/input. Movement execution
* stays in npc.cpp through the shared kernel.
* Does NOT own world collision or movement configuration.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "npc/npc-goal.h"
#include "npc/npc-surface.h"

struct World;
class Npc;
struct MovementConfig;
struct NpcMovementPolicy;
struct NpcNavigationSettings;

// Replanning + movement-commitment tuning resolved from the active behavior
// profile (config/behavior-profiles.json). The navigator owns the algorithm;
// the profile owns the values. When a caller has no profile it may pass a
// default-constructed struct, which reproduces the compiled compatibility
// defaults (repath 0.9 s, goal threshold 2.5 m).
struct MovementCommitmentSettings
{
    float repathIntervalSeconds = 0.9f;      // minimum route-rebuild interval
    float goalMoveThresholdMeters = 2.5f;    // rebuild when goal moved beyond
    bool enabled = true;
    float directionCommitSeconds = 10.0f;
    float progressCheckSeconds = 1.0f;
    float minimumProgressMeters = 0.5f;
    float candidateDistanceMeters = 20.0f;
    bool allowReverse = false;
    bool avoidRecentPath = true;
    float recentPathAvoidRadius = 5.0f;
    bool visibleEnemyAllowsCombatMovement = true;
    float forwardBias = 2.0f;
    float targetProgressBias = 4.0f;
    float openDistanceBias = 6.0f;
    float reversePenalty = 8.0f;
    // Persistent exploration travel target (Explore goal). The target is held
    // until reached or until the actor has stalled for `travelTargetHoldSeconds`
    // without making `travelTargetMinProgressMeters` of progress. It is NOT
    // retargeted on a heading reversal (that flipped the goal and oscillated).
    float travelTargetDistanceMeters = 60.0f;
    float travelTargetReachedMeters = 4.0f;
    float travelTargetHoldSeconds = 12.0f;
    float travelTargetMinProgressMeters = 6.0f;
    // Long-range routing toggle. When false the navigator uses only the local
    // rolling planner (previous behavior).
    bool useNavGraph = true;

    // Local-area trap escape. If the actor stays within areaEscapeRadiusMeters
    // of an anchor for areaEscapeSeconds, it commits to a single breakout
    // direction for areaEscapeHoldSeconds, ignoring wall-avoid/backtrack
    // flip-flop, so it maximizes net movement instead of pacing in one spot.
    float areaEscapeRadiusMeters = 5.0f;
    float areaEscapeSeconds = 4.0f;
    float areaEscapeHoldSeconds = 2.0f;
};

// Result of one commitment evaluation. Flags are true only on a decision edge,
// so callers can emit one bounded event per change instead of per tick.
struct NpcCommitmentUpdate
{
    bool created = false;
    bool replaced = false;
    bool blocked = false;
    bool progressFailed = false;
    float progressDistance = 0.0f;
    glm::vec3 previousDirection{0.0f};
    glm::vec3 direction{0.0f};
};

struct NpcNavResult
{
    glm::vec3 dir{0.0f};        // planar steering direction, zero if none
    glm::vec3 waypoint{0.0f};   // current navigation target
    glm::vec3 destination{0.0f};// goal-resolved destination point
    float heightDelta = 0.0f;   // waypoint.z - actor.z
    float distance = 0.0f;      // planar distance to the waypoint
    bool hasPath = false;       // following a cached multi-node route
    bool detour = false;        // route deviates from the direct line to dest
    bool hasGap = false;        // current segment crosses a gap (needs dash)
    bool blocked = false;       // direct route is blocked with no cached route
    bool valid = false;         // a movement target exists
    int pathNodes = 0;          // waypoints remaining on the cached route
    NavCapability capability = NavCapability::Walk;  // current segment kind
    // Replanning diagnostics (change-edge only; not per tick).
    bool planCreated = false;   // a fresh route was built this update
    bool planFailed = false;    // a rebuild was attempted and found no route
    bool replan = false;        // the rebuild was not the first route
    const char* replanReason = nullptr; // static literal: initial/finished/...
    int pathNodeCount = 0;      // nodes in the route after a successful plan
    bool travelTargetChanged = false;   // a new persistent explore target was set
    glm::vec3 travelTargetOut{0.0f};
    // Compare-only Recast/Detour evidence. It never changes the authoritative
    // custom route in the initial migration phase.
    bool recastCompareAttempted = false;
    bool recastCompareAvailable = false;
    bool recastCompareSuccess = false;
    std::uint64_t recastNavmeshVersion = 0;
    int recastPolygonCount = 0;
    float recastPathLength = 0.0f;
    double recastQueryMilliseconds = 0.0;
    bool recastStartPolyFound = false;
    bool recastDestPolyFound = false;
    std::uint64_t recastStartPolyRef = 0;
    std::uint64_t recastDestPolyRef = 0;
    glm::vec3 recastNearestStart{0.0f};
    glm::vec3 recastNearestDest{0.0f};
    float recastStartProjectionDistance = 0.0f;
    float recastDestProjectionDistance = 0.0f;
    // True when the Recast/Detour route supplied the authoritative path this
    // update. False means the custom planner owned it (compare mode or a
    // bounded Recast fallback).
    bool recastAuthoritative = false;
    std::string recastFailure;
};

// Maximum vertical rise the actor can clear with a jump, from its movement
// config (v^2 / 2g). Shared by navigation and the traversal layer.
float npcMaxJumpHeight(const MovementConfig& movement);

struct NpcNavigator
{
    NpcGoal goal;
    std::vector<glm::vec3> path;
    std::vector<uint8_t> pathGap;   // per-path-point: segment into it crosses a gap
    std::vector<uint8_t> pathCapability; // per-path-point: NetCapability of segment
    int pathIndex = 0;
    float repathTimer = 0.0f;
    glm::vec3 lastGoal{0.0f};
    bool hasLastGoal = false;

    // Persistent far exploration target for NpcGoalKind::Explore. Held across
    // ticks so a valid route is not invalidated by body-relative recomputation.
    glm::vec3 travelTarget{0.0f};
    bool hasTravelTarget = false;

    // Navmesh version the cached route was planned against. When the Recast
    // backend publishes a new version, the cached corridor is stale and must be
    // replanned rather than followed through changed geometry.
    std::uint64_t recastRouteVersion = 0;

    // Local-area trap escape state (see MovementCommitmentSettings).
    glm::vec3 areaAnchor{0.0f};
    float areaTimer = 0.0f;
    bool areaEscapeActive = false;
    float areaEscapeTimeRemaining = 0.0f;
    glm::vec3 areaEscapeDirection{0.0f};
    bool areaEscapeStartedThisUpdate = false;
    bool travelTargetChangedThisUpdate = false;
    float travelTargetTimer = 0.0f;
    float travelTargetStartDistance = 0.0f;

    // Short local recovery used when the current route points into a wall.
    bool backtrackActive = false;
    glm::vec3 backtrackDirection{0.0f};
    float backtrackRemaining = 0.0f;
    float backtrackTimeRemaining = 0.0f;

    // A wall/stuck correction is temporary steering, not a replacement for
    // the long-range commitment. While active, updateCommitment preserves the
    // original goal direction and npc.cpp resumes it as soon as the correction
    // is clear or its short hold expires.
    bool localCorrectionActive = false;
    glm::vec3 localCorrectionDirection{0.0f};
    float localCorrectionTimeRemaining = 0.0f;

    // ── Movement commitment ──────────────────────────────────────────────
    // The single forward/pursuit direction owner (replaces the old
    // state-machine patrolDir). Held until it is blocked, progress fails, it
    // times out, or a genuinely visible enemy takes steering via combat.
    bool commitmentActive = false;
    glm::vec3 committedDirection{0.0f};
    float commitmentTimeRemaining = 0.0f;
    float progressTimer = 0.0f;
    float lastProgressDistance = 0.0f;
    glm::vec3 commitmentStartPosition{0.0f};
    bool commitmentProgressFailed = false;  // set by updateCommitment this tick
    bool commitmentBlocked = false;         // committed direction is walled off

    // Recent-path memory (visited + blocked world points) owned here so the
    // commitment scorer and the no-progress watchdog share one source.
    struct SearchPoint { glm::vec3 pos{0.0f}; float time = 0.0f; };
    static constexpr int kRecentMax = 32;
    SearchPoint recentVisited[kRecentMax];
    int recentVisitedCount = 0;
    int recentVisitedHead = 0;
    SearchPoint recentBlocked[kRecentMax];
    int recentBlockedCount = 0;
    int recentBlockedHead = 0;

    // Diagnostics.
    uint32_t planCount = 0;
    uint32_t repathCount = 0;

    // Called from the NPC update after sensing and goal selection. `settings`
    // and `policy` are the actor-preset navigation/movement configuration
    // (nullptr = shared navigator defaults / legacy brain). `commitment`
    // carries the behavior-profile replan/commitment tuning.
    NpcNavResult update(Npc& npc, const NpcGoal& goal, const World& world,
                        const MovementConfig* movement, float dt,
                        const NpcNavigationSettings* settings = nullptr,
                        const NpcMovementPolicy* policy = nullptr,
                        const MovementCommitmentSettings* commitment = nullptr);

    // Evaluate the committed direction for this tick. `forwardDir` is the last
    // committed direction (or current facing); `targetDir` points at the
    // pursuit target (zero when none); `targetVisible` is genuine perception
    // visibility (alive + FOV + range + LOS), never radar/memory. Returns the
    // decision edge so the caller can emit bounded events.
    NpcCommitmentUpdate updateCommitment(Npc& npc, const glm::vec3& forwardDir,
                                         const glm::vec3& targetDir,
                                         bool targetVisible,
                                         const World& world,
                                         const std::vector<int>& candidates,
                                         const MovementCommitmentSettings& settings,
                                         float dt, float now, float memorySeconds);

    // Score and pick the open direction that best continues forward, closes on
    // the target, and avoids reversing or retracing. Never returns an
    // immediately blocked direction; zero when boxed in.
    glm::vec3 chooseBestOpenDirection(const Npc& npc, const glm::vec3& forwardDir,
                                      const glm::vec3& targetDir,
                                      const World& world,
                                      const std::vector<int>& candidates,
                                      const MovementCommitmentSettings& settings,
                                      float now, float memorySeconds) const;

    void pushVisited(const glm::vec3& pos, float now);
    void pushBlocked(const glm::vec3& pos, float now);
    float nearestRecentDistance(const glm::vec3& point, float now,
                                float memorySeconds) const;
    // Drop the current commitment so the next evaluation must pick a new one.
    void forceRecommit() { commitmentActive = false; commitmentTimeRemaining = 0.0f; }

    // Resolve the persistent far travel target for an Explore goal. `hintPoint`
    // only supplies the desired heading; the returned target is held until it
    // is reached or the desired heading reverses.
    glm::vec3 resolveExploreTarget(const Npc& npc, const World& world,
                                   const glm::vec3& hintPoint,
                                   const MovementCommitmentSettings& settings,
                                   float dt);

    // Detect a local-area trap and, past the time limit, return a committed
    // breakout direction. Zero when not escaping. While active, callers should
    // force steering to this direction (unless a visible enemy owns movement).
    glm::vec3 updateAreaEscape(const Npc& npc, const World& world,
                               const std::vector<int>& candidates,
                               const MovementCommitmentSettings& settings,
                               float dt, float now, float memorySeconds,
                               bool targetVisible);

    void reset();
    // Force a replan on the next update (e.g. target teleported).
    void requestRepath() { repathTimer = 0.0f; hasLastGoal = false; }

    void startBacktrack(const glm::vec3& blockedDirection,
                        float distance, float duration);
    void startLocalCorrection(const glm::vec3& direction,
                              float duration = 1.0f);
    void clearLocalCorrection();
};
