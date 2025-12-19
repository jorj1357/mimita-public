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
// i know, 1.8f = 1.8m is nice and pretty but i dont htink cpp is doing that so the
// numbers are bigger 

// defaults are
/*
0.5f for width
1.8f for height
0.2f for depth
0.35f for radius
*/

inline float PHYS_MULT = 2.0f;  
inline float PLAYER_WIDTH  = 0.5f * PHYS_MULT;  
inline float PLAYER_HEIGHT = 1.8f * PHYS_MULT;   
inline float PLAYER_DEPTH  = 0.2f * PHYS_MULT;  
inline float PLAYER_RADIUS = 0.35f * PHYS_MULT;

inline float MAX_SLOPE_ANGLE = 0.5f;

// dec 18 2025
// json loading stuff
inline float BLOCK_PHYS_MULT = 0.5f;

// how deep
// e.g. 0.5f = im alll up in there
// 0.01f = if im a LIL in there stop. 
// its called this bc i dont like the word penetration
inline float HOW_DEEP = 0.5f;

// almost 0 
// because we're werid
inline float ALMOST_ZERO = 0.00001f;

// everi map is now in chunks of this size 
// if ur in 1, calc onli that ones colisions
// if ur in 2, calc both
// if ur in 3+, calc those 
// just dont calc the entire freakin map
// which allows multi pass which allows working freakin collisions finalli
inline float CHUNK_SIZE = 5.0f; 

// so for now i think we clamp it to this max value
// max fall speed
// but i really like unclamped speed values 
// or at least the limit is very high
// but idk tweak this 
// be a good little PROGRAMMER and TWEAK with yo CODE 
// its move speed * 20 but we dont have move_speed rn 
inline float MAX_FALL_SPEED = 20.0f * 20; 

// for blender to map  stuff that isnt broken 
inline float ROTATION_SNAP = 15.0f;
inline float POSITION_SNAP = 0.1f;



