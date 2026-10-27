// 2026-10-27

#include "vehicles/vehicle-system.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <GLFW/glfw3.h>

#include "debug/debug-log.h"
#include "input/input-commands.h"
#include "physics/movement/physics-collision.h"
#include "physics/physical-entity.h"
#include "physics/ray-utils.h"
#include "vehicles/vehicle-config.h"
#include "world/world.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;

void vehicleFixedTick(float dt, const World& world)
{
    VehicleSystem::instance().update(dt, world);
}

// Cached broadphase ray query. The DDA path only sees chunked triangles, while
// large ground/floor triangles live in the world tree; a wheel must hit both.
bool raycastWorldCached(const World& world, const glm::vec3& origin,
                        const glm::vec3& direction, float maxDistance,
                        float& hitDistance, glm::vec3& hitNormal)
{
    static thread_local std::vector<int> candidates;
    candidates.clear();
    const glm::vec3 end = origin + direction * maxDistance;
    AABB box;
    box.min = glm::min(origin, end) - glm::vec3(0.05f);
    box.max = glm::max(origin, end) + glm::vec3(0.05f);
    appendChunkTrianglesForAABB(world, box, 0.0f, candidates, "vehicleWheel",
                                true);

    float nearest = maxDistance;
    bool found = false;
    for (int index : candidates)
    {
        if (index < 0 ||
            index >= static_cast<int>(world.collisionMesh.triangles.size()))
            continue;
        const CollisionTriangle& triangle =
            world.collisionMesh.triangles[index];
        float distance = 0.0f;
        if (!rayTriangle(origin, direction, triangle, distance))
            continue;
        if (distance < nearest)
        {
            nearest = distance;
            hitNormal = triangle.normal;
            found = true;
        }
    }
    if (found)
        hitDistance = nearest;
    return found;
}

float sampleTorque(const VehicleSpec& spec, float rpm)
{
    const auto& curve = spec.engine.torqueCurve;
    if (curve.empty())
        return 200.0f;
    if (rpm <= curve.front().x)
        return curve.front().y;
    if (rpm >= curve.back().x)
        return curve.back().y;
    for (size_t i = 1; i < curve.size(); ++i)
    {
        if (rpm > curve[i].x)
            continue;
        const float span = std::max(curve[i].x - curve[i - 1].x, 0.001f);
        const float t = (rpm - curve[i - 1].x) / span;
        return curve[i - 1].y + (curve[i].y - curve[i - 1].y) * t;
    }
    return curve.back().y;
}

int maxForwardGear(const VehicleSpec& spec)
{
    return spec.gearbox.ratios.size() >= 3
               ? static_cast<int>(spec.gearbox.ratios.size()) - 2
               : 6;
}

float gearRatio(const VehicleSpec& spec, int gear)
{
    if (spec.gearbox.ratios.empty())
        return 1.0f;
    if (gear < 0)
        return spec.gearbox.ratios.front();
    if (gear == 0)
        return 0.0f;
    const size_t index = std::min<size_t>(
        static_cast<size_t>(gear) + 1, spec.gearbox.ratios.size() - 1);
    return spec.gearbox.ratios[index];
}

int drivenWheelCount(const VehicleSpec& spec)
{
    int count = 0;
    for (const VehicleWheelSpec& wheel : spec.wheels)
        if (wheel.drive)
            ++count;
    if (count > 0)
        return count;
    for (const VehicleWheelSpec& wheel : spec.wheels)
        if (wheel.hub.x < 0.0f)
            ++count;
    return std::max(count, 1);
}

bool wheelIsDriven(const VehicleSpec& spec, const VehicleWheelSpec& wheel)
{
    bool anyExplicit = false;
    for (const VehicleWheelSpec& other : spec.wheels)
        if (other.drive)
        {
            anyExplicit = true;
            break;
        }
    return anyExplicit ? wheel.drive : wheel.hub.x < 0.0f;
}

float wheelInertia(const VehicleWheelSpec& wheel)
{
    return std::max(0.5f * wheel.unsprungMass * wheel.radius * wheel.radius,
                    0.01f);
}

} // namespace

