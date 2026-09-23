// 07 21 2026, 16 30
/* purpose
* Declares Player runtime state, rendering data, and gameplay presentation fields.
* Groups legacy movement, combat, avatar, weapon, and collision data consumed by engine systems.
* Provides Player methods for reset, render, animation, and state synchronization.
* Does NOT implement movement physics, collision solving, networking transport, or weapon fire.
* Does NOT own shared movement formulas, packet serialization, or asset-management rules.
* Does NOT replace subsystem-owned APIs for combat, audio, replay, or UI.
*/

#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "map/map_common.h"
#include "physics/physics-types.h"
#include "gui/hud/chat-bubble.h"

#include "avatar/avatar.h"
#include "combat/weapon-types.h"
#include "physics/movement/movement-types.h"
#include "tinygltf/tiny_gltf.h"
#include "vip/vip-appearance.h"

// Clears the shared immutable player-GLB parse cache. Immutable parsed model
// data (skeleton, colliders, part meshes) is cached per resolved path so
// creating many remote-player replicas reuses parsed assets instead of
// re-reading and re-parsing the GLB. Call only when models may have changed
// on disk (e.g. asset reload); never on normal replica create/destroy.
void clearPlayerModelCache();



struct ReplayBodyPartState;

struct WeaponColliderDebugSphere {
    enum class SourceType {
        JsonSphere,
        CapsuleSample,
        GeneratedProbe
    };

    std::string name;
    glm::vec3 currentCenter;
    glm::vec3 previousCenter;
    glm::vec3 sweepDelta;  // currentCenter - previousCenter, for collision solver
    float radius = 0.0f;
    bool collidesWithWorld = true;
    SourceType sourceType = SourceType::JsonSphere;
};

struct WeaponColliderDebugCapsule {
    glm::vec3 currentStart;
    glm::vec3 currentEnd;
    glm::vec3 previousStart;
    glm::vec3 previousEnd;
    float radius = 0.0f;
    bool enabled = false;
};

struct WeaponCollisionRuntimeDebug {
    std::vector<WeaponColliderDebugSphere> spheres;
    WeaponColliderDebugCapsule capsule;                  // primary capsule (backward compat)
    std::vector<WeaponColliderDebugCapsule> capsules;    // all capsules from JSON config
    std::string weaponId;
    bool valid = false;
    bool visibleFromConfig = false;  // from weaponcollisions.json "visible" field
    bool fromJsonConfig = false; // true when JSON config drives this data
    bool capsuleMode = false;    // true = smooth capsule collision (default), false = JSON spheres
    float collisionSkin = 0.04f; // per-weapon skin from config, defaults to 0.04
};

// ---------------- Player ----------------
//
// Player is pure data + presentation.
// 
// Owns:
// - state (pos, vel, flags)
// - audio triggers
// - rendering
//
// Does NOT own:
// - gravity
// - movement
// - jumping
// - collisions
//

struct OBB {
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::mat4 orientation;
};

struct PlayerOrigin {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct MovementCapsule {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    float radius = 0.0f;
    float height = 0.0f;
};

struct PerfectPoseSkeleton {
    std::vector<TransformNode> nodes;
    std::vector<glm::mat4> restLocalTransforms;
};

struct PhysicalBodyPart {
    std::string name;
    int nodeIndex = -1;
    Collider collider;
    ProceduralPose pose;
    ProceduralPose perfectPose;
    SpringState translationSpring;
    SpringState rotationSpring;
    glm::mat4 worldTransform{1.0f};
    glm::mat4 previousWorldTransform{1.0f};
};

struct PhysicalBody {
    std::vector<PhysicalBodyPart> parts;
    std::vector<Mesh> partMeshes;
};

struct PendingPlayerModel {
    std::atomic<bool> ready{false};
    std::string path;
    // Avatar identity captured when this async request was started. A reused
    // NPC may receive a new life/avatar before the worker completes.
    std::string avatarNameAtRequest;
    std::string resolvedPath;
    std::string glbDir;
    bool loadOk = false;

