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
#include "input/gamepad-input.h"
#include "input/input-commands.h"
#include "input/input-router.h"
#include "interaction/interaction-system.h"
#include "network/multiplayer-context.h"
#include "camera.h"
#include "perf/perf-spike.h"
#include "physics/movement/physics-collision.h"
#include "physics/physical-entity.h"
#include "physics/ray-utils.h"
#include "terminal/terminal-state.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-profile.h"
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
    VehicleProfileRegistry::instance().loadDirectory("config/vehicle-profiles");
    return VehicleConfigRegistry::instance().loadDirectory(directory);
}

bool VehicleSystem::pollReload()
{
    const bool profilesChanged =
        VehicleProfileRegistry::instance().pollReload();
    const bool templatesChanged =
        VehicleConfigRegistry::instance().pollReload();
    return profilesChanged || templatesChanged;
}

void VehicleSystem::setActiveProfile(const std::string& profileId)
{
    if (mActiveProfileId == profileId)
        return;
    mActiveProfileId = profileId;
    for (VehicleState& state : mVehicles)
        state.profileId = profileId;
}

const VehicleProfile* VehicleSystem::profileFor(
    const VehicleState& state) const
{
    if (state.profileId.empty())
        return nullptr;
    return VehicleProfileRegistry::instance().find(state.profileId);
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

    // The collision slab may be narrower/lower than the rendered body so the
    // wheels are the lowest and widest points. Wheel colliders make the car
    // settle onto its wheels through ordinary contact physics.
    const glm::vec3 collisionHalf =
        glm::dot(spec->collisionHalfExtents, spec->collisionHalfExtents) > 1e-6f
            ? spec->collisionHalfExtents
            : spec->halfExtents;
    std::vector<CollisionTriangle> triangles;
    buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), collisionHalf);
    for (const VehicleWheelSpec& wheel : spec->wheels)
        if (wheel.collisionRadius > 0.0f)
            buildSphereCollisionTriangles(triangles, wheel.hub,
                                          wheel.collisionRadius, 6, 10);

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
    entity->rollingResistance = spec->tire.rollingResistance;
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
    state.profileId = mActiveProfileId;
    state.entityId = entityId;
    state.chassisNetworkId = chassisNetworkId;
    state.serverAuthoritative = authoritative;
    state.serverMirror = false;
    state.presentationEnabled = !authoritative;
    state.driverActorId = driverActorId;
    state.mode = VehicleControlMode::Manual;
    state.gear = 1;
    state.clutch = 0.0f;
    if (const VehicleProfile* profile = profileFor(state))
    {
        if (profile->automatic)
            state.mode = VehicleControlMode::Automatic;
        state.engineOn = profile->engineAutoStart;
        state.boostCharge = profile->boost.max;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.profile.applied", std::to_string(state.id),
            "vehicle arcade profile applied to a spawned vehicle", 0,
            {{"vehicle_id", state.id},
             {"template", templateId},
             {"profile", profile->id},
             {"automatic", profile->automatic},
             {"engine_on", state.engineOn},
             {"boost_max", profile->boost.max},
             {"jump_delta_vel_mps", profile->jump.immediateDeltaVelMps},
             {"gravity_mps2", profile->gravityMps2},
             {"profile_source", profile->source},
             {"steering_max_deg", profile->steering.maxAngleDeg}},
            __FILE__, __LINE__, __func__);
    }
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
        existing->profileId = mActiveProfileId;
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
        const glm::vec3 collisionHalf =
            glm::dot(spec->collisionHalfExtents,
                     spec->collisionHalfExtents) > 1e-6f
                ? spec->collisionHalfExtents
                : spec->halfExtents;
        std::vector<CollisionTriangle> triangles;
        buildBoxCollisionTriangles(triangles, glm::vec3(0.0f), collisionHalf);
        for (const VehicleWheelSpec& wheel : spec->wheels)
            if (wheel.collisionRadius > 0.0f)
                buildSphereCollisionTriangles(triangles, wheel.hub,
                                              wheel.collisionRadius, 6, 10);
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
    state.profileId = mActiveProfileId;
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
    // Client-side prediction: when this client owns the car, it simulates the
    // chassis locally so throttle/steer/jump respond on the input tick instead
    // of after a server round trip. Server snapshots become corrections.
    if (MP_CONTEXT.active && !state->serverAuthoritative)
    {
        PhysicalEntity* chassis =
            PhysicalEntitySystem::instance().find(state->entityId);
        if (chassis && chassis->serverDriven)
        {
            chassis->serverDriven = false;
            chassis->clientPredicted = true;
            chassis->sleeping = false;
            state->serverMirror = false;
            state->clientPredicted = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "vehicle.prediction.enabled", std::to_string(vehicleId),
                "local client now predicts its own chassis", 0,
                {{"vehicle_id", vehicleId},
                 {"chassis_entity", chassis->id},
                 {"chassis_network_id", chassis->networkId}},
                __FILE__, __LINE__, __func__);
        }
    }
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
    // A local driver on a server mirror still owns its input. The mirror never
    // applies forces locally, but the sampled input must reach the packet so
    // the authoritative server can drive the car.
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
    if (MP_CONTEXT.active && (state->serverMirror || state->clientPredicted))
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
    const GamepadInput& pad = GamepadInput::instance();
    const VehicleProfile* profile = profileFor(state);

    const auto boundHeld = [&](const char* logical) {
        if (!profile)
            return false;
        const auto it = profile->bindings.find(logical);
        if (it == profile->bindings.end())
            return false;
        const std::string& binding = it->second;
        if (binding == "mouse_left")
            return InputRouter::mouseDown(GLFW_MOUSE_BUTTON_LEFT);
        if (binding == "mouse_right")
            return InputRouter::mouseDown(GLFW_MOUSE_BUTTON_RIGHT);
        if (binding == "key_left_shift")
            return commands.isKeyHeld(GLFW_KEY_LEFT_SHIFT);
        if (binding == "key_space")
            return commands.isKeyHeld(GLFW_KEY_SPACE);
        if (binding == "key_left_control")
            return commands.isKeyHeld(GLFW_KEY_LEFT_CONTROL);
        return commands.getState(binding).held;
    };

    input.throttle =
        commands.getState("walkforward").held ? 1.0f : 0.0f;
    input.throttle = std::max(input.throttle, pad.throttle());
    if (boundHeld("throttle"))
        input.throttle = 1.0f;

    input.brake = commands.getState("walkback").held ? 1.0f : 0.0f;
    input.brake = std::max(input.brake, pad.brake());
    if (boundHeld("brake"))
        input.brake = 1.0f;

    const float left = commands.getState("walkleft").held ? 1.0f : 0.0f;
    const float right = commands.getState("walkright").held ? 1.0f : 0.0f;
    input.steer = std::clamp(right - left + pad.steer(), -1.0f, 1.0f);
    if (boundHeld("steerLeft"))
        input.steer = -1.0f;
    if (boundHeld("steerRight"))
        input.steer = 1.0f;

    const bool clutchKey = commands.isKeyHeld(GLFW_KEY_LEFT_SHIFT);
    input.clutch = state.mode == VehicleControlMode::Manual
                       ? (clutchKey ? 1.0f : 0.0f)
                       : 0.0f;

    input.boost = std::max(pad.boost() ? 1.0f : 0.0f,
                           boundHeld("boost") ? 1.0f : 0.0f);

    input.jumpHeld = commands.getState("jump").held || pad.jump() ||
                     boundHeld("jump");
    input.powerslide = commands.getState("dash").held || pad.powerslide() ||
                       boundHeld("powerslideAirRoll");
    input.handbrake = input.powerslide ? 1.0f : 0.0f;

    // ── Rotational intent (air) ─────────────────────────────────────────
    // R1 is the free-air-roll modifier: airborne it remaps the horizontal
    // stick from yaw to roll. Square/Circle are directional air roll and stay
    // additive, so holding one against the stick makes yaw + roll == 0, which
    // is the Rocket League stall input. Pitch stays on the vertical stick.
    const float stickX = std::clamp(input.steer, -1.0f, 1.0f);
    input.pitch = std::clamp(
        pad.pitch() +
            (commands.isKeyHeld(GLFW_KEY_UP) ? 1.0f : 0.0f) -
            (commands.isKeyHeld(GLFW_KEY_DOWN) ? 1.0f : 0.0f),
        -1.0f, 1.0f);
    float directionalRoll = 0.0f;
    if (pad.airRollLeft() || commands.isKeyHeld(GLFW_KEY_Q))
        directionalRoll -= 1.0f;
    if (pad.airRollRight() || commands.isKeyHeld(GLFW_KEY_E))
        directionalRoll += 1.0f;
    input.airRollModifier = input.powerslide;
    if (input.airRollModifier)
    {
        input.roll = std::clamp(stickX + directionalRoll, -1.0f, 1.0f);
        input.yaw = 0.0f;
    }
    else
    {
        input.roll = std::clamp(directionalRoll, -1.0f, 1.0f);
        input.yaw = stickX;
    }

    const bool ballCamHeld = commands.getState("freeze").held ||
                             pad.ballCam() || boundHeld("ballCam");
    input.ballCamToggle = ballCamHeld && !mPrevBallCamHeld;
    mPrevBallCamHeld = ballCamHeld;

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

        const VehicleProfile* profile = profileFor(state);
        const float maxAngleDeg = profile
                                      ? profile->steering.maxAngleDeg
                                      : spec.steering.maxAngleDeg;
        const float speedFalloff = profile
                                       ? profile->steering.speedFalloff
                                       : spec.steering.speedFalloff;
        const float steeringReturnRate = profile
                                             ? profile->steering.returnRate
                                             : spec.steering.returnRate;

        float steerTarget = glm::radians(wheelSpec.toeDeg);
        if (wheelSpec.steer)
        {
            const float falloff =
                1.0f - std::clamp(std::fabs(state.speed) *
                                      speedFalloff / 30.0f,
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
                           glm::radians(maxAngleDeg) *
                           falloff * ackermannScale;
        }
        const float returnRate =
            std::fabs(steerTarget) < 1e-4f ? steeringReturnRate : 10.0f;
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
                mu *= profile ? profile->powerslide.gripScale
                              : spec.brakes.handbrakeGripScale;
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

void VehicleSystem::updateArcadeControls(VehicleState& state,
                                         const VehicleProfile& profile,
                                         PhysicalEntity& entity, float dt)
{
    const uint64_t tick =
        PhysicalEntitySystem::instance().simulationTick();
    const float tickScale = dt * 60.0f;

    int grounded = 0;
    for (const VehicleWheelState& wheel : state.wheels)
        if (wheel.grounded)
            ++grounded;
    state.allWheelsGrounded =
        grounded > 0 && grounded == static_cast<int>(state.wheels.size());

    const glm::vec3 forward =
        glm::normalize(entity.orientation * glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 right =
        glm::normalize(entity.orientation * glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 up =
        glm::normalize(entity.orientation * glm::vec3(0.0f, 0.0f, 1.0f));

    // The profile owns gravity so the car and ball share Rocket League's
    // 6.5 m/s^2 instead of the global object gravity.
    const float gravity = std::max(profile.gravityMps2, 0.1f);
    const float worldGravity = std::fabs(
        MimitaImpact::DestructibleWorldConfig::instance().objectGravity());
    if (worldGravity > 0.01f)
        entity.gravityScale = gravity / worldGravity;

    // Hand the profile's car->ball impulse parameters to the contact owner.
    {
        PhysicalEntityBallHitParams hitParams;
        hitParams.enabled = profile.ballHit.extraImpulseEnabled;
        hitParams.scale = profile.ballHit.extraImpulseScale;
        hitParams.zScale = profile.ballHit.extraImpulseZScale;
        hitParams.forwardScale = profile.ballHit.extraImpulseForwardScale;
        hitParams.maxDeltaVelMps = profile.ballHit.extraImpulseMaxDeltaVelMps;
        const int points = std::clamp(
            static_cast<int>(profile.ballHit.extraImpulseCurve.size()), 1, 4);
        hitParams.curvePoints = points;
        for (int i = 0; i < points; ++i)
            hitParams.curve[i] = profile.ballHit.extraImpulseCurve[i];
        setPhysicalEntityBallHitParams(hitParams);
    }

    const bool onGround =
        grounded > 0 && grounded >= 3;
    const int wheelCount = static_cast<int>(state.wheels.size());

    // ── Flip-reset surface ──────────────────────────────────────────────
    // All four wheels touching any surface restores the aerial flip. The ball
    // is one of those surfaces, which is what makes flip resets chainable.
    const bool wasOnBall = state.wheelOnBall;
    state.wheelOnBall = false;
    if (!onGround && wheelCount >= 4)
    {
        for (const PhysicalEntity& candidate :
             PhysicalEntitySystem::instance().entities())
        {
            if (candidate.persistenceId != "arena-ball")
                continue;
            const glm::vec3 ballCenter(candidate.transform[3]);
            const float ballRadius =
                candidate.halfExtents.x > 0.01f ? candidate.halfExtents.x : 0.9125f;
            int touching = 0;
            for (const VehicleWheelState& wheel : state.wheels)
            {
                const glm::vec3 wheelCenter =
                    glm::dot(wheel.worldCenter, wheel.worldCenter) > 1e-6f
                        ? wheel.worldCenter
                        : glm::vec3(entity.transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                if (glm::length(wheelCenter - ballCenter) <= ballRadius + 0.30f)
                    ++touching;
            }
            if (touching >= 4)
                state.wheelOnBall = true;
            break;
        }
    }

    // ── Jump / flip availability ────────────────────────────────────────
    if (onGround)
    {
        state.lastGroundedTick = tick;
        // JUMP_RESET_TIME_PAD: the wheels are still in contact while the jump
        // force runs, so do not clear the jump until the force has finished (or
        // a short pad after a tap). Without this the ground reset cancels the
        // sustained jump after three ticks.
        const bool jumpPadActive =
            state.isJumping ||
            (state.hasJumped &&
             state.jumpTimer < profile.jump.minSustainSeconds + 0.025f);
        if (!jumpPadActive)
        {
            state.hasFlipped = false;
            state.hasJumped = false;
            state.isFlipping = false;
            state.isJumping = false;
            state.jumpCount = 0;
            state.flipTimer = 0.0f;
            state.jumpTimer = 0.0f;
            state.jumpHoldLatched = false;
            state.airTimeSinceJump = 0.0f;
            // Keep the apex of the jump we just finished until it has been
            // reported, otherwise the landing tick overwrites it with 0.
            if (!state.airborneLogged)
            {
                state.takeoffHeightM = entity.transform[3].z;
                state.apexHeightM = state.takeoffHeightM;
            }
        }
    }
    else if (state.wheelOnBall)
    {
        // Touching the world with all four wheels re-arms the flip while still
        // airborne. Leaving the surface keeps it armed.
        state.hasFlipped = false;
        state.hasJumped = false;
        state.jumpCount = 0;
        state.isFlipping = false;
        state.flipTimer = 0.0f;
        // Restart the double-jump clock so the restored flip is immediately
        // usable, exactly like a jump from the ground.
        state.airTimeSinceJump = 0.0f;
        if (!wasOnBall)
        {
            ++state.flipResetCount;
            state.lastFlipResetTick = tick;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.flip.reset", std::to_string(state.id),
                "all four wheels touched the ball: aerial flip restored", tick,
                {{"vehicle_id", state.id},
                 {"profile", profile.id},
                 {"flip_resets", state.flipResetCount},
                 {"height_m", entity.transform[3].z}},
                __FILE__, __LINE__, __func__);
        }
    }

    if (!onGround)
        state.apexHeightM = std::max(
            state.apexHeightM, static_cast<float>(entity.transform[3].z));

    const bool jumpPressed = state.jumpHeld && !state.jumpHeldPrev;
    const glm::vec3 com = physicalEntityCenterOfMassWorld(entity);
    const float mass = std::max(entity.mass, 1.0f);

    if (jumpPressed)
    {
        if (onGround && state.jumpCount == 0)
        {
            state.jumpCount = 1;
            state.jumpStartedTick = tick;
            state.lastJumpPressedTick = tick;
            state.hasJumped = true;
            state.isJumping = true;
            state.jumpTimer = 0.0f;
            state.jumpHoldLatched = false;
            state.airTimeSinceJump = 0.0f;
            state.takeoffHeightM = entity.transform[3].z;
            state.apexHeightM = state.takeoffHeightM;
            applyPhysicalEntityImpulse(
                entity, up * (profile.jump.immediateDeltaVelMps * mass), com);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.jump", std::to_string(state.id),
                "rocket league jump impulse applied", tick,
                {{"vehicle_id", state.id},
                 {"profile", profile.id},
                 {"jump_count", state.jumpCount},
                 {"delta_vel_mps", profile.jump.immediateDeltaVelMps},
                 {"predicted_apex_m",
                  (profile.jump.immediateDeltaVelMps *
                   profile.jump.immediateDeltaVelMps) /
                      (2.0f * std::max(profile.gravityMps2, 0.1f))}},
                __FILE__, __LINE__, __func__);
        }
        else if (!onGround && state.jumpCount <= 1 &&
                 state.airTimeSinceJump <=
                     profile.jump.doubleJumpWindowSeconds)
        {
            state.lastJumpPressedTick = tick;
            // Dodge activation uses the MAGNITUDE of the rotational inputs;
            // direction uses the SIGNS. Opposing yaw and roll therefore still
            // consume the dodge while producing zero direction.
            const float magnitude = std::fabs(state.pitchInput) +
                                    std::fabs(state.yawInput) +
                                    std::fabs(state.rollInput);
            if (magnitude >= profile.jump.dodgeDeadzone)
            {
                state.jumpCount = 2;
                state.hasFlipped = true;
                state.isFlipping = true;
                state.flipTimer = 0.0f;
                state.isJumping = false;

                const float vertical = state.pitchInput;
                const float horizontal = state.yawInput + state.rollInput;
                glm::vec3 dodgeWorld(0.0f);
                if (std::fabs(vertical) >=
                        profile.flip.directionCancelThreshold ||
                    std::fabs(horizontal) >=
                        profile.flip.directionCancelThreshold)
                {
                    const glm::vec3 raw = forward * vertical + right * horizontal;
                    if (glm::dot(raw, raw) > 1e-6f)
                        dodgeWorld = glm::normalize(raw);
                }
                state.dodgeWorldDir = dodgeWorld;
                state.flipAxisWorld =
                    glm::dot(dodgeWorld, dodgeWorld) > 1e-6f
                        ? glm::normalize(glm::cross(up, dodgeWorld))
                        : glm::vec3(0.0f);

                if (glm::dot(dodgeWorld, dodgeWorld) > 1e-6f)
                {
                    glm::vec3 forward2D(forward.x, forward.y, 0.0f);
                    glm::vec3 right2D(right.x, right.y, 0.0f);
                    if (glm::dot(forward2D, forward2D) > 1e-6f)
                        forward2D = glm::normalize(forward2D);
                    if (glm::dot(right2D, right2D) > 1e-6f)
                        right2D = glm::normalize(right2D);
                    const float forwardSpeed = glm::dot(entity.velocity, forward2D);
                    const float ratio = std::clamp(
                        std::fabs(forwardSpeed) /
                            std::max(profile.ground.maxSpeedMps, 0.1f),
                        0.0f, 1.0f);
                    const float forwardComponent = glm::dot(dodgeWorld, forward2D);
                    const bool backwards =
                        std::fabs(forwardSpeed) < 1.0f
                            ? forwardComponent < 0.0f
                            : ((forwardComponent >= 0.0f) != (forwardSpeed >= 0.0f));
                    const float maxSpeedScale =
                        backwards ? profile.flip.backwardMaxSpeedScale
                                  : profile.flip.forwardMaxSpeedScale;
                    float deltaForward =
                        forwardComponent * profile.flip.initialDeltaVelMps *
                        (((maxSpeedScale - 1.0f) * ratio) + 1.0f);
                    if (backwards)
                        deltaForward *= profile.flip.backwardScaleX;
                    const float deltaSide =
                        glm::dot(dodgeWorld, right2D) *
                        profile.flip.initialDeltaVelMps *
                        (((profile.flip.sideMaxSpeedScale - 1.0f) * ratio) + 1.0f);
                    glm::vec3 deltaVel =
                        forward2D * deltaForward + right2D * deltaSide;
                    deltaVel.z = 0.0f;
                    applyPhysicalEntityImpulse(entity, deltaVel * mass, com);
                }

                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "vehicle.dodge", std::to_string(state.id),
                    "dodge program started", tick,
                    {{"vehicle_id", state.id},
                     {"profile", profile.id},
                     {"pitch", state.pitchInput},
                     {"yaw", state.yawInput},
                     {"roll", state.rollInput},
                     {"magnitude", magnitude},
                     {"stall", glm::dot(state.flipAxisWorld,
                                        state.flipAxisWorld) < 1e-6f},
                     {"dodge_dir",
                      {state.dodgeWorldDir.x, state.dodgeWorldDir.y,
                       state.dodgeWorldDir.z}}},
                    __FILE__, __LINE__, __func__);
            }
            else
            {
                state.jumpCount = 2;
                state.hasJumped = true;
                applyPhysicalEntityImpulse(
                    entity, up * (profile.jump.doubleJumpDeltaVelMps * mass), com);
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "vehicle.jump.double", std::to_string(state.id),
                    "rocket league double jump applied", tick,
                    {{"vehicle_id", state.id},
                     {"profile", profile.id},
                     {"jump_count", state.jumpCount},
                     {"delta_vel_mps", profile.jump.doubleJumpDeltaVelMps}},
                    __FILE__, __LINE__, __func__);
            }
        }
    }

    // Sustained jump force. Height is an output of this plus gravity, never a
    // target pose.
    if (state.isJumping)
    {
        if (state.jumpTimer >= profile.jump.minSustainSeconds &&
            state.jumpHeld)
            state.jumpHoldLatched = true;
        const bool sustain =
            state.jumpTimer < profile.jump.minSustainSeconds ||
            ((state.jumpHeld || state.jumpHoldLatched) &&
             state.jumpTimer < profile.jump.sustainedWindowSeconds);
        if (sustain)
        {
            float accel = profile.jump.sustainedAccelMps2;
            if (state.jumpTimer < profile.jump.minSustainSeconds)
                accel *= profile.jump.preMinAccelScale;
            applyPhysicalEntityImpulse(entity, up * (accel * mass * dt), com);
        }
        else
        {
            state.isJumping = false;
        }
    }
    if (state.hasJumped)
        state.jumpTimer += dt;

    // Surface adhesion still pulls for a few ticks after a jump, so a jump from
    // a wall or ceiling leaves the surface instead of sticking.
    if (state.hasJumped && state.jumpTimer <= profile.jump.stickyTicks * dt)
    {
        applyPhysicalEntityImpulse(
            entity, -up * (profile.jump.stickyAccelMps2 * mass * dt), com);
    }

    if (!onGround && !state.isJumping && state.hasJumped)
        state.airTimeSinceJump += dt;

    state.jumpHeldPrev = state.jumpHeld;

    // ── Boost: force along the car's nose ───────────────────────────────
    if (state.boostInput > 0.01f && state.boostCharge > 0.0f)
    {
        const float accel = onGround ? profile.boost.groundAccelMps2
                                     : profile.boost.airAccelMps2;
        applyPhysicalEntityImpulse(
            entity, forward * (accel * state.boostInput * mass * dt), com);
        state.boostCharge = std::max(
            0.0f, state.boostCharge -
                      profile.boost.drainPerTick * tickScale *
                          state.boostInput);
        if (!state.boostActive)
        {
            state.boostActive = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.boost", std::to_string(state.id),
                "rocket league boost force applied", tick,
                {{"vehicle_id", state.id},
                 {"profile", profile.id},
                 {"charge", state.boostCharge},
                 {"accel_mps2", accel}},
                __FILE__, __LINE__, __func__);
        }
    }
    else if (state.boostActive)
    {
        state.boostActive = false;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.boost.stop", std::to_string(state.id),
            "arena profile boost stopped", tick,
            {{"vehicle_id", state.id},
             {"charge", state.boostCharge}},
            __FILE__, __LINE__, __func__);
    }

    // ── Dodge program ───────────────────────────────────────────────────
    // A finite torque, not a scripted rotation. Holding the stick against the
    // flip's pitch component scales it to zero (flip cancel) while the linear
    // impulse already granted is kept.
    const bool stall =
        glm::dot(state.flipAxisWorld, state.flipAxisWorld) < 1e-6f;
    bool airControlAllowed = !state.isFlipping;
    if (state.isFlipping)
    {
        if (state.flipTimer > profile.flip.torqueSeconds)
        {
            state.isFlipping = false;
            airControlAllowed = true;
        }
        else
        {
            const float axisPitch = glm::dot(state.flipAxisWorld, right);
            float pitchScale = 1.0f;
            if (std::fabs(axisPitch) > 0.01f &&
                std::fabs(state.pitchInput) > 0.01f &&
                state.pitchInput * axisPitch < 0.0f)
            {
                pitchScale = 1.0f - std::min(std::fabs(state.pitchInput), 1.0f);
                airControlAllowed = true;
            }
            const float pitchWeight = std::fabs(axisPitch);
            const float rollWeight =
                std::fabs(glm::dot(state.flipAxisWorld, forward));
            const float accel =
                profile.flip.angularAccelPitch * pitchWeight * pitchScale +
                profile.flip.angularAccelRoll * rollWeight;
            if (accel > 0.0f)
                entity.angularVelocity += state.flipAxisWorld * (accel * dt);

            // Vertical damping is independent of the directional torque. This
            // is why a zero-direction dodge (a stall) hangs rather than drops.
            if (state.flipTimer >= profile.flip.zDampStartSeconds &&
                (entity.velocity.z < 0.0f ||
                 state.flipTimer < profile.flip.zDampEndSeconds))
            {
                entity.velocity.z *= std::pow(
                    std::max(0.0f, 1.0f - profile.flip.zDampFactor),
                    dt * 120.0f);
            }
            state.flipTimer += dt;
        }
    }
    if (stall && state.isFlipping)
        airControlAllowed = true;

    // ── Air control: alpha = A*u - D*w per local axis ───────────────────
    if (!onGround)
    {
        if (airControlAllowed)
        {
            const float wPitch = glm::dot(entity.angularVelocity, right);
            const float wYaw = glm::dot(entity.angularVelocity, up);
            const float wRoll = glm::dot(entity.angularVelocity, forward);
            const float pitchDampScale =
                profile.airControl.inputScaledDamping
                    ? (1.0f - std::fabs(state.pitchInput))
                    : 1.0f;
            const float yawDampScale =
                profile.airControl.inputScaledDamping
                    ? (1.0f - std::fabs(state.yawInput))
                    : 1.0f;
            const float aPitch =
                profile.airControl.pitchAccel * state.pitchInput -
                profile.airControl.pitchDamping * pitchDampScale * wPitch;
            const float aYaw =
                profile.airControl.yawAccel * state.yawInput -
                profile.airControl.yawDamping * yawDampScale * wYaw;
            const float aRoll =
                profile.airControl.rollAccel * state.rollInput -
                profile.airControl.rollDamping * wRoll;
            // +right pitches the nose down, -forward rolls right, +up yaws
            // right. These are the car's own axes, so the controls stay
            // car-relative no matter where the camera points.
            entity.angularVelocity +=
                (right * aPitch - forward * aRoll + up * aYaw) * dt;
        }

        const float airThrottle = state.throttle - state.brake;
        if (std::fabs(airThrottle) > 0.01f)
            applyPhysicalEntityImpulse(
                entity, forward * (profile.airControl.throttleAirAccelMps2 *
                                   airThrottle * mass * dt), com);

        const bool active = std::fabs(state.pitchInput) +
                                std::fabs(state.yawInput) +
                                std::fabs(state.rollInput) >
                            0.1f;
        if (active && !state.airRollActive)
        {
            state.airRollActive = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.airroll", std::to_string(state.id),
                "air control torque started", tick,
                {{"vehicle_id", state.id},
                 {"pitch", state.pitchInput},
                 {"yaw", state.yawInput},
                 {"roll", state.rollInput}},
                __FILE__, __LINE__, __func__);
        }
        else if (!active)
        {
            state.airRollActive = false;
        }
    }
    else
    {
        state.airRollActive = false;
    }

    // ── Angular velocity clamp: the only global rotational limit ────────
    const float omega = glm::length(entity.angularVelocity);
    if (profile.airControl.maxAngularVelocity > 0.0f &&
        omega > profile.airControl.maxAngularVelocity)
        entity.angularVelocity *= profile.airControl.maxAngularVelocity / omega;

    // ── Ground drive: throttle / brake / coast curves ───────────────────
    if (onGround)
    {
        // Drive along the surface the wheels are on: horizontal on the floor,
        // projected onto the contact plane on walls and the ceiling.
        glm::vec3 driveDir(forward);
        if (grounded > 0)
        {
            glm::vec3 contactNormal(0.0f);
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.grounded)
                    contactNormal += wheel.contactNormal;
            if (glm::dot(contactNormal, contactNormal) > 1e-6f)
                contactNormal = glm::normalize(contactNormal);
            if (contactNormal.z > 0.5f)
            {
                driveDir = glm::vec3(forward.x, forward.y, 0.0f);
            }
            else
            {
                driveDir = forward - contactNormal *
                                       glm::dot(forward, contactNormal);
            }
        }
        if (glm::dot(driveDir, driveDir) > 1e-6f)
            driveDir = glm::normalize(driveDir);
        else
            driveDir = forward;
        const float vf = glm::dot(entity.velocity, driveDir);
        const float drive =
            std::clamp(state.throttle - state.brake, -1.0f, 1.0f);
        float desired = vf;
        if (std::fabs(drive) <= profile.ground.throttleDeadzone)
        {
            const float step = profile.ground.coastingDecelMps2 * dt;
            desired = vf > 0.0f ? std::max(0.0f, vf - step)
                                : std::min(0.0f, vf + step);
        }
        else
        {
            const float dir = drive > 0.0f ? 1.0f : -1.0f;
            if (vf * dir < -0.25f)
            {
                desired = vf + dir * profile.ground.brakingDecelMps2 * dt;
                if (desired * dir > 0.0f)
                    desired = 0.0f;
            }
            else
            {
                // Velocity-dependent approach to the unboosted top speed. The
                // 0.75 s time constant matches the documented RL acceleration
                // fit, so 0 -> 14.1 m/s is an outcome, not a hardcoded ramp.
                const float along = std::max(0.0f, vf * dir);
                const float approach =
                    (profile.ground.maxUnboostedMps - along) / 0.75f;
                const float accel =
                    std::clamp(approach, 0.0f, 20.0f) * std::fabs(drive);
                desired = vf + dir * accel * dt;
            }
        }
        const float deltaV = desired - vf;
        if (std::fabs(deltaV) > 1e-5f)
            applyPhysicalEntityImpulse(entity, driveDir * (deltaV * mass), com);

        const float rate = state.powerslide
                               ? profile.ground.powerslideRisePerSecond
                               : -profile.ground.powerslideFallPerSecond;
        state.handbrakeVal =
            std::clamp(state.handbrakeVal + rate * dt, 0.0f, 1.0f);
        state.handbrake = state.handbrakeVal;
    }
    else
    {
        state.handbrakeVal = std::clamp(
            state.handbrakeVal - profile.ground.powerslideFallPerSecond * dt,
            0.0f, 1.0f);
        state.handbrake = state.handbrakeVal;
    }

    // ── Total speed clamp: cars never exceed the RL maximum ─────────────
    if (profile.ground.maxSpeedMps > 0.0f)
    {
        const float speed = glm::length(entity.velocity);
        if (speed > profile.ground.maxSpeedMps)
            entity.velocity *= profile.ground.maxSpeedMps / speed;
    }

    // ── Orientation / resting state (diagnostics only) ──────────────────
    // Orienting the car is ordinary contact physics: the wheels are real
    // colliders and the collision slab is narrower and lower than them, so a
    // tipped car has no stable face to rest on. Nothing here applies a torque.
    state.upDotWorldUp = up.z;
    state.wheelMask = 0;
    for (int i = 0; i < wheelCount; ++i)
        if (state.wheels[static_cast<size_t>(i)].grounded)
            state.wheelMask |= (1 << i);

    if (!state.wasSleeping && entity.sleeping)
    {
        state.wasSleeping = true;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.rest", std::to_string(state.id),
            "body came to rest through contact physics", tick,
            {{"vehicle_id", state.id},
             {"profile", profile.id},
             {"up_dot_world_up", state.upDotWorldUp},
             {"tilt_deg", glm::degrees(std::acos(std::clamp(
                              state.upDotWorldUp, -1.0f, 1.0f)))},
             {"wheel_mask", state.wheelMask},
             {"wheels_contact", grounded},
             {"speed_mps", glm::length(entity.velocity)},
             {"mass_kg", entity.mass}},
            __FILE__, __LINE__, __func__);
    }
    else if (state.wasSleeping && !entity.sleeping)
    {
        state.wasSleeping = false;
    }

    // ── Metric telemetry ────────────────────────────────────────────────
    const glm::vec3 position(entity.transform[3]);
    if (state.lastTelemetryTick == 0)
    {
        state.lastTelemetryPosition = position;
        state.lastTelemetryTick = tick;
    }
    if (tick - state.lastTelemetryTick >= 30)
    {
        const float step = glm::length(
            glm::vec2(position.x - state.lastTelemetryPosition.x,
                      position.y - state.lastTelemetryPosition.y));
        state.distanceTravelled += step;
        state.lastTelemetryPosition = position;
        state.lastTelemetryTick = tick;
        const float speed = glm::length(entity.velocity);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.telemetry", std::to_string(state.id),
            "metric vehicle state sample", tick,
            {{"vehicle_id", state.id},
             {"profile", profile.id},
             {"speed_mps", speed},
             {"speed_kmh", speed * 3.6f},
             {"up_dot_world_up", state.upDotWorldUp},
             {"tilt_deg", glm::degrees(std::acos(std::clamp(
                              state.upDotWorldUp, -1.0f, 1.0f)))},
             {"wheel_mask", state.wheelMask},
             {"asleep", entity.sleeping},
             {"altitude_agl_m", position.z},
             {"airborne", !onGround},
             {"wheels_contact", grounded},
             {"throttle_input", state.throttle},
             {"steer_input", state.steer},
             {"pitch_input", state.pitchInput},
             {"yaw_input", state.yawInput},
             {"roll_input", state.rollInput},
             {"angular_speed", glm::length(entity.angularVelocity)},
             {"distance_travelled_m", state.distanceTravelled},
             {"apex_height_m", state.apexHeightM},
             {"flip_resets", state.flipResetCount},
             {"jump_timer", state.jumpTimer},
             {"is_jumping", state.isJumping},
             {"jump_held", state.jumpHeld},
             {"jump_count", state.jumpCount},
             {"ballcam", state.ballCam}},
            __FILE__, __LINE__, __func__);

        if (!onGround && !state.airborneLogged)
        {
            state.airborneLogged = true;
            state.airborneSinceTick = tick;
        }
        else if (onGround && state.airborneLogged)
        {
            state.airborneLogged = false;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.jump.apex", std::to_string(state.id),
                "measured jump apex and airtime", tick,
                {{"vehicle_id", state.id},
                 {"apex_height_m", state.apexHeightM - state.takeoffHeightM},
                 {"air_time_seconds",
                  static_cast<float>(tick - state.airborneSinceTick) / 60.0f}},
                __FILE__, __LINE__, __func__);
            state.takeoffHeightM = entity.transform[3].z;
            state.apexHeightM = state.takeoffHeightM;
        }
    }

    // ── Heading diagnostic ──────────────────────────────────────────────
    // Bounded sample of chassis heading against planar velocity. The car has
    // been observed sliding at very large slip with no steering input; this
    // pins whether the forward convention or the spawn orientation is at fault.
    {
        const float planarSpeed =
            glm::length(glm::vec2(entity.velocity.x, entity.velocity.y));
        if (planarSpeed > 2.0f && tick - state.lastHeadingTick >= 60)
        {
            state.lastHeadingTick = tick;
            glm::vec3 forward2D(forward.x, forward.y, 0.0f);
            if (glm::dot(forward2D, forward2D) > 1e-6f)
                forward2D = glm::normalize(forward2D);
            const glm::vec3 velocity2D = glm::normalize(
                glm::vec3(entity.velocity.x, entity.velocity.y, 0.0f));
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.heading", std::to_string(state.id),
                "chassis forward against planar velocity", tick,
                {{"vehicle_id", state.id},
                 {"forward", {forward.x, forward.y, forward.z}},
                 {"velocity2d", {velocity2D.x, velocity2D.y}},
                 {"slip_deg",
                  glm::degrees(std::acos(std::clamp(
                      glm::dot(forward2D, velocity2D), -1.0f, 1.0f)))},
                 {"speed_mps", planarSpeed},
                 {"steer", state.steer},
                 {"up_dot", state.upDotWorldUp}},
                __FILE__, __LINE__, __func__);
        }
    }

    state.speed =
        glm::length(glm::vec2(entity.velocity.x, entity.velocity.y));
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

        if (state.serverMirror && !state.clientPredicted)
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
        state.boostInput = input.boost;
        state.jumpHeld = input.jumpHeld;
        state.powerslide = input.powerslide;
        state.pitchInput = input.pitch;
        state.yawInput = input.yaw;
        state.rollInput = input.roll;
        if (input.ballCamToggle)
            state.ballCam = !state.ballCam;

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

        // An arcade profile owns acceleration directly (Rocket League curves),
        // so the template drivetrain must not add engine torque on top.
        {
            const VehicleProfile* activeProfile = profileFor(state);
            state.torqueScale =
                (activeProfile && activeProfile->arcade) ? 0.0f : 1.0f;
        }
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

        if (const VehicleProfile* profile = profileFor(state))
            updateArcadeControls(state, *profile, *entity, dt);

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
                // Any supported pose gets suspension/tire angular damping, not
                // just a four-wheel stance. This bleeds the energy of a car
                // tumbling on a wheel so contact physics can finish the settle.
                if (grounded >= 1)
                {
                    const float angularDamping =
                        spec->stabilityAngularDamping;
                    entity->angularVelocity *= std::max(
                        0.0f, 1.0f - angularDamping * dt);
                }
                if (allGrounded)
                {
                    const float angularDamping =
                        spec->stabilityAngularDamping;
                    entity->angularVelocity *= std::max(
                        0.0f, 1.0f - angularDamping * dt);
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
