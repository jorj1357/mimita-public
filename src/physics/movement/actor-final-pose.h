// 2026-10-31
/* purpose
* Contain an actor's final animated body and held-weapon meshes inside static
* world geometry by translating only the actor root.
* Owns the last-clear pose fallback and bounded safe-point recovery.
* Does NOT rewrite limb transforms, animation samples, damage, or networking.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Player;
struct World;

struct ActorFinalPoseContact
{
    const char* actorPart = "";
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec3 surfaceNormal{0.0f};
    float penetration = 0.0f;
    int worldTriangle = -1;
    uint32_t solidId = 0;
};

struct ActorFinalPoseState
{
    bool hasClearPose = false;
    glm::vec3 clearPosition{0.0f};
    float clearYaw = 0.0f;
    bool clearOnGround = false;
    std::uint64_t clearTick = 0;
};

struct ActorFinalPoseResult
{
    bool clear = true;
    bool corrected = false;
    bool restored = false;
    bool usedSafePoint = false;
    bool failed = false;
    bool budgetExceeded = false;
    bool rootInsideClosedSolid = false;
    const char* stage = "final";
    int partCount = 0;
    int extraPartCount = 0;
    std::string skipReason;
    int iterations = 0;
    float initialPenetration = 0.0f;
    float finalPenetration = 0.0f;
    glm::vec3 correction{0.0f};
    std::vector<ActorFinalPoseContact> contacts;
    std::string resetReason;
};

bool containActorFinalPose(Player& actor,
                           const World& world,
                           ActorFinalPoseState& state,
                           ActorFinalPoseResult& result,
                           float standoff = 0.02f,
                           const char* stage = "final");

bool actorFinalPoseSelfTest(std::string* outSummary = nullptr);

// True when the root is inside a closed collision solid volume, not merely
// inside the map's enclosing room shell. Uses the nearest floor triangle's
// solid id and tests parity against that solid only.
bool actorRootInsideClosedSolid(const World& world, const glm::vec3& point);
