// 2026-10-27

#include "vehicles/vehicle-commands.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "physics/movement/physics-collision.h"
#include "physics/physical-entity.h"
#include "terminal/terminal-state.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-system.h"

namespace {

bool parseFloat(const std::string& text, float& out)
{
    try
    {
        size_t used = 0;
        out = std::stof(text, &used);
        return used == text.size() && std::isfinite(out);
    }
    catch (...)
    {
        return false;
    }
}

bool parseUint(const std::string& text, uint32_t& out)
{
    try
    {
        size_t used = 0;
        unsigned long value = std::stoul(text, &used);
        if (used != text.size())
            return false;
        out = static_cast<uint32_t>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

const char* modeName(VehicleControlMode mode)
{
    return mode == VehicleControlMode::Automatic ? "AUTO" : "MANUAL";
}

const char* gearName(int gear, char* buffer, size_t size)
{
    if (gear < 0)
        return "R";
    if (gear == 0)
        return "N";
    std::snprintf(buffer, size, "%d", gear);
    return buffer;
}

uint32_t nearestVehicleToPlayer()
{
    VehicleSystem& vehicles = VehicleSystem::instance();
    Player& player = THE_PLAYER;
    uint32_t best = 0;
    float bestDistance = 1.0e30f;
    for (const VehicleState& state : vehicles.vehicles())
    {
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(state.entityId);
        if (!entity)
            continue;
        const float distance =
            glm::distance(player.pos, glm::vec3(entity->transform[3]));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = state.id;
        }
    }
    return best;
}

void logVehicleState(const VehicleState& state)
{
    const PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state.entityId);
    const glm::vec3 position =
        entity ? glm::vec3(entity->transform[3]) : glm::vec3(0.0f);
    char gearBuffer[8] = {0};
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "[VEHICLE] id=%u template=%s entity=%u pos=(%.2f %.2f %.2f) "
                  "engine=%d rpm=%.0f torque=%.0f boost=%.2f mode=%s gear=%s "
                  "throttle=%.2f brake=%.2f steer=%.2f clutch=%.2f "
                  "handbrake=%.2f speed=%.2f driver=%d seat=%d",
                  state.id, state.templateId.c_str(), state.entityId,
                  position.x, position.y, position.z,
                  state.engineOn ? 1 : 0, state.engineRpm,
                  state.engineTorque, state.boost, modeName(state.mode),
                  gearName(state.gear, gearBuffer, sizeof(gearBuffer)),
                  state.throttle, state.brake, state.steer, state.clutch,
                  state.handbrake, state.speed, state.driverActorId,
                  state.driverSeat);
    Terminal::instance().addLog(buffer);

    nlohmann::json fields;
    fields["vehicleId"] = state.id;
    fields["template"] = state.templateId;
    fields["entityId"] = state.entityId;
    fields["x"] = position.x;
    fields["y"] = position.y;
    fields["z"] = position.z;
    fields["engineOn"] = state.engineOn;
    fields["rpm"] = state.engineRpm;
    fields["torque"] = state.engineTorque;
    fields["boost"] = state.boost;
    fields["mode"] = modeName(state.mode);
    fields["gear"] = state.gear;
    fields["throttle"] = state.throttle;
    fields["brake"] = state.brake;
    fields["steer"] = state.steer;
    fields["clutch"] = state.clutch;
    fields["handbrake"] = state.handbrake;
    fields["speed"] = state.speed;
    fields["driverActorId"] = state.driverActorId;
    fields["driverSeat"] = state.driverSeat;
    fields["localDriver"] = state.localDriver;
    nlohmann::json wheelArray = nlohmann::json::array();
    for (size_t i = 0; i < state.wheels.size(); ++i)
    {
        const VehicleWheelState& wheel = state.wheels[i];
        wheelArray.push_back({
            {"index", i},
            {"grounded", wheel.grounded},
            {"rayHit", wheel.rayHit},
            {"rayDistance", wheel.rayDistance},
            {"rayMaxDistance", wheel.rayMaxDistance},
            {"suspensionLength", wheel.suspensionLength},
            {"compression", wheel.compression},
            {"load", wheel.load},
            {"slipRatio", wheel.slipRatio},
            {"slipAngle", wheel.slipAngle},
            {"spinVelocity", wheel.spinVelocity},
            {"steerAngle", wheel.steerAngle}});
    }
    fields["wheels"] = wheelArray;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "vehicle.state", std::to_string(state.id),
        "vehicle state snapshot", 0, fields, __FILE__, __LINE__, __func__);
}

} // namespace