bool vehicleRaycastWorld(const World& world, const glm::vec3& origin,
                         const glm::vec3& direction, float maxDistance,
                         float& hitDistance, glm::vec3& hitNormal)
{
    return raycastWorldCached(world, origin, direction, maxDistance,
                              hitDistance, hitNormal);
}

VehicleSystem& VehicleSystem::instance()
{
    static VehicleSystem system;
    return system;
}

bool VehicleSystem::loadConfig(const std::string& directory)
{
    return VehicleConfigRegistry::instance().loadDirectory(directory);
}

bool VehicleSystem::pollReload()
{
    return VehicleConfigRegistry::instance().pollReload();
}

void VehicleSystem::registerFixedTick()
{
    PhysicalEntitySystem::instance().addFixedTickCallback(&vehicleFixedTick);
}

uint32_t VehicleSystem::spawn(const std::string& templateId,
                              const glm::mat4& transform,
                              uint32_t driverActorId)
{
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(templateId);
    if (!spec)
    {
        Debug::warn(Debug::Category::Physics,
                    "[VEHICLE] spawn rejected: unknown template %s\n",
                    templateId.c_str());
        return 0;
    }

    std::vector<CollisionTriangle> triangles;
    buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), spec->halfExtents);

    PhysicalEntitySystem& entities = PhysicalEntitySystem::instance();
    const uint32_t entityId = entities.add(
        triangles, transform, PhysicalEntityMotion::Dynamic, 0);
    PhysicalEntity* entity = entities.find(entityId);
    if (!entity)
        return 0;

    entity->shape = PhysicalEntityShape::Box;
    entity->halfExtents = spec->halfExtents;
    entity->mass = spec->massKg;
    entity->density = spec->density;
    entity->friction = 1.0f;
    entity->restitution = 0.0f;
    entity->linearDamping = 0.02f;
    entity->angularDamping = 0.25f;
    entity->maxAngularSpeed = 12.0f;
    entity->sleepLinearThreshold = 0.15f;
    entity->sleepAngularThreshold = 0.15f;
    entity->sleepRequiredTicks = 80;
    entity->persistenceId = "vehicle-" + std::to_string(entityId);
    entity->modelPath = "generated:vehicle-box";
    entity->texturePath = "assets/textureshq/clouds11.png";
    refreshEntityMassProperties(*entity);
    entity->centerOfMass = spec->centerOfMass;
    entity->sleeping = false;
    entity->sleepTicks = 0;

    VehicleState state;
    state.id = mNextId++;
    state.templateId = templateId;
    state.entityId = entityId;
    state.driverActorId = driverActorId;
    state.mode = VehicleControlMode::Manual;
    state.gear = 1;
    state.wheels.resize(spec->wheels.size());
    mVehicles.push_back(std::move(state));

    Debug::log(Debug::Category::Physics,
               "[VEHICLE] spawned id=%u template=%s entity=%u\n",
               mVehicles.back().id, templateId.c_str(), entityId);

    return mVehicles.back().id;
}

bool VehicleSystem::despawn(uint32_t vehicleId)
{
    for (auto it = mVehicles.begin(); it != mVehicles.end(); ++it)
    {
        if (it->id != vehicleId)
            continue;
        PhysicalEntitySystem::instance().remove(it->entityId);
        if (mLocalVehicleId == vehicleId)
            mLocalVehicleId = 0;
        mVehicles.erase(it);
        return true;
    }
    return false;
}

void VehicleSystem::clear()
{
    for (VehicleState& state : mVehicles)
        PhysicalEntitySystem::instance().remove(state.entityId);
    mVehicles.clear();
    mLocalVehicleId = 0;
    mNextId = 1;
}

VehicleState* VehicleSystem::find(uint32_t vehicleId)
{
    for (VehicleState& state : mVehicles)
        if (state.id == vehicleId)
            return &state;
    return nullptr;
}

const VehicleState* VehicleSystem::find(uint32_t vehicleId) const
{
    for (const VehicleState& state : mVehicles)
        if (state.id == vehicleId)
            return &state;
    return nullptr;
}

void VehicleSystem::setInput(uint32_t vehicleId, const VehicleInput& input)
{
    if (VehicleState* state = find(vehicleId))
        state->pendingInput = input;
}

