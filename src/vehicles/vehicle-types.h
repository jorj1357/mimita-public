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
    float width = 0.22f;
    glm::vec3 suspensionAxis{0.0f, 0.0f, -1.0f};
    float restLength = 0.32f;
    float travel = 0.20f;
    float springRate = 42000.0f;
    float damperRate = 4200.0f;
    float antiRoll = 9000.0f;
    float unsprungMass = 38.0f;
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
};

struct VehicleGearboxSpec {
    // Index 0 is reverse, index 1 is neutral, indexes 2.. are forward gears.
    std::vector<float> ratios{-3.4f, 0.0f, 3.6f, 2.1f, 1.45f, 1.05f, 0.82f, 0.68f};
    float finalDrive = 3.7f;
    float shiftTime = 0.18f;
    float clutchCapacity = 420.0f;
};

struct VehicleDifferentialSpec {
    std::string type = "lsd";
    float frontRearSplit = 0.0f;
};

struct VehicleSteeringSpec {
    float maxAngleDeg = 32.0f;
    float speedFalloff = 0.55f;
    float returnRate = 3.5f;
};

struct VehicleBrakeSpec {
    float maxTorque = 5200.0f;
    float handbrakeTorque = 3000.0f;
};

struct VehicleSeatSpec {
    std::string name;
    glm::vec3 anchor{0.0f};
    std::string pose = "sit";
};

struct VehicleSpec {
    std::string id;
    float massKg = 1400.0f;
    float density = 120.0f;
    glm::vec3 centerOfMass{0.0f};
    glm::vec3 halfExtents{2.2f, 0.9f, 0.6f};
    float drag = 0.32f;
    float downforce = 0.0f;
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
};

// Vehicle input is deliberately a plain value so the same struct can come from
// local input, a terminal command, an NPC controller, or the network.
struct VehicleInput {
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    float clutch = 1.0f;
    float handbrake = 0.0f;
    bool engineToggle = false;
    bool modeToggle = false;
    bool shiftUp = false;
    bool shiftDown = false;
};

struct VehicleState {
    uint32_t id = 0;
    std::string templateId;
    uint32_t entityId = 0;

    VehicleControlMode mode = VehicleControlMode::Manual;
    bool engineOn = false;
    int gear = 1;            // -1 = reverse, 0 = neutral, 1..N forward
    float engineRpm = 0.0f;
    float engineTorque = 0.0f;
    float boost = 0.0f;
    float shiftTimer = 0.0f;

    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;
    float clutch = 1.0f;
    float handbrake = 0.0f;
    float speed = 0.0f;

    // Edge latches consumed by the drivetrain on this fixed tick.
    bool flagsShiftUp = false;
    bool flagsShiftDown = false;

    int driverActorId = 0;
    int driverSeat = -1;
    bool localDriver = false;

    // Input used when no local driver is attached (terminal, NPC, network).
    VehicleInput pendingInput;

    std::vector<VehicleWheelState> wheels;
};