void registerVehicleCommands()
{
    Terminal::instance().registerCommand({
        "vehicle_reload",
        "Reload config/vehicles/*.json templates",
        "vehicle_reload",
        [](const std::vector<std::string>&) {
            VehicleSystem::instance().loadConfig("config/vehicles");
            const auto ids = VehicleConfigRegistry::instance().ids();
            std::string message = "[VEHICLE] templates:";
            for (const std::string& id : ids)
                message += " " + id;
            if (ids.empty())
                message += " (none)";
            Terminal::instance().addLog(message);

            nlohmann::json fields;
            fields["ids"] = ids;
            fields["revision"] = VehicleConfigRegistry::instance().revision();
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.config.loaded", "vehicle-config",
                "vehicle templates reloaded", 0, fields,
                __FILE__, __LINE__, __func__);
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_spawn",
        "Spawn a vehicle template ahead of the player",
        "vehicle_spawn <template> [x y z yaw]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog(
                    "[VEHICLE] usage: vehicle_spawn <template> [x y z yaw]");
                return;
            }

            const VehicleSpec* spec =
                VehicleConfigRegistry::instance().find(args[0]);
            if (!spec)
            {
                Terminal::instance().addLog(
                    "[VEHICLE] unknown template; run vehicle_reload");
                return;
            }

            glm::vec3 position(0.0f);
            float yaw = 0.0f;
            bool spawnGroundHit = false;
            float spawnGroundZ = 0.0f;
            if (args.size() >= 5)
            {
                if (!parseFloat(args[1], position.x) ||
                    !parseFloat(args[2], position.y) ||
                    !parseFloat(args[3], position.z) ||
                    !parseFloat(args[4], yaw))
                {
                    Terminal::instance().addLog(
                        "[VEHICLE] position/yaw must be finite numbers");
                    return;
                }
            }
            else
            {
                glm::vec3 forward = THE_CAMERA.front;
                if (glm::dot(forward, forward) < 1e-6f)
                    forward = glm::vec3(1.0f, 0.0f, 0.0f);
                forward = glm::normalize(forward);
                position = THE_PLAYER.pos + forward * 3.5f +
                           glm::vec3(0.0f, 0.0f, 0.6f);

                // Place the chassis on the first ground below the chosen spot.
                // Fall back to the ground under the player when the forward
                // spot is over a void or platform edge.
                World& world = THE_WORLD;
                float hitDistance = 0.0f;
                glm::vec3 hitNormal(0.0f, 0.0f, 1.0f);
                glm::vec3 target = THE_PLAYER.pos;
                glm::vec3 rayOrigin = target + glm::vec3(0.0f, 0.0f, 6.0f);
                bool groundHit = vehicleRaycastWorld(
                    world, rayOrigin, glm::vec3(0.0f, 0.0f, -1.0f), 40.0f,
                    hitDistance, hitNormal);
                if (!groundHit)
                {
                    target = THE_PLAYER.pos;
                    rayOrigin = target + glm::vec3(0.0f, 0.0f, 6.0f);
                    groundHit = vehicleRaycastWorld(
                        world, rayOrigin, glm::vec3(0.0f, 0.0f, -1.0f), 40.0f,
                        hitDistance, hitNormal);
                }
                if (groundHit)
                {
                    const float groundZ = rayOrigin.z - hitDistance;
                    spawnGroundHit = true;
                    spawnGroundZ = groundZ;
                    position.x = target.x;
                    position.y = target.y;
                    // Ride height: chassis origin to wheel bottom is
                    // restLength + radius + |hub.z|.
                    position.z = groundZ + 0.95f;
                }
                yaw = std::atan2(forward.y, forward.x);
            }

            const glm::mat4 transform =
                glm::translate(glm::mat4(1.0f), position) *
                glm::mat4_cast(
                    glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)));
            const uint32_t id =
                VehicleSystem::instance().spawn(args[0], transform);
            if (id == 0)
            {
                Terminal::instance().addLog("[VEHICLE] spawn failed");
                return;
            }

            char buffer[192];
            std::snprintf(buffer, sizeof(buffer),
                          "[VEHICLE] spawned id=%u template=%s at "
                          "(%.2f %.2f %.2f) yaw=%.2f",
                          id, args[0].c_str(), position.x, position.y,
                          position.z, yaw);
            Terminal::instance().addLog(buffer);

            nlohmann::json fields;
            fields["vehicleId"] = id;
            fields["template"] = args[0];
            fields["x"] = position.x;
            fields["y"] = position.y;
            fields["z"] = position.z;
            fields["groundHit"] = spawnGroundHit;
            fields["groundZ"] = spawnGroundZ;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.spawned", std::to_string(id),
                "vehicle template spawned by terminal", 0, fields,
                __FILE__, __LINE__, __func__);
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_list",
        "List spawned vehicles and drivetrain state",
        "vehicle_list",
        [](const std::vector<std::string>&) {
            const auto& vehicles = VehicleSystem::instance().vehicles();
            Terminal::instance().addLog("[VEHICLE] count=" +
                                        std::to_string(vehicles.size()));
            for (const VehicleState& state : vehicles)
                logVehicleState(state);
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_despawn",
        "Remove a vehicle by id",
        "vehicle_despawn <id>",
        [](const std::vector<std::string>& args) {
            uint32_t id = 0;
            if (args.empty() || !parseUint(args[0], id))
            {
                Terminal::instance().addLog(
                    "[VEHICLE] usage: vehicle_despawn <id>");
                return;
            }
            const bool removed = VehicleSystem::instance().despawn(id);
            Terminal::instance().addLog(
                removed ? "[VEHICLE] despawned id=" + std::to_string(id)
                        : "[VEHICLE] id not found");
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_enter",
        "Enter the nearest vehicle seat and take local control",
        "vehicle_enter [vehicleId] [seat]",
        [](const std::vector<std::string>& args) {
            VehicleSystem& vehicles = VehicleSystem::instance();
            uint32_t id = 0;
            int seat = 0;

            if (!args.empty())
            {
                uint32_t parsed = 0;
                if (parseUint(args[0], parsed) && vehicles.find(parsed))
                {
                    id = parsed;
                    if (args.size() >= 2)
                    {
                        uint32_t seatValue = 0;
                        if (parseUint(args[1], seatValue))
                            seat = static_cast<int>(seatValue);
                    }
                }
                else
                {
                    uint32_t seatValue = 0;
                    if (parseUint(args[0], seatValue))
                        seat = static_cast<int>(seatValue);
                    id = nearestVehicleToPlayer();
                }
            }
            else
            {
                id = nearestVehicleToPlayer();
            }

            if (id == 0)
            {
                Terminal::instance().addLog("[VEHICLE] no vehicle found");
                return;
            }
            if (!vehicles.enterLocalDriver(id, seat))
            {
                Terminal::instance().addLog("[VEHICLE] enter failed");
                return;
            }
            Terminal::instance().addLog(
                "[VEHICLE] entered id=" + std::to_string(id) +
                " seat=" + std::to_string(seat));
            nlohmann::json fields;
            fields["vehicleId"] = id;
            fields["seat"] = seat;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.entered", std::to_string(id),
                "local actor entered vehicle seat", 0, fields,
                __FILE__, __LINE__, __func__);
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_exit",
        "Leave the current vehicle",
        "vehicle_exit",
        [](const std::vector<std::string>&) {
            if (!VehicleSystem::instance().hasLocalDriver())
            {
                Terminal::instance().addLog("[VEHICLE] not in a vehicle");
                return;
            }
            VehicleSystem::instance().exitLocalDriver();
            Terminal::instance().addLog("[VEHICLE] exited");
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.exited", "vehicle-exit",
                "local actor left vehicle seat", 0,
                nlohmann::json::object(), __FILE__, __LINE__, __func__);
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_input",
        "Inject non-local vehicle input for validation (0..1 / -1..1)",
        "vehicle_input <id> <throttle> <brake> <steer> <clutch> <handbrake>",
        [](const std::vector<std::string>& args) {
            uint32_t id = 0;
            float throttle = 0.0f;
            float brake = 0.0f;
            float steer = 0.0f;
            float clutch = 1.0f;
            float handbrake = 0.0f;
            if (args.size() < 6 || !parseUint(args[0], id) ||
                !parseFloat(args[1], throttle) ||
                !parseFloat(args[2], brake) ||
                !parseFloat(args[3], steer) ||
                !parseFloat(args[4], clutch) ||
                !parseFloat(args[5], handbrake))
            {
                Terminal::instance().addLog(
                    "[VEHICLE] usage: vehicle_input <id> <throttle> <brake> "
                    "<steer> <clutch> <handbrake>");
                return;
            }
            VehicleInput input;
            input.throttle = throttle;
            input.brake = brake;
            input.steer = steer;
            input.clutch = clutch;
            input.handbrake = handbrake;
            VehicleSystem::instance().setInput(id, input);
            Terminal::instance().addLog("[VEHICLE] input queued for id=" +
                                        std::to_string(id));
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_press",
        "Queue a one-shot vehicle control edge (engine/mode/shiftup/shiftdown)",
        "vehicle_press <id> <engine|mode|shiftup|shiftdown>",
        [](const std::vector<std::string>& args) {
            uint32_t id = 0;
            if (args.size() < 2 || !parseUint(args[0], id))
            {
                Terminal::instance().addLog(
                    "[VEHICLE] usage: vehicle_press <id> "
                    "<engine|mode|shiftup|shiftdown>");
                return;
            }
            VehicleState* state = VehicleSystem::instance().find(id);
            if (!state)
            {
                Terminal::instance().addLog("[VEHICLE] id not found");
                return;
            }
            VehicleInput input = state->pendingInput;
            if (args[1] == "engine")
                input.engineToggle = true;
            else if (args[1] == "mode")
                input.modeToggle = true;
            else if (args[1] == "shiftup")
                input.shiftUp = true;
            else if (args[1] == "shiftdown")
                input.shiftDown = true;
            else
            {
                Terminal::instance().addLog(
                    "[VEHICLE] unknown control; use "
                    "engine|mode|shiftup|shiftdown");
                return;
            }
            VehicleSystem::instance().setInput(id, input);
            Terminal::instance().addLog("[VEHICLE] pressed " + args[1] +
                                        " for id=" + std::to_string(id));
        }
    });

    Terminal::instance().registerCommand({
        "vehicle_debug",
        "Toggle the vehicle debug overlay flag",
        "vehicle_debug [on|off]",
        [](const std::vector<std::string>& args) {
            static bool enabled = false;
            if (!args.empty())
                enabled = args[0] == "on" || args[0] == "1";
            else
                enabled = !enabled;
            Terminal::instance().addLog(
                enabled ? "[VEHICLE] debug on" : "[VEHICLE] debug off");
        }
    });
}
