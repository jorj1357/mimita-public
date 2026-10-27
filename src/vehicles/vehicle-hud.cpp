// 2026-10-27

#include "vehicles/vehicle-hud.h"

#include <cstdio>

#include <glm/glm.hpp>

#include "gui/ui-system.h"
#include "vehicles/vehicle-system.h"

namespace {

const char* gearLabel(int gear, char* buffer, size_t size)
{
    if (gear < 0)
        return "R";
    if (gear == 0)
        return "N";
    std::snprintf(buffer, size, "%d", gear);
    return buffer;
}

} // namespace

void drawVehicleHud()
{
    VehicleSystem& vehicles = VehicleSystem::instance();
    if (!vehicles.hasLocalDriver())
        return;
    const VehicleState* state = vehicles.find(vehicles.localVehicleId());
    if (!state)
        return;

    char gearBuffer[8] = {0};
    const char* gear = gearLabel(state->gear, gearBuffer, sizeof(gearBuffer));
    const char* mode =
        state->mode == VehicleControlMode::Automatic ? "AUTO" : "MANUAL";
    const int speedKmh = (int)(state->speed * 3.6f + 0.5f);

    char line[160];
    std::snprintf(line, sizeof(line), "%s  %s  GEAR %s",
                  state->engineOn ? "ENGINE ON" : "ENGINE OFF", mode, gear);
    const float scale = 0.34f;
    const float x = 24.0f;
    float y = uiScreenH() - 150.0f;
    uiDrawText(line, x, y, scale, {1.0f, 1.0f, 1.0f, 1.0f});
    y += 22.0f;

    std::snprintf(line, sizeof(line), "%d km/h   %d rpm",
                  speedKmh, (int)(state->engineRpm + 0.5f));
    uiDrawText(line, x, y, scale, {1.0f, 1.0f, 1.0f, 1.0f});
    y += 22.0f;

    std::snprintf(line, sizeof(line), "CLUTCH %s  BRAKE %s  HANDBRAKE %s",
                  state->clutch > 0.5f ? "DOWN" : "UP",
                  state->brake > 0.05f ? "ON" : "OFF",
                  state->handbrake > 0.05f ? "ON" : "OFF");
    const glm::vec4 color =
        state->handbrake > 0.05f ? glm::vec4(1.0f, 0.55f, 0.2f, 1.0f)
                                 : glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
    uiDrawText(line, x, y, 0.30f, color);
}