    std::vector<TransformNode> nodes;
    std::vector<glm::mat4> restLocalTransforms;
    std::vector<Collider> bodyColliders;
    std::vector<BodyPart> bodyParts;
    std::vector<Mesh> bodyPartMeshes;
    PerfectPoseSkeleton perfectPose;
    PhysicalBody physicalBodyData;
    Mesh renderMeshData;

    std::vector<tinygltf::Image> images;
    int imageCount = 0;
    nlohmann::json bodypartOverrides;
};


// -------- State Groups (prevents duplicate state variables) --------
struct GroundState {
    bool onGround = false;
    bool stableOnGround = true;
    bool wasOnGround = false;
    bool hasWorldContact = false;
    bool realWorldContactThisFrame = false;
    float groundLostTimer = 0.0f;
    float airborneTimer = 0.0f;
    float landingAirborneDuration = 0.0f;
    float landingImpactSpeed = 0.0f;
    float landingCooldown = 0.0f;
    float worldContactLostTimer = 0.0f;
    bool didLand = false;
};

struct JumpState {
    int airJumpsLeft = 1;
    bool jumpHeldPrev = false;
    bool airJumpLocked = false;
    bool airJumpArmed = false;
    float jumpIntentTimer = 0.0f;
    float coyoteTimer = 0.0f;
    bool didGroundJump = false;
    bool didAirJump = false;
    float jumpSoundTimer = 0.0f;
};

struct DashState {
    bool dashAvailable = true;
    bool downDashAvailable = true;
    bool dashHeldPrev = false;
    bool moveHeldPrev = false;
    int dashMovementTicks = 0;
    int lastDashQuality = 0;
    bool didDash = false;
    bool didDownDash = false;
    float frictionOverride = 1.0f;
    bool tickPerfectDash = false;
    float dashGraceTimer = 0.0f;
    bool momentumProtectionActive = false;
    bool momentumProtectionUsedCameraForwardFallback = false;
    glm::vec2 momentumProtectedMoveAxes{0.0f};
    uint32_t movementInputGeneration = 0;
};

struct FreezeState {
    bool freezeAvailable = true;
    bool freezeHeldPrev = false;
    bool freezeActive = false;
    float freezeTimer = 0.0f;
    bool freezeHoldSoundPlayed = false;
    bool didFreeze = false;
};

struct CollisionState {
    int stuckFrames = 0;
    float bounceCooldown = 0.0f;
    bool hasWeaponCollisionCapsule = false;

    // Diagnostics: track values that should never grow unbounded
    int diagPrevCandidates = 0;
    int diagCandidateGrowthFrames = 0;
    int diagPrevContacts = 0;
    int diagContactGrowthFrames = 0;
    int diagPrevBodySpheres = 0;
    int diagBodySphereGrowthFrames = 0;
};

struct GroundReturnState {
    bool available = true;
    int charges = 0;
    float rechargeTimer = 0.0f;
};

class Player {
public:
    std::string username = "admin";
    MimitaVip::VipAppearance vipAppearance;
    MimitaVip::VipStyleDetail vipStyleDetail;
    int currentHp = 100;
    int maxHp = 100;
    bool inventoryOpen = false;
    int equippedSlot = 1;
    bool hasValidWeapon = false;
    uint8_t networkWeaponState = 0;
    float networkShootEffectTimer = 0.0f;
    uint16_t networkLastDashSerial = 0;
    uint16_t networkLastGroundJumpSerial = 0;
    uint16_t networkLastAirJumpSerial = 0;
    uint16_t networkLastDownDashSerial = 0;
    uint16_t networkLastDirectionChangeSerial = 0;
    uint16_t networkLastFreezeSerial = 0;
    uint16_t networkLastEquipSerial = 0;
    uint16_t networkStateFlags = 0;
    int revolverCylinder = 6;
    int revolverReserve = 1337;
    bool dead = false;
    bool netPredictedDead = false;  // local client prediction; server confirm clears/rolls back
    // Set when a reliable kill event already presented the death (anim + red
    // ellipsoid) for this life, so the snapshot health transition does not
    // play a second death. Cleared on respawn.
    bool networkDeathPresented = false;
    bool proceduralFrozen = false;
    bool ragdollModeActive = false;
    // While ragdolled the model root must be the physical torso (full 3-axis
    // orientation), not the yaw-only movement capsule. Ragdoll mode owns this.
    bool modelRootRotationActive = false;
    glm::quat modelRootRotation{1.0f, 0.0f, 0.0f, 0.0f};
    float respawnTimer = 0.0f;
    float spawnFlashTimer = 0.0f;
    glm::vec3 respawnPosition{1.0f, 5.0f, 60.0f};
    std::string killedBy;
    std::string killedByWeapon;
    std::string lastDamagedBy;
    bool voidDeathTriggered = false;
    float sizeScale = 1.0f;  // player size multiplier
    int matchTeam = -1;  // -1 = no team (FFA), 0 = red, 1 = blue
    uint32_t spawnGeneration = 0;
    uint16_t networkTransformEpoch = 0;
    uint64_t movementSimulationTick = 0;
    uint64_t bodySparkTick = 0;
    MovementContactSet movementContacts;
    MovementContactHistory movementContactHistory;
    // -------- Core State --------
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 externalImpulse{0.0f};
    // Source-mode impulse shaping (synced with MovementState).
    float externalImpulseCarryTimer = 0.0f;
    float externalImpulseMagnitude = 0.0f;
    // Per-tick air-strafe projection debug (synced from the movement kernel).
    MovementAirDebug airDebug;
    glm::vec2 inputWishMove{0.0f};

