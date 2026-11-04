// 2026-10-27

#include "vehicles/vehicle-hud.h"

#include <cstdio>

#include <glm/glm.hpp>

#include "debug/structured-log.h"
#include "gui/ui-system.h"
#include "physics/physical-entity.h"
#include "vehicles/vehicle-config.h"
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
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(state->templateId);
    const char* diff = spec ? spec->differential.type.c_str() : "?";
    const int speedKmh = (int)(state->speed * 3.6f + 0.5f);

    char line[192];
    std::snprintf(line, sizeof(line), "%s  %s  GEAR %s  DIFF %s",
                  state->engineOn ? "ENGINE ON" : "ENGINE OFF", mode, gear,
                  diff);
    const float scale = 0.34f;
    const float x = 24.0f;
    float y = uiScreenH() - 300.0f;
    uiDrawRect({x - 8.0f, y - 8.0f, 470.0f, 178.0f},
               {0.03f, 0.04f, 0.06f, 0.72f}, "vehicle-hud-bg");
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
    y += 20.0f;

    std::snprintf(line, sizeof(line),
                  "THROTTLE %d%%  BRAKE %d%%  STEER %+.2f  SEATED",
                  (int)(state->throttle * 100.0f + 0.5f),
                  (int)(state->brake * 100.0f + 0.5f), state->steer);
    uiDrawText(line, x, y, 0.28f, {0.8f, 0.85f, 0.9f, 1.0f});
    y += 24.0f;

    const glm::vec4 hintColor{1.0f, 0.86f, 0.35f, 1.0f};
    uiDrawText(state->engineOn ? "V  ENGINE OFF" : "V  START ENGINE",
               x, y, 0.30f, hintColor);
    y += 20.0f;
    uiDrawText("B  AUTO/MANUAL   X  SHIFT UP   Z  SHIFT DOWN", x, y,
               0.25f, {0.85f, 0.9f, 1.0f, 1.0f});
    y += 18.0f;
    uiDrawText("W/S THROTTLE/BRAKE  A/D STEER  SHIFT CLUTCH  SPACE HANDBRAKE"
               "  F EXIT",
               x, y, 0.23f, {0.8f, 0.85f, 0.9f, 1.0f});

    const uint64_t hudTick =
        PhysicalEntitySystem::instance().simulationTick();
    static uint64_t lastHudLogTick = 0;
    if (hudTick - lastHudLogTick >= 60)
    {
        lastHudLogTick = hudTick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Gui, StructuredLevel::Important,
            "vehicle.hud", std::to_string(state->id),
            "vehicle HUD rendered live state", hudTick,
            {{"vehicle_id", state->id},
             {"gear", state->gear},
             {"gear_label", gear},
             {"mode", mode},
             {"engine_on", state->engineOn},
             {"speed_kmh", speedKmh},
             {"differential", diff},
             {"screen_w", uiScreenW()},
             {"screen_h", uiScreenH()}});
    }
}
