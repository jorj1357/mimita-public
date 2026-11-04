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
#include "network/multiplayer-context.h"
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

float wheelInertia(const VehicleWheelSpec& wheel);
float sampleTorque(const VehicleSpec& spec, float rpm);
float gearRatio(const VehicleSpec& spec, int gear);
int maxForwardGear(const VehicleSpec& spec);

void logVehicleWheelSpec(const VehicleState& state, const VehicleSpec& spec)
{
    nlohmann::json wheels = nlohmann::json::array();
    for (size_t i = 0; i < spec.wheels.size(); ++i)
    {
        const VehicleWheelSpec& wheel = spec.wheels[i];
        wheels.push_back({
            {"index", i},
            {"hub", {wheel.hub.x, wheel.hub.y, wheel.hub.z}},
            {"radius", wheel.radius},
            {"width", wheel.width},
            {"restLength", wheel.restLength},
            {"travel", wheel.travel},
            {"springRate", wheel.springRate},
            {"damperRate", wheel.damperRate},
            {"steer", wheel.steer},
            {"drive", wheel.drive},
            {"brake", wheel.brake},
        });
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.wheel.spec", std::to_string(state.id),
        "wheel geometry loaded from the vehicle template", 0,
        {{"vehicle_id", state.id},
         {"template", state.templateId},
         {"edit_file", "config/vehicles/" + state.templateId + ".json"},
         {"wheelSegments", spec.wheelSegments},
         {"wheels", wheels}},
        __FILE__, __LINE__, __func__);
}

void logVehicleTuning(const VehicleState& state, const VehicleSpec& spec)
{
    const float gravity = std::fabs(
        MimitaImpact::DestructibleWorldConfig::instance().objectGravity());
    const int wheelCount =
        std::max(1, static_cast<int>(spec.wheels.size()));
    const float staticLoad = spec.massKg * gravity /
                             static_cast<float>(wheelCount);
    float minSpring = 1.0e9f;
    float minTravel = 1.0e9f;
    float maxSpinInertia = 0.0f;
    float maxSpinVelocity = 0.0f;
    nlohmann::json wheels = nlohmann::json::array();
    for (size_t i = 0; i < spec.wheels.size(); ++i)
    {
        const VehicleWheelSpec& wheel = spec.wheels[i];
        minSpring = std::min(minSpring, wheel.springRate);
        minTravel = std::min(minTravel, wheel.travel);
        maxSpinInertia = std::max(maxSpinInertia,
                                  wheelInertia(wheel));
        maxSpinVelocity = std::max(maxSpinVelocity,
                                   wheel.maxSpinVelocity);
        wheels.push_back({
            {"index", i},
            {"unsprungMass", wheel.unsprungMass},
            {"spinInertia", wheelInertia(wheel)},
            {"maxSpinVelocity", wheel.maxSpinVelocity},
            {"camberDeg", wheel.camberDeg},
            {"toeDeg", wheel.toeDeg},
            {"casterDeg", wheel.casterDeg},
            {"springRate", wheel.springRate},
            {"damperRate", wheel.damperRate},
            {"travel", wheel.travel},
            {"antiRoll", wheel.antiRoll},
            {"antiRollDamping", wheel.antiRollDamping},
        });
    }
    const float staticCompression =
        minSpring > 0.0f ? staticLoad / minSpring : 1.0e9f;
    const float effectiveMu =
        spec.tire.mu *
        (1.0f - spec.tire.loadSensitivity *
                    (staticLoad - spec.tire.nominalLoad));

    float avgRadius = 0.0f;
    for (const VehicleWheelSpec& wheel : spec.wheels)
        avgRadius += wheel.radius;
    if (!spec.wheels.empty())
        avgRadius /= static_cast<float>(spec.wheels.size());
    const int topGear = maxForwardGear(spec);
    const float topTotalRatio =
        gearRatio(spec, topGear) * spec.gearbox.finalDrive;
    const float topWheelAngular =
        spec.engine.redlineRpm * 2.0f * kPi / 60.0f /
        std::max(std::fabs(topTotalRatio), 0.01f);
    const float estimatedTopKmh =
        topWheelAngular * avgRadius * 3.6f;
    const float firstTotalRatio =
        gearRatio(spec, 1) * spec.gearbox.finalDrive;
    const float launchRpm = std::max(spec.engine.idleRpm + 1000.0f, 2000.0f);
    const float launchTorque = sampleTorque(spec, launchRpm);
    const float launchForce =
        launchTorque * firstTotalRatio * 0.9f /
        std::max(avgRadius, 0.01f);
    const float estimated0to60 =
        16.667f / std::max(launchForce / spec.massKg, 0.01f);

    nlohmann::json warnings = nlohmann::json::array();
    if (staticCompression > minTravel * 0.75f)
        warnings.push_back("static compression exceeds 75% of travel");
    if (staticCompression >= minTravel)
        warnings.push_back("spring bottoms out at rest");
    if (effectiveMu < 0.4f)
        warnings.push_back("effective tire mu below 0.4 at static load");
    if (spec.maxSuspensionForce < staticLoad * 1.2f)
        warnings.push_back("maxSuspensionForce below 1.2x static load");
    if (spec.tire.nominalLoad <= 0.0f ||
        std::fabs(spec.tire.nominalLoad - staticLoad) > staticLoad * 0.35f)
        warnings.push_back("tire nominalLoad is far from static corner load");
    if (spec.targetZeroTo60Seconds > 0.0f &&
        std::fabs(estimated0to60 - spec.targetZeroTo60Seconds) >
            spec.targetZeroTo60Seconds * 0.15f)
        warnings.push_back("estimated 0-60 km/h misses speedTargets");
    if (spec.targetTopSpeedKmh > 0.0f &&
        std::fabs(estimatedTopKmh - spec.targetTopSpeedKmh) >
            spec.targetTopSpeedKmh * 0.10f)
        warnings.push_back("estimated top speed misses speedTargets");

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.tuning", std::to_string(state.id),
        "effective vehicle tuning loaded from the template", 0,
        {{"vehicle_id", state.id},
         {"template", state.templateId},
         {"edit_file", "config/vehicles/" + state.templateId + ".json"},
         {"massKg", spec.massKg},
         {"centerOfMass", {spec.centerOfMass.x, spec.centerOfMass.y,
                           spec.centerOfMass.z}},
         {"bodyFriction", spec.bodyFriction},
         {"bodyWallFriction", spec.bodyWallFriction},
         {"bodyRestitution", spec.bodyRestitution},
         {"reboundScale", spec.reboundScale},
         {"differential", {{"type", spec.differential.type},
                            {"preloadTorque", spec.differential.preloadTorque},
                            {"powerLock", spec.differential.powerLock},
                            {"coastLock", spec.differential.coastLock},
                            {"lockStrength", spec.differential.lockStrength},
                            {"biasRatio", spec.differential.biasRatio}}},
         {"steering", {{"maxAngleDeg", spec.steering.maxAngleDeg},
                        {"speedFalloff", spec.steering.speedFalloff},
                        {"directionSign", spec.steering.directionSign},
                        {"ackermann", spec.steering.ackermann}}},
         {"brakes", {{"maxTorque", spec.brakes.maxTorque},
                      {"handbrakeTorque", spec.brakes.handbrakeTorque},
                      {"handbrakeGripScale",
                       spec.brakes.handbrakeGripScale}}},
         {"coastDeceleration", spec.coastDeceleration},
         {"coastSpinDamping", spec.coastSpinDamping},
         {"linearDamping", spec.linearDamping},
         {"angularDamping", spec.angularDamping},
         {"maxAngularSpeed", spec.maxAngularSpeed},
         {"maxSuspensionForce", spec.maxSuspensionForce},
         {"bumpStopStiffness", spec.bumpStopStiffness},
         {"bumpStopDamping", spec.bumpStopDamping},
         {"antiRollForceScale", spec.antiRollForceScale},
         {"stabilityVerticalDamping", spec.stabilityVerticalDamping},
         {"stabilityAngularDamping", spec.stabilityAngularDamping},
         {"idleSettleSpeed", spec.idleSettleSpeed},
         {"speedTargets", {{"zeroTo60KmhSeconds",
                            spec.targetZeroTo60Seconds},
                           {"topSpeedKmh", spec.targetTopSpeedKmh}}},
         {"estimated", {{"zeroTo60KmhSeconds", estimated0to60},
                        {"topSpeedKmh", estimatedTopKmh},
                        {"firstTotalRatio", firstTotalRatio},
                        {"topTotalRatio", topTotalRatio},
                        {"launchTorqueNm", launchTorque},
                        {"launchForceN", launchForce}}},
         {"tire", {{"mu", spec.tire.mu},
                   {"loadSensitivity", spec.tire.loadSensitivity},
                   {"nominalLoad", spec.tire.nominalLoad},
                   {"peakSlipRatio", spec.tire.peakSlipRatio},
                   {"peakSlipAngle", spec.tire.peakSlipAngle},
                   {"lowSpeed", spec.tire.lowSpeed},
                   {"lowSpeedGrip", spec.tire.lowSpeedGrip},
                   {"rollingResistance", spec.tire.rollingResistance},
                   {"maxLoadScale", spec.tire.maxLoadScale}}},
         {"engine", {{"inertia", spec.engine.inertia},
                     {"idleRpm", spec.engine.idleRpm},
                     {"redlineRpm", spec.engine.redlineRpm},
                     {"frictionTorque", spec.engine.frictionTorque},
                     {"turboMaxBoost", spec.engine.turboMaxBoost}}},
         {"wheels", wheels}},
        __FILE__, __LINE__, __func__);

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics,
        warnings.empty() ? StructuredLevel::Important
                         : StructuredLevel::Errors,
        "vehicle.config.validation", std::to_string(state.id),
        warnings.empty() ? "vehicle tuning is coherent"
                         : "vehicle tuning has static-load warnings", 0,
        {{"vehicle_id", state.id},
         {"template", state.templateId},
         {"gravity", gravity},
         {"wheelCount", wheelCount},
         {"staticLoadPerWheel", staticLoad},
         {"staticCompression", staticCompression},
         {"travel", minTravel},
         {"staticCompressionRatio",
          minTravel > 0.0f ? staticCompression / minTravel : 0.0f},
         {"effectiveMu", effectiveMu},
         {"estimatedZeroTo60KmhSeconds", estimated0to60},
         {"estimatedTopSpeedKmh", estimatedTopKmh},
         {"targetZeroTo60KmhSeconds", spec.targetZeroTo60Seconds},
         {"targetTopSpeedKmh", spec.targetTopSpeedKmh},
         {"maxSpinInertia", maxSpinInertia},
         {"maxSpinVelocity", maxSpinVelocity},
         {"warnings", warnings}},
        __FILE__, __LINE__, __func__);
}

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
                       float& hitDistance, glm::vec3& hitNormal,
                       int* hitTriangleId = nullptr)
{
    float nearest = maxDistance;
    bool found = false;
    int nearestId = -1;
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
            nearestId = index;
            found = true;
        }
    }
    if (found)
    {
        hitDistance = nearest;
        if (hitTriangleId)
            *hitTriangleId = nearestId;
    }
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
    if (wheel.spinInertia > 0.0f)
        return std::max(wheel.spinInertia, 0.01f);
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
    return spawnInternal(templateId, transform, driverActorId, 0, false);
}

