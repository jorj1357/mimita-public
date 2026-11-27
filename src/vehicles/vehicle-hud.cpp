// 2026-10-27

#include "vehicles/vehicle-hud.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

#include "debug/structured-log.h"
#include "gui/ui-system.h"
#include "physics/physical-entity.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-profile.h"
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

bool isArenaProfile(const std::string& profileId)
{
    return profileId.rfind("rl_soccar", 0) == 0;
}

void logVehicleHud(const VehicleState& state, const char* mode,
                   const char* gear, int speedKmh, int boostPercent,
                   bool ballCam)
{
    const uint64_t hudTick =
        PhysicalEntitySystem::instance().simulationTick();
    static uint64_t lastHudLogTick = 0;
    if (hudTick - lastHudLogTick < 60)
        return;
    lastHudLogTick = hudTick;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Gui, StructuredLevel::Important,
        "vehicle.hud", std::to_string(state.id),
        "vehicle HUD rendered live state", hudTick,
        {{"vehicle_id", state.id},
         {"hud_mode", mode},
         {"gear", state.gear},
         {"gear_label", gear},
         {"mode", state.mode == VehicleControlMode::Automatic ? "AUTO"
                                                             : "MANUAL"},
         {"engine_on", state.engineOn},
         {"speed_kmh", speedKmh},
         {"boost_percent", boostPercent},
         {"ball_cam", ballCam},
         {"hud_speed", VehicleSystem::instance().hudSpeedEnabled()},
         {"hud_car_assist", VehicleSystem::instance().hudCarAssistEnabled()},
         {"screen_w", uiScreenW()},
         {"screen_h", uiScreenH()}});
}

// Rocket League presentation: boost is the only always-on gauge, on the
// bottom-right, with a ball-cam label. Speed/rpm and engine/gear are debug
// readouts and stay hidden unless their console preference is on.
void drawArenaVehicleHud(const VehicleState& state, const VehicleSpec* spec,
                         const VehicleProfile* profile, int speedKmh)
{
    const float maxBoost = profile ? std::max(profile->boost.max, 0.01f) : 100.0f;
    const int boostPercent = (int)(100.0f * state.boostCharge / maxBoost + 0.5f);
    const float fraction = std::clamp(state.boostCharge / maxBoost, 0.0f, 1.0f);

    const float screenW = uiScreenW();
    const float screenH = uiScreenH();
    const float barW = 220.0f;
    const float barH = 16.0f;
    const float barX = screenW - barW - 26.0f;
    const float barY = screenH - 52.0f;

    uiDrawRect({barX - 8.0f, barY - 28.0f, barW + 16.0f, 54.0f},
               {0.03f, 0.04f, 0.06f, 0.62f}, "arena-hud-boost-bg");
    uiDrawText("BOOST", barX, barY - 24.0f, 0.26f,
               {0.75f, 0.82f, 0.9f, 0.95f});
    char value[16];
    std::snprintf(value, sizeof(value), "%d", boostPercent);
    const float valueW = uiMeasureText(value, 0.30f);
    uiDrawText(value, barX + barW - valueW, barY - 25.0f, 0.30f,
               {1.0f, 1.0f, 1.0f, 1.0f});

    uiDrawRect({barX, barY, barW, barH}, {0.08f, 0.09f, 0.12f, 0.9f},
               "arena-hud-boost-track");
    const glm::vec4 fillColor = fraction >= 0.999f
        ? glm::vec4(0.35f, 1.0f, 0.55f, 1.0f)
        : glm::vec4(0.25f, 0.78f, 1.0f, 1.0f);
    if (fraction > 0.0f)
        uiDrawRect({barX, barY, barW * fraction, barH}, fillColor,
                   "arena-hud-boost-fill");
    uiDrawRectOutline({barX, barY, barW, barH},
                      {0.55f, 0.62f, 0.72f, 0.9f},
                      "arena-hud-boost-border");
    for (int segment = 1; segment < 10; ++segment)
    {
        const float x = barX + barW * (float)segment / 10.0f;
        uiDrawRect({x, barY, 1.0f, barH}, {0.03f, 0.04f, 0.06f, 0.55f},
                   "arena-hud-boost-segment");
    }

    char camText[24];
    std::snprintf(camText, sizeof(camText), "BALL CAM %s",
                  state.ballCam ? "ON" : "OFF");
    const float camW = uiMeasureText(camText, 0.24f);
    uiDrawText(camText, barX + barW - camW, barY - 48.0f, 0.24f,
               state.ballCam ? glm::vec4(0.55f, 0.9f, 1.0f, 0.95f)
                             : glm::vec4(0.62f, 0.66f, 0.72f, 0.85f));

    VehicleSystem& vehicles = VehicleSystem::instance();
    float debugY = screenH - 44.0f;
    if (vehicles.hudSpeedEnabled())
    {
        char speedLine[96];
        std::snprintf(speedLine, sizeof(speedLine), "%d km/h   %d rpm",
                      speedKmh, (int)(state.engineRpm + 0.5f));
        uiDrawText(speedLine, 24.0f, debugY, 0.30f,
                   {1.0f, 1.0f, 1.0f, 0.95f});
        debugY -= 22.0f;
    }
    if (vehicles.hudCarAssistEnabled())
    {
        char gearBuffer[8] = {0};
        const char* gear = gearLabel(state.gear, gearBuffer, sizeof(gearBuffer));
        const char* mode =
            state.mode == VehicleControlMode::Automatic ? "AUTO" : "MANUAL";
        const char* diff = spec ? spec->differential.type.c_str() : "?";
        char assistLine[160];
        std::snprintf(assistLine, sizeof(assistLine),
                      "%s  %s  GEAR %s  DIFF %s",
                      state.engineOn ? "ENGINE ON" : "ENGINE OFF", mode, gear,
                      diff);
        uiDrawText(assistLine, 24.0f, debugY, 0.28f,
                   {0.9f, 0.94f, 1.0f, 0.9f});
    }

    char gearBuffer[8] = {0};
    logVehicleHud(state, "arena", gearLabel(state.gear, gearBuffer,
                                            sizeof(gearBuffer)),
                  speedKmh, boostPercent, state.ballCam);
}

} // namespace

