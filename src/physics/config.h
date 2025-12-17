// C:\important\go away v5\s\mimita-v5\src\physics\config.h
// the physics.json file at 
// C:\important\quiet\n\mimita-public\mimita-public\config\physics.json
// this has hot reload values that i think are more accurate

#pragma once

struct PhysicsConfig {
    float gravity;
    float moveSpeed;
    float jumpStrength;
};

inline PhysicsConfig PHYS = {
    -25.0f,
    20.0f,
    12.0f
};

// Player dimensions (RAW FLOATS ONLY)
inline float PLAYER_WIDTH  = 0.7f;   // meters
inline float PLAYER_HEIGHT = 1.8f;   // meters
inline float PLAYER_DEPTH  = 0.7f;

// do we use this idk
// i put it jsut so errors stop 
inline float PLAYER_RADIUS = 0.35f;
