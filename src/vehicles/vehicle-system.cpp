// 2026-10-27

#include "vehicles/vehicle-system.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "entities/player.h"
#include "impact/destructible-world-config.h"
#include "input/input-commands.h"
#include "interaction/interaction-system.h"
#include "camera.h"
#include "perf/perf-spike.h"
#include "physics/movement/physics-collision.h"
#include "physics/physical-entity.h"
#include "physics/ray-utils.h"
#include "terminal/terminal-state.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-audio.h"
#include "void-death/void-death.h"
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

bool raycastCandidates(const World& world, const glm::vec3& origin,
                       const glm::vec3& direction, float maxDistance,
                       const std::vector<int>& candidates,
                       float& hitDistance, glm::vec3& hitNormal)
{
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

int bestSeatForCamera(const VehicleSpec& spec, const PhysicalEntity& entity,
                      const Camera& camera)
{
    if (spec.seats.empty())
        return -1;

    const glm::vec3 origin = camera.pos;
    glm::vec3 direction = camera.front;
    if (glm::dot(direction, direction) < 1e-6f)
        return 0;
    direction = glm::normalize(direction);

    float bestDistance = 6.0f;
    int bestSeat = -1;
    for (size_t i = 0; i < spec.seats.size(); ++i)
    {
        const glm::vec3 center = glm::vec3(
            entity.transform * glm::vec4(spec.seats[i].anchor, 1.0f));
        const float along = glm::dot(center - origin, direction);
        if (along < 0.0f || along > bestDistance)
            continue;
        const glm::vec3 closest = origin + direction * along;
        if (glm::length(center - closest) > 0.9f)
            continue;
        bestDistance = along;
        bestSeat = static_cast<int>(i);
    }
    return bestSeat >= 0 ? bestSeat : 0;
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
    entity->texturePath = spec->bodyTexture.empty()
                              ? "assets/textureshq/240sx.png"
                              : spec->bodyTexture;
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
    state.spawnOrigin = glm::vec3(transform[3]);
    state.massKg = spec->massKg;
    state.wheels.resize(spec->wheels.size());
    mVehicles.push_back(std::move(state));

    VehicleState& created = mVehicles.back();
    InteractionTarget target;
    target.owner = "vehicle:" + std::to_string(created.id);
    target.label = "enter driver seat";
    target.promptOrigin = glm::vec3(transform[3]) +
                          glm::vec3(0.0f, 0.0f, spec->halfExtents.z + 1.0f);
    target.interactOrigin = glm::vec3(transform[3]);
    target.radius = std::max(1.0f, spec->interactionRadius);
    target.priority = 10;
    const uint32_t vehicleId = created.id;
    target.activate = [vehicleId](const InteractionContext& context) {
        return VehicleSystem::instance().activateVehicleInteraction(
            vehicleId, context.camera);
    };
    created.interactionTargetId = InteractionSystem::instance().add(target);

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
        InteractionSystem::instance().clearOwner(
            "vehicle:" + std::to_string(vehicleId));
        stopVehicleEngineAudio(vehicleId);
        PhysicalEntitySystem::instance().remove(it->entityId);
        if (mLocalVehicleId == vehicleId)
            mLocalVehicleId = 0;
        InteractionSystem::instance().setStickyTarget(0);
        mVehicles.erase(it);
        return true;
    }
    return false;
}

void VehicleSystem::clear()
{
    for (VehicleState& state : mVehicles)
    {
        InteractionSystem::instance().clearOwner(
            "vehicle:" + std::to_string(state.id));
        stopVehicleEngineAudio(state.id);
        PhysicalEntitySystem::instance().remove(state.entityId);
    }
    mVehicles.clear();
    mLocalVehicleId = 0;
    InteractionSystem::instance().setStickyTarget(0);
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
    {
        state->pendingInput = input;
        state->inputOverride = true;
    }
}