uint32_t VehicleSystem::spawnAuthoritative(const std::string& templateId,
                                           const glm::mat4& transform,
                                           uint32_t chassisNetworkId,
                                           uint32_t driverActorId)
{
    return spawnInternal(templateId, transform, driverActorId,
                         chassisNetworkId, true);
}

uint32_t VehicleSystem::spawnInternal(const std::string& templateId,
                                      const glm::mat4& transform,
                                      uint32_t driverActorId,
                                      uint32_t chassisNetworkId,
                                      bool authoritative)
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
    entity->friction = spec->bodyFriction;
    entity->wallFriction = spec->bodyWallFriction;
    entity->restitution = spec->bodyRestitution;
    entity->linearDamping = spec->linearDamping;
    entity->angularDamping = spec->angularDamping;
    entity->maxAngularSpeed = spec->maxAngularSpeed;
    entity->sleepLinearThreshold = spec->sleepLinearThreshold;
    entity->sleepAngularThreshold = spec->sleepAngularThreshold;
    entity->sleepRequiredTicks = spec->sleepRequiredTicks;
    entity->persistenceId = "vehicle-" + std::to_string(entityId);
    entity->networkId = chassisNetworkId;
    entity->replicate = chassisNetworkId != 0;
    entity->serverSimulated = authoritative;
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
    state.chassisNetworkId = chassisNetworkId;
    state.serverAuthoritative = authoritative;
    state.serverMirror = false;
    state.presentationEnabled = !authoritative;
    state.driverActorId = driverActorId;
    state.mode = VehicleControlMode::Manual;
    state.gear = 1;
    state.clutch = 0.0f;
    state.spawnOrigin = glm::vec3(transform[3]);
    state.massKg = spec->massKg;
    state.wheels.resize(spec->wheels.size());
    state.seatOccupants.assign(spec->seats.size(), 0);
    if (driverActorId != 0 && !state.seatOccupants.empty())
    {
        state.seatOccupants[0] = static_cast<int>(driverActorId);
        state.driverSeat = 0;
    }
    mVehicles.push_back(std::move(state));

    VehicleState& created = mVehicles.back();
    if (!authoritative)
    {
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
    }

    Debug::log(Debug::Category::Physics,
               "[VEHICLE] spawned id=%u template=%s entity=%u network=%u "
               "authoritative=%d\n",
               mVehicles.back().id, templateId.c_str(), entityId,
               chassisNetworkId, authoritative ? 1 : 0);
    logVehicleWheelSpec(mVehicles.back(), *spec);
    logVehicleTuning(mVehicles.back(), *spec);

    return mVehicles.back().id;
}

