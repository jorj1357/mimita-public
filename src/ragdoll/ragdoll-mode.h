#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct World;
class Camera;
struct Player;
struct InputState;

struct RagdollModePart {
    std::string name;
    int configIndex = -1;
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float mass = 1.0f;
    float capsuleRadius = 0.15f;
    float capsuleHalfHeight = 0.15f;
    glm::vec3 restOffset{0.0f};
    int parentIndex = -1;
    glm::vec3 parentAttachmentOffset{0.0f};
    float coneLimitDeg = 90.0f;
    float restLength = 0.0f;
};

struct RagdollGrabState {
    bool active = false;
    bool wasActive = false;
    glm::vec3 grabPoint{0.0f};
    glm::vec3 grabNormal{0.0f};
    glm::vec3 handPosition{0.0f};
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

    const std::vector<RagdollModePart>& parts() const { return mParts; }
    const RagdollGrabState& leftGrab() const { return mLeftGrab; }
    const RagdollGrabState& rightGrab() const { return mRightGrab; }

private:
    RagdollModeSystem() = default;

    void initParts(const Player& player);
    void solveConstraints(float dt);
    void processGrab(const InputState& input, const Camera& camera, const World& world);
    void processExtend(const InputState& input, const Camera& camera);
    void worldCollision(const World& world);
    void syncToPlayer(Player& player);

    bool mActive = false;
    std::vector<RagdollModePart> mParts;
    RagdollGrabState mLeftGrab;
    RagdollGrabState mRightGrab;
    glm::vec3 mTorsoPosition{0.0f};
    glm::quat mTorsoRotation{1.0f, 0.0f, 0.0f, 0.0f};
    int mTorsoIndex = -1;
    int mHeadIndex = -1;
    int mLeftArmIndex = -1;
    int mRightArmIndex = -1;
    int mLeftLegIndex = -1;
    int mRightLegIndex = -1;
    float mActivationTime = 0.0f;
};
