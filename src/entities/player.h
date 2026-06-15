// C:\important\quiet\n\mimita-priv-v7\src\entities\player.h
// feb 10 2026 CLEANED: slim + physics-safe

#pragma once
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "map/map_common.h"
#include "physics/physics-types.h"
#include "gui/hud/chat-bubble.h"

#include "combat/weapon-types.h"

struct ReplayBodyPartState;

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
    bool onGround = false;
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

struct AxisLock {
    bool x = true;
    bool y = true;
    bool z = true;
};

struct AnimKeyframePart {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
};

struct AnimKeyframe {
    int tick = 0;
    std::unordered_map<std::string, AnimKeyframePart> parts;
};

struct AnimClip {
    int durationTicks = 60;
    bool loop = true;
    bool speedScaleFromVelocity = true;
    std::vector<AnimKeyframe> keyframes;
};

struct WeaponPoseConfig {
    bool useWeaponPose = true;
    AnimKeyframePart leftArm;
    AnimKeyframePart rightArm;
};

struct LayeredAnimConfig {
    std::unordered_map<std::string, AnimClip> animations;
    AnimKeyframePart reloadOverlay;
};

struct PlayerProceduralConfig
{
    float leftArmRaise;
    float leftArmForward;
    float leftArmTwist;
    float rightArmRaise;
    float rightArmForward;
    float rightArmTwist;
    float weaponSwayAmount;
    float weaponSwaySpeed;
    float torsoAimYawStrength;
    float torsoAimPitchStrength;
    float armAimYawStrength;
    float armAimPitchStrength;
    float armSwingAmount;
    float legSwingAmount;
    float torsoLeanAmount;
    float headCounterAmount;
    float bobHeight;
    float walkFrequency;
    float walkFrequencyMultiplier;
    float reloadArmLowerZ;
    float reloadArmLowerX;
    float reloadHandLower;
    float armInfluenceMultiplier;
    float idleSwayAmount;
    float idleSwaySpeed;
    float revolverOffsetX;
    float revolverOffsetY;
    float revolverOffsetZ;
    float revolverRotX;
    float revolverRotY;
    float revolverRotZ;
    float shotgunOffsetX;
    float shotgunOffsetY;
    float shotgunOffsetZ;
    float shotgunRotX;
    float shotgunRotY;
    float shotgunRotZ;
    LayeredAnimConfig layers;
    std::unordered_map<std::string, WeaponPoseConfig> weaponPoses;
    std::unordered_map<std::string, AxisLock> axisLocks;
    int walkStartTickOnEnter = 7;
    int animationStateTransitionFrames = 1;
};

extern PlayerProceduralConfig gPlayerProcedural;

// Poll config/player-procedural.json for changes.
// Call each frame from main loop to ensure hot reload works regardless of physics state.
// Uses wall-clock 250ms throttle internally.
void updatePlayerProceduralHotReload(float dt);

class Player {
public:
    std::string username = "admin";
    int currentHp = 100;
    int maxHp = 100;
    bool inventoryOpen = false;
    int equippedSlot = 1;
    bool hasValidWeapon = false;
    uint8_t networkWeaponState = 0;
    float networkShootEffectTimer = 0.0f;
    uint32_t networkLastDashSerial = 0;
    int revolverCylinder = 6;
    int revolverReserve = 1337;
    bool dead = false;
    bool proceduralFrozen = false;
    float respawnTimer = 0.0f;
    float spawnFlashTimer = 0.0f;
    glm::vec3 respawnPosition{1.0f, 5.0f, 60.0f};
    std::string killedBy;
    // -------- Core State --------
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    glm::vec3 externalImpulse{0.0f};
    glm::vec2 inputWishMove{0.0f};

    bool onGround = false;
    float yaw = 0.0f;

    // -------- Jump --------
    int airJumpsLeft = 1;
    bool jumpHeldPrev = false;
    // so we dont use air jump on ground
    bool airJumpLocked = false;
    // so we press space to air jump, not let go of space
    bool airJumpArmed = false;
    float jumpIntentTimer = 0.0f;
    float coyoteTimer = 0.0f;