bool VehicleSystem::enterLocalDriver(uint32_t vehicleId, int seat)
{
    VehicleState* state = find(vehicleId);
    if (!state)
        return false;
    state->localDriver = true;
    state->driverSeat = seat;
    state->driverActorId = 1;
    mLocalVehicleId = vehicleId;
    if (PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state->entityId))
    {
        entity->sleeping = false;
        entity->sleepTicks = 0;
    }
    return true;
}

void VehicleSystem::exitLocalDriver()
{
    if (VehicleState* state = find(mLocalVehicleId))
    {
        state->localDriver = false;
        state->driverSeat = -1;
        state->driverActorId = 0;
    }
    mLocalVehicleId = 0;
}

bool VehicleSystem::localSeatWorld(glm::vec3& outPosition,
                                   glm::vec3& outForward) const
{
    const VehicleState* state = find(mLocalVehicleId);
    if (!state)
        return false;
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(state->templateId);
    const PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state->entityId);
    if (!spec || !entity)
        return false;
    int seatIndex = state->driverSeat;
    if (seatIndex < 0 || seatIndex >= static_cast<int>(spec->seats.size()))
        seatIndex = 0;
    if (spec->seats.empty())
        return false;

    outPosition = glm::vec3(
        entity->transform * glm::vec4(spec->seats[seatIndex].anchor, 1.0f));
    outForward = glm::normalize(entity->orientation *
                                glm::vec3(1.0f, 0.0f, 0.0f));
    return true;
}

glm::vec3 VehicleSystem::localVehiclePosition() const
{
    const VehicleState* state = find(mLocalVehicleId);
    if (!state)
        return glm::vec3(0.0f);
    const PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state->entityId);
    return entity ? glm::vec3(entity->transform[3]) : glm::vec3(0.0f);
}

VehicleInput VehicleSystem::readLocalInput(const VehicleState& state)
{
    VehicleInput input;
    InputCommandSystem& commands = InputCommandSystem::instance();

    input.throttle =
        commands.getState("walkforward").held ? 1.0f : 0.0f;
    input.brake = commands.getState("walkback").held ? 1.0f : 0.0f;
    const float left = commands.getState("walkleft").held ? 1.0f : 0.0f;
    const float right = commands.getState("walkright").held ? 1.0f : 0.0f;
    input.steer = right - left;

    const bool clutchKey = commands.isKeyHeld(GLFW_KEY_LEFT_SHIFT);
    input.clutch = state.mode == VehicleControlMode::Manual
                       ? (clutchKey ? 1.0f : 0.0f)
                       : 0.0f;
    input.handbrake = commands.isKeyHeld(GLFW_KEY_SPACE) ? 1.0f : 0.0f;

    const bool engineKey = commands.isKeyHeld(GLFW_KEY_V);
    const bool modeKey = commands.isKeyHeld(GLFW_KEY_B);
    const bool shiftUpKey = commands.isKeyHeld(GLFW_KEY_X);
    const bool shiftDownKey = commands.isKeyHeld(GLFW_KEY_Z);

    input.engineToggle = engineKey && !mPrevEngineKey;
    input.modeToggle = modeKey && !mPrevModeKey;
    input.shiftUp = shiftUpKey && !mPrevShiftUpKey;
    input.shiftDown = shiftDownKey && !mPrevShiftDownKey;

    mPrevEngineKey = engineKey;
    mPrevModeKey = modeKey;
    mPrevShiftUpKey = shiftUpKey;
    mPrevShiftDownKey = shiftDownKey;
    return input;
}

