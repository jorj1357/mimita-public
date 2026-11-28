// 2026-10-27

#include "vehicles/vehicle-render.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
#include "game/resident-state-module.h"
#include "effects/effect-part.h"
#include "gamemode/map-config.h"
#include "network/multiplayer-context.h"
#include "physics/physical-entity.h"
#include "render/procedural-cylinder.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-system.h"
#include "world/texture-store.h"

namespace {

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float lengthSq = glm::dot(value, value);
    return lengthSq > 1e-8f ? value / std::sqrt(lengthSq) : fallback;
}

} // namespace

void drawVehicleWheels(const Camera& camera)
{
    VehicleSystem& vehicles = VehicleSystem::instance();
    for (const VehicleState& state : vehicles.vehicles())
    {
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(state.templateId);
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        if (!spec || !entity)
            continue;

        // Same presentation pose as the car body: the wheels were drawn from the
        // raw 120 Hz transform while the body interpolated, so they visibly
        // led/jittered by one tick.
        const glm::mat4 renderTransform =
            physicalEntityRenderTransform(*entity);
        const glm::quat chassisRotation =
            glm::normalize(glm::quat_cast(glm::mat3(renderTransform)));
        const unsigned int texture = gTextures.getPath(
            spec->wheelTexture.empty() ? "assets/textureshq/dark1.png"
                                       : spec->wheelTexture);

        for (size_t i = 0; i < spec->wheels.size() &&
                           i < state.wheels.size();
             ++i)
        {
            const VehicleWheelSpec& wheelSpec = spec->wheels[i];
            const VehicleWheelState& wheel = state.wheels[i];
            const TexturedCylinderMesh& mesh = getTexturedCylinderMesh(
                wheelSpec.radius, wheelSpec.width,
                std::max(spec->wheelSegments, 8));

            const glm::vec3 hub = glm::vec3(
                renderTransform * glm::vec4(wheelSpec.hub, 1.0f));
            const glm::vec3 suspensionAxis = safeNormalize(
                chassisRotation * wheelSpec.suspensionAxis,
                glm::vec3(0.0f, 0.0f, -1.0f));
            float suspensionLength = wheelSpec.restLength;
            if (wheel.rayHit)
            {
                // The wheel sits on the surface: its centre moves up relative
                // to the hub as the suspension compresses, so the wheel can
                // never be drawn inside the ground. Clamping at zero (the old
                // behaviour) pushed the wheel through the floor whenever the
                // suspension compressed past the wheel radius.
                suspensionLength = std::clamp(wheel.suspensionLength,
                                              -wheelSpec.travel,
                                              wheelSpec.restLength);
            }
            const glm::vec3 center =
                hub + suspensionAxis * suspensionLength;

            const glm::vec3 spinAxis = safeNormalize(
                wheelSpec.spinAxis, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 steerAxis = safeNormalize(
                wheelSpec.steerAxis, glm::vec3(0.0f, 0.0f, 1.0f));

            const glm::quat align =
                glm::rotation(glm::vec3(0.0f, 0.0f, 1.0f), spinAxis);
            const glm::quat steer =
                glm::angleAxis(wheel.steerAngle, steerAxis);
            const glm::quat camber = glm::angleAxis(
                glm::radians(wheelSpec.camberDeg), glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::quat caster = glm::angleAxis(
                glm::radians(wheelSpec.casterDeg), glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::quat spin =
                glm::angleAxis(wheel.spinAngle, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rotation =
                chassisRotation * steer * caster * camber * align * spin;

            const glm::mat4 model =
                glm::translate(glm::mat4(1.0f), center) *
                glm::mat4_cast(rotation);
            drawTexturedCylinder(camera, model, mesh, texture,
                                 glm::vec3(0.55f, 0.55f, 0.58f), 1.0f);
        }
    }
}

namespace {
bool sVehicleAxesEnabled = true;
double boostPadNowMs()
{
    return (double)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
               .count();
}
} // namespace

void setVehicleAxesEnabled(bool enabled) { sVehicleAxesEnabled = enabled; }
bool vehicleAxesEnabled() { return sVehicleAxesEnabled; }

void drawArenaBoostPads()
{
    const MapObjectiveConfig& map = MapConfigRegistry::instance().current();
    if (map.boostPads.empty())
        return;
    static double lastSpawnMs = 0.0;
    const double now = boostPadNowMs();
    if (now - lastSpawnMs < 180.0)
        return;
    lastSpawnMs = now;
    const MimitaNet::MultiplayerContext* ctx =
        ::mimita::game::mpContextPtr();
    for (size_t i = 0; i < map.boostPads.size() && i < 64; ++i)
    {
        const BoostPadConfig& pad = map.boostPads[i];
        if (!pad.enabled)
            continue;
        const bool depleted = ctx && ctx->active &&
                              ctx->boostPadRemainingSeconds[i] > 0.01f;
        const float radius = pad.kind == "big" ? 2.0f : 1.0f;
        const glm::vec3 color = depleted
            ? glm::vec3(0.42f, 0.42f, 0.44f)
            : (pad.kind == "big" ? glm::vec3(1.0f, 0.55f, 0.10f)
                                 : glm::vec3(1.0f, 0.88f, 0.20f));
        if (EffectPart* fx = EffectPartSystem::instance().spawnCustom(
                pad.position + glm::vec3(0.0f, 0.0f, radius * 0.35f),
                color, 0.42f, "arena_boost_pad"))
        {
            fx->scale = radius * 1.5f;
            fx->endScale = radius * 1.5f;
            fx->halfSize = glm::vec3(radius);
            fx->alpha = depleted ? 0.35f : 0.55f;
            fx->endAlpha = 0.0f;
            fx->useEndAlpha = true;
        }
    }
}