    float yaw = 0.0f;
    float aimBodyPitch = 0.0f;

    // -------- Grouped State --------
    GroundState ground;
    JumpState jump;
    DashState dash;
    FreezeState freeze;
    CollisionState collision;
    GroundReturnState groundReturn;

    // -------- Godball replication (set from network) --------
    glm::vec3 godballPosition{0.0f};
    glm::vec3 godballVelocity{0.0f};
    bool godballActive = false;

    // -------- Body/weapon collision push (debug) --------
    glm::vec3 debugBodyCollisionPush{0.0f};
    glm::vec3 debugWeaponCollisionPush{0.0f};

    float footstepTimer = 0.0f;

    // -------- Weapon / Aiming --------
    glm::vec3 aimDirection{0.0f, 1.0f, 0.0f};  // Camera forward for aiming
    glm::vec3 aimPosition{0.0f};                // Camera position for aim origin
    bool hasAimData = false;
    float weaponSwayTime = 0.0f;

    // -------- Death animation --------
    struct DeathAnimState {
        bool active = false;
        int tick = 0;
        int totalTicks = 180;
        float startAlpha = 1.0f;
        float endAlpha = 0.0f;
        glm::vec3 startRotation{0.0f, 0.0f, 0.0f};
        glm::vec3 endRotation{-90.0f, 0.0f, 0.0f};
        glm::vec3 frozenPosition{0.0f};
    };
    DeathAnimState deathAnim;

    // -------- Rendering --------
    bool renderGhost = false; // rendered as transparent ghost (server_showghost)
    glm::vec3 meshScale  {1,1,1};
    glm::vec3 meshOffset {0,0,0};
    Mesh renderMesh;
    bool modelLoaded = false;
    std::vector<TransformNode> nodes;
    std::vector<glm::mat4> restLocalTransforms;
    std::vector<Collider> bodyColliders;
    std::vector<BodyPart> bodyParts;
    std::vector<Mesh> bodyPartMeshes;
    PlayerOrigin origin;
    MovementCapsule movementCapsule;
    PerfectPoseSkeleton perfectPoseSkeleton;
    PhysicalBody physicalBody;
    glm::vec3 previousProceduralVelocity{0.0f};
    float proceduralTime = 0.0f;
    float previousMove01 = 0.0f;
    float animStateTime = 0.0f;
    std::string currentAnimName = "idle";