void VehicleSystem::updateDrivetrain(VehicleState& state,
                                     const VehicleSpec& spec, float dt)
{
    state.shiftTimer = std::max(0.0f, state.shiftTimer - dt);

    state.throttle = std::clamp(state.throttle, 0.0f, 1.0f);
    state.brake = std::clamp(state.brake, 0.0f, 1.0f);
    state.steer = std::clamp(state.steer, -1.0f, 1.0f);
    state.handbrake = std::clamp(state.handbrake, 0.0f, 1.0f);

    const float clutchInput = std::clamp(state.clutch, 0.0f, 1.0f);
    state.clutch = state.mode == VehicleControlMode::Automatic
                       ? (state.shiftTimer > 0.0f ? 1.0f : 0.0f)
                       : clutchInput;

    const int maxGear = maxForwardGear(spec);

    if (state.mode == VehicleControlMode::Manual)
    {
        if ((state.flagsShiftUp || state.flagsShiftDown) &&
            state.clutch > 0.5f && state.shiftTimer <= 0.0f)
        {
            const int next = std::clamp(
                state.gear + (state.flagsShiftUp ? 1 : -1), -1, maxGear);
            if (next != state.gear)
            {
                state.gear = next;
                state.shiftTimer = spec.gearbox.shiftTime;
            }
        }
    }
    else
    {
        if (state.gear == 0)
            state.gear = state.brake > 0.5f ? -1 : 1;
        if (state.gear == -1 && state.throttle > 0.1f &&
            std::fabs(state.speed) < 1.0f)
            state.gear = 1;
        if (state.gear < -1)
            state.gear = -1;
        if (state.gear > maxGear)
            state.gear = maxGear;
    }
    state.flagsShiftUp = false;
    state.flagsShiftDown = false;

    // Wheel-driven engine speed.
    float drivenAngular = 0.0f;
    int driven = 0;
    for (size_t i = 0; i < spec.wheels.size() && i < state.wheels.size(); ++i)
    {
        if (!wheelIsDriven(spec, spec.wheels[i]))
            continue;
        drivenAngular += state.wheels[i].spinVelocity;
        ++driven;
    }
    if (driven > 0)
        drivenAngular /= static_cast<float>(driven);

    const float ratio = gearRatio(spec, state.gear) * spec.gearbox.finalDrive;
    float targetRpm = std::fabs(drivenAngular) * std::fabs(ratio) *
                      60.0f / (2.0f * kPi);
    if (!state.engineOn)
        targetRpm = 0.0f;
    else if (state.gear == 0 || state.clutch > 0.5f)
        targetRpm = spec.engine.idleRpm +
                    state.throttle *
                        (spec.engine.redlineRpm - spec.engine.idleRpm);

    targetRpm = std::clamp(targetRpm, 0.0f, spec.engine.redlineRpm * 1.05f);
    const float followRate =
        (state.gear == 0 || state.clutch > 0.5f) ? 8.0f : 24.0f;
    state.engineRpm += (targetRpm - state.engineRpm) *
                       std::min(1.0f, followRate * dt);
    state.engineRpm = std::clamp(state.engineRpm, 0.0f,
                                 spec.engine.redlineRpm * 1.05f);

    if (state.mode == VehicleControlMode::Automatic && state.engineOn &&
        state.shiftTimer <= 0.0f && state.gear > 0)
    {
        if (state.engineRpm > spec.engine.redlineRpm * 0.85f &&
            state.gear < maxGear)
        {
            ++state.gear;
            state.shiftTimer = spec.gearbox.shiftTime;
        }
        else if (state.engineRpm < spec.engine.redlineRpm * 0.35f &&
                 state.gear > 1)
        {
            --state.gear;
            state.shiftTimer = spec.gearbox.shiftTime;
        }
    }

    float torque = sampleTorque(spec, state.engineRpm) * state.throttle;
    const float boostTarget =
        state.engineOn
            ? spec.engine.turboMaxBoost * state.throttle *
                  std::clamp(state.engineRpm / std::max(spec.engine.redlineRpm,
                                                        1.0f),
                             0.0f, 1.0f)
            : 0.0f;
    const float spool = std::max(spec.engine.turboLag, 0.05f);
    state.boost += (boostTarget - state.boost) *
                   std::min(1.0f, dt / spool);
    state.boost = std::clamp(state.boost, 0.0f, spec.engine.turboMaxBoost);
    torque *= 1.0f + state.boost;

    if (state.engineOn)
        torque -= spec.engine.frictionTorque;
    else
        torque = 0.0f;

    state.engineTorque = std::max(torque, 0.0f);

    // Engine torque reaction on the chassis (roll about local forward).
    if (PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId))
    {
        if (state.engineTorque > 0.0f)
        {
            const glm::vec3 axis =
                glm::normalize(entity->orientation *
                               glm::vec3(1.0f, 0.0f, 0.0f));
            applyPhysicalEntityAngularImpulse(
                *entity, axis * (-state.engineTorque * 0.05f * dt));
        }
    }
}

