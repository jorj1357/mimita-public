#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics/physical-body.h"

struct World;
class Camera;
struct Player;
struct InputState;

struct RagdollModePart {
    std::string name;
    int configIndex = -1;
    RigidBody body;

    int parentIndex = -1;
    glm::vec3 parentLocalAnchor{0.0f};
    glm::vec3 childLocalAnchor{0.0f};
    glm::vec3 restDirectionLocal{0.0f};
    float coneLimitDeg = 90.0f;
    float restLength = 0.0f;

    // Per-axis angular limits relative to the bind orientation.
    glm::quat bindRelativeRotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool hasRotationLimits = false;
    glm::vec3 rotMinDeg{-180.0f, -180.0f, -180.0f};
    glm::vec3 rotMaxDeg{ 180.0f,  180.0f,  180.0f};
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
    glm::vec3 getTorsoPosition() const { return mTorsoPosition; }

    // Camera position with configurable smoothing. Call once per render frame.
    // smooth_factor 0 = glued to head (instant), 1 = smooth, 10 = very slow.
    glm::vec3 computeCameraPosition(float dt);

    const std::vector<RagdollModePart>& parts() const { return mParts; }
    const RagdollGrabState& leftGrab() const { return mLeftGrab; }
    const RagdollGrabState& rightGrab() const { return mRightGrab; }

private:
    RagdollModeSystem() = default;

    void initParts(const Player& player);
    void applyControls(float dt, const InputState& input, const Camera& camera);
    void solveJoints(int iterations, bool positionPass);
    void solveConeLimits();
    void solveRotationLimits();
    void solveGrabs(int iterations);
    void processGrab(const InputState& input, const Camera& camera, const World& world);
    void processExtend(const InputState& input, const Camera& camera, float dt);
    void selfCollision();
    void syncToPlayer(Player& player);

    bool mActive = false;
    std::vector<RagdollModePart> mParts;
    RagdollGrabState mLeftGrab;
    RagdollGrabState mRightGrab;
    glm::vec3 mTorsoPosition{0.0f};
    int mTorsoIndex = -1;
    int mHeadIndex = -1;
    int mLeftArmIndex = -1;
    int mRightArmIndex = -1;
    int mLeftLegIndex = -1;
    int mRightLegIndex = -1;
    float mActivationTime = 0.0f;
    glm::vec3 mCameraSmoothPos{0.0f};
    bool mCameraSmoothInit = false;
};
