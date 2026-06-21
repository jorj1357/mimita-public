#include "physics/movement-config.h"
#include "physics/config.h"
#include "devtools/terminal.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    auto ft = fs::last_write_time(path, ec);
    if (ec) return 0;
    return ft.time_since_epoch().count();
}

template <typename T>
void readValue(const json& j, const char* name, T& value)
{
    if (j.contains(name))
        value = j[name].get<T>();
}

}

MovementConfig& MovementConfig::instance()
{
    static MovementConfig cfg;
    return cfg;
}

bool MovementConfig::load(const std::string& path)
{
    mPath = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        mLastError = "cannot open " + path;
        printf("[MOVEMENT CONFIG] %s\n", mLastError.c_str());
        return false;
    }

    try {
        json root;
        file >> root;

        // PhysicsConfig struct
        readValue(root, "gravity", PHYS.gravity);
        readValue(root, "moveSpeed", PHYS.moveSpeed);
        readValue(root, "jumpStrength", PHYS.jumpStrength);

        // Player dimensions
        readValue(root, "playerWidth", PLAYER_WIDTH);
        readValue(root, "playerHeight", PLAYER_HEIGHT);
        readValue(root, "playerDepth", PLAYER_DEPTH);
        readValue(root, "playerRadius", PLAYER_RADIUS);

        // Collision / general
        readValue(root, "maxWalkableSlopeDot", MAX_WALKABLE_SLOPE_DOT);
        readValue(root, "howDeep", HOW_DEEP);
        readValue(root, "chunkSize", CHUNK_SIZE);
        readValue(root, "maxFallSpeed", MAX_FALL_SPEED);
        readValue(root, "maxPlayerMoveSpeed", MAX_PLAYER_MOVE_SPEED);
        readValue(root, "maxExternalImpulseSpeed", MAX_EXTERNAL_IMPULSE_SPEED);
        readValue(root, "externalImpulseDecay", EXTERNAL_IMPULSE_DECAY);
        readValue(root, "externalImpulseSteerRate", EXTERNAL_IMPULSE_STEER_RATE);
        readValue(root, "externalImpulseBrakeRate", EXTERNAL_IMPULSE_BRAKE_RATE);

        readValue(root, "rotationSnap", ROTATION_SNAP);
        readValue(root, "positionSnap", POSITION_SNAP);
        readValue(root, "collisionsGracePeriod", COLLISIONS_GRACE_PERIOD);
        readValue(root, "coyoteJumpTime", COYOTE_JUMP_TIME);

        // Friction
        readValue(root, "frictionAmount", FRICTION_AMOUNT);
        readValue(root, "groundFrictionAccelAmount", GROUND_FRICTION_ACCEL_AMOUNT);
        readValue(root, "groundFrictionAmount", GROUND_FRICTION_AMOUNT);
        readValue(root, "groundAccelerate", GROUND_ACCELERATE);

        // Air
        readValue(root, "airFrictionAmount", AIR_FRICTION_AMOUNT);
        readValue(root, "airAccelAmount", AIR_ACCEL_AMOUNT);
        readValue(root, "airSpeedCap", AIR_SPEED_CAP);

        // Drag / opposing
        readValue(root, "dragFrictionMultiplier", DRAG_FRICTION_MULTIPLIER);
        readValue(root, "oppositeFrictionAmount", OPPOSITE_FRICTION_AMOUNT);

        // TODO: load dash quality multipliers (physics-dash.h dashQualityMultiplier)
        // TODO: load freeze velocity curve params (physics-freeze.cpp freezeVelocityMultiplier)

        // Dash
        readValue(root, "dashImpulse", DASH_IMPULSE);
        readValue(root, "airDashImpulse", AIR_DASH_IMPULSE);
        readValue(root, "downDashSpeed", DOWN_DASH_SPEED);
        readValue(root, "dashMaxCharges", DASH_MAX_CHARGES);
        readValue(root, "dashRechargeTime", DASH_RECHARGE_TIME);
        readValue(root, "dashCooldown", DASH_COOLDOWN);
        readValue(root, "dashSpeedMult", DASH_SPEED_MULT);
        readValue(root, "dashInfinite", DASH_INFINITE);

        // Freeze
        readValue(root, "freezeMaxTime", FREEZE_MAX_TIME);

        // Ground return
        readValue(root, "groundReturnMaxCharges", GROUND_RETURN_MAX_CHARGES);
        readValue(root, "groundReturnSpeed", GROUND_RETURN_SPEED);
        readValue(root, "groundReturnRechargeTime", GROUND_RETURN_RECHARGE_TIME);

        // Slope
        readValue(root, "slopeOverlap", SLOPE_OVERLAP);
        readValue(root, "slopeSkin", SLOPE_SKIN);
        readValue(root, "bodySampleRadius", BODY_SAMPLE_RADIUS);
        readValue(root, "maxStepHeight", MAX_STEP_HEIGHT);
        readValue(root, "slopeVelocityPushupMult", SLOPE_VELOCITY_PUSHUP_MULT);
        readValue(root, "slopeSnapDist", SLOPE_SNAP_DIST);
        readValue(root, "slopeWallClearance", SLOPE_WALL_CLEARANCE);
        readValue(root, "slopeExitTimer", SLOPE_EXIT_TIMER);
        readValue(root, "slopeSupportTimer", SLOPE_SUPPORT_TIMER);

        // Jump
        readValue(root, "jumpBufferTime", JUMP_BUFFER_TIME);
        readValue(root, "airJumpsMax", AIR_JUMPS_MAX);

        // NPC
        readValue(root, "npcRespawnDelaySeconds", npcRespawnDelaySeconds);

        mLastError.clear();
        mLastWriteTime = modifiedTime(path);
        printf("[MOVEMENT CONFIG] loaded %s\n", path.c_str());
        return true;

    } catch (const std::exception& e) {
        mLastError = "parse error: ";
        mLastError += e.what();
        printf("[MOVEMENT CONFIG] %s\n", mLastError.c_str());
        return false;
    }
}

void MovementConfig::pollReload()
{
    const int64_t current = modifiedTime(mPath);
    if (current != 0 && current != mLastWriteTime) {
        printf("[MOVEMENT CONFIG] file changed, reloading...\n");
        load(mPath);
    }
}