    // -------- Construction --------
    Player();
    explicit Player(bool loadRenderModel);
    void reset();
    bool loadModel(const char* path);
    void applyReplayPose(
        const glm::vec3& rootPosition,
        float rootYaw,
        const ReplayBodyPartState* parts,
        uint8_t partCount);
    void renderCurrentPose(unsigned int shader,
                           const glm::mat4& view,
                           const glm::mat4& proj,
                           bool whiteOverride = false,
                           bool hideHead = false,
                           bool outlinePass = false,
                           float outlineThickness = 0.0f,
                           const glm::vec4& outlineColor = glm::vec4(1.0f)) const;
    void syncLegacyStateToLayers();
    void syncLayersToLegacyState();
    void updateModelWorldTransforms();

    // -------- Queries --------
    Capsule getCapsule() const;
    OBB     getOBB() const;

    // -------- Character Loading --------
    bool loadCharacter(const std::string& characterName);
    const std::string& characterName() const { return mCharacterName; }
    const std::string& avatarName() const { return mAvatarName; }
    void setAvatarName(const std::string& name) { mAvatarName = name; }
    void ensureCharacterLoaded();
    bool mLazyLoadRequested = false;
    void requestModelLoad(const std::string& filepath);
    void finalizeModelIfReady();
    std::shared_ptr<PendingPlayerModel> mPendingModel;

    // -------- Systems --------
    void updateAudio(float dt);
    void render(unsigned int shader,
                const glm::mat4& view,
                const glm::mat4& proj,
                bool hideHead = false) const;
    void renderDepth(unsigned int shadowShader,
                     const glm::mat4& lightViewProj) const;



    // Full weapon local-to-arm transform including viewmodel config and animations.
    // Set during viewmodel update, used by physics to recompute colliders at the
    // correct position matching the rendered weapon.
    glm::mat4 weaponLocalToArm{1.0f};
    glm::mat4 weaponCollisionWorld{1.0f};  // Full weapon world transform for configurable colliders
    glm::vec3 weaponGripLocal{0.0f};
    glm::vec3 weaponMuzzleLocal{0.0f};
    float weaponRadiusLocal = 0.0f;

    // -------- Weapon system --------
    std::string equippedWeaponId;
    // Generic runtime tool equipped via `equiptool <name>`. Non-zero selects the
    // generic action path (no registered network weapon required).
    uint64_t runtimeToolId = 0;
    std::string mCharacterName = "DefaultGuy";
    std::string mAvatarName;
    struct AvatarInstance* avatarInstance = nullptr;
    std::unordered_map<std::string, WeaponRuntime> weaponRuntimes;

    // Previous frame body sample positions for limb sweep collisions
    std::vector<glm::vec3> previousBodySamplePositions;
    Capsule weaponCollisionCapsule{};
    std::vector<Capsule> weaponCollisionCapsules;
    Capsule prevWeaponCollisionCapsule{}; // previous frame for sweep delta computation
    std::string weaponCollisionName;
    glm::mat4 weaponModelTransform{1.0f}; // full world-space weapon model transform (3 axes)

    // Runtime debug data populated by collision solver, consumed by debug visuals
    WeaponCollisionRuntimeDebug weaponCollisionDebug;

    // -------- Combat --------
    void takeDamage(int damage, const glm::vec3& knockbackDir = glm::vec3(0), float knockbackForce = 0.0f);

    // -------- Chat Bubble State --------
    ActorChatState chatState;

    // -------- Typing Indicator State --------
    bool isTyping = false;
    uint64_t typingStartedMs = 0;

    // -------- Pose overlay state (dash, freeze, etc.) --------
    float dashPoseTimer = -1.0f;
    bool forceDashPose = false;
    float freezePoseTimer = -1.0f;
    bool freezePoseActive = false;

    // -------- Per-part color tints (from avatar.json colors) --------
    std::vector<glm::vec3> outfitPartColors;

    // -------- Cosmetics (loaded GLB attachments) --------
    std::vector<CosmeticSlot> mCosmetics;
    const std::vector<CosmeticSlot>& getCosmetics() const { return mCosmetics; }
    void setCosmetics(const std::vector<CosmeticSlot>& c) { mCosmetics = c; }
};

// Upload mesh to shared body-part VAO for ragdoll rendering
void uploadBodyPartMesh(const Mesh& mesh);

// Build a 4x4 transform from position + rotation quaternion
glm::mat4 transformMatrix(const glm::vec3& position, const glm::quat& rotation);