void VehicleSystem::updateWheel(VehicleState& state, const VehicleSpec& spec,
                                int wheelIndex, float dt, const World& world)
{
    if (wheelIndex < 0 || wheelIndex >= static_cast<int>(spec.wheels.size()) ||
        wheelIndex >= static_cast<int>(state.wheels.size()))
        return;

    const VehicleWheelSpec& wheelSpec = spec.wheels[wheelIndex];
    VehicleWheelState& wheel = state.wheels[wheelIndex];
    PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state.entityId);
    if (!entity)
        return;

    const glm::vec3 hub =
        glm::vec3(entity->transform * glm::vec4(wheelSpec.hub, 1.0f));
    glm::vec3 axis = entity->orientation * wheelSpec.suspensionAxis;
    if (glm::dot(axis, axis) < 1e-6f)
        axis = glm::vec3(0.0f, 0.0f, -1.0f);
    axis = glm::normalize(axis);

    const float ratio = gearRatio(spec, state.gear) * spec.gearbox.finalDrive;
    const int drivenCount = std::max(1, drivenWheelCount(spec));
    const float reflectedInertia =
        spec.engine.inertia * ratio * ratio / static_cast<float>(drivenCount) *
        (1.0f - state.clutch);
    const float spinInertia =
        wheelInertia(wheelSpec) +
        (wheelIsDriven(spec, wheelSpec) ? reflectedInertia : 0.0f);

    const float maxDistance =
        wheelSpec.restLength + wheelSpec.travel + wheelSpec.radius + 0.5f;
    float hitDistance = 0.0f;
    glm::vec3 normal(0.0f, 0.0f, 1.0f);
    const bool hit = raycastWorldCached(world, hub, axis, maxDistance,
                                        hitDistance, normal);
    wheel.rayHit = hit;
    wheel.rayDistance = hit ? hitDistance : 0.0f;
    wheel.rayMaxDistance = maxDistance;
    wheel.suspensionLength = hit ? hitDistance - wheelSpec.radius : 0.0f;
    const float restDistance = wheelSpec.restLength + wheelSpec.radius;
    const bool inContact =
        hit && hitDistance <= restDistance + wheelSpec.travel;

    wheel.previousCompression = wheel.compression;
    wheel.grounded = false;
    wheel.load = 0.0f;
    wheel.slipRatio = 0.0f;
    wheel.slipAngle = 0.0f;

    if (inContact)
    {
        if (glm::dot(normal, axis) > 0.0f)
            normal = -normal;
        normal = glm::normalize(normal);
        const float suspensionLength = hitDistance - wheelSpec.radius;
        const float rawCompression =
            wheelSpec.restLength - suspensionLength;
        wheel.compression =
            std::clamp(rawCompression, 0.0f, wheelSpec.travel);
        const float compressionVelocity =
            std::clamp((wheel.compression - wheel.previousCompression) / dt,
                       -15.0f, 15.0f);
        float springForce = wheelSpec.springRate * wheel.compression +
                            wheelSpec.damperRate * compressionVelocity;
        if (rawCompression > wheelSpec.travel)
        {
            const float bump = std::min(rawCompression - wheelSpec.travel,
                                        wheelSpec.travel);
            springForce += wheelSpec.springRate * 4.0f * bump;
        }
        springForce = std::clamp(springForce, 0.0f,
                                 spec.massKg * 40.0f);
        wheel.grounded = true;
        wheel.load = springForce;
        wheel.contactPoint = hub + axis * hitDistance;
        wheel.contactNormal = normal;

        // Suspension pushes the chassis up along -axis.
        applyPhysicalEntityImpulse(*entity, (-axis * springForce) * dt, hub);

        glm::vec3 forward = entity->orientation * glm::vec3(1.0f, 0.0f, 0.0f);
        forward -= normal * glm::dot(forward, normal);
        if (glm::dot(forward, forward) < 1e-6f)
            forward = glm::vec3(1.0f, 0.0f, 0.0f);
        forward = glm::normalize(forward);
        glm::vec3 right = glm::normalize(glm::cross(normal, forward));

        float steerTarget = 0.0f;
        if (wheelSpec.steer)
        {
            const float falloff =
                1.0f - std::clamp(std::fabs(state.speed) *
                                      spec.steering.speedFalloff / 30.0f,
                                  0.0f, 0.85f);
            steerTarget = state.steer *
                          glm::radians(spec.steering.maxAngleDeg) * falloff;
        }
        const float returnRate =
            std::fabs(steerTarget) < 1e-4f ? spec.steering.returnRate : 10.0f;
        wheel.steerAngle +=
            (steerTarget - wheel.steerAngle) *
            std::min(1.0f, returnRate * dt);

        const glm::quat steerRotation =
            glm::angleAxis(wheel.steerAngle, normal);
        const glm::vec3 wheelForward = steerRotation * forward;
        const glm::vec3 wheelRight = steerRotation * right;

        const glm::vec3 contactVelocity =
            physicalEntityPointVelocity(*entity, wheel.contactPoint);
        const float longSpeed = glm::dot(contactVelocity, wheelForward);
        const float latSpeed = glm::dot(contactVelocity, wheelRight);
        const float lowSpeed = std::max(spec.tire.lowSpeed, 0.01f);
        const float surfaceSpeed = wheel.spinVelocity * wheelSpec.radius;
        wheel.slipRatio =
            (surfaceSpeed - longSpeed) /
            std::max(std::fabs(longSpeed), lowSpeed);
        wheel.slipAngle = std::atan2(latSpeed,
                                     std::max(std::fabs(longSpeed), lowSpeed));

        const float normalizedRatio =
            wheel.slipRatio / std::max(spec.tire.peakSlipRatio, 0.01f);
        const float normalizedAngle =
            wheel.slipAngle / std::max(spec.tire.peakSlipAngle, 0.01f);
        const float combined =
            std::sqrt(normalizedRatio * normalizedRatio +
                      normalizedAngle * normalizedAngle);
        if (combined > 1e-4f)
        {
            float mu = spec.tire.mu *
                       (1.0f - spec.tire.loadSensitivity *
                                   (wheel.load - spec.tire.nominalLoad));
            mu = std::clamp(mu, 0.05f, spec.tire.mu * 2.0f);
            const float tireLoad =
                std::min(wheel.load, spec.tire.nominalLoad * 3.0f);
            const float forceFraction = std::min(combined, 1.0f);
            const float forceMagnitude = mu * tireLoad * forceFraction;
            const glm::vec3 forceDirection =
                (wheelForward * normalizedRatio -
                 wheelRight * normalizedAngle) /
                combined;
            const glm::vec3 force = forceDirection * forceMagnitude;
            applyPhysicalEntityImpulse(*entity, force * dt,
                                       wheel.contactPoint);
            wheel.spinVelocity -=
                glm::dot(force, wheelForward) * wheelSpec.radius /
                spinInertia * dt;

            // A non-driven wheel has no engine torque to balance the tire
            // reaction; relax it toward the rolling speed so it cannot enter a
            // numerical spin-up loop on a seam or slope.
            if (!wheelIsDriven(spec, wheelSpec))
            {
                const float targetSpin =
                    longSpeed / std::max(wheelSpec.radius, 0.01f);
                wheel.spinVelocity +=
                    (targetSpin - wheel.spinVelocity) *
                    std::min(1.0f, 12.0f * dt);
            }
        }
    }
    else
    {
        wheel.compression = 0.0f;
    }

    // Drivetrain and brakes act on the wheel spin even in the air.
    if (state.engineOn && wheelIsDriven(spec, wheelSpec) &&
        ratio != 0.0f)
    {
        const float driveTorque =
            state.engineTorque * (1.0f - state.clutch) * ratio * 0.9f /
            static_cast<float>(drivenCount);
        wheel.spinVelocity += driveTorque / spinInertia * dt;
    }

    if (wheelSpec.brake)
    {
        float brakeTorque = state.brake * spec.brakes.maxTorque;
        if (state.handbrake > 0.0f && wheelSpec.hub.x < 0.0f)
            brakeTorque += state.handbrake * spec.brakes.handbrakeTorque;
        const float maxDelta = brakeTorque * dt / spinInertia;
        if (std::fabs(wheel.spinVelocity) <= maxDelta)
            wheel.spinVelocity = 0.0f;
        else
            wheel.spinVelocity -=
                std::copysign(maxDelta, wheel.spinVelocity);
    }

    wheel.spinVelocity =
        std::clamp(wheel.spinVelocity, -400.0f, 400.0f);
    if (!wheel.grounded)
        wheel.spinVelocity *= std::max(0.0f, 1.0f - 2.0f * dt);
    wheel.spinAngle += wheel.spinVelocity * dt;
    if (wheel.spinAngle > 2.0f * kPi)
        wheel.spinAngle -= 2.0f * kPi;
    else if (wheel.spinAngle < -2.0f * kPi)
        wheel.spinAngle += 2.0f * kPi;
}