bool VehicleSystem::spawnMirror(uint32_t vehicleId,
                                const std::string& templateId,
                                uint32_t chassisNetworkId,
                                const glm::mat4& transform,
                                const std::vector<int>& occupants)
{
    if (vehicleId == 0 || chassisNetworkId == 0)
        return false;
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(templateId);
    if (!spec)
    {
        Debug::warn(Debug::Category::Physics,
                    "[VEHICLE NET] mirror rejected: unknown template %s\n",
                    templateId.c_str());
        return false;
    }
    if (VehicleState* existing = find(vehicleId))
    {
        existing->templateId = templateId;
        existing->chassisNetworkId = chassisNetworkId;
        existing->seatOccupants = occupants;
        if (PhysicalEntity* entity =
                PhysicalEntitySystem::instance().find(existing->entityId))
        {
            entity->transform = transform;
            entity->previousTransform = transform;
            entity->orientation =
                glm::normalize(glm::quat_cast(glm::mat3(transform)));
        }
        return true;
    }

    PhysicalEntitySystem& entities = PhysicalEntitySystem::instance();
    PhysicalEntity* entity = entities.findByNetworkId(chassisNetworkId);
    uint32_t entityId = 0;
    if (entity)
    {
        entityId = entity->id;
        entity->transform = transform;
        entity->previousTransform = transform;
        entity->orientation =
            glm::normalize(glm::quat_cast(glm::mat3(transform)));
    }
    else
    {
        std::vector<CollisionTriangle> triangles;
        buildBoxCollisionTriangles(triangles, glm::vec3(0.0f),
                                   spec->halfExtents);
        entityId = entities.addReplicated(chassisNetworkId, triangles,
                                          transform,
                                          PhysicalEntityMotion::Dynamic, 0);
        if (entityId == 0)
            return false;
        entity = entities.find(entityId);
        if (!entity)
            return false;
        entity->halfExtents = spec->halfExtents;
        entity->shape = PhysicalEntityShape::Box;
        entity->density = spec->density;
        entity->friction = spec->bodyFriction;
        entity->wallFriction = spec->bodyWallFriction;
        entity->restitution = spec->bodyRestitution;
        entity->texturePath = spec->bodyTexture;
        entity->modelPath = "generated:vehicle-box";
    }

    VehicleState state;
    state.id = vehicleId;
    state.templateId = templateId;
    state.entityId = entityId;
    state.chassisNetworkId = chassisNetworkId;
    state.serverAuthoritative = true;
    state.serverMirror = true;
    state.presentationEnabled = true;
    state.mode = VehicleControlMode::Manual;
    state.gear = 1;
    state.clutch = 0.0f;
    state.spawnOrigin = glm::vec3(transform[3]);
    state.origin = state.spawnOrigin;
    state.orientation =
        glm::normalize(glm::quat_cast(glm::mat3(transform)));
    state.massKg = spec->massKg;
    state.wheels.resize(spec->wheels.size());
    state.seatOccupants = occupants;
    state.seatOccupants.resize(spec->seats.size(), 0);
    mVehicles.push_back(std::move(state));
    if (mNextId <= vehicleId)
        mNextId = vehicleId + 1;

    VehicleState& created = mVehicles.back();
    InteractionTarget target;
    target.owner = "vehicle:" + std::to_string(created.id);
    target.label = "enter driver seat";
    target.promptOrigin = glm::vec3(transform[3]) +
                          glm::vec3(0.0f, 0.0f, spec->halfExtents.z + 1.0f);
    target.interactOrigin = glm::vec3(transform[3]);
    target.radius = std::max(1.0f, spec->interactionRadius);
    target.priority = 10;
    const uint32_t mirrorVehicleId = created.id;
    target.activate = [mirrorVehicleId](const InteractionContext& context) {
        return VehicleSystem::instance().activateVehicleInteraction(
            mirrorVehicleId, context.camera);
    };
    created.interactionTargetId = InteractionSystem::instance().add(target);

    Debug::log(Debug::Category::Physics,
               "[VEHICLE NET] mirror spawned id=%u template=%s network=%u "
               "entity=%u\n",
               vehicleId, templateId.c_str(), chassisNetworkId, entityId);
    logVehicleWheelSpec(created, *spec);
    return true;
}

