// 2026-10-27
/* purpose
* Define vehicle template data and fixed-tick runtime state.
* A vehicle is a chassis PhysicalEntity plus data-driven suspension units and a
* drivetrain. No car/truck-specific physics branches exist.
* Does NOT load JSON, run physics, input, rendering, or networking.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct VehicleWheelSpec {
    glm::vec3 hub{0.0f};
    float radius = 0.34f;
    // Optional collision sphere at the hub. 0 disables it. Giving the wheels
    // real collision geometry is what makes the car settle onto them through
    // ordinary contact physics instead of a scripted righting behaviour.
    float collisionRadius = 0.0f;
    float width = 0.22f;
    glm::vec3 suspensionAxis{0.0f, 0.0f, -1.0f};
    // Visual/log axes. Physics rolling is still forward-roll; these let the
    // renderer and logs be tested on every axis live.
    glm::vec3 spinAxis{0.0f, 1.0f, 0.0f};
    glm::vec3 steerAxis{0.0f, 0.0f, 1.0f};
    float restLength = 0.32f;
    float travel = 0.20f;
    float springRate = 42000.0f;
    float damperRate = 4200.0f;
    float antiRoll = 9000.0f;
    // Anti-roll damping in N per (m/s), 0 = derive from antiRoll.
    float antiRollDamping = 0.0f;
    float unsprungMass = 38.0f;
    // Wheel spin inertia in kg*m^2. 0 = derive from unsprungMass and radius.
    float spinInertia = 0.0f;
    float maxSpinVelocity = 400.0f;
    float camberDeg = 0.0f;
    float toeDeg = 0.0f;
    float casterDeg = 0.0f;
    bool steer = false;
    bool drive = false;
    bool brake = true;
};

struct VehicleTireSpec {
    float mu = 1.35f;
    float loadSensitivity = 0.00018f;
    float nominalLoad = 4200.0f;
    float peakSlipRatio = 0.12f;
    float peakSlipAngle = 0.12f;
    bool frictionCircle = true;
    float lowSpeed = 0.6f;
    // Grip multiplier below lowSpeed. >1 makes a parked/slow wheel stick.
    float lowSpeedGrip = 1.0f;
    // Rolling resistance coefficient applied against travel direction.
    float rollingResistance = 0.015f;
    // Tire normal-load cap as a multiple of nominalLoad.
    float maxLoadScale = 3.0f;
    float slipStiffness = 1.0f;
    float camberGain = 0.0f;
};

struct VehicleEngineSpec {
    float inertia = 0.22f;
    float idleRpm = 850.0f;
    float redlineRpm = 6800.0f;
    float frictionTorque = 18.0f;
    std::vector<glm::vec2> torqueCurve; // rpm, newton-metres
    float turboMaxBoost = 0.0f;
    float turboSpoolRate = 2.4f;
    float turboLag = 0.18f;
    float clutchKickBoost = 0.5f;
};

struct VehicleGearboxSpec {
    // Index 0 is reverse, index 1 is neutral, indexes 2.. are forward gears.
    std::vector<float> ratios{-3.4f, 0.0f, 3.6f, 2.1f, 1.45f, 1.05f, 0.82f, 0.68f};
    float finalDrive = 3.7f;
    float shiftTime = 0.18f;
    float clutchCapacity = 420.0f;
};

struct VehicleDifferentialSpec {
    // welded | race_lsd | open
    std::string type = "welded";
    float frontRearSplit = 0.0f;
    float preloadTorque = 120.0f;
    float powerLock = 0.65f;
    float coastLock = 0.30f;
    float lockStrength = 0.85f;
    float biasRatio = 2.0f;
};

struct VehicleSteeringSpec {
    float maxAngleDeg = 32.0f;
    float speedFalloff = 0.55f;
    float returnRate = 3.5f;
    // +1 means positive input steers right; -1 inverts.
    float directionSign = -1.0f;
    float ackermann = 0.0f;
};

struct VehicleBrakeSpec {
    float maxTorque = 5200.0f;
    float handbrakeTorque = 3000.0f;
    // Rear lateral grip multiplier while the handbrake is held.
    float handbrakeGripScale = 0.45f;
};

struct VehicleSeatSpec {
    std::string name;
    glm::vec3 anchor{0.0f};
    std::string pose = "sit";
    // Local offset applied to the actor origin after the seat anchor. Keeps the
    // seated animation/collision root above the cushion instead of inside it.
    glm::vec3 riderOffset{0.0f, 0.0f, 0.35f};
};

struct VehicleSpec {
    std::string id;
    float massKg = 1400.0f;
    float density = 120.0f;
    glm::vec3 centerOfMass{0.0f};
    glm::vec3 halfExtents{2.2f, 0.9f, 0.6f};
    // Collision extents may differ from the rendered body. Zero means "use
    // halfExtents". The box is the car's whole envelope and must contain the
    // wheels: its bottom face is the wheel contact plane, so no part of the car
    // can sit below the surface at rest.
    glm::vec3 collisionHalfExtents{0.0f};
    // Offset of the collision box from the chassis origin. Used to put the box
    // bottom exactly on the wheel contact plane without moving the visual body.
    glm::vec3 collisionCenter{0.0f};
    float drag = 0.32f;
    float downforce = 0.0f;
    // Chassis-vs-world material response. Low friction lets the body slide off
    // ledges instead of catching; a small restitution gives a 0.1 bounce.
    float bodyFriction = 0.20f;
    float bodyWallFriction = 0.05f;
    float bodyRestitution = 0.10f;
    // Rebound damping multiplier: <1 gives a bouncier suspension.
    float reboundScale = 1.0f;
    // Guaranteed coast-down when there is no throttle/brake/steer input.
    float coastDeceleration = 3.0f;
    float coastSpinDamping = 2.0f;
    // Documentation/validation targets. They do not override physics; they
    // make the gearing/torque math checkable from events.jsonl.
    float targetZeroTo60Seconds = 0.0f;
    float targetTopSpeedKmh = 0.0f;
    // Chassis rigid-body response. These used to be hardcoded in
    // vehicle-system.cpp.
    float linearDamping = 0.02f;
    float angularDamping = 0.25f;
    float maxAngularSpeed = 12.0f;
    float sleepLinearThreshold = 0.15f;
    float sleepAngularThreshold = 0.15f;
    int sleepRequiredTicks = 80;
    // Suspension integration safety. maxSuspensionForce is per wheel.
    float maxSuspensionForce = 56000.0f;
    float bumpStopStiffness = 0.0f; // 0 = 4 * springRate
    float bumpStopDamping = 0.0f;   // 0 = damperRate
    float antiRollForceScale = 10.0f;
    float stabilityVerticalDamping = 8.0f;
    float stabilityAngularDamping = 8.0f;
    float idleSettleSpeed = 2.0f;
    float idleSettleHorizontal = 0.80f;
    float idleSettleVertical = 0.10f;
    float idleSettleAngular = 0.10f;
    // F-interaction radius. The vehicle prompt uses this directly and the
    // authoritative seat request allows this plus a small network margin.
    float interactionRadius = 15.0f;
    std::string bodyTexture = "assets/textureshq/240sx.png";
    std::string wheelTexture = "assets/textureshq/dark1.png";
    std::string engineSound =
        "assets/sound/weapon/chainsaw/chainsawidle.wav";
    int wheelSegments = 32;
    std::vector<VehicleWheelSpec> wheels;
    VehicleTireSpec tire;
    VehicleEngineSpec engine;
    VehicleGearboxSpec gearbox;
    VehicleDifferentialSpec differential;
    VehicleSteeringSpec steering;
    VehicleBrakeSpec brakes;
    std::vector<VehicleSeatSpec> seats;
};

enum class VehicleControlMode : uint8_t {
    Automatic = 0,
    Manual = 1
};

struct VehicleWheelState {
    float spinAngle = 0.0f;
    float spinVelocity = 0.0f;
    float steerAngle = 0.0f;
    float compression = 0.0f;
    float previousCompression = 0.0f;
    float compressionVelocity = 0.0f;
    float load = 0.0f;
    float slipRatio = 0.0f;
    float slipAngle = 0.0f;
    bool grounded = false;
    bool rayHit = false;
    float rayDistance = 0.0f;
    float rayMaxDistance = 0.0f;
    float suspensionLength = 0.0f;
    glm::vec3 contactPoint{0.0f};
    glm::vec3 contactNormal{0.0f, 0.0f, 1.0f};
    int contactTriangleId = -1;
    glm::vec3 worldCenter{0.0f};
    glm::vec3 spinAxisWorld{0.0f, 1.0f, 0.0f};
    glm::vec3 steerAxisWorld{0.0f, 0.0f, 1.0f};
    glm::vec3 worldVelocity{0.0f};
};

// Vehicle input is deliberately a plain value so the same struct can come from
// local input, a terminal command, an NPC controller, or the network.
struct VehicleInput {
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    // Normalized rotational intent. Every device (gamepad, keyboard, touch,
    // NPC policy) publishes these three axes so the aerial solver never knows
    // which device produced them. See docs/specs/vehicles/vehicles.md.
    //   pitch: nose down (+1) / nose up (-1) about the car's right axis
    //   yaw:   about the car's up axis
    //   roll:  about the car's forward axis (includes directional air roll)
    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;
    // R1/free-air-roll modifier: airborne it remaps stick X from yaw to roll.
    bool airRollModifier = false;
    float clutch = 1.0f;
    float handbrake = 0.0f;
    float boost = 0.0f;
    bool jumpHeld = false;
    bool jumpPressed = false;
    bool powerslide = false;
    bool engineToggle = false;
    bool modeToggle = false;
    bool shiftUp = false;
    bool shiftDown = false;
};

struct VehicleState {
    uint32_t id = 0;
    std::string templateId;
    // Named arcade profile selected by the gamemode. Empty = template physics.
    std::string profileId;
    uint32_t entityId = 0;
    // Server-assigned identity for the chassis mirror. 0 on a purely local
    // offline vehicle.
    uint32_t chassisNetworkId = 0;
    // True on the dedicated server's authoritative vehicle.
    bool serverAuthoritative = false;
    // True on a client mirror. Mirrors never apply suspension/tire/engine
    // forces and never run the drivetrain locally.
    bool serverMirror = false;
    // True when this client simulates its own chassis (prediction). Snapshots
    // then act as corrections instead of authority.
    bool clientPredicted = false;
    // Audio/interaction presentation is disabled for the dedicated server.
    bool presentationEnabled = true;
    // Actor id (server player/NPC id) per seat, or 0 when empty.
    std::vector<int> seatOccupants;

    VehicleControlMode mode = VehicleControlMode::Manual;
    bool engineOn = false;
    int gear = 1;            // -1 = reverse, 0 = neutral, 1..N forward
    float engineRpm = 0.0f;
    float engineTorque = 0.0f;
    // Engine turbo spool value, distinct from the arena boost charge meter.
    float boost = 0.0f;
    // Arena boost charge. Profile data owns capacity, drain, and force.
    float boostCharge = 0.0f;
    // Arena jump/flip state.
    int jumpCount = 0;
    uint64_t lastGroundedTick = 0;
    uint64_t jumpStartedTick = 0;
    uint64_t lastJumpPressedTick = 0;
    bool jumpHeld = false;
    bool jumpHeldPrev = false;
    bool jumpPressed = false;
    // Latches a hold that outlived the minimum jump, so the sustained force is
    // not cancelled by a late or dropped held-state packet.
    bool jumpHoldLatched = false;
    // Rocket League aerial/dodge state. All timing is seconds, all lengths are
    // meters. The dodge is a finite torque + impulse program; stall, flip
    // cancel and flip reset emerge from these rules (no dedicated mechanics).
    bool hasFlipped = false;
    bool isFlipping = false;
    bool isJumping = false;
    bool hasJumped = false;
    float flipTimer = 0.0f;
    float jumpTimer = 0.0f;
    float airTimeSinceJump = 0.0f;
    float handbrakeVal = 0.0f;
    glm::vec3 flipTorqueLocal{0.0f};
    glm::vec3 flipAxisWorld{0.0f};
    glm::vec3 dodgeWorldDir{0.0f};
    int flipResetCount = 0;
    bool wheelOnBall = false;
    uint64_t lastFlipResetTick = 0;
    // Orientation and support tracking. These are diagnostics and solver input,
    // not a righting behaviour: the car settles because of its own geometry.
    float upDotWorldUp = 1.0f;
    int wheelMask = 0;
    bool wasSleeping = false;
    // Metric telemetry accumulators (meters / seconds / newtons).
    float distanceTravelled = 0.0f;
    float apexHeightM = 0.0f;
    float takeoffHeightM = 0.0f;
    glm::vec3 lastTelemetryPosition{0.0f};
    uint64_t airborneSinceTick = 0;
    bool airborneLogged = false;
    // Consecutive supported ticks, so contact flicker cannot fake a landing.
    uint32_t groundedRunTicks = 0;
    // Consecutive ticks resting on the chassis (not wheels, hips, roof).
    uint32_t sideRestTicks = 0;
    bool sideRecoveryLogged = false;
    uint64_t lastTelemetryTick = 0;
    uint64_t lastHeadingTick = 0;
    float boostInput = 0.0f;
    bool powerslide = false;
    bool boostActive = false;
    bool airRollActive = false;
    bool allWheelsGrounded = false;
    bool ballCam = false;
    float shiftTimer = 0.0f;
    float autoShiftLockTimer = 0.0f;
    int lastGearLogged = -999;
    float clutchKickTimer = 0.0f;
    float prevClutch = 0.0f;
    float lastWallContactTick = -1.0f;
    float lastSteerYaw = 0.0f;

    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    // Last sampled rotational intent (see VehicleInput). Kept on state so the
    // fixed tick can evaluate the dodge and air-control model.
    float pitchInput = 0.0f;
    float yawInput = 0.0f;
    float rollInput = 0.0f;
    float clutch = 1.0f;
    float handbrake = 0.0f;
    float speed = 0.0f;

    // Live tuning scales applied on top of the template.
    float springScale = 1.0f;
    float damperScale = 1.0f;
    float gripScale = 1.0f;
    float torqueScale = 1.0f;
    float dragScale = 1.0f;

    // Edge latches consumed by the drivetrain on this fixed tick.
    bool flagsShiftUp = false;
    bool flagsShiftDown = false;

    int driverActorId = 0;
    int driverSeat = -1;
    bool localDriver = false;
    bool inputOverride = false;
    // Ticks left on an injected (console/scenario) input. Injected input is a
    // short command, not a mode: it used to latch forever, so real keyboard and
    // pad were ignored until the driver reseated.
    uint32_t inputOverrideTicks = 0;
    uint32_t interactionTargetId = 0;

    // Stable origin and per-tick motion telemetry. The future server uses the
    // same origin to offset attachments and spawned parts.
    glm::vec3 spawnOrigin{0.0f};
    glm::vec3 origin{0.0f};
    glm::vec3 originVelocity{0.0f};
    glm::vec3 originAngularVelocity{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    float distanceFromSpawn = 0.0f;
    float massKg = 0.0f;
    float weightN = 0.0f;
    uint64_t lastSummaryTick = 0;
    uint64_t nextEnginePulseTick = 0;
    uint64_t lastAudioLogTick = 0;
    bool audioEngineWasOn = false;
    int enginePulsesSinceLog = 0;
    int settleTicks = 0;
    bool settleReported = false;
    bool flipReported = false;
    // One-shot latch so a flip cancel is logged once per dodge.
    bool flipCancelLogged = false;
    // Total rotation this dodge has accumulated, radians. Proves a full 360.
    float flipRotationRad = 0.0f;
    // Stick direction at dodge start in the car's own input axes (pitch,
    // yaw+roll). Flip cancel compares against this, so car rotation cannot make
    // the check fire by itself.
    glm::vec2 flipCancelLocal{0.0f};
    glm::vec3 settleAnchor{0.0f};
    bool settleAnchorValid = false;
    // Rate limit for the sustained-penetration diagnostic.
    uint32_t lastPenetrationLogTick = 0;

    // Input used when no local driver is attached (terminal, NPC, network).
    VehicleInput pendingInput;

    std::vector<VehicleWheelState> wheels;
};
