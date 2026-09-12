#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct RagdollModeCapsuleConfig {
    // Negative means "derive from the mesh collider bounds".
    float radius = -1.0f;
    float halfHeight = -1.0f;
    // Center offset in the part (mesh node) frame; lets the capsule sit higher
    // or lower on the limb without moving the body.
    glm::vec3 offset{0.0f};
    // Optional long-axis override in the part frame. Zero = derive.
    glm::vec3 axis{0.0f};
    bool hasAxis = false;
    // Per-part center of mass, in the canonical part frame, relative to the
    // capsule center. Zero = COM at the capsule center.
    glm::vec3 centerOfMass{0.0f};
    // Debug capsule render alpha (1 opaque, 0 invisible).
    float alpha = 0.9f;
};

struct RagdollModeAimConfig {
    // Which local (part frame) axis should point along the camera forward, and
    // which should point up. Signed axes, e.g. [1,0,0] or [-1,0,0].
    glm::vec3 frontAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 upAxis{0.0f, 0.0f, 1.0f};
};

struct RagdollModeAttachmentConfig {
    std::string parent;
    // Connection point on the parent body, in the parent's part frame. Kept as
    // the legacy alias for parentOffset.
    glm::vec3 offset{0.0f};
    bool hasParentOffset = false;
    // Connection point on the child limb, in the child's part frame. Set this
    // to the top of an arm so the joint (and extension) pivots at the shoulder
    // instead of the capsule center.
    glm::vec3 childOffset{0.0f};
    bool hasChildOffset = false;
    float coneLimitDeg = 90.0f;

    // Optional per-axis rotation limits in the parent's local frame.
    bool hasRotationLimits = false;
    glm::vec3 rotMinDeg{-180.0f, -180.0f, -180.0f};
    glm::vec3 rotMaxDeg{ 180.0f,  180.0f,  180.0f};
};

struct RagdollModeConfigData {
    bool enabled = true;
    std::string toggleKey = "G";
    float gravityScale = 1.0f;
    float linearDamping = 0.15f;
    float angularDamping = 0.3f;
    float jointStiffness = 2000.0f;
    float jointDamping = 80.0f;
    bool worldCollision = true;

    // Corpse ragdolls (death presentation). The corpse reuses the same solver
    // as alive ragdoll mode, but without input, grabs, or look motors.
    float corpseLifetimeSeconds = 20.0f;
    float corpseFadeSeconds = 4.0f;
    float corpseDeathImpulseMultiplier = 1.0f;
    float corpseSpawnVelocityMultiplier = 1.0f;
    float corpseBloodIntervalSeconds = 0.18f;
    bool corpseBloodEnabled = true;

    // ── Editable simulation rates (primitives) ──────────────────────
    // solver_hz: fixed rate of the ragdoll.solver domain. The 60 Hz gameplay
    // tick stays untouched; the solver substeps at this rate and is clamped by
    // the domain catch-up. Hot-reloadable via config/ragdoll.json.
    float solverHz = 120.0f;
    // Client replication cadence for PACKET_RAGDOLL_STATE, in gameplay ticks.
    int snapshotSendIntervalTicks = 3;
    // Replicate deterministic corpse spawns to peers (PACKET_CORPSE_SPAWN).
    bool replicateCorpses = true;

    // Rigid-body solver
    int solverIterations = 24;
    float maxFallSpeed = 60.0f;
    float restitution = 0.0f;
    float friction = 0.5f;
    bool selfCollision = true;
    float bodyLinearDamping = 0.5f;
    float bodyAngularDamping = 2.5f;

    // A part slower than these is treated as at rest and its velocity is zeroed
    // at the end of the tick, so limbs come to a natural stop.
    float stopLinearSpeed = 0.1f;
    float stopAngularSpeed = 0.4f;

    // Solver tuning (anti-jitter).
    float jointPositionBeta = 0.5f;
    float limitPositionBeta = 0.25f;
    int selfCollisionIterations = 2;
    float selfCollisionBeta = 0.5f;
    // Self-collision skin/slop: overlaps smaller than this are ignored.
    float selfCollisionSkin = 0.015f;
    // Cap on per-pass position correction so deep overlap resolves smoothly.
    float selfCollisionMaxCorrection = 0.05f;
    float maxAngularSpeed = 25.0f;
    // How quickly the head/torso look motor approaches its target angular
    // velocity (higher = snappier, too high = oscillation).
    float lookDamping = 12.0f;
    // Exponential smoothing of the rendered part transforms (0 = off).
    float bodySmoothing = 0.0f;

    // Mass (kg). Names match the body part IDs; globalMultiplier scales all.
    std::unordered_map<std::string, float> massKg;
    float massGlobalMultiplier = 1.0f;

    std::unordered_map<std::string, RagdollModeCapsuleConfig> capsules;
    std::unordered_map<std::string, RagdollModeAttachmentConfig> attachments;
    std::unordered_map<std::string, RagdollModeAimConfig> aim;

    // Master debug hitbox visibility (all ragdoll capsules/axes/links).
    bool debugHitboxesVisible = true;
    // Debug draw of attachment anchors/links (ragdoll.json attachments_visible).
    bool attachmentsVisible = false;

    // Incremented on every successful load so owners can re-apply live changes.
    uint64_t generation = 0;

    // Grab settings
    std::string leftKey = "A";
    std::string rightKey = "D";
    int extendLeftMouse = 0;
    int extendRightMouse = 1;
    float extendForce = 50.0f;
    float grabReach = 2.5f;
    float grabRadius = 0.3f;
    float grabCompliance = 0.02f;
    float grabGraceDistance = 0.5f;

    // Arms
    float armExtendStrength = 40.0f;
    float armExtendMaxSpeed = 12.0f;
    // How far the arm's shoulder anchor may separate from the torso while
    // extending, in meters.
    float armMaxStretch = 0.25f;
    // Linear force used to push the arm out during extension.
    float armStretchForce = 50.0f;
    // Optional forward pull applied to the torso once the arm is fully
    // stretched. 0 = no pull without a grab.
    float armBodyPull = 0.0f;

    // Weapon
    bool oneHandedWeaponEnabled = true;
    std::string weaponHand = "right";

    // Camera
    std::string cameraMode = "locked_to_head";
    float cameraSmoothFactor = 0.0f;
    bool thirdPersonAllowed = true;

    // Torso look direction
    float torsoLookSpring = 8.0f;
    float torsoMaxAngularStep = 15.0f;

    // Head aim
    float headRotationStrength = 12.0f;
    float headRotationSpeed = 18.0f;

    // Exit
    bool exitPreserveVelocity = true;
    float exitHopVelocity = 3.0f;
};

class RagdollModeConfig {
public:
    static RagdollModeConfig& instance();
    bool load(const std::string& path = "config/ragdoll.json");
    bool pollReload();
    const RagdollModeConfigData& data() const { return mData; }
    RagdollModeConfigData& data() { return mData; }

private:
    RagdollModeConfig() = default;
    RagdollModeConfigData mData;
    std::string mPath = "config/ragdoll.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mWatchLogged = false;
};