    // -------- Dash --------
    // mar 8 2026, no dashcharges, its airjump style, touch object = get a dash back
    // int dashCharges = 3;
    // mar 7 2026 we need to remove cooldowns i think?
    // i want less cooldowns less waiting no waiting at all
    // but spamming moves intrinsically limits itself
    // bc dash will go infinite, but u gain so much speed that u end up crashing into a wall
    // and dying
    // removing the need for a artificial cooldown
    // plauers will cool themselves down
    // float dashRechargeTimer = 0.0f;
    // float dashCooldown = 0.0f;
    // no holding it to dash forever 1000/sec
    bool dashHeldPrev = false;
    bool moveHeldPrev = false;

    // reset dash when touching object mar 8 2026
    bool dashAvailable = true;

    // -------- Ground Return --------
    // old mar 8 2026
    // uncommented but whatever set to 0 mar 8 2026
    // int   groundReturnCharges = 3;
    int   groundReturnCharges = 0;
    float groundReturnRechargeTimer = 0.0f;

    // new cool mar 8 2026 reset when touching smth 
    bool groundReturnAvailable = true;

    // -------- Down Dash --------
    bool downDashAvailable = true;

    // -------- Freeze --------
    bool freezeAvailable = true;
    bool freezeHeldPrev = false;
    bool freezeActive = false;

    float freezeTimer = 0.0f;

    // -------- Freeze sounds --------
    bool freezeHoldSoundPlayed = false;

    // -------- One-Frame Events (set by physics) --------
    bool didGroundJump = false;
    bool didAirJump    = false;
    bool didDash       = false;
    bool didLand       = false;

    // -------- Audio helpers --------
    bool  wasOnGround = false;
    bool  wasGroundedLastFrame = false;
    bool  wasStableGroundedLastFrame = false;

    float groundLostTimer = 0.0f;
    float airborneTimer   = 0.0f;
    bool  stableOnGround  = true;
    int collisionStuckFrames = 0;
    float collisionBounceCooldown = 0.0f;

    float footstepTimer = 0.0f;

    // -------- Weapon / Aiming --------
    glm::vec3 aimDirection{0.0f, 1.0f, 0.0f};  // Camera forward for aiming
    glm::vec3 aimPosition{0.0f};                // Camera position for aim origin
    bool hasAimData = false;
    float weaponSwayTime = 0.0f;

    // -------- Rendering --------
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
    void reset();
    bool loadModel(const char* path);
    void applyReplayPose(
        const glm::vec3& rootPosition,
        float rootYaw,
        const std::vector<ReplayBodyPartState>& parts);
    void renderCurrentPose(unsigned int shader,
                           const glm::mat4& view,
                           const glm::mat4& proj,
                           bool whiteOverride = false) const;
    void syncLegacyStateToLayers();
    void syncLayersToLegacyState();
    void updateModelWorldTransforms();
    void updateProceduralAnimation(float dt, const glm::vec3& camForward = glm::vec3(0,1,0), const glm::vec3& camPos = glm::vec3(0));

    // -------- Queries --------
    Capsule getCapsule() const;
    OBB     getOBB() const;

    // -------- Systems --------
    void updateAudio(float dt);
    void render(unsigned int shader,
                const glm::mat4& view,
                const glm::mat4& proj) const;
    void renderDepth(unsigned int shadowShader,
                     const glm::mat4& lightViewProj) const;

    // -------- Weapon system --------
    std::string equippedWeaponId;
    std::unordered_map<std::string, WeaponRuntime> weaponRuntimes;

    // Previous frame body sample positions for limb sweep collisions
    std::vector<glm::vec3> previousBodySamplePositions;

    // -------- Combat --------
    void takeDamage(int damage, const glm::vec3& knockbackDir = glm::vec3(0), float knockbackForce = 0.0f);

    // -------- Chat Bubble State --------
    ActorChatState chatState;
};

// Upload mesh to shared body-part VAO for ragdoll rendering
void uploadBodyPartMesh(const Mesh& mesh);
