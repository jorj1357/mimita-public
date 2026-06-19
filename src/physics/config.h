#pragma once
#include <glm/glm.hpp>

struct PhysicsConfig {
    float gravity;
    float moveSpeed;
    float jumpStrength;
};

inline PhysicsConfig PHYS = {
    -58.0f, // gravity
    20.0f,  // move speed
    19.0f   // jump strength
};

inline float PLAYER_WIDTH  = 1.0f;
inline float PLAYER_HEIGHT = 3.6f;
inline float PLAYER_DEPTH  = 0.4f;
inline float PLAYER_RADIUS = 0.7f;

struct PlayerCapsule {
    glm::vec3 a;
    glm::vec3 b;
    float r;
};

constexpr float COLLISION_SKIN = 0.02f;
inline float MAX_WALKABLE_SLOPE_DOT = 0.80f;
inline float HOW_DEEP = 0.5f;
inline float ALMOST_ZERO = 0.00001f;
inline float CHUNK_SIZE = 3.0f;
inline float MAX_FALL_SPEED = 400.0f;
inline float MAX_PLAYER_MOVE_SPEED = PHYS.moveSpeed * 100;
inline float MAX_EXTERNAL_IMPULSE_SPEED = 120.0f;
inline float EXTERNAL_IMPULSE_DECAY = 0.6f;
inline float EXTERNAL_IMPULSE_STEER_RATE = 40.0f;
inline float EXTERNAL_IMPULSE_BRAKE_RATE = 20.0f;
inline float ROTATION_SNAP = 15.0f;
inline float POSITION_SNAP = 0.1f;
inline float COLLISIONS_GRACE_PERIOD = 0.1f;
inline float COYOTE_JUMP_TIME = 0.001f;
inline float FRICTION_AMOUNT = 0.2f;
inline float GROUND_FRICTION_ACCEL_AMOUNT = 0.2f;
inline float GROUND_FRICTION_AMOUNT = 10.0f;
inline float AIR_FRICTION_AMOUNT = 2.0f;
inline float AIR_ACCEL_AMOUNT = 50.0f;
inline float DRAG_FRICTION_MULTIPLIER = 0.25f;
inline float OPPOSITE_FRICTION_AMOUNT = 0.99f;
inline float DASH_IMPULSE = 100.0f;
inline float AIR_DASH_IMPULSE = 50.0f;
constexpr float DOWN_DASH_SPEED = -100.0f;
constexpr int   DASH_MAX_CHARGES   = 3;
constexpr float DASH_RECHARGE_TIME = 1.0f;
constexpr float FREEZE_MAX_TIME = 5.0f;
constexpr float GROUND_RETURN_MAX_CHARGES = 3.0f;
constexpr float GROUND_RETURN_SPEED = -150.0f;
constexpr float GROUND_RETURN_RECHARGE_TIME = 1.0f;
inline float DASH_COOLDOWN = 0.001f;
inline float SLOPE_OVERLAP = 0.1f;
inline float SLOPE_SKIN = 0.02f;
inline float BODY_SAMPLE_RADIUS = 0.15f;
inline float MAX_STEP_HEIGHT = 0.25f;
inline float SLOPE_VELOCITY_PUSHUP_MULT = 1.01f;
inline float SLOPE_SNAP_DIST = 0.15f;
inline float SLOPE_WALL_CLEARANCE = 2.5f;
inline float SLOPE_EXIT_TIMER = 0.16f;
inline float SLOPE_SUPPORT_TIMER = 0.16f;
inline float JUMP_BUFFER_TIME = 0.12f;
constexpr int AIR_JUMPS_MAX = 1;
inline float DASH_SPEED_MULT = 1.5f;
inline bool DASH_INFINITE = true;

inline float npcRespawnDelaySeconds = 0.01f;
inline glm::vec3 npcSpawnPoint = glm::vec3(0.0f, 0.0f, 0.0f);
