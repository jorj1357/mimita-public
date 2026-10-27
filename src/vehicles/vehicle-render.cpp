// 2026-10-27

#include "vehicles/vehicle-render.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
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

        const glm::quat chassisRotation = glm::normalize(entity->orientation);
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
                entity->transform * glm::vec4(wheelSpec.hub, 1.0f));
            const glm::vec3 suspensionAxis = safeNormalize(
                chassisRotation * wheelSpec.suspensionAxis,
                glm::vec3(0.0f, 0.0f, -1.0f));
            float suspensionLength = wheelSpec.restLength;
            if (wheel.rayHit)
                suspensionLength = std::clamp(
                    wheel.suspensionLength, 0.0f, wheelSpec.restLength);
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
            const glm::quat spin =
                glm::angleAxis(wheel.spinAngle, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::quat rotation =
                chassisRotation * steer * align * spin;

            const glm::mat4 model =
                glm::translate(glm::mat4(1.0f), center) *
                glm::mat4_cast(rotation);
            drawTexturedCylinder(camera, model, mesh, texture,
                                 glm::vec3(0.55f, 0.55f, 0.58f), 1.0f);
        }
    }
}