bool vehicleArenaHudActive()
{
    VehicleSystem& vehicles = VehicleSystem::instance();
    if (!vehicles.hasLocalDriver())
        return false;
    if (isArenaProfile(vehicles.activeProfileId()))
        return true;
    // The client's global active profile can be empty in free play because no
    // match packet selects it. The per-car resolved profile is the reliable
    // owner of the presentation choice.
    const VehicleState* state = vehicles.find(vehicles.localVehicleId());
    auto profile = state ? vehicles.profileFor(*state) : nullptr;
    return profile && isArenaProfile(profile->id);
}

void drawVehicleHud()
{
    VehicleSystem& vehicles = VehicleSystem::instance();
    if (!vehicles.hasLocalDriver())
        return;
    const VehicleState* state = vehicles.find(vehicles.localVehicleId());
    if (!state)
        return;

    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(state->templateId);
    auto profile = vehicles.profileFor(*state);
    const int speedKmh = (int)(state->speed * 3.6f + 0.5f);

    if (vehicleArenaHudActive())
    {
        drawArenaVehicleHud(*state, spec, profile ? &*profile : nullptr,
                            speedKmh);
        return;
    }

    char gearBuffer[8] = {0};
    const char* gear = gearLabel(state->gear, gearBuffer, sizeof(gearBuffer));
    const char* mode =
        state->mode == VehicleControlMode::Automatic ? "AUTO" : "MANUAL";
    const char* diff = spec ? spec->differential.type.c_str() : "?";

    char line[192];
    const float scale = 0.34f;
    const float x = 24.0f;
    float y = uiScreenH() - 300.0f;
    uiDrawRect({x - 8.0f, y - 8.0f, 470.0f, 178.0f},
               {0.03f, 0.04f, 0.06f, 0.72f}, "vehicle-hud-bg");

    // Engine/gear assist readout is a console-toggled presentation preference,
    // disabled by default (see docs/specs/gamemodes/vehicle-arena.md §15).
    if (vehicles.hudCarAssistEnabled())
    {
        std::snprintf(line, sizeof(line), "%s  %s  GEAR %s  DIFF %s",
                      state->engineOn ? "ENGINE ON" : "ENGINE OFF", mode, gear,
                      diff);
        uiDrawText(line, x, y, scale, {1.0f, 1.0f, 1.0f, 1.0f});
        y += 22.0f;
    }

    if (vehicles.hudSpeedEnabled())
    {
        std::snprintf(line, sizeof(line), "%d km/h   %d rpm",
                      speedKmh, (int)(state->engineRpm + 0.5f));
        uiDrawText(line, x, y, scale, {1.0f, 1.0f, 1.0f, 1.0f});
        y += 22.0f;
    }

    if (profile)
    {
        std::snprintf(line, sizeof(line), "BOOST %d%%   BALL CAM %s",
                      (int)(100.0f * state->boostCharge /
                            std::max(profile->boost.max, 0.01f) + 0.5f),
                      state->ballCam ? "ON" : "OFF");
        uiDrawText(line, x, y, scale,
                   {0.55f, 0.85f, 1.0f, 1.0f});
        y += 22.0f;
    }

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

    logVehicleHud(*state, "car", gear, speedKmh,
                  profile ? (int)(100.0f * state->boostCharge /
                                  std::max(profile->boost.max, 0.01f) + 0.5f)
                          : 0,
                  state->ballCam);
}
