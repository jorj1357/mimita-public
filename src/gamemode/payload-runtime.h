// 2026-10-23
/* purpose
* Own the pure Payload cart rules: route distance, interaction-box overlap,
* push/contest speed, rollback, checkpoints, overtime, and healing cadence.
* The server supplies actor snapshots and applies healing/round transitions.
* Does NOT move actors, own the round clock, render, or send packets.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "gamemode/gamemode.h"
#include "gamemode/map-config.h"

namespace MimitaGamemode {

// One authoritative actor considered for the cart interaction box.
struct PayloadActorView
{
    uint32_t id = 0;
    int team = -1;
    bool alive = false;
    glm::vec3 position{0.0f};
    float radius = 0.65f;
    float height = 3.5f;
};

struct PayloadCheckpoint
{
    int routeIndex = -1;
    float distance = 0.0f;
    float radius = 2.0f;
    int waitTicks = 0;
    float timeBonusSeconds = 0.0f;
    bool final = false;
};

// One authored route point used for arrival radius and authored cart pauses.
// Checkpoints own clock/final rules; waypoints own the shared radius/wait rule
// so a non-checkpoint waypoint can also pause the cart.
struct PayloadWaypoint
{
    int routeIndex = -1;
    float distance = 0.0f;
    float radius = 2.0f;
    int waitTicks = 0;
    bool reached = false;
};

// Public cart state. The server owns the instance; the client never simulates it.
struct PayloadRuntime
{
    bool active = false;

    // Authored data copied at match start so a live map edit applies next round.
    glm::vec3 cartOrigin{0.0f};
    float cartYaw = 0.0f;
    bool cartFixedFacing = false;
    glm::vec3 cartSize{4.0f, 3.0f, 2.0f};
    glm::vec3 interactionSize{6.0f, 4.5f, 3.0f};
    glm::vec3 interactionOffset{0.0f};
    glm::vec3 interactionRotationEuler{0.0f};
    std::vector<glm::vec3> routePoints;
    std::vector<float> cumulativeDistances;
    std::vector<PayloadCheckpoint> checkpoints;
    std::vector<PayloadWaypoint> waypoints;

    // Match rules.
    int attackerTeam = 0;
    int defenderTeam = 1;
    float pushMetersPerSecondPerAttacker = 0.2f;
    int maxPushAttackers = 6;
    float unattendedRollbackDelaySeconds = 30.0f;
    float rollbackMetersPerSecond = 0.2f;
    float overtimeSeconds = 10.0f;
    int healHpPer10Ticks = 1;
    bool healDefenders = false;

    // Live cart state.
    float totalDistance = 0.0f;
    float distanceTravelled = 0.0f;
    float lastCheckpointDistance = 0.0f;
    std::vector<uint8_t> reachedCheckpoints;
    glm::vec3 cartPosition{0.0f};
    glm::vec3 previousCartPosition{0.0f};
    glm::vec3 cartDelta{0.0f};
    glm::mat4 cartTransform{1.0f};
    glm::mat4 previousCartTransform{1.0f};
    glm::quat cartOrientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::mat4 interactionTransform{1.0f};

    int overtimeTicksRemaining = 0;
    bool overtimeActive = false;
    bool finished = false;
    float unattendedSeconds = 0.0f;
    int waitTicksRemaining = 0;
    bool waiting = false;

    int attackers = 0;
    int defenders = 0;
    bool advancing = false;
    bool blocked = false;
    bool rollingBack = false;

    // Per-actor healing remainder in ticks. Keeps 1 HP / 10 ticks exact.
    std::unordered_map<uint32_t, float> healAccumulators;
};

// One fixed-tick result. Fixed arrays avoid hot-loop allocations.
struct PayloadTickResult
{
    bool cartMoved = false;
    bool checkpointReached = false;
    int checkpointRouteIndex = -1;
    float checkpointBonusSeconds = 0.0f;
    bool finished = false;
    bool overtimeStarted = false;
    bool overtimeRefreshed = false;
    bool overtimeExpired = false;
    bool checkpointEndedOvertime = false;
    bool waitStarted = false;
    int waitTicksRemaining = 0;
    int attackers = 0;
    int defenders = 0;
    float distanceMoved = 0.0f;
    uint32_t healedIds[64] = {};
    uint8_t healedCount = 0;
};

// Build the runtime from map data + mode rules. False when the map has no
// usable cart origin or fewer than two route points.
bool payloadRuntimeInit(PayloadRuntime& runtime,
                        const PayloadConfig& mapPayload,
                        const GamemodePayload& rules);

// Reset live state to the start of a round. Keeps authored data and rules.
void payloadRuntimeReset(PayloadRuntime& runtime);

// One authoritative gameplay tick. `roundEndTick` is the server's configured
// active-clock end; zero means no clock/overtime.
void payloadRuntimeTick(PayloadRuntime& runtime,
                        const PayloadActorView* actors,
                        int actorCount,
                        uint32_t tick,
                        uint32_t roundEndTick,
                        PayloadTickResult& result);

// True when the actor's collision capsule intersects the transformed box.
bool payloadActorIntersectsInteractionBox(const PayloadRuntime& runtime,
                                          const PayloadActorView& actor);

// Resolve one actor capsule against the physical cart box. Supports standing
// on the top surface (and carries by the cart's current tick delta) and pushes
// the actor out of the sides. Server-authoritative player/NPC movement calls
// this after world collision.
bool payloadResolveActorAgainstCart(const PayloadRuntime& runtime,
                                    const glm::vec3& actorPos,
                                    const glm::vec3& actorVel,
                                    bool onGround,
                                    float radius,
                                    float height,
                                    glm::vec3& outPos,
                                    glm::vec3& outVel,
                                    bool& outOnGround);

// Cart world position at a route distance (local route frame).
glm::vec3 payloadRoutePositionAtDistance(const PayloadRuntime& runtime, float distance);

// Recompute the cart transform and interaction transform from distance.
void payloadRuntimeUpdateTransforms(PayloadRuntime& runtime);

// World-independent selftest for movement, contest, rollback, checkpoints,
// overtime, healing, and overlap.
bool payloadRuntimeSelfTest(std::string& report);

} // namespace MimitaGamemode