bool VehicleSystem::enterLocalDriver(uint32_t vehicleId, int seat)
{
    VehicleState* state = find(vehicleId);
    if (!state)
        return false;
    state->localDriver = true;
    state->driverSeat = seat;
    state->driverActorId = 1;
    state->inputOverride = false;
    mLocalVehicleId = vehicleId;
    InteractionSystem::instance().setStickyTarget(
        state->interactionTargetId);
    if (PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state->entityId))
    {
        entity->sleeping = false;
        entity->sleepTicks = 0;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.entered", std::to_string(vehicleId),
        "local actor entered vehicle seat", 0,
        {{"vehicleId", vehicleId}, {"seat", seat}}, __FILE__, __LINE__,
        __func__);
    return true;
}

void VehicleSystem::exitLocalDriver()
{
    const uint32_t vehicleId = mLocalVehicleId;
    if (VehicleState* state = find(mLocalVehicleId))
    {
        state->localDriver = false;
        state->driverSeat = -1;
        state->driverActorId = 0;
        state->inputOverride = false;
    }
    mLocalVehicleId = 0;
    InteractionSystem::instance().setStickyTarget(0);
    if (vehicleId != 0)
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.exited", std::to_string(vehicleId),
            "local actor left vehicle seat", 0,
            {{"vehicleId", vehicleId}}, __FILE__, __LINE__, __func__);
}

uint32_t VehicleSystem::localVehicleEntityId() const
{
    const VehicleState* state = find(mLocalVehicleId);
    return state ? state->entityId : 0;
}

bool VehicleSystem::activateVehicleInteraction(uint32_t vehicleId,
                                               const Camera* camera)
{
    VehicleState* state = find(vehicleId);
    if (!state)
        return false;

    if (state->localDriver)
    {
        glm::vec3 seatPosition(0.0f);
        glm::vec3 seatForward(0.0f, 1.0f, 0.0f);
        if (localSeatWorld(seatPosition, seatForward))
        {
            const glm::vec3 right =
                glm::normalize(glm::cross(seatForward,
                                          glm::vec3(0.0f, 0.0f, 1.0f)));
            THE_PLAYER.pos = seatPosition + right * 1.4f;
            THE_PLAYER.vel = glm::vec3(0.0f);
            THE_PLAYER.externalImpulse = glm::vec3(0.0f);
        }
        exitLocalDriver();
        return true;
    }

    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(state->templateId);
    const PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state->entityId);
    if (!spec || !entity || !camera)
        return false;

    int seat = bestSeatForCamera(*spec, *entity, *camera);
    if (seat < 0)
        seat = 0;
    return enterLocalDriver(vehicleId, seat);
}

