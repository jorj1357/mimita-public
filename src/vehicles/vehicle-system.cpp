// 2026-10-27

#include "vehicles/vehicle-system.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <random>
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
#include "audio/audio.h"
#include "effects/effect-part.h"
#include "entities/player.h"
#include "gamemode/map-config.h"
#include "impact/destructible-world-config.h"
#include "input/gamepad-input.h"
#include "input/input-commands.h"
#include "input/input-router.h"
#include "interaction/interaction-system.h"
#include "network/multiplayer-context.h"
#include "camera.h"
#include "config/camera-config.h"
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
void vehicleFeedbackSound(const char* name, const glm::vec3& position,
                          float volume);
EffectPart* vehicleFeedbackBurst(const glm::vec3& position,
                                 const glm::vec3& color, float scale,
                                 float lifetime, bool gravity,
                                 const char* label);
} // namespace

namespace {

constexpr float kPi = 3.14159265358979323846f;

// One owner for "is this logical vehicle action held?". The vehicle profile
// maps a logical action to an input-command action or a raw key/mouse name,
// and the pad publishes its own device mapping (config/input/gamepad.json).
// Nothing in the vehicle input path reads a global key directly, so a rebind
// in profile or pad config can never be shadowed by a hardcoded fallback.
bool profileBindingHeld(const VehicleProfile* profile, const char* logical)
{
    if (!profile)
        return false;
    const auto it = profile->bindings.find(logical);
    if (it == profile->bindings.end())
        return false;
    const std::string& binding = it->second;
    InputCommandSystem& commands = InputCommandSystem::instance();
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
}

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
    // An empty id from a mode that did not name a profile must not wipe the
    // active one: doing so removed every vehicle's profile and silently
    // disabled all key bindings (client run 20261127_042504: local vehicle
    // profile "", walkforward held true, throttle 0).
    if (profileId.empty() && !mActiveProfileId.empty())
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "vehicle.profile.empty_ignored", mActiveProfileId,
            "empty profile id ignored; keeping the active profile", 0,
            {{"active_profile", mActiveProfileId}});
        return;
    }
    mActiveProfileId = profileId;
    for (VehicleState& state : mVehicles)
        state.profileId = profileId;
}

VehicleProfileRegistry::ProfilePtr VehicleSystem::profileFor(
    const VehicleState& state) const
{
    if (!state.profileId.empty())
    {
        if (VehicleProfileRegistry::ProfilePtr found =
                VehicleProfileRegistry::instance().find(state.profileId))
            return found;
    }
    if (!mActiveProfileId.empty())
    {
        if (VehicleProfileRegistry::ProfilePtr found =
                VehicleProfileRegistry::instance().find(mActiveProfileId))
            return found;
    }
    // Last resort so a client that never learned the gamemode profile still has
    // bindings. Physics stays server-authoritative; this only restores input.
    return VehicleProfileRegistry::instance().defaultProfile();
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

    // The collision box is the car's whole envelope. Its bottom face is the
    // wheel contact plane (see collisionCenter), so the wheels can never sit
    // below the surface at rest.
    const glm::vec3 collisionHalf =
        glm::dot(spec->collisionHalfExtents, spec->collisionHalfExtents) > 1e-6f
            ? spec->collisionHalfExtents
            : spec->halfExtents;
    std::vector<CollisionTriangle> triangles;
    buildBoxCollisionTriangles(triangles, spec->collisionCenter, collisionHalf);
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
    // The shared object angular damping (1.5/s) is for crates; applied to a car
    // it bleeds a flip's rotation before the dodge can complete 360 degrees.
    entity->angularDampingOverride = 0.2f;
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
    // Spawn at rest: a chassis that starts with angular velocity spins during the
    // drop and lands crooked, which looked like "rotates for no reason".
    entity->velocity = glm::vec3(0.0f);
    entity->angularVelocity = glm::vec3(0.0f);

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
    if (auto profile = profileFor(state))
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
        buildBoxCollisionTriangles(triangles, spec->collisionCenter,
                                   collisionHalf);
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
        {
            mLocalVehicleId = 0;
            CamConfig::instance().clearProximityFadeOverride();
        }
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
        // ~2 s of life. An injection is a poke, not a takeover.
        state->inputOverrideTicks = 120;
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
    state->inputOverrideTicks = 0;
    // Client-side prediction: when this client owns the car, it simulates the
    // chassis locally so throttle/steer/jump respond on the input tick instead
    // of after a server round trip. The local car is the authority for its own
    // pose; the server adopts it and snapshots never rubber-band it.
    if (!isServerProcess() && !state->clientPredicted)
    {
        PhysicalEntity* chassis =
            PhysicalEntitySystem::instance().find(state->entityId);
        if (chassis)
        {
            const bool wasServerDriven = chassis->serverDriven;
            chassis->serverDriven = false;
            chassis->clientPredicted = true;
            chassis->renderPoseTimed = false;
            chassis->sleeping = false;
            state->serverMirror = false;
            state->clientPredicted = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "vehicle.prediction.enabled", std::to_string(vehicleId),
                "local client now predicts and owns its chassis", 0,
                {{"vehicle_id", vehicleId},
                 {"chassis_entity", chassis->id},
                 {"chassis_network_id", chassis->networkId},
                 {"was_server_driven", wasServerDriven},
                 {"server_authoritative", state->serverAuthoritative},
                 {"mp_active", MP_CONTEXT.active}},
                __FILE__, __LINE__, __func__);
        }
        else
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "vehicle.prediction.blocked", std::to_string(vehicleId),
                "local seat has no chassis entity to predict", 0,
                {{"vehicle_id", vehicleId},
                 {"entity_id", state->entityId},
                 {"mp_active", MP_CONTEXT.active}},
                __FILE__, __LINE__, __func__);
        }
    }
    if (seat >= 0 && seat < static_cast<int>(state->seatOccupants.size()))
        state->seatOccupants[seat] = static_cast<int>(actorId);
    mLocalVehicleId = vehicleId;
    // Vehicle arena presentation: the chase camera sits ~2.2 m from the car,
    // so world proximity fade makes the wall/floor in front of it translucent.
    // Force it off while seated; the mode JSON keeps the same default for the
    // gamemode policy path.
    CamConfig::instance().setProximityFadeOverride(false);
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
    CamConfig::instance().clearProximityFadeOverride();
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
    // Presentation pose: fixed-tick alpha for the local predicted chassis so the
    // camera never follows the raw 120 Hz step.
    return entity ? physicalEntityRenderPosition(*entity) : glm::vec3(0.0f);
}

bool VehicleSystem::resetToPose(uint32_t vehicleId, const glm::vec3& position,
                                float yawDegrees)
{
    VehicleState* state = find(vehicleId);
    if (!state)
        return false;
    PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state->entityId);
    if (!entity)
        return false;
    const glm::quat attitude = glm::angleAxis(
        glm::radians(yawDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    entity->transform = glm::translate(glm::mat4(1.0f), position) *
                        glm::mat4_cast(attitude);
    entity->previousTransform = entity->transform;
    entity->orientation = glm::normalize(attitude);
    entity->velocity = glm::vec3(0.0f);
    entity->angularVelocity = glm::vec3(0.0f);
    entity->sleeping = false;
    entity->sleepTicks = 0;
    entity->renderPoseTimed = false;
    state->origin = position;
    state->originVelocity = glm::vec3(0.0f);
    state->originAngularVelocity = glm::vec3(0.0f);
    state->orientation = entity->orientation;
    state->jumpCount = 0;
    state->jumpHeld = false;
    state->jumpHeldPrev = false;
    state->jumpPressed = false;
    state->hasFlipped = false;
    state->isFlipping = false;
    state->isJumping = false;
    state->hasJumped = false;
    state->flipTimer = 0.0f;
    state->jumpTimer = 0.0f;
    state->airTimeSinceJump = 0.0f;
    state->sideRestTicks = 0;
    state->sideRecoveryLogged = false;
    state->isAutoFlipping = false;
    state->autoFlipTimer = 0.0f;
    if (auto profile = profileFor(*state))
        state->boostCharge = profile->boost.max;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.reset", std::to_string(vehicleId),
        "vehicle reset to spawn pose", 0,
        {{"vehicle_id", vehicleId},
         {"position", {position.x, position.y, position.z}},
         {"yaw_deg", yawDegrees}},
        __FILE__, __LINE__, __func__);
    return true;
}

