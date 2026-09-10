#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics/physical-body.h"
#include "entities/player.h"

struct World;
class Camera;
struct InputState;

struct RagdollModePart {
    std::string name;
    int configIndex = -1;
    RigidBody body;

    int parentIndex = -1;
    glm::vec3 parentLocalAnchor{0.0f};
    glm::vec3 childLocalAnchor{0.0f};
    float restLength = 0.0f;
    // Max separation of the anchors (arm stretch). 0 = rigid joint.
    float maxStretch = 0.0f;

    // Skeleton mapping: body-part node and its nearest skeleton ancestor that
    // is also a body part (-1 = the model root).
    int nodeIndex = -1;
    int skeletonParentPart = -1;
    // Transform from the canonical physics body frame to the mesh node frame
    // (carries the model's baked Z-up rotation so the body frame stays clean).
    glm::mat4 meshLocal{1.0f};

    // Per-axis angular limits relative to the bind orientation.
    glm::quat bindRelativeRotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool hasRotationLimits = false;
    glm::vec3 rotMinDeg{-180.0f, -180.0f, -180.0f};
    glm::vec3 rotMaxDeg{ 180.0f,  180.0f,  180.0f};

    // Offset from the configured aim axes to lookRotation's local (+Y forward,
    // +Z up) convention, so the chosen axis points along the camera forward.
    glm::quat aimOffset{1.0f, 0.0f, 0.0f, 0.0f};

    // Render-only smoothed transform (body_smoothing).
    glm::vec3 renderPosition{0.0f};
    glm::quat renderOrientation{1.0f, 0.0f, 0.0f, 0.0f};
    bool renderSmoothed = false;
};

struct RagdollGrabState {
    bool active = false;
    bool wasActive = false;
    glm::vec3 grabPoint{0.0f};
    glm::vec3 grabNormal{0.0f};
    glm::vec3 handPosition{0.0f};
    glm::vec3 handLocalAnchor{0.0f};
    int partIndex = -1;
};

// Per-body ragdoll state. Shared by the local player's alive ragdoll mode and
// by every simulated corpse, so the two use one owner and one solver.
struct RagdollBody {
    std::vector<RagdollModePart> parts;
    std::vector<int> rootAncestorNodes;
    glm::vec3 rootOffsetLocal{0.0f};
    glm::vec3 rootWorldPosition{0.0f};
    glm::vec3 torsoPosition{0.0f};
    int torsoIndex = -1;
    int headIndex = -1;
    int leftArmIndex = -1;
    int rightArmIndex = -1;
    int leftLegIndex = -1;
    int rightLegIndex = -1;
    RagdollGrabState leftGrab;
    RagdollGrabState rightGrab;
    bool leftArmExtending = false;
    bool rightArmExtending = false;
    float activationTime = 0.0f;
};

class RagdollModeSystem {
public:
    static RagdollModeSystem& instance();

    void activate(Player& player);
    void deactivate(Player& player);
    bool isActive() const { return mActive; }

    void update(float dt, const World& world, Player& player,
                const InputState& input, const Camera& camera);

    void render(const Camera& camera) const;

    glm::vec3 getHeadPosition() const;
    glm::mat4 getHeadTransform() const;
    glm::vec3 getTorsoPosition() const { return mAlive.torsoPosition; }

    // Camera position with configurable smoothing. Call once per render frame.
    // smooth_factor 0 = glued to head (instant), 1 = smooth, 10 = very slow.
    glm::vec3 computeCameraPosition(float dt);

    const std::vector<RagdollModePart>& parts() const { return mAlive.parts; }
    const RagdollGrabState& leftGrab() const { return mAlive.leftGrab; }
    const RagdollGrabState& rightGrab() const { return mAlive.rightGrab; }

    // ── Corpse ragdolls ─────────────────────────────────────────────
    // Spawn a physically simulated corpse for a dead actor (player or NPC).
    // The corpse owns a cloned Player body and is driven by the same solver as
    // the alive ragdoll. Client-side now; the event is shaped for the server.
    void spawnCorpse(const Player& victim, const glm::vec3& deathImpulse,
                     const std::string& actorId, uint32_t ownerId = 0);
    void updateCorpses(float dt, const World& world);
    void renderCorpses(const Camera& camera) const;
    void removeCorpsesForOwner(uint32_t ownerId);
    void clearCorpses();
    std::size_t corpseCount() const { return mCorpses.size(); }

private:
    RagdollModeSystem() = default;

    struct RagdollCorpse {
        RagdollBody body;
        Player actor;
        uint32_t ownerId = 0;
        std::string actorId;
        float age = 0.0f;
        float lifetime = 20.0f;
        float fade = 0.0f;
        float bloodTimer = 0.0f;
        bool bloodInit = false;
        glm::vec3 lastBloodPos{0.0f};
    };

    // Shared body construction / writeback, used by alive mode and corpses.
    void initParts(const Player& player, RagdollBody& b);
    void reinitPreservingState(Player& player, RagdollBody& b);
    void applyControls(float dt, const InputState& input, const Camera& camera, RagdollBody& b);
    void solveJoints(int iterations, bool positionPass, RagdollBody& b);
    void solveRotationLimits(float betaOverride, RagdollBody& b);
    void solveGrabs(int iterations, RagdollBody& b);
    void processGrab(const InputState& input, const Camera& camera, const World& world, RagdollBody& b);
    void processExtend(const InputState& input, const Camera& camera, float dt, RagdollBody& b);
    void selfCollision(RagdollBody& b);
    void syncToPlayer(Player& player, RagdollBody& b);

    // Physics-only step shared by corpses (no input, no motors).
    void stepBody(RagdollBody& b, const World& world, float dt);
    void sprayCorpseBlood(RagdollCorpse& corpse, float dt);

    bool mActive = false;
    RagdollBody mAlive;
    std::vector<RagdollCorpse> mCorpses;
    glm::vec3 mCameraSmoothPos{0.0f};
    bool mCameraSmoothInit = false;
    uint64_t mAppliedConfigGeneration = 0;
    uint32_t mNextCorpseSerial = 0;
};
