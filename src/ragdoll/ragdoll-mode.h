#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
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
    // Entity-to-entity constraint: index of the grabbed limb within the same
    // body (-1 = static world anchor). Cross-actor targets arrive as a
    // replicated moving grabPoint instead.
    int targetPart = -1;
    glm::vec3 targetLocalAnchor{0.0f};
    float strength = 1.0f;
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

    // Owner actor id used to bind the alive ragdoll into the entity registry.
    std::uint32_t ownerActorId() const { return mOwnerActorId; }
    void setOwnerActorId(std::uint32_t owner) { mOwnerActorId = owner; }

    // Entity-to-entity grab: pin a hand to another limb of this body. Returns
    // false when inactive or the indices are invalid. releaseGrab clears it.
    bool grabLimb(bool left, int targetLimbIndex, float strength = 1.0f);
    void releaseGrab(bool left);
    int grabTargetPart(bool left) const;

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
    // Full alive body for the entity/component projection.
    const RagdollBody& aliveBody() const { return mAlive; }

    // ── Corpse ragdolls ─────────────────────────────────────────────
    // Spawn a physically simulated corpse for a dead actor (player or NPC).
    // The corpse owns a cloned Player body and is driven by the same solver as
    // the alive ragdoll. Client-side now; the event is shaped for the server.
    void spawnCorpse(const Player& victim, const glm::vec3& deathImpulse,
                     const std::string& actorId, uint32_t ownerId = 0,
                     uint32_t deathTick = 0, uint32_t deathEventId = 0);
    void updateCorpses(float dt, const World& world);
    void renderCorpses(const Camera& camera) const;
    void removeCorpsesForOwner(uint32_t ownerId);
    void clearCorpses();
    std::size_t corpseCount() const { return mCorpses.size(); }
    // Deterministic seed of the most recently spawned corpse (0 if none).
    std::uint64_t lastCorpseSeed() const { return mLastCorpseSeed; }

    // Lightweight identity of the most recent corpse, for replication. The
    // serial increments on every spawn so the network layer can detect one.
    struct CorpseSpawnInfo {
        std::uint32_t ownerId = 0;
        std::uint32_t deathTick = 0;
        std::uint32_t deathEventId = 0;
        glm::vec3 impulse{0.0f};
        std::string actorId;
    };
    std::uint64_t corpseSerial() const { return mCorpseSerial; }
    const CorpseSpawnInfo& lastCorpseInfo() const { return mLastCorpseInfo; }

    // Records a peer's death identity so this client derives the same corpse
    // when it presents that remote death. Returns false if already present.
    bool noteNetworkDeath(std::uint32_t ownerId, std::uint32_t deathTick,
                          std::uint32_t deathEventId);
    bool consumeNetworkDeath(std::uint32_t ownerId, std::uint32_t& deathTick,
                             std::uint32_t& deathEventId);

private:
    RagdollModeSystem() = default;

    struct RagdollCorpse {
        RagdollBody body;
        Player actor;
        uint32_t ownerId = 0;
        std::string actorId;
        // Deterministic spawn identity: hash(worldSeed, owner, deathTick,
        // deathEventId). Same death on every client yields the same corpse.
        std::uint64_t seed = 0;
        std::uint32_t deathTick = 0;
        std::uint32_t deathEventId = 0;
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
    void processGrab(const InputState& input, const Camera& camera, const World& world, RagdollBody& b);
    void processExtend(const InputState& input, const Camera& camera, float dt, RagdollBody& b);
    void syncToPlayer(Player& player, RagdollBody& b);

    // Physics-only step shared by corpses (no input, no motors).
    void sprayCorpseBlood(RagdollCorpse& corpse, float dt);

    std::uint32_t mOwnerActorId = 1;
    bool mActive = false;
    RagdollBody mAlive;
    std::vector<RagdollCorpse> mCorpses;
    glm::vec3 mCameraSmoothPos{0.0f};
    bool mCameraSmoothInit = false;
    uint64_t mAppliedConfigGeneration = 0;
    uint32_t mNextCorpseSerial = 0;
    std::uint64_t mLastCorpseSeed = 0;
    std::uint64_t mCorpseSerial = 0;
    CorpseSpawnInfo mLastCorpseInfo;
    // owner -> (deathTick, deathEventId) learned from peers.
    std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> mNetworkDeaths;
};
