// 2026-10-27

#include "vehicles/vehicle-seat.h"

#include <cmath>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "camera.h"
#include "physics/physical-entity.h"
#include "terminal/terminal-state.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-system.h"

bool vehicleFindAimedSeat(const Camera& camera, uint32_t& outVehicleId,
                          int& outSeat, float maxDistance)
{
    outVehicleId = 0;
    outSeat = -1;

    const glm::vec3 origin = camera.pos;
    glm::vec3 direction = camera.front;
    if (glm::dot(direction, direction) < 1e-6f)
        return false;
    direction = glm::normalize(direction);

    float bestDistance = maxDistance;
    VehicleSystem& vehicles = VehicleSystem::instance();
    for (const VehicleState& state : vehicles.vehicles())
    {
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(state.templateId);
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        if (!spec || !entity)
            continue;

        for (size_t seatIndex = 0; seatIndex < spec->seats.size(); ++seatIndex)
        {
            const glm::vec3 center = glm::vec3(
                entity->transform *
                glm::vec4(spec->seats[seatIndex].anchor, 1.0f));
            const float toCenter =
                glm::dot(center - origin, direction);
            if (toCenter < 0.0f || toCenter > bestDistance)
                continue;
            const glm::vec3 closest = origin + direction * toCenter;
            const float perpendicular =
                glm::length(center - closest);
            if (perpendicular > 0.9f)
                continue;
            bestDistance = toCenter;
            outVehicleId = state.id;
            outSeat = static_cast<int>(seatIndex);
        }
    }
    return outVehicleId != 0 && outSeat >= 0;
}

bool vehicleHandleInteractionKey(GLFWwindow* window, const Camera& camera)
{
    if (!window)
        return false;

    static bool previousHeld = false;
    const bool held = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    const bool pressed = held && !previousHeld;
    previousHeld = held;
    if (!pressed)
        return false;

    VehicleSystem& vehicles = VehicleSystem::instance();
    if (vehicles.hasLocalDriver())
    {
        glm::vec3 seatPosition(0.0f);
        glm::vec3 seatForward(0.0f, 1.0f, 0.0f);
        if (vehicles.localSeatWorld(seatPosition, seatForward))
        {
            const glm::vec3 right =
                glm::normalize(glm::cross(seatForward,
                                          glm::vec3(0.0f, 0.0f, 1.0f)));
            THE_PLAYER.pos = seatPosition + right * 1.4f;
            THE_PLAYER.vel = glm::vec3(0.0f);
            THE_PLAYER.externalImpulse = glm::vec3(0.0f);
        }
        vehicles.exitLocalDriver();
        return true;
    }

    uint32_t vehicleId = 0;
    int seat = -1;
    if (!vehicleFindAimedSeat(camera, vehicleId, seat))
        return false;
    return vehicles.enterLocalDriver(vehicleId, seat);
}