VehicleInput VehicleSystem::readLocalInput(const VehicleState& state)
{
    VehicleInput input;
    InputCommandSystem& commands = InputCommandSystem::instance();
    const GamepadInput& pad = GamepadInput::instance();
    auto profile = profileFor(state);

    // A local car with no resolved profile still drives from the pad, but every
    // key binding silently returns false because bindings live in the profile.
    // Log it once so "keys do nothing, pad works" is attributable.
    if (!profile && state.localDriver)
    {
        static bool sLogged = false;
        if (!sLogged)
        {
            sLogged = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "vehicle.input.no_profile", std::to_string(state.id),
                "local vehicle has no resolved profile; key bindings inactive", 0,
                {{"vehicle_id", state.id},
                 {"profile_id", state.profileId},
                 {"active_profile", mActiveProfileId}});
        }
    }

    const auto boundHeld = [&](const char* logical) {
        return profileBindingHeld(profile.get(), logical);
    };

    input.throttle =
        std::max(boundHeld("throttle") ? 1.0f : 0.0f, pad.throttle());
    input.brake = std::max(boundHeld("brake") ? 1.0f : 0.0f, pad.brake());

    float steer = pad.steer();
    if (boundHeld("steerLeft"))
        steer = -1.0f;
    if (boundHeld("steerRight"))
        steer = 1.0f;
    input.steer = std::clamp(steer, -1.0f, 1.0f);

    const bool clutchKey = commands.isKeyHeld(GLFW_KEY_LEFT_SHIFT);
    input.clutch = state.mode == VehicleControlMode::Manual
                       ? (clutchKey ? 1.0f : 0.0f)
                       : 0.0f;

    input.boost = std::max(pad.boost() ? 1.0f : 0.0f,
                           boundHeld("boost") ? 1.0f : 0.0f);

    input.jumpHeld = pad.jump() || boundHeld("jump");
    input.powerslide = pad.powerslide() || boundHeld("powerslideAirRoll");
    input.handbrake = input.powerslide ? 1.0f : 0.0f;

    // ── Rotational intent (air) ─────────────────────────────────────────
    // R1 is the free-air-roll modifier: airborne it remaps the horizontal
    // stick from yaw to roll. Square/Circle are directional air roll and stay
    // additive, so holding one against the stick makes yaw + roll == 0, which
    // is the Rocket League stall input. Pitch stays on the vertical stick, and
    // the throttle/brake keys double as pitch for keyboard-only players
    // exactly like Rocket League does.
    const float stickX = std::clamp(input.steer, -1.0f, 1.0f);
    input.pitch = std::clamp(
        pad.pitch() +
            (boundHeld("throttle") ? 1.0f : 0.0f) -
            (boundHeld("brake") ? 1.0f : 0.0f),
        -1.0f, 1.0f);
    float directionalRoll = 0.0f;
    if (pad.airRollLeft())
        directionalRoll -= 1.0f;
    if (pad.airRollRight())
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

void VehicleSystem::pollLocalExitKey(const VehicleState& state)
{
    if (!state.localDriver)
        return;
    const bool held =
        InputCommandSystem::instance().isKeyHeld(GLFW_KEY_F);
    const bool pressed = held && !mPrevExitKey;
    mPrevExitKey = held;
    if (!pressed)
        return;
    VehicleState* mutableState = find(state.id);
    if (!mutableState)
        return;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.exit.key", std::to_string(state.id),
        "local driver pressed the exit key", 0,
        {{"vehicle_id", state.id},
         {"networked", mutableState->serverMirror},
         {"prompt_enabled", mExitPrompt}},
        __FILE__, __LINE__, __func__);
    if (mutableState->serverMirror)
        requestSeat(state.id, -1, true);
    else
        exitLocalDriver();
}