bool VehicleSystem::despawnByChassisNetworkId(uint32_t chassisNetworkId)
{
    uint32_t vehicleId = 0;
    for (const VehicleState& state : mVehicles)
        if (state.chassisNetworkId == chassisNetworkId)
        {
            vehicleId = state.id;
            break;
        }
    return vehicleId != 0 ? despawn(vehicleId) : false;
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

VehicleState* VehicleSystem::findByChassisNetworkId(uint32_t chassisNetworkId)
{
    if (chassisNetworkId == 0)
        return nullptr;
    for (VehicleState& state : mVehicles)
        if (state.chassisNetworkId == chassisNetworkId)
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

bool VehicleSystem::enterLocalDriver(uint32_t vehicleId, int seat,
                                     uint32_t actorId)
{
    VehicleState* state = find(vehicleId);
    if (!state)
        return false;
    state->localDriver = true;
    state->driverSeat = seat;
    state->driverActorId = static_cast<int>(actorId);
    state->inputOverride = false;
    if (seat >= 0 && seat < static_cast<int>(state->seatOccupants.size()))
        state->seatOccupants[seat] = static_cast<int>(actorId);
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

bool VehicleSystem::seatOccupied(uint32_t vehicleId, int seat,
                                 uint32_t* actorId) const
{
    const VehicleState* state = find(vehicleId);
    if (!state || seat < 0 ||
        seat >= static_cast<int>(state->seatOccupants.size()))
        return false;
    const int occupant = state->seatOccupants[seat];
    if (actorId) *actorId = occupant > 0 ? static_cast<uint32_t>(occupant) : 0;
    return occupant > 0;
}

bool VehicleSystem::seatWorld(uint32_t vehicleId, int seat,
                              glm::vec3& outPosition,
                              glm::vec3& outForward) const
{
    const VehicleState* state = find(vehicleId);
    if (!state)
        return false;
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(state->templateId);
    const PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state->entityId);
    if (!spec || !entity || spec->seats.empty())
        return false;
    if (seat < 0 || seat >= static_cast<int>(spec->seats.size()))
        seat = 0;
    const VehicleSeatSpec& seatSpec = spec->seats[seat];
    const glm::vec3 riderAnchor =
        seatSpec.anchor + seatSpec.riderOffset;
    outPosition = glm::vec3(
        entity->transform * glm::vec4(riderAnchor, 1.0f));
    outForward = glm::normalize(entity->orientation *
                                glm::vec3(1.0f, 0.0f, 0.0f));
    return true;
}

bool VehicleSystem::enterSeat(uint32_t vehicleId, int seat, uint32_t actorId)
{
    VehicleState* state = find(vehicleId);
    if (!state || actorId == 0)
        return false;
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(state->templateId);
    if (!spec || seat < 0 || seat >= static_cast<int>(spec->seats.size()))
        return false;
    if (state->seatOccupants.size() < spec->seats.size())
        state->seatOccupants.resize(spec->seats.size(), 0);
    if (seat >= 0 && state->seatOccupants[seat] != 0 &&
        state->seatOccupants[seat] != static_cast<int>(actorId))
        return false;

    // One vehicle per actor. A driver cannot occupy two cars.
    exitSeatByActor(actorId);

    state->seatOccupants[seat] = static_cast<int>(actorId);
    if (seat == 0)
    {
        state->driverActorId = static_cast<int>(actorId);
        state->driverSeat = 0;
    }
    if (state->serverAuthoritative)
    {
        if (PhysicalEntity* entity =
                PhysicalEntitySystem::instance().find(state->entityId))
        {
            entity->sleeping = false;
            entity->sleepTicks = 0;
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.seat.entered", std::to_string(vehicleId),
        "actor entered vehicle seat", 0,
        {{"vehicleId", vehicleId},
         {"seat", seat},
         {"actorId", actorId},
         {"driver", seat == 0}},
        __FILE__, __LINE__, __func__);
    return true;
}

bool VehicleSystem::exitSeat(uint32_t vehicleId, int seat, uint32_t actorId)
{
    VehicleState* state = find(vehicleId);
    if (!state || seat < 0 ||
        seat >= static_cast<int>(state->seatOccupants.size()))
        return false;
    if (state->seatOccupants[seat] != static_cast<int>(actorId))
        return false;
    state->seatOccupants[seat] = 0;
    if (seat == 0)
    {
        state->driverActorId = 0;
        state->driverSeat = -1;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.seat.exited", std::to_string(vehicleId),
        "actor left vehicle seat", 0,
        {{"vehicleId", vehicleId}, {"seat", seat}, {"actorId", actorId}},
        __FILE__, __LINE__, __func__);
    return true;
}

uint32_t VehicleSystem::exitSeatByActor(uint32_t actorId)
{
    if (actorId == 0)
        return 0;
    for (VehicleState& state : mVehicles)
    {
        for (size_t seat = 0; seat < state.seatOccupants.size(); ++seat)
        {
            if (state.seatOccupants[seat] != static_cast<int>(actorId))
                continue;
            state.seatOccupants[seat] = 0;
            if (seat == 0)
            {
                state.driverActorId = 0;
                state.driverSeat = -1;
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.seat.exited", std::to_string(state.id),
                "actor left vehicle seat", 0,
                {{"vehicleId", state.id},
                 {"seat", static_cast<int>(seat)},
                 {"actorId", actorId}},
                __FILE__, __LINE__, __func__);
            return state.id;
        }
    }
    return 0;
}

bool VehicleSystem::isDriver(uint32_t vehicleId, uint32_t actorId) const
{
    const VehicleState* state = find(vehicleId);
    return state && state->driverActorId != 0 &&
           static_cast<uint32_t>(state->driverActorId) == actorId &&
           state->driverSeat == 0;
}

VehicleInput VehicleSystem::sampleLocalInput()
{
    VehicleState* state = find(mLocalVehicleId);
    if (!state || !state->localDriver)
        return VehicleInput{};
    if (state->inputOverride)
    {
        VehicleInput input = state->pendingInput;
        state->pendingInput.engineToggle = false;
        state->pendingInput.modeToggle = false;
        state->pendingInput.shiftUp = false;
        state->pendingInput.shiftDown = false;
        return input;
    }
    if (state->serverMirror)
        return VehicleInput{};
    return readLocalInput(*state);
}

void VehicleSystem::requestSeat(uint32_t vehicleId, int seat, bool exit)
{
    ++mSeatRequestSerial;
    if (mSeatRequestSerial == 0)
        mSeatRequestSerial = 1;
    mSeatRequestPending = true;
    mSeatRequestVehicleId = vehicleId;
    mSeatRequestSeat = seat;
    mSeatRequestExit = exit;
}

void VehicleSystem::clearSeatRequest()
{
    mSeatRequestPending = false;
    mSeatRequestVehicleId = 0;
    mSeatRequestSeat = -1;
    mSeatRequestExit = false;
}

uint32_t VehicleSystem::localVehicleEntityId() const
{
    const VehicleState* state = find(mLocalVehicleId);
    return state ? state->entityId : 0;
}

bool VehicleSystem::activateVehicleInteraction(uint32_t vehicleId,
                                               const Camera* camera)
{
    (void)camera;
    VehicleState* state = find(vehicleId);
    if (!state)
        return false;

    // F should put the local actor in the car. Prefer the driver seat; only
    // fall through to another seat when the driver seat is occupied by another
    // actor (remote passenger support remains data-driven).
    const auto chooseSeat = [&]() {
        const uint32_t localActor =
            MP_CONTEXT.active ? MP_CONTEXT.localPlayerId : 1u;
        uint32_t occupant = 0;
        if (!seatOccupied(vehicleId, 0, &occupant) ||
            occupant == localActor || occupant == 0)
            return 0;
        for (size_t i = 1; i < state->seatOccupants.size(); ++i)
        {
            uint32_t other = 0;
            if (!seatOccupied(vehicleId, static_cast<int>(i), &other) ||
                other == 0 || other == localActor)
                return static_cast<int>(i);
        }
        return 0;
    };

    // Only server-mirrored vehicles go through the authoritative seat request.
    // A vehicle created by the local `vehicle_spawn` test command has no server
    // record, so requesting that id is always rejected; it must use the local
    // seat path instead.
    if (MP_CONTEXT.active && state->serverMirror)
    {
        if (state->localDriver)
        {
            requestSeat(vehicleId, state->driverSeat, true);
            return true;
        }
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(state->templateId);
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state->entityId);
        if (!spec || !entity)
            return false;
        requestSeat(vehicleId, chooseSeat(), false);
        return true;
    }

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
    if (!spec || !entity)
        return false;

    return enterLocalDriver(vehicleId, chooseSeat());
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
    // One seat-anchor owner: local, server, camera, and exit all use the same
    // anchor plus rider offset.
    return seatWorld(mLocalVehicleId, state->driverSeat, outPosition,
                     outForward);
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
    state.clutchKickTimer = std::max(0.0f, state.clutchKickTimer - dt);
    state.autoShiftLockTimer =
        std::max(0.0f, state.autoShiftLockTimer - dt);

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
                state.autoShiftLockTimer = 1.5f;
            }
        }
    }
    else
    {
        // Sequential shift keys work in automatic mode too; they override the
        // automatic selector for the next gear and then the selector resumes.
        if ((state.flagsShiftUp || state.flagsShiftDown) &&
            state.shiftTimer <= 0.0f)
        {
            const int next = std::clamp(
                state.gear + (state.flagsShiftUp ? 1 : -1), -1, maxGear);
            if (next != state.gear)
            {
                state.gear = next;
                state.shiftTimer = spec.gearbox.shiftTime;
                state.autoShiftLockTimer = 1.5f;
            }
        }
        if (state.autoShiftLockTimer <= 0.0f)
        {
            if (state.gear == 0)
                state.gear = state.brake > 0.5f ? -1 : 1;
            if (state.gear == -1 && state.throttle > 0.1f &&
                std::fabs(state.speed) < 1.0f)
                state.gear = 1;
        }
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
        state.shiftTimer <= 0.0f && state.autoShiftLockTimer <= 0.0f &&
        state.gear > 0)
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
    int hitTriangleId = -1;
    const bool hit = raycastCandidates(world, hub, axis, maxDistance,
                                       candidates, hitDistance, normal,
                                       &hitTriangleId);
    wheel.rayHit = hit;
    wheel.contactTriangleId = hitTriangleId;
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
                       -5.0f, 5.0f);
        wheel.compressionVelocity = compressionVelocity;
        const float damperScale =
            compressionVelocity < 0.0f ? spec.reboundScale : 1.0f;
        float springForce = wheelSpec.springRate * state.springScale *
                                wheel.compression +
                            wheelSpec.damperRate * state.damperScale *
                                damperScale * compressionVelocity;
        if (rawCompression > wheelSpec.travel)
        {
            const float bump = std::min(rawCompression - wheelSpec.travel,
                                        wheelSpec.travel);
            const float bumpStiffness =
                spec.bumpStopStiffness > 0.0f
                    ? spec.bumpStopStiffness
                    : wheelSpec.springRate * 4.0f;
            const float bumpDamping =
                spec.bumpStopDamping > 0.0f ? spec.bumpStopDamping
                                            : wheelSpec.damperRate;
            springForce +=
                (bumpStiffness * state.springScale * bump +
                 bumpDamping * state.damperScale * compressionVelocity) *
                0.5f;
        }
        // Rebound damping must be allowed to pull the chassis down. Clamping
        // the spring force at zero removed rebound entirely and produced a
        // 60 Hz pogo/roll oscillation; the tire still only sees positive load.
        springForce = std::clamp(springForce, -spec.tire.nominalLoad,
                                 spec.maxSuspensionForce);
        wheel.grounded = true;
        wheel.load = std::max(springForce, 0.0f);
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

        float steerTarget = glm::radians(wheelSpec.toeDeg);
        if (wheelSpec.steer)
        {
            const float falloff =
                1.0f - std::clamp(std::fabs(state.speed) *
                                      spec.steering.speedFalloff / 30.0f,
                                  0.0f, 0.85f);
            float ackermannScale = 1.0f;
            if (spec.steering.ackermann > 0.0f &&
                std::fabs(state.steer) > 0.01f)
            {
                const bool inner =
                    (state.steer > 0.0f) == (wheelSpec.hub.y < 0.0f);
                ackermannScale = inner
                                     ? 1.0f + 0.25f *
                                                  spec.steering.ackermann
                                     : 1.0f - 0.15f *
                                                  spec.steering.ackermann;
            }
            steerTarget += state.steer * spec.steering.directionSign *
                           glm::radians(spec.steering.maxAngleDeg) *
                           falloff * ackermannScale;
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
            const float speed01 =
                std::clamp(std::fabs(longSpeed) / lowSpeed, 0.0f, 1.0f);
            mu *= spec.tire.lowSpeedGrip +
                  (1.0f - spec.tire.lowSpeedGrip) * speed01;
            if (state.handbrake > 0.01f && wheelSpec.hub.x < 0.0f)
                mu *= spec.brakes.handbrakeGripScale;
            const float tireLoad =
                std::min(wheel.load,
                         spec.tire.nominalLoad * spec.tire.maxLoadScale);
            const float forceFraction = std::min(combined, 1.0f);
            const float forceMagnitude = mu * tireLoad * forceFraction *
                                         spec.tire.slipStiffness;
            const glm::vec3 forceDirection =
                (wheelForward * normalizedRatio -
                 wheelRight * normalizedAngle) /
                combined;
            glm::vec3 force = forceDirection * forceMagnitude;
            force += wheelRight *
                     (spec.tire.camberGain *
                      glm::radians(wheelSpec.camberDeg) * wheel.load);
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

        if (spec.tire.rollingResistance > 0.0f && wheel.load > 0.0f)
        {
            const glm::vec2 planar(contactVelocity.x, contactVelocity.y);
            const float planarSpeed = glm::length(planar);
            if (planarSpeed > 0.01f)
            {
                const glm::vec3 rollingDirection =
                    glm::vec3(-planar.x / planarSpeed,
                              -planar.y / planarSpeed, 0.0f);
                applyPhysicalEntityImpulse(
                    *entity,
                    rollingDirection * (wheel.load *
                                        spec.tire.rollingResistance * dt),
                    wheel.contactPoint);
            }
        }
    }
    else
    {
        wheel.compression = 0.0f;
        wheel.compressionVelocity = 0.0f;
    }

    // Drivetrain and brakes act on the wheel spin even in the air.
    if (state.engineOn && wheelIsDriven(spec, wheelSpec) &&
        ratio != 0.0f)
    {
        const float kick = state.clutchKickTimer > 0.0f
                               ? (1.0f + spec.engine.clutchKickBoost)
                               : 1.0f;
        const float driveTorque =
            state.engineTorque * state.torqueScale * kick *
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

    if (spec.coastSpinDamping > 0.0f &&
        state.throttle < 0.01f && state.brake < 0.01f &&
        state.handbrake < 0.01f)
    {
        wheel.spinVelocity *= std::max(
            0.0f, 1.0f - spec.coastSpinDamping * dt);
    }
    wheel.spinVelocity = std::clamp(wheel.spinVelocity,
                                    -wheelSpec.maxSpinVelocity,
                                    wheelSpec.maxSpinVelocity);
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

        if (state.presentationEnabled && state.interactionTargetId != 0)
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

        if (state.serverMirror)
        {
            // Chassis motion and wheel presentation come from the server.
            // Never apply suspension/tire/engine forces to a mirror.
            state.origin = glm::vec3(entity->transform[3]);
            state.orientation = entity->orientation;
            state.originVelocity = entity->velocity;
            state.originAngularVelocity = entity->angularVelocity;
            state.distanceFromSpawn =
                glm::length(state.origin - state.spawnOrigin);
            state.massKg = entity->mass;
            // Smooth visual wheel spin between 30 Hz state packets. The
            // authoritative spin velocity and the next packet correct drift.
            for (VehicleWheelState& wheel : state.wheels)
            {
                wheel.spinAngle += wheel.spinVelocity * dt;
                if (wheel.spinAngle > 2.0f * kPi)
                    wheel.spinAngle -= 2.0f * kPi;
                else if (wheel.spinAngle < -2.0f * kPi)
                    wheel.spinAngle += 2.0f * kPi;
            }
            if (state.presentationEnabled)
            {
                updateVehicleEngineAudio(state, *spec, state.origin,
                                         simulationTick);
            }
            continue;
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

        if (state.prevClutch > 0.5f && state.clutch < 0.5f &&
            state.throttle > 0.30f && state.engineRpm > 3000.0f)
        {
            state.clutchKickTimer = 0.25f;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.clutch.kick", std::to_string(state.id),
                "clutch kick released at high rpm", 0,
                {{"vehicle_id", state.id},
                 {"rpm", state.engineRpm},
                 {"throttle", state.throttle},
                 {"gear", state.gear},
                 {"boost", spec->engine.clutchKickBoost}},
                __FILE__, __LINE__, __func__);
        }
        state.prevClutch = state.clutch;

        updateDrivetrain(state, *spec, dt);

        if (state.gear != state.lastGearLogged)
        {
            state.lastGearLogged = state.gear;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.gear.changed", std::to_string(state.id),
                "vehicle gear changed", 0,
                {{"vehicle_id", state.id},
                 {"gear", state.gear},
                 {"mode", state.mode == VehicleControlMode::Automatic
                              ? "AUTO"
                              : "MANUAL"},
                 {"engineOn", state.engineOn},
                 {"rpm", state.engineRpm},
                 {"speed", state.speed},
                 {"autoShiftLock", state.autoShiftLockTimer}},
                __FILE__, __LINE__, __func__);
        }

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
            {
                appendChunkTrianglesForAABB(world, wheelBox, 0.0f,
                                            sWheelCandidates,
                                            "vehicleWheel", true);
                // Large floor/ground triangles live in the world tree rather
                // than a chunk. Wheel rays must see both owners or a vehicle
                // can rest on its chassis while all four wheels report air.
                world.collisionTree.query(wheelBox, sWheelCandidates);
            }
        }

        for (int i = 0; i < static_cast<int>(spec->wheels.size()); ++i)
            updateWheel(state, *spec, i, dt, world, sWheelCandidates);

        // Anti-roll bars: couple the left/right wheels of each axle. Without
        // this, a small roll excites one side to the bump stop while the other
        // side unloads, producing a sustained 60 Hz rock even on flat ground.
        {
            const glm::vec3 up = state.orientation * glm::vec3(0.0f, 0.0f, 1.0f);
            for (size_t i = 0; i < spec->wheels.size(); ++i)
            {
                if (i >= state.wheels.size())
                    break;
                const VehicleWheelSpec& a = spec->wheels[i];
                if (a.antiRoll <= 0.0f)
                    continue;
                for (size_t j = i + 1; j < spec->wheels.size(); ++j)
                {
                    if (j >= state.wheels.size())
                        break;
                    const VehicleWheelSpec& b = spec->wheels[j];
                    if (std::fabs(a.hub.x - b.hub.x) > 0.05f)
                        continue;
                    if (a.hub.y * b.hub.y >= 0.0f)
                        continue;
                    const float delta =
                        state.wheels[i].compression -
                        state.wheels[j].compression;
                    const float velocityDelta =
                        state.wheels[i].compressionVelocity -
                        state.wheels[j].compressionVelocity;
                    const float rate = std::min(a.antiRoll, b.antiRoll);
                    float damping =
                        (a.antiRollDamping + b.antiRollDamping) * 0.5f;
                    if (damping <= 0.0f)
                        damping = rate * 0.20f;
                    const float maxForce =
                        spec->massKg * spec->antiRollForceScale;
                    const float force = std::clamp(
                        rate * delta + damping * velocityDelta,
                        -maxForce, maxForce);
                    if (std::fabs(force) <= 0.01f)
                        continue;
                    const glm::vec3 hubA = glm::vec3(
                        entity->transform * glm::vec4(a.hub, 1.0f));
                    const glm::vec3 hubB = glm::vec3(
                        entity->transform * glm::vec4(b.hub, 1.0f));
                    applyPhysicalEntityImpulse(*entity, (up * force) * dt,
                                               hubA);
                    applyPhysicalEntityImpulse(*entity, (-up * force) * dt,
                                               hubB);
                }
            }
        }

        // Differential coupling on each driven axle. "welded" is the default
        // and forces both wheels to one spin speed. "race_lsd" transfers torque
        // with preload + power/coast lock. "open" does nothing.
        if (spec->differential.type != "open")
        {
            for (size_t i = 0; i < spec->wheels.size(); ++i)
            {
                if (!wheelIsDriven(*spec, spec->wheels[i]))
                    continue;
                for (size_t j = i + 1; j < spec->wheels.size(); ++j)
                {
                    if (!wheelIsDriven(*spec, spec->wheels[j]))
                        continue;
                    if ((spec->wheels[i].hub.x < 0.0f) !=
                        (spec->wheels[j].hub.x < 0.0f))
                        continue;
                    float& left = state.wheels[i].spinVelocity;
                    float& right = state.wheels[j].spinVelocity;
                    if (spec->differential.type == "welded")
                    {
                        const float shared = (left + right) * 0.5f;
                        left = shared;
                        right = shared;
                    }
                    else
                    {
                        const float delta = left - right;
                        if (std::fabs(delta) < 1e-5f)
                            continue;
                        const float lock =
                            state.throttle > 0.05f
                                ? spec->differential.powerLock
                                : spec->differential.coastLock;
                        const float capacity =
                            spec->differential.preloadTorque +
                            lock * std::fabs(state.engineTorque);
                        const float maxDelta =
                            capacity * dt /
                            std::max(wheelInertia(spec->wheels[i]), 0.01f);
                        const float correction = std::clamp(
                            delta * 0.5f * spec->differential.lockStrength,
                            -maxDelta, maxDelta);
                        left -= correction;
                        right += correction;
                    }
                }
            }
        }

        // Stability damping: an all-wheel-grounded chassis should not keep
        // oscillating vertically or rolling after a landing. Damp only the
        // unsupported motion so throttle, steer, and jumps remain untouched.
        {
            int grounded = 0;
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.grounded)
                    ++grounded;
            if (grounded > 0)
            {
                const bool allGrounded =
                    grounded == static_cast<int>(state.wheels.size());
                const float verticalDamping =
                    allGrounded ? spec->stabilityVerticalDamping : 1.0f;
                entity->velocity.z *= std::max(
                    0.0f, 1.0f - verticalDamping * dt);
                if (allGrounded)
                {
                    const float angularDamping =
                        spec->stabilityAngularDamping;
                    entity->angularVelocity *= std::max(
                        0.0f, 1.0f - angularDamping * dt);

                    // Idle settle: no throttle/brake/steer means the car is
                    // parked. Kill residual suspension pogo and roll instead
                    // of letting the explicit 60 Hz contact loop sustain it.
                    const bool idleInput =
                        state.throttle < 0.01f && state.brake < 0.01f &&
                        std::fabs(state.steer) < 0.01f &&
                        state.handbrake < 0.01f;
                    bool compressionOkay = true;
                    for (size_t wheelIndex = 0;
                         wheelIndex < state.wheels.size() &&
                         wheelIndex < spec->wheels.size();
                         ++wheelIndex)
                        if (state.wheels[wheelIndex].compression >
                            spec->wheels[wheelIndex].travel * 1.0f)
                            compressionOkay = false;
                    const bool alreadyNearRest = compressionOkay;
                    if (idleInput && alreadyNearRest &&
                        glm::length(glm::vec2(entity->velocity.x,
                                              entity->velocity.y)) <
                            spec->idleSettleSpeed)
                    {
                        entity->velocity.x *= spec->idleSettleHorizontal;
                        entity->velocity.y *= spec->idleSettleHorizontal;
                        entity->velocity.z *= spec->idleSettleVertical;
                        entity->angularVelocity.x *= spec->idleSettleAngular;
                        entity->angularVelocity.y *= spec->idleSettleAngular;
                        entity->angularVelocity.z *= 0.90f;
                        for (VehicleWheelState& wheel : state.wheels)
                            wheel.spinVelocity *= 0.10f;
                    }
                }
            }
        }

        {
            const glm::vec3 up =
                state.orientation * glm::vec3(0.0f, 0.0f, 1.0f);
            int grounded = 0;
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.grounded)
                    ++grounded;
            const bool allGrounded =
                grounded == static_cast<int>(state.wheels.size());
            const bool maybeSettled =
                allGrounded && state.speed < 0.20f &&
                glm::length(state.originAngularVelocity) < 0.20f &&
                up.z > 0.95f;
            const bool anchorMoved =
                !state.settleAnchorValid ||
                glm::length(state.origin - state.settleAnchor) > 0.02f;
            if (maybeSettled && !anchorMoved)
            {
                if (state.settleTicks < 0xFFFF)
                    ++state.settleTicks;
                if (state.settleTicks >= 60 && !state.settleReported)
                {
                    state.settleReported = true;
                    float maxCompression = 0.0f;
                    for (const VehicleWheelState& wheel : state.wheels)
                        maxCompression =
                            std::max(maxCompression, wheel.compression);
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Physics, StructuredLevel::Important,
                        "vehicle.settle", std::to_string(state.id),
                        "vehicle reached a stable resting state",
                        static_cast<uint32_t>(
                            PhysicalEntitySystem::instance().simulationTick()),
                        {{"vehicle_id", state.id},
                         {"tick", PhysicalEntitySystem::instance()
                                      .simulationTick()},
                         {"settle_ticks", state.settleTicks},
                         {"up_dot", up.z},
                         {"grounded_count", grounded},
                         {"speed", state.speed},
                         {"vz", state.originVelocity.z},
                         {"angular_speed",
                          glm::length(state.originAngularVelocity)},
                         {"max_compression", maxCompression}});
                }
            }
            else
            {
                state.settleTicks = 0;
                state.settleReported = false;
                state.settleAnchor = state.origin;
                state.settleAnchorValid = true;
            }

            if (up.z < 0.55f && !state.flipReported)
            {
                state.flipReported = true;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "vehicle.flip", std::to_string(state.id),
                    "vehicle up vector crossed the flip threshold",
                    static_cast<uint32_t>(
                        PhysicalEntitySystem::instance().simulationTick()),
                    {{"vehicle_id", state.id},
                     {"up", {up.x, up.y, up.z}},
                     {"origin", {state.origin.x, state.origin.y,
                                 state.origin.z}},
                     {"velocity", {state.originVelocity.x,
                                   state.originVelocity.y,
                                   state.originVelocity.z}},
                     {"angular_velocity",
                      {state.originAngularVelocity.x,
                       state.originAngularVelocity.y,
                       state.originAngularVelocity.z}},
                     {"grounded_count", grounded}});
            }
            else if (up.z > 0.90f)
            {
                state.flipReported = false;
            }
        }

        // Coast-down: with no driver input the car loses speed at a fixed
        // rate until it stops instead of coasting forever.
        if (spec->coastDeceleration > 0.0f &&
            state.throttle < 0.01f && state.brake < 0.01f &&
            state.handbrake < 0.01f &&
            std::fabs(state.steer) < 0.01f)
        {
            glm::vec2 planar(entity->velocity.x, entity->velocity.y);
            const float planarSpeed = glm::length(planar);
            if (planarSpeed > 0.01f)
            {
                const float drop = std::min(
                    planarSpeed, spec->coastDeceleration * dt);
                planar -= planar / planarSpeed * drop;
                entity->velocity.x = planar.x;
                entity->velocity.y = planar.y;
            }
        }

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

        const uint64_t bounceTick =
            PhysicalEntitySystem::instance().simulationTick();
        const glm::vec3 forward =
            state.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
        const float yaw = std::atan2(forward.y, forward.x);
        if (std::fabs(state.steer) > 0.01f && bounceTick % 6u == 0u)
        {
            std::vector<float> frontAngles;
            for (size_t i = 0; i < spec->wheels.size() &&
                               i < state.wheels.size();
                 ++i)
                if (spec->wheels[i].steer)
                    frontAngles.push_back(state.wheels[i].steerAngle);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.steer", std::to_string(state.id),
                "steering input and front wheel angle", bounceTick,
                {{"vehicle_id", state.id},
                 {"input", state.steer},
                 {"directionSign", spec->steering.directionSign},
                 {"frontWheelAngles", frontAngles},
                 {"yaw", yaw},
                 {"yawDelta", yaw - state.lastSteerYaw},
                 {"speed", state.speed}},
                __FILE__, __LINE__, __func__);
            state.lastSteerYaw = yaw;
        }
        if (entity->lastWallContactTick != state.lastWallContactTick)
        {
            state.lastWallContactTick = entity->lastWallContactTick;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.wall.contact", std::to_string(state.id),
                "vehicle chassis contacted a near-vertical wall", bounceTick,
                {{"vehicle_id", state.id},
                 {"normal", {entity->lastWallContactNormal.x,
                             entity->lastWallContactNormal.y,
                             entity->lastWallContactNormal.z}},
                 {"impactSpeed", entity->lastWallImpactSpeed},
                 {"speed", state.speed},
                 {"velocity", {state.originVelocity.x,
                               state.originVelocity.y,
                               state.originVelocity.z}},
                 {"yaw", yaw},
                 {"wallFriction", spec->bodyWallFriction}},
                __FILE__, __LINE__, __func__);
        }
        if ((state.serverAuthoritative || !state.serverMirror ||
             state.localDriver) &&
            bounceTick % 6u == 0u)
        {
            const glm::vec3 up = state.orientation * glm::vec3(0.0f, 0.0f, 1.0f);
            nlohmann::json wheelTrace = nlohmann::json::array();
            int groundedCount = 0;
            float minSuspension = 0.0f;
            float maxCompression = 0.0f;
            for (size_t i = 0; i < state.wheels.size(); ++i)
            {
                const VehicleWheelState& wheel = state.wheels[i];
                if (wheel.grounded)
                    ++groundedCount;
                minSuspension = std::min(minSuspension,
                                         wheel.suspensionLength);
                maxCompression = std::max(maxCompression,
                                          wheel.compression);
                glm::vec3 hub(0.0f);
                if (i < spec->wheels.size())
                    hub = glm::vec3(entity->transform *
                                    glm::vec4(spec->wheels[i].hub, 1.0f));
                wheelTrace.push_back({
                    {"index", i},
                    {"grounded", wheel.grounded},
                    {"hub", {hub.x, hub.y, hub.z}},
                    {"contactPoint", {wheel.contactPoint.x,
                                      wheel.contactPoint.y,
                                      wheel.contactPoint.z}},
                    {"contactNormal", {wheel.contactNormal.x,
                                       wheel.contactNormal.y,
                                       wheel.contactNormal.z}},
                    {"normalUpDot", glm::dot(wheel.contactNormal, up)},
                    {"triangleId", wheel.contactTriangleId},
                    {"rayDistance", wheel.rayDistance},
                    {"compression", wheel.compression},
                    {"suspensionLength", wheel.suspensionLength},
                    {"load", wheel.load},
                    {"slipRatio", wheel.slipRatio},
                    {"spinVelocity", wheel.spinVelocity},
                });
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.bounce", std::to_string(state.id),
                "numerical chassis/wheel bounce sample", bounceTick,
                {{"vehicle_id", state.id},
                 {"origin", {state.origin.x, state.origin.y, state.origin.z}},
                 {"velocity", {state.originVelocity.x,
                               state.originVelocity.y,
                               state.originVelocity.z}},
                 {"angularVelocity", {state.originAngularVelocity.x,
                                      state.originAngularVelocity.y,
                                      state.originAngularVelocity.z}},
                 {"orientation", {state.orientation.w, state.orientation.x,
                                  state.orientation.y, state.orientation.z}},
                 {"up", {up.x, up.y, up.z}},
                 {"speed", state.speed},
                 {"differential", spec->differential.type},
                 {"wheelSpin", [&]() {
                      std::vector<float> spins;
                      for (const VehicleWheelState& wheel : state.wheels)
                          spins.push_back(wheel.spinVelocity);
                      return spins;
                  }()},
                 {"throttle", state.throttle},
                 {"clutch", state.clutch},
                 {"gear", state.gear},
                 {"engineOn", state.engineOn},
                 {"sleeping", entity->sleeping},
                 {"sleepTicks", entity->sleepTicks},
                 {"serverSimulated", entity->serverSimulated},
                 {"serverDriven", entity->serverDriven},
                 {"motion", static_cast<int>(entity->motion)},
                 {"entityMass", entity->mass},
                 {"groundedCount", groundedCount},
                 {"allWheelsGrounded", groundedCount ==
                                           static_cast<int>(state.wheels.size())},
                 {"minSuspension", minSuspension},
                 {"maxCompression", maxCompression},
                 {"wheels", wheelTrace}},
                __FILE__, __LINE__, __func__);
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

        if (state.presentationEnabled)
        {
            updateVehicleEngineAudio(state, *spec, state.origin,
                                     simulationTick);
        }

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