void VehicleSystem::updateChassisAero(VehicleState& state,
                                      const VehicleSpec& spec, float dt)
{
    PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state.entityId);
    if (!entity)
        return;
    const float speed = glm::length(entity->velocity);
    if (speed > 0.01f && spec.drag > 0.0f)
    {
        const glm::vec3 dragForce =
            -glm::normalize(entity->velocity) * spec.drag * speed * speed;
        applyPhysicalEntityImpulse(
            *entity, dragForce * dt,
            physicalEntityCenterOfMassWorld(*entity));
    }
    if (spec.downforce > 0.0f)
    {
        applyPhysicalEntityImpulse(
            *entity,
            glm::vec3(0.0f, 0.0f, -spec.downforce * speed * speed * dt),
            physicalEntityCenterOfMassWorld(*entity));
    }
    state.speed = glm::length(glm::vec2(entity->velocity.x,
                                       entity->velocity.y));
}

void VehicleSystem::update(float dt, const World& world)
{
    if (dt <= 0.0f)
        return;

    for (VehicleState& state : mVehicles)
    {
        PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(state.templateId);
        if (!entity || !spec)
            continue;

        if (state.wheels.size() != spec->wheels.size())
            state.wheels.resize(spec->wheels.size());

        VehicleInput input = state.localDriver ? readLocalInput(state)
                                               : state.pendingInput;
        if (!state.localDriver && input.engineToggle)
            state.engineOn = !state.engineOn;
        if (!state.localDriver && input.modeToggle)
            state.mode = state.mode == VehicleControlMode::Manual
                             ? VehicleControlMode::Automatic
                             : VehicleControlMode::Manual;
        state.flagsShiftUp = input.shiftUp;
        state.flagsShiftDown = input.shiftDown;

        if (!state.localDriver)
        {
            // Pending input edges are one-shot; continuous axes stay latched
            // until the next terminal/network update.
            state.pendingInput.engineToggle = false;
            state.pendingInput.modeToggle = false;
            state.pendingInput.shiftUp = false;
            state.pendingInput.shiftDown = false;
        }

        if (state.localDriver)
        {
            if (input.engineToggle)
                state.engineOn = !state.engineOn;
            if (input.modeToggle)
                state.mode = state.mode == VehicleControlMode::Manual
                                 ? VehicleControlMode::Automatic
                                 : VehicleControlMode::Manual;
            state.throttle = input.throttle;
            state.brake = input.brake;
            state.steer = input.steer;
            state.clutch = input.clutch;
            state.handbrake = input.handbrake;
        }
        else
        {
            state.throttle = input.throttle;
            state.brake = input.brake;
            state.steer = input.steer;
            state.clutch = input.clutch;
            state.handbrake = input.handbrake;
        }

        updateDrivetrain(state, *spec, dt);
        for (int i = 0; i < static_cast<int>(spec->wheels.size()); ++i)
            updateWheel(state, *spec, i, dt, world);
        updateChassisAero(state, *spec, dt);

        if (state.engineOn || state.localDriver || state.throttle > 0.01f ||
            state.brake > 0.01f || state.steer != 0.0f)
        {
            entity->sleeping = false;
            entity->sleepTicks = 0;
        }
    }
}