bool VehicleSystem::pollLocalBallCam(const VehicleState& state, uint32_t tick)
{
    if (!state.localDriver || state.inputOverride)
        return false;
    const bool held = GamepadInput::instance().ballCam() ||
                      profileBindingHeld(profileFor(state).get(),
                                         "ballCam");
    const bool toggled = held && !mPrevBallCamHeld;
    mPrevBallCamHeld = held;
    if (!toggled)
        return false;
    VehicleState* mutableState = find(state.id);
    if (!mutableState)
        return false;
    mutableState->ballCam = !mutableState->ballCam;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.ballcam", std::to_string(state.id),
        "local driver toggled ball cam", tick,
        {{"vehicle_id", state.id},
         {"on", mutableState->ballCam},
         {"source", GamepadInput::instance().ballCam() ? "pad"
                                                       : "profile"}},
        __FILE__, __LINE__, __func__);
    return true;
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
        // The ramp profile is a triangulated curve; the nearest-triangle
        // normal steps at each seam and the suspension/friction direction
        // jerks with it. Low-pass only when the new normal is close, so a real
        // surface change (floor -> wall) is not smoothed away.
        if (glm::dot(wheel.contactNormal, wheel.contactNormal) > 1e-4f &&
            glm::dot(wheel.contactNormal, normal) > 0.7f)
            normal = glm::normalize(
                glm::mix(wheel.contactNormal, normal, 0.35f));
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

        auto profile = profileFor(state);
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
            // RocketSim NON_STICKY_FRICTION_FACTOR_CURVE: with no throttle,
            // wheel friction drops to 0.1 on a wall, 0.5 at 45 degrees, and 1.0
            // on the floor. This is what lets a car slide along a wall or roof
            // without catching while still gripping a flat floor.
            if (std::fabs(state.throttle) < 0.05f)
            {
                const float nz = std::clamp(
                    std::fabs(wheel.contactNormal.z), 0.0f, 1.0f);
                const float nonSticky =
                    nz < 0.7075f
                        ? 0.1f + (0.5f - 0.1f) * (nz / 0.7075f)
                        : 0.5f + (1.0f - 0.5f) *
                                     ((nz - 0.7075f) / (1.0f - 0.7075f));
                mu *= nonSticky;
            }
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

    glm::vec3 forward =
        glm::normalize(entity.orientation * glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 right =
        glm::normalize(entity.orientation * glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 up =
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

    // Grounded means every wheel is touching. Any wheel off the surface puts the
    // car in its air-control state, which is what makes air roll respond the
    // moment the car leaves the ground instead of only after a long hop.
    const int wheelCount = static_cast<int>(state.wheels.size());
    const bool onGround =
        wheelCount > 0 && grounded == wheelCount;
    // "Supported" is any ground contact: wheels, or the chassis resting on its
    // side or roof. A car lying on two wheels is on the ground, not flying.
    // Air control and the landing event use this; only jumps and flip resets
    // require all four wheels.
    const bool supported =
        (wheelCount > 0 && grounded > 0) || entity.lastContactedWorld;

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
        // RocketSim `_UpdateAutoFlip`: jump while inverted on a floor-like
        // contact. The impulse lifts the car off the surface; a timed
        // forward-axis torque rolls it back toward the wheels. No pose is set.
        const glm::vec3 autoFlipNormal =
            glm::length(entity.lastContactNormal) > 1e-5f
                ? glm::normalize(entity.lastContactNormal)
                : glm::vec3(0.0f, 0.0f, 1.0f);
        const float autoFlipRoll =
            std::atan2(glm::dot(right, glm::vec3(0.0f, 0.0f, 1.0f)),
                       glm::dot(up, glm::vec3(0.0f, 0.0f, 1.0f)));
        if (entity.lastContactedWorld &&
            autoFlipNormal.z > profile.settle.autoFlipNormalZThreshold &&
            std::fabs(autoFlipRoll) > profile.settle.autoFlipRollThresholdRad)
        {
            applyPhysicalEntityImpulse(
                entity, -up * (profile.settle.autoFlipImpulseMps * mass), com);
            state.isAutoFlipping = true;
            state.autoFlipTimer =
                profile.settle.autoFlipSeconds *
                (std::fabs(autoFlipRoll) / glm::pi<float>());
            state.autoFlipTorqueScale = autoFlipRoll >= 0.0f ? 1.0f : -1.0f;
            state.jumpCount = 2;
            state.isJumping = false;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.autoflip", std::to_string(state.id),
                "auto-flip from an inverted contact", tick,
                {{"vehicle_id", state.id},
                 {"up_dot", state.upDotWorldUp},
                 {"roll_rad", autoFlipRoll},
                 {"normal_z", autoFlipNormal.z},
                 {"impulse_mps", profile.settle.autoFlipImpulseMps},
                 {"angular_accel", profile.settle.autoFlipAngularAccel}},
                __FILE__, __LINE__, __func__);
            state.jumpHeldPrev = state.jumpHeld;
        }
        else if (onGround && state.jumpCount == 0)
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
            if (state.localDriver && !mServerProcess)
            {
                const glm::vec3 fxPos(entity.transform[3]);
                vehicleFeedbackSound("vehicle/jump", fxPos, 0.8f);
                vehicleFeedbackBurst(fxPos, glm::vec3(0.55f, 0.85f, 1.0f),
                                     0.38f, 0.28f, false, "vehicle_jump_fx");
            }
        }
        else if (!onGround &&
                 (state.jumpCount <= 1 || profile.jump.infiniteJumps) &&
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
                state.flipRotationRad = 0.0f;
                state.flipCancelLogged = false;
                // Flip cancel is car-relative: remember the stick direction in
                // the car's own input axes. Comparing against world vectors made
                // the check fire spuriously once the car had rotated.
                state.flipCancelLocal = glm::vec2(state.pitchInput,
                                                  state.yawInput +
                                                      state.rollInput);
                state.isJumping = false;

                const float vertical = state.pitchInput;
                const float horizontal = state.yawInput + state.rollInput;
                glm::vec3 dodgeWorld(0.0f);
                if (std::fabs(vertical) >=
                        profile.flip.directionCancelThreshold ||
                    std::fabs(horizontal) >=
                        profile.flip.directionCancelThreshold)
                {
                    // y is the car's left, so a rightward dodge is -right.
                    const glm::vec3 raw =
                        forward * vertical - right * horizontal;
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
                if (state.localDriver && !mServerProcess)
                {
                    const glm::vec3 fxPos(entity.transform[3]);
                    vehicleFeedbackSound("vehicle/airjump", fxPos, 0.85f);
                    vehicleFeedbackBurst(fxPos, glm::vec3(1.0f, 0.55f, 0.15f),
                                         0.45f, 0.32f, false,
                                         "vehicle_airjump_fx");
                }
            }
        }
        else
        {
            // A press that produced nothing. Recorded so a test can tell an
            // input that never arrived from a jump that was rejected.
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.jump.rejected", std::to_string(state.id),
                "jump press rejected", tick,
                {{"vehicle_id", state.id},
                 {"on_ground", onGround},
                 {"jump_count", state.jumpCount},
                 {"air_time_since_jump", state.airTimeSinceJump},
                 {"window_seconds", profile.jump.doubleJumpWindowSeconds}},
                __FILE__, __LINE__, __func__);
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

    // ── RocketSim auto-flip timer ───────────────────────────────────────
    if (state.isAutoFlipping)
    {
        if (state.autoFlipTimer <= 0.0f)
        {
            state.isAutoFlipping = false;
            state.autoFlipTimer = 0.0f;
        }
        else
        {
            const float invI =
                glm::dot(forward,
                         physicalEntityInverseInertiaWorld(entity, forward));
            const float inertia = invI > 1e-6f ? 1.0f / invI : 1.0f;
            applyPhysicalEntityAngularImpulse(
                entity, forward * (profile.settle.autoFlipAngularAccel *
                                   state.autoFlipTorqueScale * inertia * dt));
            state.autoFlipTimer -= dt;
        }
    }

    // ── RocketSim sticky force ──────────────────────────────────────────
    // Wheel contact only. At rest this is 0.5 g into the surface; with
    // throttle or forward speed the wall/ceiling term rises to 1.5 g, which is
    // what keeps a car on a curved wall while boosting.
    glm::vec3 groundUp(0.0f);
    int wheelContacts = 0;
    for (const VehicleWheelState& wheel : state.wheels)
    {
        if (!wheel.grounded)
            continue;
        groundUp += wheel.contactNormal;
        ++wheelContacts;
    }
    if (wheelContacts > 0 && glm::length(groundUp) > 1e-5f)
    {
        groundUp = glm::normalize(groundUp);
        const bool fullStick = std::fabs(state.throttle) > 0.05f ||
                               std::fabs(state.speed) > 0.25f;
        float scale = profile.settle.stickyForceGravityScale;
        if (fullStick)
            scale += 1.0f - std::fabs(groundUp.z);
        // Adhesion must point INTO the surface. groundUp is the wheel contact
        // normal (up on the floor, out of the wall), so the old +groundUp
        // impulse lifted the car off the floor every tick and pushed it away
        // from walls/ceilings. That was the flat-ground pogo and the poor
        // wall/ceiling grip.
        applyPhysicalEntityImpulse(
            entity, -groundUp * (scale * gravity * mass * dt), com);
    }

    // ── RocketSim auto-roll ─────────────────────────────────────────────
    // Throttle only (not boost), and only when the body is actually lying on
    // its roof: inverted AND no wheel is touching. Ceiling driving is also
    // inverted but the wheels are on the ceiling; auto-roll must not fire then
    // or it twists the car mid-slide.
    const bool roofed = state.upDotWorldUp < -0.5f;
    const bool onRoof = roofed && wheelContacts == 0 &&
                        entity.lastContactedWorld;
    if (onRoof && std::fabs(state.throttle) > 0.05f)
    {
        glm::vec3 rollUp = groundUp;
        if (glm::length(rollUp) < 1e-5f && entity.lastContactedWorld &&
            glm::length(entity.lastContactNormal) > 1e-5f)
            rollUp = glm::normalize(entity.lastContactNormal);
        if (glm::length(rollUp) > 1e-5f)
        {
            const glm::vec3 groundDown = -rollUp;
            const glm::vec3 crossRight = glm::cross(rollUp, forward);
            const glm::vec3 crossForward = glm::cross(groundDown, crossRight);
            const float rightFactor = 1.0f - std::clamp(
                glm::dot(right, crossRight), 0.0f, 1.0f);
            const float forwardFactor = 1.0f - std::clamp(
                glm::dot(forward, crossForward), 0.0f, 1.0f);
            const glm::vec3 torqueDirRight =
                forward * (glm::dot(right, rollUp) >= 0.0f ? -1.0f : 1.0f);
            const glm::vec3 torqueDirForward =
                right * (glm::dot(forward, rollUp) >= 0.0f ? 1.0f : -1.0f);
            const glm::vec3 torqueDir =
                torqueDirRight * rightFactor + torqueDirForward * forwardFactor;
            if (glm::length(torqueDir) > 1e-5f)
            {
                const glm::vec3 axis = glm::normalize(torqueDir);
                const float invI = glm::dot(
                    axis, physicalEntityInverseInertiaWorld(entity, axis));
                const float inertia = invI > 1e-6f ? 1.0f / invI : 1.0f;
                applyPhysicalEntityImpulse(
                    entity, groundDown * (profile.settle.autoRollForceMps2 *
                                          mass * dt), com);
                applyPhysicalEntityAngularImpulse(
                    entity, axis * (profile.settle.autoRollAngularAccel *
                                    inertia * dt));
                static uint64_t sLastAutoRollLogTick = 0;
                if (tick - sLastAutoRollLogTick >= 30u)
                {
                    sLastAutoRollLogTick = tick;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Physics, StructuredLevel::Important,
                        "vehicle.autoroll", std::to_string(state.id),
                        "RocketSim auto-roll force and torque applied", tick,
                        {{"vehicle_id", state.id},
                         {"wheel_mask", state.wheelMask},
                         {"ground_up", {rollUp.x, rollUp.y, rollUp.z}},
                         {"right_factor", rightFactor},
                         {"forward_factor", forwardFactor},
                         {"axis", {axis.x, axis.y, axis.z}},
                         {"force_mps2", profile.settle.autoRollForceMps2},
                         {"angular_accel", profile.settle.autoRollAngularAccel},
                         {"throttle", state.throttle}},
                        __FILE__, __LINE__, __func__);
                }
            }
        }
    }

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
            if (state.localDriver && !mServerProcess)
            {
                const glm::vec3 fxPos(entity.transform[3]);
                vehicleFeedbackSound("vehicle/boost", fxPos, 0.9f);
                vehicleFeedbackBurst(
                    fxPos - state.orientation * glm::vec3(0.8f, 0.0f, 0.0f),
                    glm::vec3(1.0f, 0.55f, 0.10f), 0.40f, 0.22f, false,
                    "vehicle_boost_fx");
            }
        }
        // White boost orb behind the car while boost is held.
        if (state.localDriver && !mServerProcess && (tick & 1u) == 0u)
        {
            const glm::vec3 behind =
                glm::vec3(entity.transform[3]) -
                state.orientation * glm::vec3(0.95f, 0.0f, 0.0f) +
                state.orientation * glm::vec3(0.0f, 0.0f, 0.12f);
            if (EffectPart* fx = EffectPartSystem::instance().spawnCustom(
                    behind, glm::vec3(1.0f, 1.0f, 1.0f), 0.10f,
                    "vehicle_boost_sphere"))
            {
                fx->scale = 0.45f;
                fx->halfSize = glm::vec3(0.22f);
                fx->alpha = 0.9f;
                fx->endAlpha = 0.0f;
                fx->useEndAlpha = true;
            }
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
    bool airControlAllowed = !state.isFlipping && !state.isAutoFlipping;
    if (state.isFlipping)
    {
        // A dodge is a fixed 360 degree flip. Stop the torque when the flip has
        // completed a full turn instead of running the torque window to its
        // limit, so the car rotates 360 every time regardless of tuning.
        const bool rotationComplete =
            state.flipRotationRad >= glm::two_pi<float>() - 0.06f;
        if (state.flipTimer > profile.flip.torqueSeconds || rotationComplete)
        {
            state.isFlipping = false;
            airControlAllowed = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.flip", std::to_string(state.id),
                "dodge finished", tick,
                {{"vehicle_id", state.id},
                 {"rotation_deg_total",
                  glm::degrees(state.flipRotationRad)},
                 {"cancelled", state.flipCancelLogged},
                 {"flip_limit", profile.flip.maxAngularVelocity},
                 {"air_limit", profile.airControl.maxAngularVelocity},
                 {"flip_seconds", profile.flip.torqueSeconds},
                 {"angular_speed", glm::length(entity.angularVelocity)}},
                __FILE__, __LINE__, __func__);
            if (state.localDriver && !mServerProcess)
            {
                vehicleFeedbackSound("vehicle/dodge",
                                     glm::vec3(entity.transform[3]), 0.75f);
            }
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
            // Flip cancel: pushing the stick back against the dodge direction
            // stops the flip torque and keeps the momentum already gained. This
            // is the Rocket League half-flip / flip-cancel input and it works on
            // any dodge axis, not only pitch.
            float cancelScale = 1.0f;
            const glm::vec2 stickLocal(state.pitchInput,
                                       state.yawInput + state.rollInput);
            if (state.flipTimer <= profile.flip.pitchLockSeconds &&
                glm::dot(state.dodgeWorldDir, state.dodgeWorldDir) > 1e-6f &&
                glm::dot(stickLocal, stickLocal) > 0.01f &&
                glm::dot(state.flipCancelLocal, state.flipCancelLocal) > 0.01f)
            {
                const float against = glm::dot(
                    glm::normalize(stickLocal),
                    glm::normalize(state.flipCancelLocal));
                if (against < -profile.flip.directionCancelThreshold)
                {
                    cancelScale = 0.0f;
                    airControlAllowed = true;
                    if (!state.flipCancelLogged)
                    {
                        state.flipCancelLogged = true;
                        // Stopping the torque alone leaves the flip's angular
                        // velocity, so the body coasted to a full 360 anyway
                        // (run 20261126_185032: cancelled = true but 359.39
                        // degrees). A flip cancel stops the rotation: remove
                        // the flip-axis component and keep any other spin.
                        const float flipSpin = glm::dot(
                            entity.angularVelocity, state.flipAxisWorld);
                        entity.angularVelocity -= state.flipAxisWorld * flipSpin;
                        StructuredLogger::instance().writeEvent(
                            StructuredCategory::Physics,
                            StructuredLevel::Important,
                            "vehicle.flip.cancel", std::to_string(state.id),
                            "flip cancelled by opposing stick input", tick,
                            {{"vehicle_id", state.id},
                             {"against", against},
                             {"flip_timer", state.flipTimer}},
                            __FILE__, __LINE__, __func__);
                    }
                }
            }
            const float pitchWeight = std::fabs(axisPitch);
            const float rollWeight =
                std::fabs(glm::dot(state.flipAxisWorld, forward));
            const float accel =
                (profile.flip.angularAccelPitch * pitchWeight * pitchScale +
                 profile.flip.angularAccelRoll * rollWeight) * cancelScale;
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
            state.flipRotationRad +=
                glm::length(entity.angularVelocity) * dt;
        }
    }
    if (stall && state.isFlipping)
        airControlAllowed = true;

    // ── Air control: alpha = A*u - D*w per local axis ───────────────────
    // A car with any wheel on a surface is not flying, even if it landed on
    // two wheels or its side.
    if (!supported)
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
            // Axis convention: x is forward, z is up, so y is the car's LEFT
            // (the template compensates with steering.directionSign = -1).
            // Screen-right is therefore -y. +pitch about +y pitches the nose
            // down (correct), while roll and yaw must be negated so the car
            // rotates toward the side the player pushes the stick.
            //   +roll  -> clockwise seen from behind -> top toward screen right
            //   +yaw   -> nose toward screen right
            entity.angularVelocity +=
                (right * aPitch + forward * aRoll - up * aYaw) * dt;
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
            // No rotational input and not flipping: bleed the last of any
            // residual angular velocity so the car does not keep turning in the
            // air for no reason. This is a deadzone on leftover spin, not a
            // control behaviour.
            if (!state.isFlipping)
            {
                if (glm::length(entity.angularVelocity) < 0.30f)
                    entity.angularVelocity = glm::vec3(0.0f);
                else
                    entity.angularVelocity *= std::max(0.0f, 1.0f - 6.0f * dt);
            }
        }
    }
    else
    {
        state.airRollActive = false;
    }

    // ── Angular velocity clamp ─────────────────────────────────────────
    // Air control is limited to CAR_MAX_ANG_SPEED, but a dodge is not air
    // control: Rocket League flips far faster than 5.5 rad/s. Applying the air
    // clamp during a flip capped every dodge at about 205 degrees, which is why
    // a 360 dodge was impossible.
    const float omega = glm::length(entity.angularVelocity);
    const float angularLimit =
        state.isFlipping && profile.flip.maxAngularVelocity > 0.0f
            ? profile.flip.maxAngularVelocity
            : profile.airControl.maxAngularVelocity;
    if (angularLimit > 0.0f && omega > angularLimit)
        entity.angularVelocity *= angularLimit / omega;

    // ── Ground drive: throttle / brake / coast curves ───────────────────
    // Any wheel with contact can drive the car. Requiring all four meant that a
    // front wheel lifting under reverse braked the rear wheels too, so R2 did
    // nothing. Rocket League drives through whichever wheels are touching.
    if (grounded > 0)
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

    // ── Ground attitude: the car follows the surface ────────────────────
    // Rocket League cars are rigid on the ground. Tyre and suspension forces
    // are applied at the contact patch, which otherwise pitches the chassis
    // under acceleration or reverse (weight transfer). A grounded car keeps
    // only its yaw rate and rotates its up axis onto the averaged wheel
    // contact normal, so it drives flat on the floor and still follows ramps.
    if (profile.ground.attitudeLock && supported && up.z > 0.7f &&
        !state.isFlipping &&
        !state.wheelOnBall)
    {
        glm::vec3 surfaceNormal(0.0f);
        for (const VehicleWheelState& wheel : state.wheels)
            if (wheel.grounded)
                surfaceNormal += wheel.contactNormal;
        if (glm::dot(surfaceNormal, surfaceNormal) > 1e-6f)
        {
            surfaceNormal = glm::normalize(surfaceNormal);
            const glm::vec3 chassisUp = glm::normalize(
                entity.orientation * glm::vec3(0.0f, 0.0f, 1.0f));
            const float alignment = glm::dot(chassisUp, surfaceNormal);
            if (profile.ground.alignToSurface && alignment < 0.9999f &&
                alignment > -0.999f)
            {
                const float angle =
                    std::acos(std::clamp(alignment, -1.0f, 1.0f));
                const float step = std::min(
                    angle,
                    glm::radians(profile.ground.alignRateDegPerSecond) * dt);
                const glm::quat delta =
                    glm::rotation(chassisUp, surfaceNormal);
                const glm::quat partial = glm::slerp(
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f), delta,
                    angle > 1e-6f ? step / angle : 0.0f);
                entity.orientation =
                    glm::normalize(partial * entity.orientation);
            }
            // Steering owns the grounded yaw rate. A car cannot rotate in
            // place: the tyres turn it at a rate set by the steering input and
            // how fast it is rolling. Writing the rate directly (instead of
            // letting contact friction decide it) is what makes the car drive
            // straight, stop creeping while parked, and stop spinning when
            // the steering is held. Contact friction previously integrated a
            // tiny asymmetric ground torque into 4 degrees of drift while the
            // car sat still, and 225 degrees of curve over 9 s while driving.
            // Rocket League's yaw rate is roughly constant with speed (about
            // 2.3 rad/s at both 5 and 14 m/s), so the ramp to full rate is
            // short. A /6 ramp turned the car at 0.17 rad/s at 1 m/s, which
            // took 18 s to turn around and left the arena bots circling in a
            // corner.
            const float speedFactor =
                std::clamp(state.speed / 1.5f, 0.0f, 1.0f);
            // Stick right must yaw right. The wheels already apply
            // steering.directionSign (-1); the yaw model used the raw stick, so
            // the body turned left while the wheels pointed right.
            // Car-local +y is left and steering.directionSign is -1 for every
            // template, so a rightward stick is a negative yaw. The wheels
            // already apply the same sign; this is the one place the body yaw
            // is set.
            constexpr float kSteerToYaw = -1.0f;
            const float targetYawRate =
                state.steer * kSteerToYaw *
                profile.ground.maxYawRateRadPerSecond * speedFactor;
            float yawRate = glm::dot(entity.angularVelocity, surfaceNormal);
            const float yawStep =
                profile.ground.yawAccelRadPerSecond2 * dt;
            yawRate +=
                std::clamp(targetYawRate - yawRate, -yawStep, yawStep);
            // Parked cars do not creep: below 0.02 rad/s the residual is noise
            // from the chassis slab resting on the floor.
            if (std::fabs(yawRate) < 0.02f)
                yawRate = 0.0f;
            entity.angularVelocity = surfaceNormal * yawRate;
        }
        // Lateral grip: a grounded car with no steering input does not slide
        // sideways. Powerslide is the exception - that is the whole point of it.
        if (std::fabs(state.steer) < 0.05f && state.speed > 2.0f &&
            !state.powerslide)
        {
            const glm::vec3 flatRight = glm::normalize(
                glm::vec3(right.x, right.y, 0.0f));
            const float lateral =
                glm::dot(glm::vec3(entity.velocity.x, entity.velocity.y, 0.0f),
                         flatRight);
            entity.velocity -= flatRight * (lateral *
                                            std::min(1.0f, 10.0f * dt));
        }
        forward = glm::normalize(
            entity.orientation * glm::vec3(1.0f, 0.0f, 0.0f));
        right = glm::normalize(
            entity.orientation * glm::vec3(0.0f, 1.0f, 0.0f));
        up = glm::normalize(entity.orientation * glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // ── Attitude sample: intent versus actual rotation ──────────────────
    // One event carries everything a proof needs: the decoded intent, the
    // resulting rotation about the car's own axes, how upright it is, and the
    // deepest wheel penetration. "Intent sign == rotation sign", "no pitch or
    // roll while grounded" and "no wheel below the surface" are all literal
    // assertions on these fields.
    if (tick % 6u == 0u)
    {
        float surfaceNormalZ = 0.0f;
        {
            glm::vec3 n(0.0f);
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.grounded)
                    n += wheel.contactNormal;
            const float len = glm::length(n);
            surfaceNormalZ = len > 1e-5f ? (n / len).z : 0.0f;
        }
        // Chassis penetration comes from the contact solver, so it measures the
        // deepest point of the car's real collision shape inside world geometry.
        // A compressed suspension is not penetration: the wheel simply sits
        // higher relative to the hub.
        // A sleeping body is not re-measured, so its last value is stale: a
        // body can only sleep below the 0.05 m penetration gate anyway.
        const float maxPenetration =
            entity.sleeping ? 0.0f : entity.lastContactPenetration;
        // A hard impact can overshoot the solver by a step or two; that is not
        // "caught under the ground". Only a body that is already slow and still
        // inside geometry is the defect this proof looks for.
        const float speedNow = glm::length(entity.velocity);
        const float restingPenetration =
            speedNow < 2.0f ? maxPenetration : 0.0f;
        const float angularSpeed = glm::length(entity.angularVelocity);
        const float pitchRate = glm::dot(entity.angularVelocity, right);
        // Signed so positive is always the driver's right: the car's local +y
        // axis is its left (steering.directionSign = -1), so a right yaw has a
        // negative rate about up. Negating keeps "intent sign == rate sign"
        // true for roll and yaw both.
        const float yawRate = -glm::dot(entity.angularVelocity, up);
        const float rollRate = glm::dot(entity.angularVelocity, forward);
        // Direction contract: in the air, a held roll/yaw must rotate the car
        // the way the stick points. The sample only counts when the input and
        // the response are both meaningful, so drift does not fake agreement.
        const bool airborne = !supported && !state.isFlipping;
        // Only measure an axis while the other one is quiet; a car that is
        // still rolling mixes roll into the yaw reading and vice versa.
        const bool rollMeaningful =
            airborne && std::fabs(state.rollInput) > 0.5f &&
            std::fabs(rollRate) > 0.5f && std::fabs(yawRate) < 0.5f;
        const bool yawMeaningful =
            airborne && std::fabs(state.yawInput) > 0.5f &&
            std::fabs(yawRate) > 0.5f && std::fabs(rollRate) < 0.5f;
        const bool rollAgrees = !rollMeaningful ||
                                ((state.rollInput > 0.0f) == (rollRate > 0.0f));
        const bool yawAgrees = !yawMeaningful ||
                               ((state.yawInput > 0.0f) == (yawRate > 0.0f));
        nlohmann::json attitudeFields = {
            {"vehicle_id", state.id},
            {"grounded", onGround},
            {"wheel_mask", state.wheelMask},
            {"up_dot_world_up", state.upDotWorldUp},
            {"tilt_deg", glm::degrees(std::acos(std::clamp(
                             state.upDotWorldUp, -1.0f, 1.0f)))},
            {"pitch_rate_local", pitchRate},
            {"yaw_rate_local", yawRate},
            {"roll_rate_local", rollRate},
            {"pitch_input", state.pitchInput},
            {"yaw_input", state.yawInput},
            {"roll_input", state.rollInput},
            {"angular_speed", angularSpeed},
            {"max_penetration_m", maxPenetration},
            {"resting_penetration_m", restingPenetration},
            {"supported", supported},
            {"all_wheels", onGround},
            {"resting_contact_count", grounded},
            {"surface_z", surfaceNormalZ},
            {"penetration_normal_z", entity.lastContactNormal.z},
            {"chassis_z", entity.transform[3].z},
            {"roll_meaningful", rollMeaningful},
            {"yaw_meaningful", yawMeaningful},
            {"roll_agrees", rollAgrees},
            {"yaw_agrees", yawAgrees}};
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.attitude", std::to_string(state.id),
            "orientation intent against actual rotation", tick,
            attitudeFields, __FILE__, __LINE__, __func__);

        if (rollMeaningful && !rollAgrees)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.aircontrol.mismatch", std::to_string(state.id),
                "roll intent and rotation direction disagree", tick,
                {{"vehicle_id", state.id},
                 {"axis", "roll"},
                 {"intent", state.rollInput},
                 {"rate", rollRate}},
                __FILE__, __LINE__, __func__);
        }
        if (yawMeaningful && !yawAgrees)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.aircontrol.mismatch", std::to_string(state.id),
                "yaw intent and rotation direction disagree", tick,
                {{"vehicle_id", state.id},
                 {"axis", "yaw"},
                 {"intent", state.yawInput},
                 {"rate", yawRate}},
                __FILE__, __LINE__, __func__);
        }

        if (restingPenetration > 0.01f &&
            tick - state.lastPenetrationLogTick >= 30u)
        {
            state.lastPenetrationLogTick = tick;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.penetration", std::to_string(state.id),
                "the car is below the surface while slow", tick,
                {{"vehicle_id", state.id},
                 {"max_penetration_m", maxPenetration},
                 {"resting_penetration_m", restingPenetration},
                 {"source", "chassis_contact"},
                 {"normal_z", entity.lastContactNormal.z},
                 {"chassis_z", entity.transform[3].z},
                 {"grounded", onGround},
                 {"up_dot_world_up", state.upDotWorldUp},
                 {"speed_mps", speedNow}},
                __FILE__, __LINE__, __func__);
        }
    }

    if (tick % 12u == 0u)
    {
        // Chassis-vs-support geometry: proves the wheels are the lowest part of
        // the car and that they sit on the surface, not below it.
        {
            float lowestWheelZ = std::numeric_limits<float>::max();
            float groundZ = std::numeric_limits<float>::max();
            float surfaceZ = 0.0f;
            glm::vec3 normalSum(0.0f);
            for (const VehicleWheelState& wheel : state.wheels)
            {
                if (!wheel.rayHit)
                    continue;
                const float radius =
                    std::max(wheel.rayDistance - wheel.suspensionLength, 0.0f);
                lowestWheelZ = std::min(lowestWheelZ,
                                        wheel.worldCenter.z - radius);
                if (wheel.grounded)
                {
                    groundZ = std::min(groundZ, wheel.contactPoint.z);
                    normalSum += wheel.contactNormal;
                }
            }
            const float normalLength = glm::length(normalSum);
            if (normalLength > 1e-5f)
                surfaceZ = (normalSum / normalLength).z;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.chassis", std::to_string(state.id),
                "chassis, wheel and support heights", tick,
                {{"vehicle_id", state.id},
                 {"origin_z", entity.transform[3].z},
                 {"lowest_wheel_z",
                  lowestWheelZ == std::numeric_limits<float>::max()
                      ? 0.0f
                      : lowestWheelZ},
                 {"wheel_contact_z",
                  groundZ == std::numeric_limits<float>::max() ? 0.0f
                                                               : groundZ},
                 {"support_normal_z", surfaceZ},
                 {"up_dot_world_up", up.z},
                 {"supported", supported},
                 {"grounded_wheels", grounded},
                 {"speed_mps", glm::length(entity.velocity)},
                 {"angular_speed", glm::length(entity.angularVelocity)}},
                __FILE__, __LINE__, __func__);
        }

        // Consolidated state sample: orientation, per-wheel contact, decoded
        // inputs and settle state in one event so a proof can assert on it.
        const char* support = "none";
        if (wheelCount > 0 && grounded == wheelCount)
            support = state.upDotWorldUp > 0.7f ? "wheels" : "surface";
        else if (grounded > 0)
            support = "partial";
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.state", std::to_string(state.id),
            "orientation, wheels and decoded inputs", tick,
            {{"vehicle_id", state.id},
             {"orientation",
              {entity.orientation.w, entity.orientation.x,
               entity.orientation.y, entity.orientation.z}},
             {"forward", {forward.x, forward.y, forward.z}},
             {"up", {up.x, up.y, up.z}},
             {"up_dot_world_up", state.upDotWorldUp},
             {"tilt_deg", glm::degrees(std::acos(std::clamp(
                              state.upDotWorldUp, -1.0f, 1.0f)))},
             {"angular_velocity",
              {entity.angularVelocity.x, entity.angularVelocity.y,
               entity.angularVelocity.z}},
             {"angular_speed", glm::length(entity.angularVelocity)},
             {"velocity",
              {entity.velocity.x, entity.velocity.y, entity.velocity.z}},
             {"speed_mps", glm::length(entity.velocity)},
             {"airborne", !supported},
             {"grounded_wheels", grounded},
             {"wheel_mask", state.wheelMask},
             {"support", support},
             {"pitch_input", state.pitchInput},
             {"yaw_input", state.yawInput},
             {"roll_input", state.rollInput},
             {"steer_input", state.steer},
             {"throttle_input", state.throttle},
             {"brake_input", state.brake},
             {"boost_input", state.boostInput},
             {"boost_charge", state.boostCharge},
             {"jump_held", state.jumpHeld},
             {"jump_count", state.jumpCount},
             {"is_jumping", state.isJumping},
             {"is_flipping", state.isFlipping},
             {"powerslide", state.powerslide},
             {"handbrake", state.handbrakeVal},
             {"air_control", state.airRollActive},
             {"wheel0_grounded", state.wheels.size() > 0 ? state.wheels[0].grounded : false},
             {"wheel0_compression", state.wheels.size() > 0 ? state.wheels[0].compression : 0.0f},
             {"wheel0_spin", state.wheels.size() > 0 ? state.wheels[0].spinVelocity : 0.0f},
             {"wheel1_grounded", state.wheels.size() > 1 ? state.wheels[1].grounded : false},
             {"wheel1_compression", state.wheels.size() > 1 ? state.wheels[1].compression : 0.0f},
             {"wheel1_spin", state.wheels.size() > 1 ? state.wheels[1].spinVelocity : 0.0f},
             {"wheel2_grounded", state.wheels.size() > 2 ? state.wheels[2].grounded : false},
             {"wheel2_compression", state.wheels.size() > 2 ? state.wheels[2].compression : 0.0f},
             {"wheel2_spin", state.wheels.size() > 2 ? state.wheels[2].spinVelocity : 0.0f},
             {"wheel3_grounded", state.wheels.size() > 3 ? state.wheels[3].grounded : false},
             {"wheel3_compression", state.wheels.size() > 3 ? state.wheels[3].compression : 0.0f},
             {"wheel3_spin", state.wheels.size() > 3 ? state.wheels[3].spinVelocity : 0.0f}},
            __FILE__, __LINE__, __func__);
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
        const bool tiltedRest = state.upDotWorldUp <= 0.95f;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            tiltedRest ? "vehicle.rest.tilted" : "vehicle.rest",
            std::to_string(state.id),
            "body came to rest through contact physics", tick,
            {{"vehicle_id", state.id},
             {"profile", profile.id},
             {"up_dot_world_up", state.upDotWorldUp},
             {"tilt_deg", glm::degrees(std::acos(std::clamp(
                              state.upDotWorldUp, -1.0f, 1.0f)))},
             {"wheel_mask", state.wheelMask},
             {"wheels_contact", grounded},
             {"settled_on_wheels",
              state.upDotWorldUp > 0.95f && grounded == wheelCount},
             {"tilted", state.upDotWorldUp <= 0.95f},
             {"speed_mps", glm::length(entity.velocity)},
             {"mass_kg", entity.mass}},
            __FILE__, __LINE__, __func__);
    }
    else if (state.wasSleeping && !entity.sleeping)
    {
        state.wasSleeping = false;
    }

    // ── Settle righting ─────────────────────────────────────────────────
    // Rocket League cannot leave a car lying on an edge, corner or side: a
    // supported car that has stopped in any pose other than wheels or roof is
    // flipped out after 0.25 s. Wheels and roof are the only accepted rests.
    // This is an outcome assist, not a second physics solver: the contact model
    // still decides the landing; the assist applies the same kind of angular
    // velocity and small hop a player uses to flip off an edge.
    {
        const uint32_t fullMask =
            wheelCount >= 32 ? 0xFFFFFFFFu : ((1u << wheelCount) - 1u);
        const float restSpeed = glm::length(entity.velocity);
        const float restAngular = glm::length(entity.angularVelocity);
        const bool upright = up.z > 0.95f && state.wheelMask == fullMask;
        const bool roof = up.z < -0.90f;
        const bool supported = state.wheelMask != 0 || grounded > 0 ||
                               entity.lastContactedWorld || entity.sleeping;
        const bool still = restSpeed < 0.5f && restAngular < 0.5f;
        if (profile.settle.manualRightingEnabled && supported && still &&
            !upright && !roof)
            ++state.sideRestTicks;
        else
        {
            state.sideRestTicks = 0;
            state.sideRecoveryLogged = false;
        }
        if (profile.settle.manualRightingEnabled &&
            state.sideRestTicks >= 15u)
        {
            // Rotate the up axis toward world up about the shorter arc.
            glm::vec3 axis = glm::cross(up, glm::vec3(0.0f, 0.0f, 1.0f));
            if (glm::length(axis) < 0.2f)
                axis = right; // nearly upside down: roll off the roof
            axis = glm::normalize(axis);
            entity.angularVelocity += axis * 7.5f;
            if (supported)
                entity.velocity.z = std::max(entity.velocity.z, 1.2f);
            entity.sleeping = false;
            entity.sleepTicks = 0;
            state.sideRestTicks = 0;
            state.sideRecoveryLogged = true;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.side.recovery", std::to_string(state.id),
                "node/edge/side rest flipped toward wheels or roof", tick,
                {{"vehicle_id", state.id},
                 {"profile", profile.id},
                 {"up_dot_world_up", up.z},
                 {"wheel_mask", state.wheelMask},
                 {"grounded", grounded},
                 {"last_contacted_world", entity.lastContactedWorld},
                 {"sleeping", entity.sleeping},
                 {"speed_mps", restSpeed},
                 {"angular_speed", restAngular},
                 {"rest_ticks", 15},
                 {"angular_impulse", 7.5},
                 {"hop_mps", 1.2},
                 {"axis", {axis.x, axis.y, axis.z}}},
                __FILE__, __LINE__, __func__);
        }
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
             {"position", {position.x, position.y, position.z}},
             {"altitude_agl_m", position.z},
             {"airborne", !supported},
             {"wheels_contact", grounded},
             {"throttle_input", state.throttle},
             {"steer_input", state.steer},
             {"pitch_input", state.pitchInput},
             {"yaw_input", state.yawInput},
             {"roll_input", state.rollInput},
             {"angular_speed", glm::length(entity.angularVelocity)},
             {"yaw_deg", glm::degrees(std::atan2(forward.y, forward.x))},
             {"velocity", {entity.velocity.x, entity.velocity.y,
                           entity.velocity.z}},
             {"angular_velocity", {entity.angularVelocity.x,
                                   entity.angularVelocity.y,
                                   entity.angularVelocity.z}},
             {"distance_travelled_m", state.distanceTravelled},
             {"apex_height_m", state.apexHeightM},
             {"flip_resets", state.flipResetCount},
             {"jump_timer", state.jumpTimer},
             {"is_jumping", state.isJumping},
             {"jump_held", state.jumpHeld},
             {"jump_count", state.jumpCount},
             {"ballcam", state.ballCam}},
            __FILE__, __LINE__, __func__);

        if (supported)
            ++state.groundedRunTicks;
        else
            state.groundedRunTicks = 0;
        if (!supported && !state.airborneLogged)
        {
            state.airborneLogged = true;
            state.airborneSinceTick = tick;
        }
        else if (supported && state.airborneLogged &&
                 state.groundedRunTicks >= 4u)
        {
            // Four consecutive supported ticks. A car resting on its side
            // flickers between grounded and airborne every few ticks and used
            // to log 126 "landings" in one run (20261126_233803).
            state.airborneLogged = false;
            // Landing sample: the pose at touchdown, the impact speed and the
            // deepest wheel penetration on this tick. Proves whether a landing
            // arrived on the wheels and whether any wheel went under the
            // surface.
            float landingPenetration = 0.0f;
            glm::vec3 landingNormalSum(0.0f);
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.grounded)
                    landingNormalSum += wheel.contactNormal;
            const float landingNormalLen = glm::length(landingNormalSum);
            const float landingNormalZ =
                landingNormalLen > 1e-5f
                    ? (landingNormalSum / landingNormalLen).z
                    : 0.0f;
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.rayHit)
                    landingPenetration = std::max(
                        landingPenetration,
                        std::max(0.0f, -wheel.suspensionLength));
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.landing", std::to_string(state.id),
                "vehicle touched down after being airborne", tick,
                {{"vehicle_id", state.id},
                 {"air_time_seconds",
                  static_cast<float>(tick - state.airborneSinceTick) / 60.0f},
                 {"impact_speed_mps", glm::length(entity.velocity)},
                 {"impact_vz", entity.velocity.z},
                 {"up_dot_world_up", up.z},
                 {"tilt_deg", glm::degrees(std::acos(std::clamp(
                                  up.z, -1.0f, 1.0f)))},
                 {"wheel_mask", state.wheelMask},
                 {"wheels_contact", grounded},
                 {"settled_on_wheels",
                  up.z > 0.95f && grounded == wheelCount},
                 {"max_penetration_m", landingPenetration},
                 {"surface_normal_z", landingNormalZ},
                 {"contact_parts",
                  state.wheelMask == 0 ? "chassis" : "wheels"},
                 {"angular_speed", glm::length(entity.angularVelocity)},
                 {"pitch_rate_local", glm::dot(entity.angularVelocity, right)},
                 {"roll_rate_local",
                  glm::dot(entity.angularVelocity, forward)}},
                __FILE__, __LINE__, __func__);
            if (state.localDriver && !mServerProcess)
            {
                const float impact = glm::length(entity.velocity);
                vehicleFeedbackSound(
                    "vehicle/land", glm::vec3(entity.transform[3]),
                    std::clamp(0.25f + impact / 30.0f, 0.25f, 1.0f));
            }
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