void VehicleSystem::checkVoidDeath(Player& player, float killZ)
{
    std::vector<uint32_t> doomed;
    for (const VehicleState& state : mVehicles)
    {
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        if (!entity)
            continue;
        if (entity->transform[3].z >= killZ)
            continue;
        doomed.push_back(state.id);
        if (state.localDriver)
        {
            glm::vec3 seatPosition(0.0f);
            glm::vec3 seatForward(0.0f, 1.0f, 0.0f);
            localSeatWorld(seatPosition, seatForward);
            exitLocalDriver();
            player.pos = seatPosition;
            player.vel = glm::vec3(0.0f);
            player.externalImpulse = glm::vec3(0.0f);
            player.inputWishMove = glm::vec2(0.0f);
            if (!player.dead)
                ::checkVoidDeath(player, player.username, "player");
        }

        nlohmann::json fields;
        fields["vehicleId"] = state.id;
        fields["entityId"] = state.entityId;
        fields["z"] = entity->transform[3].z;
        fields["killZ"] = killZ;
        fields["driver"] = state.localDriver;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.void", std::to_string(state.id),
            "vehicle fell below the void kill plane and was destroyed", 0,
            fields, __FILE__, __LINE__, __func__);
    }

    for (uint32_t id : doomed)
        despawn(id);
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
                                int wheelIndex, float dt, const World& world,
                                const std::vector<int>& candidates)
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
    const bool hit = raycastCandidates(world, hub, axis, maxDistance,
                                       candidates, hitDistance, normal);
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
        float springForce = wheelSpec.springRate * state.springScale *
                                wheel.compression +
                            wheelSpec.damperRate * state.damperScale *
                                compressionVelocity;
        if (rawCompression > wheelSpec.travel)
        {
            const float bump = std::min(rawCompression - wheelSpec.travel,
                                        wheelSpec.travel);
            springForce += wheelSpec.springRate * state.springScale * 4.0f *
                           bump;
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
            float mu = spec.tire.mu * state.gripScale *
                       (1.0f - spec.tire.loadSensitivity *
                                   (wheel.load - spec.tire.nominalLoad));
            mu = std::clamp(mu, 0.05f, spec.tire.mu * state.gripScale * 2.0f);
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
            state.engineTorque * state.torqueScale *
            (1.0f - state.clutch) * ratio * 0.9f /
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
            -glm::normalize(entity->velocity) * spec.drag *
            state.dragScale * speed * speed;
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
    MIMITA_PERF_SCOPE("Vehicle::Update");
    if (dt <= 0.0f)
        return;

    const auto started = std::chrono::steady_clock::now();
    int awakeVehicles = 0;

    for (VehicleState& state : mVehicles)
    {
        PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(state.templateId);
        if (!entity || !spec)
            continue;

        const uint64_t simulationTick =
            PhysicalEntitySystem::instance().simulationTick();

        if (state.wheels.size() != spec->wheels.size())
            state.wheels.resize(spec->wheels.size());

        if (state.interactionTargetId != 0)
        {
            const glm::vec3 origin(entity->transform[3]);
            const glm::vec3 promptOrigin =
                origin + glm::vec3(0.0f, 0.0f,
                                   spec->halfExtents.z + 1.0f);
            std::string label = "exit vehicle";
            if (!state.localDriver)
            {
                const int seat =
                    bestSeatForCamera(*spec, *entity, THE_CAMERA);
                const std::string seatName =
                    seat >= 0 &&
                            seat < static_cast<int>(spec->seats.size()) &&
                            !spec->seats[seat].name.empty()
                        ? spec->seats[seat].name
                        : "driver";
                label = "enter " + seatName + " seat";
            }
            InteractionSystem::instance().updateTarget(
                state.interactionTargetId, promptOrigin, origin, label,
                spec->interactionRadius, true);
        }

        VehicleInput input = (state.localDriver && !state.inputOverride)
                                 ? readLocalInput(state)
                                 : state.pendingInput;
        if (!state.localDriver && input.engineToggle)
            state.engineOn = !state.engineOn;
        if (!state.localDriver && input.modeToggle)
            state.mode = state.mode == VehicleControlMode::Manual
                             ? VehicleControlMode::Automatic
                             : VehicleControlMode::Manual;
        state.flagsShiftUp = input.shiftUp;
        state.flagsShiftDown = input.shiftDown;

        if (!state.localDriver || state.inputOverride)
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

        // One broadphase gather per vehicle per fixed tick. All wheel rays test
        // the same candidate triangle list instead of gathering four times.
        static thread_local std::vector<int> sWheelCandidates;
        sWheelCandidates.clear();
        {
            AABB wheelBox;
            bool first = true;
            for (size_t i = 0; i < spec->wheels.size(); ++i)
            {
                const VehicleWheelSpec& wheelSpec = spec->wheels[i];
                const glm::vec3 hub = glm::vec3(
                    entity->transform * glm::vec4(wheelSpec.hub, 1.0f));
                glm::vec3 axis = entity->orientation *
                                 wheelSpec.suspensionAxis;
                if (glm::dot(axis, axis) < 1e-6f)
                    axis = glm::vec3(0.0f, 0.0f, -1.0f);
                axis = glm::normalize(axis);
                const float maxDistance = wheelSpec.restLength +
                                          wheelSpec.travel +
                                          wheelSpec.radius + 0.5f;
                const glm::vec3 end = hub + axis * maxDistance;
                const glm::vec3 lo = glm::min(hub, end) -
                                     glm::vec3(0.06f);
                const glm::vec3 hi = glm::max(hub, end) +
                                     glm::vec3(0.06f);
                if (first)
                {
                    wheelBox.min = lo;
                    wheelBox.max = hi;
                    first = false;
                }
                else
                {
                    wheelBox.min = glm::min(wheelBox.min, lo);
                    wheelBox.max = glm::max(wheelBox.max, hi);
                }
            }
            if (!first)
                appendChunkTrianglesForAABB(world, wheelBox, 0.0f,
                                            sWheelCandidates,
                                            "vehicleWheel", true);
        }

        for (int i = 0; i < static_cast<int>(spec->wheels.size()); ++i)
            updateWheel(state, *spec, i, dt, world, sWheelCandidates);
        updateChassisAero(state, *spec, dt);

        // Origin and per-part telemetry. The same data will be replicated by
        // the future server vehicle owner.
        state.origin = glm::vec3(entity->transform[3]);
        state.orientation = entity->orientation;
        state.originVelocity = entity->velocity;
        state.originAngularVelocity = entity->angularVelocity;
        state.distanceFromSpawn =
            glm::length(state.origin - state.spawnOrigin);
        state.massKg = entity->mass;
        state.weightN =
            entity->mass *
            std::fabs(MimitaImpact::DestructibleWorldConfig::instance()
                          .objectGravity());
        for (size_t i = 0; i < spec->wheels.size() &&
                           i < state.wheels.size();
             ++i)
        {
            const VehicleWheelSpec& wheelSpec = spec->wheels[i];
            VehicleWheelState& wheel = state.wheels[i];
            const glm::vec3 hub = glm::vec3(
                entity->transform * glm::vec4(wheelSpec.hub, 1.0f));
            glm::vec3 axis = entity->orientation * wheelSpec.suspensionAxis;
            if (glm::dot(axis, axis) < 1e-6f)
                axis = glm::vec3(0.0f, 0.0f, -1.0f);
            axis = glm::normalize(axis);
            const float length = wheel.rayHit
                                     ? std::clamp(wheel.suspensionLength,
                                                  0.0f,
                                                  wheelSpec.restLength)
                                     : wheelSpec.restLength;
            wheel.worldCenter = hub + axis * length;
            glm::vec3 spinAxis = entity->orientation * wheelSpec.spinAxis;
            glm::vec3 steerAxis = entity->orientation * wheelSpec.steerAxis;
            wheel.spinAxisWorld =
                glm::dot(spinAxis, spinAxis) > 1e-8f
                    ? glm::normalize(spinAxis)
                    : glm::vec3(0.0f, 1.0f, 0.0f);
            wheel.steerAxisWorld =
                glm::dot(steerAxis, steerAxis) > 1e-8f
                    ? glm::normalize(steerAxis)
                    : glm::vec3(0.0f, 0.0f, 1.0f);
            wheel.worldVelocity = physicalEntityPointVelocity(
                *entity, wheel.worldCenter);
        }

        if (state.engineOn || state.throttle > 0.01f ||
            state.brake > 0.01f || state.steer != 0.0f ||
            std::fabs(state.speed) > 0.05f)
        {
            entity->sleeping = false;
            entity->sleepTicks = 0;
        }
        if (!entity->sleeping)
            ++awakeVehicles;

        updateVehicleEngineAudio(state, *spec, state.origin,
                                 simulationTick);

        if (simulationTick - state.lastSummaryTick >= 60)
        {
            state.lastSummaryTick = simulationTick;
            nlohmann::json fields;
            fields["vehicleId"] = state.id;
            fields["template"] = state.templateId;
            fields["entityId"] = state.entityId;
            fields["origin"] = {state.origin.x, state.origin.y,
                                state.origin.z};
            fields["spawnOrigin"] = {state.spawnOrigin.x,
                                     state.spawnOrigin.y,
                                     state.spawnOrigin.z};
            fields["distanceFromSpawn"] = state.distanceFromSpawn;
            fields["velocity"] = {state.originVelocity.x,
                                  state.originVelocity.y,
                                  state.originVelocity.z};
            fields["angularVelocity"] = {state.originAngularVelocity.x,
                                         state.originAngularVelocity.y,
                                         state.originAngularVelocity.z};
            fields["orientation"] = {state.orientation.x,
                                     state.orientation.y,
                                     state.orientation.z,
                                     state.orientation.w};
            fields["massKg"] = state.massKg;
            fields["weightN"] = state.weightN;
            fields["engineOn"] = state.engineOn;
            fields["mode"] = state.mode == VehicleControlMode::Automatic
                                 ? "AUTO"
                                 : "MANUAL";
            fields["gear"] = state.gear;
            fields["rpm"] = state.engineRpm;
            fields["torque"] = state.engineTorque;
            fields["boost"] = state.boost;
            fields["speed"] = state.speed;
            fields["throttle"] = state.throttle;
            fields["brake"] = state.brake;
            fields["steer"] = state.steer;
            fields["clutch"] = state.clutch;
            fields["handbrake"] = state.handbrake;
            fields["localDriver"] = state.localDriver;
            fields["driverSeat"] = state.driverSeat;
            nlohmann::json wheelsJson = nlohmann::json::array();
            for (size_t i = 0; i < state.wheels.size(); ++i)
            {
                const VehicleWheelState& wheel = state.wheels[i];
                wheelsJson.push_back({
                    {"index", i},
                    {"grounded", wheel.grounded},
                    {"worldCenter", {wheel.worldCenter.x,
                                     wheel.worldCenter.y,
                                     wheel.worldCenter.z}},
                    {"spinAxisWorld", {wheel.spinAxisWorld.x,
                                       wheel.spinAxisWorld.y,
                                       wheel.spinAxisWorld.z}},
                    {"steerAxisWorld", {wheel.steerAxisWorld.x,
                                        wheel.steerAxisWorld.y,
                                        wheel.steerAxisWorld.z}},
                    {"spinAngle", wheel.spinAngle},
                    {"spinVelocity", wheel.spinVelocity},
                    {"steerAngle", wheel.steerAngle},
                    {"suspensionLength", wheel.suspensionLength},
                    {"compression", wheel.compression},
                    {"load", wheel.load},
                    {"slipRatio", wheel.slipRatio},
                    {"slipAngle", wheel.slipAngle},
                    {"worldVelocity", {wheel.worldVelocity.x,
                                       wheel.worldVelocity.y,
                                       wheel.worldVelocity.z}}});
            }
            fields["wheels"] = wheelsJson;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.tick.summary", std::to_string(state.id),
                "sampled vehicle origin and per-part state",
                simulationTick, fields, __FILE__, __LINE__, __func__);
        }
    }

    static uint64_t sLastPerfTick = 0;
    const uint64_t nowTick =
        PhysicalEntitySystem::instance().simulationTick();
    if (nowTick - sLastPerfTick >= 60)
    {
        sLastPerfTick = nowTick;
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started)
                .count();
        nlohmann::json fields;
        fields["vehicles"] = mVehicles.size();
        fields["awakeVehicles"] = awakeVehicles;
        fields["updateMs"] = elapsedMs;
        fields["physicsEntities"] =
            PhysicalEntitySystem::instance().entities().size();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.performance", "vehicle-performance",
            "sampled vehicle fixed-tick cost", nowTick, fields, __FILE__,
            __LINE__, __func__);
    }
}