namespace {
// Client-side vehicle feedback. Placeholder assets live in
// assets/sound/vehicle/*.wav and are meant to be replaced later.
void vehicleFeedbackSound(const char* name, const glm::vec3& position,
                          float volume)
{
    playWorldSound(name, position, volume, 1.0f, 45.0f);
}

EffectPart* vehicleFeedbackBurst(const glm::vec3& position,
                                 const glm::vec3& color, float scale,
                                 float lifetime, bool gravity,
                                 const char* label)
{
    EffectPart* fx = EffectPartSystem::instance().spawnCustom(
        position, color, lifetime, label);
    if (!fx)
        return nullptr;
    fx->scale = scale;
    fx->endScale = scale * 1.6f;
    fx->halfSize = glm::vec3(std::max(scale * 0.5f, 0.05f));
    fx->alpha = 0.85f;
    fx->endAlpha = 0.0f;
    fx->useEndAlpha = true;
    fx->affectedByGravity = gravity;
    fx->gravity = gravity ? 6.0f : 0.0f;
    return fx;
}
} // namespace

namespace {
// Last-resort containment for arena cars. Wall geometry must stop the car, but
// a high-speed seam or solver failure must never leave it outside the world.
void containArenaVehicle(VehicleState& state, PhysicalEntity& entity,
                         uint64_t tick)
{
    const MapObjectiveConfig& map = MapConfigRegistry::instance().current();
    if (!map.arena.present)
        return;
    float goalHalf = 8.92755f;
    float goalHeight = 6.42775f;
    for (const ArenaGoalConfig& goal : map.arena.goals)
    {
        goalHalf = std::max(goalHalf, goal.halfExtents.y);
        goalHeight = std::max(goalHeight, goal.halfExtents.z * 2.0f);
    }
    const glm::vec3 p(entity.transform[3]);
    // Only a real escape beyond the solid outer faces, not penetration inside
    // the 2 m walls / 3 m floor. The solver owns internal penetration; this
    // guard owns "the car is outside the world" and nothing else.
    const bool inField = std::fabs(p.x) <= 53.0f &&
                         std::fabs(p.y) <= 43.0f && p.z >= -3.0f &&
                         p.z <= 20.6f;
    const bool inGoalMouth = std::fabs(p.x) <= 60.0f &&
                             std::fabs(p.y) <= goalHalf &&
                             p.z <= goalHeight + 0.2f;
    if (inField || inGoalMouth)
        return;

    glm::vec3 clamped = p;
    if (std::fabs(p.x) > 53.0f)
        clamped.x = std::copysign(51.2f, p.x);
    if (std::fabs(p.y) > 43.0f)
        clamped.y = std::copysign(40.96f, p.y);
    if (p.z < -3.0f)
        clamped.z = 0.35f;
    if (p.z > 20.6f)
        clamped.z = 20.2f;
    glm::vec3 velocity = entity.velocity;
    if (p.x > 53.0f) velocity.x = std::min(velocity.x, 0.0f);
    if (p.x < -53.0f) velocity.x = std::max(velocity.x, 0.0f);
    if (p.y > 43.0f) velocity.y = std::min(velocity.y, 0.0f);
    if (p.y < -43.0f) velocity.y = std::max(velocity.y, 0.0f);
    if (p.z < -3.0f) velocity.z = std::max(velocity.z, 0.0f);
    if (p.z > 20.6f) velocity.z = std::min(velocity.z, 0.0f);

    entity.transform[3] = glm::vec4(clamped, 1.0f);
    entity.previousTransform[3] = entity.transform[3];
    entity.velocity = velocity;
    state.origin = clamped;
    state.originVelocity = velocity;
    state.originAngularVelocity = entity.angularVelocity;

    static uint64_t sLastContainLogTick = 0;
    if (tick > sLastContainLogTick + 120u)
    {
        sLastContainLogTick = tick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.out_of_world", std::to_string(state.id),
            "arena containment projected the car back inside", tick,
            {{"vehicle_id", state.id},
             {"was", {p.x, p.y, p.z}},
             {"now", {clamped.x, clamped.y, clamped.z}},
             {"velocity", {velocity.x, velocity.y, velocity.z}},
             {"map", map.mapId}},
            __FILE__, __LINE__, __func__);
    }
}
} // namespace

void VehicleSystem::update(float dt, const World& world)
{
    MIMITA_PERF_SCOPE("Vehicle::Update");
    if (dt <= 0.0f)
        return;

    const auto started = std::chrono::steady_clock::now();
    int awakeVehicles = 0;

    // Randomized settling probe: MIMITA_VEHICLE_FUZZ seeds repeated kicks so a
    // run can prove "lands on wheels most of the time" across many drops.
    static const char* sFuzzEnv = std::getenv("MIMITA_VEHICLE_FUZZ");
    static std::mt19937 sFuzzRng(
        sFuzzEnv ? static_cast<uint32_t>(std::strtoul(sFuzzEnv, nullptr, 10))
                 : 0u);
    static uint64_t sLastFuzzTick = 0;
    // Random air-control/boost window after each fuzz kick. The probe must
    // exercise every pose (air roll, boost, wall/ceiling bounce), not only a
    // passive drop.
    static VehicleInput sFuzzAirInput;
    static uint32_t sFuzzAirTicks = 0;
    // The kick window also exercises jump -> dodge and boost on/off. Throttle
    // is applied late in the window so auto-roll has input once the car lands.
    static bool sFuzzBoostOn = false;
    static bool sFuzzThrottleOn = false;
    // The probe models a player holding throttle through the recovery, which
    // is what Rocket League's throttle-conditional auto-roll needs. The
    // budget starts at each new wheel contact and lasts 2 s so the car can
    // still come to rest and be classified instead of driving forever.
    static uint32_t sFuzzRecoveryTicks = 0;
    static bool sFuzzWasWheelContact = false;
    static bool sFuzzDriveEnabled = false;
    static uint64_t sFuzzJumpAtTick = 0;
    static uint64_t sFuzzDodgeAtTick = 0;

    for (VehicleState& state : mVehicles)
    {
        PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(state.templateId);
        if (!entity || !spec)
            continue;

        containArenaVehicle(state, *entity,
                            PhysicalEntitySystem::instance().simulationTick());

        // A car driven hard into a wall can hold a few centimetres of floor
        // overlap while the contact solver carries the squeeze. Lift the
        // chassis out so no part of the car ever ends a tick below the surface.
        // Only floor contacts are corrected here; wall overlaps stay with the
        // generic solver.
        if (entity->lastContactPenetration > 0.005f)
        {
            // Any surface, not just the floor: a car pressed into a wall or a
            // ceiling must be pushed out the same way. The normal comes from the
            // solver, so the correction is along the real contact direction.
            const glm::vec3 normal = entity->lastContactNormal;
            const float lift = entity->lastContactPenetration - 0.005f;
            const glm::vec3 push = normal * lift;
            entity->transform[3] += glm::vec4(push, 0.0f);
            entity->previousTransform[3] += glm::vec4(push, 0.0f);
            entity->lastContactPenetration = 0.005f;
        }

        const uint64_t simulationTick =
            PhysicalEntitySystem::instance().simulationTick();

        if (sFuzzEnv && state.id == 1 && sFuzzDriveEnabled)
        {
            VehicleInput fuzzInput{};
            if (sFuzzAirTicks > 0)
            {
                fuzzInput = sFuzzAirInput;
                fuzzInput.boost = sFuzzBoostOn && sFuzzAirTicks > 90 ? 0.6f : 0.0f;
                if (simulationTick == sFuzzJumpAtTick ||
                    simulationTick == sFuzzDodgeAtTick)
                    fuzzInput.jumpHeld = true;
                --sFuzzAirTicks;
            }
            // Every fresh wheel contact gets a 2 s throttle recovery window.
            // This is the input Rocket League's auto-roll requires; releasing
            // throttle at the end of the kick alone allowed legal no-input side
            // rests (run 20261127_180452, up_dot 0.40 on one wheel).
            const bool wheelContact = state.wheelMask != 0;
            if (wheelContact && !sFuzzWasWheelContact)
                sFuzzRecoveryTicks = 120;
            sFuzzWasWheelContact = wheelContact;
            // Stop driving once the body is upright or on its roof: the probe
            // then measures the rest instead of holding throttle forever.
            const bool needsRecovery =
                state.upDotWorldUp < 0.95f && state.upDotWorldUp >= -0.90f;
            if (!needsRecovery)
                sFuzzRecoveryTicks = 0;
            if (sFuzzRecoveryTicks > 0)
                --sFuzzRecoveryTicks;
            fuzzInput.throttle =
                (sFuzzAirTicks > 0 || sFuzzRecoveryTicks > 0) ? 1.0f : 0.0f;
            VehicleSystem::instance().setInput(state.id, fuzzInput);
        }

        // A randomized kick can put the car through the goal mouth and behind
        // the back wall. Respawn it so the probe keeps sampling poses instead of
        // freezing outside the arena.
        static uint32_t sFuzzEscapes = 0;
        if (sFuzzEnv && state.id == 1)
        {
            const glm::vec3 probePosition(entity->transform[3]);
            const float absX = std::fabs(probePosition.x);
            const float absY = std::fabs(probePosition.y);
            // The playfield is |x| <= 51.2 and |y| <= 40.96. The goal recess
            // is legal inside the goal mouth (|y| <= 8.93 up to the back net at
            // |x| = 60). A car behind the back wall outside the mouth, or past
            // a side wall, has escaped and would otherwise sit in a pocket and
            // freeze the settle probe.
            // Small contact overlap with the 1 m wall is normal; only count a
            // real escape beyond the wall face. The old exact 40.96 threshold
            // respawned a car merely 0.14 m into the wall.
            const bool inField = absX <= 51.4f && absY <= 41.2f;
            const bool inGoalMouth = absX <= 60.0f && absY <= 8.93f;
            if ((!inField && !inGoalMouth) || probePosition.z < -3.0f ||
                probePosition.z > 25.0f)
            {
                const glm::vec3 safe(0.0f, 0.0f, 1.0f);
                entity->transform = glm::mat4(1.0f);
                entity->transform[3] = glm::vec4(safe, 1.0f);
                entity->previousTransform = entity->transform;
                entity->orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                entity->velocity = glm::vec3(0.0f);
                entity->angularVelocity = glm::vec3(0.0f);
                entity->sleeping = false;
                entity->sleepTicks = 0;
                state.origin = safe;
                state.originVelocity = glm::vec3(0.0f);
                state.originAngularVelocity = glm::vec3(0.0f);
                sFuzzAirTicks = 0;
                sFuzzDriveEnabled = false;
                sFuzzRecoveryTicks = 0;
                sFuzzWasWheelContact = false;
                VehicleSystem::instance().setInput(state.id, VehicleInput{});
                ++sFuzzEscapes;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "vehicle.fuzz.respawn", std::to_string(state.id),
                    "fuzz car left the arena and was respawned",
                    static_cast<uint32_t>(simulationTick),
                    {{"vehicle_id", state.id},
                     {"position", {probePosition.x, probePosition.y,
                                   probePosition.z}},
                     {"escapes", sFuzzEscapes}},
                    __FILE__, __LINE__, __func__);
            }
        }

        // One randomized kick every four seconds on the first arena car.
        if (sFuzzEnv && state.id == 1 && profileFor(state) &&
            simulationTick - sLastFuzzTick >= 240u)
        {
            // A settle must be supported. Speed alone also matches a car at the
            // apex of a throw, which reported an airborne pose as a side rest
            // and kicked again from mid-air (run 20261127_052415: 11 reports had
            // last_contacted_world=false).
            const bool supported = state.wheelMask != 0 ||
                                   entity->lastContactedWorld ||
                                   entity->sleeping;
            const bool settled = supported && state.wheels.size() > 0 &&
                                 state.speed < 0.5f &&
                                 glm::length(entity->angularVelocity) < 0.5f;
            // Counters for the settle report. The probe's question is "after a
            // random tumble, does the body come to rest on its wheels?" - so a
            // settle is classified just before the next kick, when the car has
            // actually stopped.
            static uint32_t sFuzzKicks = 0;
            static uint32_t sFuzzWheels = 0;
            static uint32_t sFuzzUprightPartial = 0;
            static uint32_t sFuzzTilted = 0;
            static uint32_t sFuzzRoof = 0;
            static uint32_t sFuzzSide = 0;
            static uint32_t sFuzzNose = 0;
            static uint32_t sFuzzTail = 0;
            static uint32_t sFuzzSettleHold = 0;
            static float sFuzzWorstUpDot = 1.0f;
            if (settled)
                ++sFuzzSettleHold;
            else
                sFuzzSettleHold = 0;
            // The acceptance is "a side rest that survives 0.4 s". Classifying
            // the first settled frame kicked the car before the recovery rule
            // could act, so hold a supported rest for ~0.5 s first.
            const bool classify = sFuzzSettleHold >= 30u;
            const bool timeoutKick =
                simulationTick - sLastFuzzTick >= 480u;
            if (classify || timeoutKick)
            {
                sFuzzSettleHold = 0;
                sLastFuzzTick = simulationTick;
                if (classify)
                {
                    ++sFuzzKicks;
                    const uint32_t fullMask =
                        (1u << std::max(0, static_cast<int>(state.wheels.size()))) - 1u;
                    // Pose is judged by the body up vector first. A car resting
                    // upright against a wall can have only two or three wheels
                    // in contact without being "on its side"; that is reported
                    // separately, never as a side rest.
                    if (state.upDotWorldUp > 0.95f)
                    {
                        if (state.wheelMask == fullMask)
                            ++sFuzzWheels;
                        else
                            ++sFuzzUprightPartial;
                    }
                    else if (state.upDotWorldUp < -0.90f)
                        ++sFuzzRoof;
                    else
                    {
                        // Not upright, not roof: a side/nose/tail rest. This is
                        // the failure the probe exists to find.
                        const glm::vec3 restForward =
                            state.orientation * glm::vec3(1.0f, 0.0f, 0.0f);
                        if (restForward.z < -0.85f)
                            ++sFuzzNose;
                        else if (restForward.z > 0.85f)
                            ++sFuzzTail;
                        ++sFuzzTilted;
                        ++sFuzzSide;
                        sFuzzWorstUpDot = std::min(sFuzzWorstUpDot,
                                                   state.upDotWorldUp);
                    }
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "vehicle.fuzz.report", std::to_string(state.id),
                    "cumulative settling probe report", 
                    static_cast<uint32_t>(simulationTick),
                    {{"vehicle_id", state.id},
                     {"kicks", sFuzzKicks},
                     {"wheels_landings", sFuzzWheels},
                     {"upright_partial_rests", sFuzzUprightPartial},
                     {"tilted_rests", sFuzzTilted},
                     {"roof_rests", sFuzzRoof},
                     {"side_rests", sFuzzSide},
                     {"nose_rests", sFuzzNose},
                     {"tail_rests", sFuzzTail},
                     {"position", {entity->transform[3].x,
                                   entity->transform[3].y,
                                   entity->transform[3].z}},
                     {"settled_up_dot", state.upDotWorldUp},
                     {"settled_wheel_mask", state.wheelMask},
                     {"last_contacted_world", entity->lastContactedWorld},
                     {"sleeping", entity->sleeping},
                     {"side_rest_ticks", state.sideRestTicks},
                     {"escapes", sFuzzEscapes},
                     {"worst_settled_up_dot", sFuzzWorstUpDot}},
                    __FILE__, __LINE__, __func__);
                }
                else
                {
                    // A car can be pinned in a non-accepted pose without ever
                    // satisfying the "supported settle" test. Kick it anyway so
                    // the probe keeps sampling instead of freezing for minutes.
                    // This also makes the fuzz run bounded after any collision
                    // that leaves the car resting on the ball instead of the
                    // world; the kick cycles it back into the probe.
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Physics, StructuredLevel::Important,
                        "vehicle.fuzz.timeout", std::to_string(state.id),
                        "fuzz settle timeout, forcing the next kick",
                        static_cast<uint32_t>(simulationTick),
                        {{"vehicle_id", state.id},
                         {"up_dot_world_up", state.upDotWorldUp},
                         {"wheel_mask", state.wheelMask},
                         {"supported", supported},
                         {"speed_mps", state.speed}},
                        __FILE__, __LINE__, __func__);
                }
                std::uniform_real_distribution<float> vy(-8.0f, 8.0f);
                std::uniform_real_distribution<float> vz(4.0f, 9.0f);
                // Edge-biased tumble: enough to land on a corner or side
                // start, not so violent that the car always ends flat on a
                // face. The report asks whether an edge tips onto the wheels.
                std::uniform_real_distribution<float> spin(-6.0f, 6.0f);
                const glm::vec3 kick(vy(sFuzzRng), vy(sFuzzRng), vz(sFuzzRng));
                const glm::vec3 spinVel(spin(sFuzzRng), spin(sFuzzRng),
                                        spin(sFuzzRng));
                // Air-roll and boost through the tumble, like a player trying
                // to recover. The pose diversity is the point of the probe.
                std::uniform_real_distribution<float> ctrl(-1.0f, 1.0f);
                sFuzzAirInput = VehicleInput{};
                sFuzzAirInput.roll = ctrl(sFuzzRng);
                sFuzzAirInput.pitch = ctrl(sFuzzRng);
                sFuzzAirInput.yaw = ctrl(sFuzzRng);
                std::uniform_int_distribution<int> chance(0, 1);
                sFuzzBoostOn = chance(sFuzzRng) != 0;
                sFuzzThrottleOn = true;
                std::uniform_int_distribution<int> jumpDelay(1, 8);
                std::uniform_int_distribution<int> dodgeDelay(10, 28);
                sFuzzJumpAtTick = simulationTick + jumpDelay(sFuzzRng);
                sFuzzDodgeAtTick = simulationTick + dodgeDelay(sFuzzRng);
                sFuzzAirTicks = 180;
                sFuzzDriveEnabled = true;
                sFuzzRecoveryTicks = 0;
                sFuzzWasWheelContact = false;
                entity->velocity = kick;
                entity->angularVelocity = spinVel;
                entity->sleeping = false;
                entity->sleepTicks = 0;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "vehicle.drop.fuzz", std::to_string(state.id),
                    "random settling probe kick",
                    static_cast<uint32_t>(simulationTick),
                    {{"vehicle_id", state.id},
                     {"velocity", {kick.x, kick.y, kick.z}},
                     {"angular_velocity",
                      {spinVel.x, spinVel.y, spinVel.z}},
                     {"boost_on", sFuzzBoostOn},
                     {"throttle_on", sFuzzThrottleOn},
                     {"jump_at_tick", sFuzzJumpAtTick},
                     {"dodge_at_tick", sFuzzDodgeAtTick},
                     {"up_dot_world_up", state.upDotWorldUp}},
                    __FILE__, __LINE__, __func__);
            }
        }

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
            if (mExitPrompt)
            {
                InteractionSystem::instance().updateTarget(
                    state.interactionTargetId, promptOrigin, origin, label,
                    spec->interactionRadius, true);
            }
        }

        // Ball cam is presentation state owned by the local player. Poll it
        // here, outside the simulation branch, because a networked client owns
        // its car as a server mirror and returns before the input path.
        pollLocalBallCam(state, static_cast<uint32_t>(simulationTick));
        pollLocalExitKey(state);

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

        if (state.inputOverride && state.inputOverrideTicks > 0 &&
            --state.inputOverrideTicks == 0)
            state.inputOverride = false;
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
            auto activeProfile = profileFor(state);
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

        if (auto profile = profileFor(state))
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

        // Vertical stability: an all-wheel-grounded chassis should not keep
        // bouncing after a landing. Nothing here damps rotation. A car that
        // landed on a corner must stay free to tip flat; the old "any grounded
        // wheel" angular damping froze it mid-tip and let it sleep on a corner.
        {
            int grounded = 0;
            for (const VehicleWheelState& wheel : state.wheels)
                if (wheel.grounded)
                    ++grounded;
            if (grounded > 0 &&
                grounded == static_cast<int>(state.wheels.size()))
            {
                entity->velocity.z *= std::max(
                    0.0f, 1.0f - spec->stabilityVerticalDamping * dt);
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
            // A body may only sleep in an accepted pose: all four wheels
            // upright, or the roof. Any other supported pose stays awake so the
            // recovery rule can act. The old up.z <= 0.90 gate allowed a car at
            // 0.91-0.95 to sleep and be counted as a side rest.
            const bool acceptedPose =
                (up.z > 0.95f && grounded == static_cast<int>(state.wheels.size())) ||
                up.z < -0.90f;
            if (!acceptedPose && entity->sleeping)
            {
                entity->sleeping = false;
                entity->sleepTicks = 0;
            }
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
            // Sustained wall driving re-touches every tick; log at most every
            // 0.5 s so 2263 lines per 8 minutes cannot bury the real events.
            static uint64_t sLastWallContactLogTick = 0;
            if (bounceTick > sLastWallContactLogTick + 60u)
            {
                sLastWallContactLogTick = bounceTick;
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
                if (state.localDriver && !mServerProcess)
                {
                    glm::vec3 contactNormal = entity->lastWallContactNormal;
                    if (glm::dot(contactNormal, contactNormal) < 1e-6f)
                        contactNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                    contactNormal = glm::normalize(contactNormal);
                    const glm::vec3 contactPos =
                        glm::vec3(entity->transform[3]) - contactNormal * 0.6f;
                    vehicleFeedbackSound(
                        "vehicle/wall_bump", contactPos,
                        std::clamp(0.25f + entity->lastWallImpactSpeed / 20.0f,
                                   0.25f, 1.0f));
                    vehicleFeedbackBurst(contactPos,
                                         glm::vec3(1.0f, 0.55f, 0.15f),
                                         0.30f, 0.18f, true,
                                         "vehicle_wall_fx");
                }
            }
        }
        if ((state.serverAuthoritative || !state.serverMirror ||
             state.localDriver) &&
            bounceTick % 12u == 0u)
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
