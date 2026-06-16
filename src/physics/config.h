// C:\important\quiet\n\mimita-priv-v7\src\physics\config.h
// feb 10 2026
// PLZ USE THIS FOR HITBOX SIZES IDK WHERE THEU ARE BUT USE THIS PLZ

#pragma once
#include <glm/glm.hpp>

// =====================================================
// CORE PHYSICS CONFIG (game feel)
// =====================================================

struct PhysicsConfig {
    float gravity;
    float moveSpeed;
    float jumpStrength;
};

inline PhysicsConfig PHYS = {
    // -28.0f, // gravity
    -58.0f, // gravity
    20.0f, // move speed
    // 30.0f, // move speed
    // 12.0f, // jump strength
    // 22.0f, // jump strength
    19.0f, // jump strength
    // 22.0f, // jump strength
    // 6 6 2026 these values dont even do antthing its just 3  not 4 
    // 1.1f // amount of whatever gained in air feb 10 2026
    // 1.0f // amount of whatever gained in air feb 10 2026
};

// explain what this does even 
// i think its to go from blender to our system?
// i thought blender was 1 meter = 1 blender unit = 1 mimita (mimita is meters)
// mar 7 2026 i cannot remmeber bruh but i thought blender was 0.5 units to 1 real unit idk
// MAR 7 2026 DONT EVEN USE THIS ITS STUPID
// inline float PHYS_MULT = 2.0f;  
// inline float PHYS_MULT = 2.0f;  

// defaults are
/*
0.5f for width
1.8f for height
0.2f for depth
0.35f for radius
*/

// jan 30 2026 test not multpling these w phs mult
inline float PLAYER_WIDTH  = 1.0f;  
// too short, ? mar 7 2026 testing 5.0f
// mar 7 2026 , plr width 1.0f, depth = 0.4f, plr radius = 0.7f, 
// these are great so far as i can see
// inline float PLAYER_HEIGHT = 3.6f;
// height at 5.0f is too tall it just makes mesh change ugh mar 7 2026    
// leaveing it at 3.6f because 1.8m is average height times 2 
inline float PLAYER_HEIGHT = 3.6f;   
inline float PLAYER_DEPTH  = 0.4f;  
// mar 7 2026 do we use this? plr radius ? 
// mar 7 2026 we do use this we use all this
// MAR 7 2026 PUT THIS TO BE THE SINGLE PLACE WHERE PLAYER HITBOXES ARE 
inline float PLAYER_RADIUS = 0.7f;

// =====================================================
// PLAYER CAPSULE TYPE (the real hitbox)
// =====================================================

struct PlayerCapsule {
    glm::vec3 a;   // bottom sphere center
    glm::vec3 b;   // top sphere center
    float r;
};

// Collision skin width — treats surfaces as slightly thicker
// to reduce jitter, snagging, and seam penetration.
// 0.02f = 2cm virtual margin around all collision geometry.
constexpr float COLLISION_SKIN = 0.02f;

// =====================================================
// WORLD / COLLISION TUNING
// =====================================================

// cosine of max walkable slope
// normal.z >= MAX_WALKABLE_SLOPE_DOT = slope is ground
// normal.z < MAX_WALKABLE_SLOPE_DOT = slope is slide/surf surface
// default 0.707f = 45 degree max walkable slope
// Changed to 0.80f (~37 degrees) to prevent standing on steep curved/sloped
// geometry that should not be standable.
inline float MAX_WALKABLE_SLOPE_DOT = 0.80f;

// json loading stuff
// jan 30 2026 do we need this?  i would like to have no conversions 
// no mults etc but idk
// jan 30 2026 im just making this 1 bc i dontn wanna use it and dont wana break others
// MAR 7 2026 DONT USE THIS IT FUCKS EVERTHING UP AND I HATE IT 
// inline float BLOCK_PHYS_MULT = 0.5f;
// jan 30 2026 nvm lets just use 0.5f because world mesh wants 0.5f mult to be happy
// inline float BLOCK_PHYS_MULT = 1.0f;

// how deep
// e.g. 0.5f = im alll up in there
// 0.01f = if im a LIL in there stop. 
// its called this bc i dont like the word penetration
inline float HOW_DEEP = 0.5f;

// almost 0 
// because we're weird
// dec 19 2025 todo use this everwhere for consistenet epsilon for the 0.1% improvement it gives
inline float ALMOST_ZERO = 0.00001f;

// everi map is now in chunks of this size 
// if ur in 1, calc onli that ones colisions
// if ur in 2, calc both
// if ur in 3+, calc those 
// just dont calc the entire freakin map
// test smaller chunk size? idk feb 3 2026
// inline float CHUNK_SIZE = 5.0f; 
inline float CHUNK_SIZE = 3.0f; 

// max fall speed
inline float MAX_FALL_SPEED = 20.0f * 20; 

// max speed of plr in whole phs engine feb 8 2026
// max speed is 100x the plr default move speed
inline float MAX_PLAYER_MOVE_SPEED = PHYS.moveSpeed * 100; 

// External momentum is intentionally separate from normal movement speed.
inline float MAX_EXTERNAL_IMPULSE_SPEED = 120.0f;
inline float EXTERNAL_IMPULSE_DECAY = 0.6f;
// inline float EXTERNAL_IMPULSE_DECAY = 3.0f;
// inline float EXTERNAL_IMPULSE_DECAY = 0.9f;
// inline float EXTERNAL_IMPULSE_DECAY = 1.1f;
// inline float EXTERNAL_IMPULSE_DECAY = 10.1f;
// inline float EXTERNAL_IMPULSE_DECAY = 5.0f;
// inline float EXTERNAL_IMPULSE_STEER_RATE = 4.0f;
inline float EXTERNAL_IMPULSE_STEER_RATE = 40.0f;
// inline float EXTERNAL_IMPULSE_BRAKE_RATE = 2.5f;
inline float EXTERNAL_IMPULSE_BRAKE_RATE = 20.0f;

// for blender to map stuff that isnt broken 
inline float ROTATION_SNAP = 15.0f;
inline float POSITION_SNAP = 0.1f;

// grace period testing dec 19 2025 collisions
inline float COLLISIONS_GRACE_PERIOD = 0.1f;

// feb 6 2026 allowing double jumps or more permissive jumps? idk  
// 0.08f should be 80ms, so we will just call it 0.001f bc we like 1ms grace
inline float COYOTE_JUMP_TIME = 0.001f;

// higher = more friction, lower = less friction
// feb 6 2026 testing the default of 0.2f
// general version, feb 8 2026 i think we have air and ground friction separate?
inline float FRICTION_AMOUNT = 0.2f;

// 0.2f as of feb 8 2026 
inline float GROUND_FRICTION_ACCEL_AMOUNT = 0.2f;

// friction for ground, 0.2f as of feb 8 2026
// 0.2f is way too low. testing 5.0f
// 5.0f its kinda annoying its not instant like i want.
// near instant is what i wnt but a little slide 
// testing 20.0f
// controls friction mar 8 2026
// mar 8 2026 testing 10.0f for less
inline float GROUND_FRICTION_AMOUNT = 10.0f;

// friction for air, 0.02f as of feb 8 2026
// i want to have just same friction for both but whatever mar 8 2026
// mar 8 2026 testing 2.0f
inline float AIR_FRICTION_AMOUNT = 2.0f;

// accel for air for BUNN HOPS AYYYAYYAY feb 8 2026 
// first value; 50.0f test
inline float AIR_ACCEL_AMOUNT = 50.0f;

// 1.0f too high
// testing 0.25f
inline float DRAG_FRICTION_MULTIPLIER = 0.25f;

// anti friction? higher = lower friction, 
// lower = more friction?
inline float OPPOSITE_FRICTION_AMOUNT = 0.99f;

// dash power
// prob keep it at 50.0f, just need to make it decay faster speed wise
// mar 8 2026 setting to 100.0f to test with the touch object = dash reset sstem
// mar 8 2026 its actuall waaaau too fast so put to like 50.0f is fine
// at least i think? bc right now its 2x mi movement speed so 100.0f is like 200.0f so wtv
// mar 8 2026 its fixed bc dont multiply bi current move speed 
// or do idk
// mult with current move speed = faster plauer wins
// single set impulse = more movement smart plauer wins 
// imsetting it to like 100.0f bc 50 too low mar 8 2026
inline float DASH_IMPULSE = 100.0f;

// air dash impulse (weaker than ground dash)
inline float AIR_DASH_IMPULSE = 50.0f;

// down dash slam speed (Q key)
constexpr float DOWN_DASH_SPEED = -100.0f;
// constexpr float DOWN_DASH_SPEED = -25.0f;
// what if not having this at all idk 6 14 2026 
// constexpr float DOWN_DASH_SPEED = -5.0f;

// how many dash charges u got
// 6 14 2026 not used bc dashes just recharge if u touch a world object
constexpr int   DASH_MAX_CHARGES   = 3;

// how long between each dash charge 
constexpr float DASH_RECHARGE_TIME = 1.0f; // seconds

// how long can i be frozen for maximum
// MAR 8 2026 REMOVE THIS MAKE THIS INFINITE BC NO LIMITS ON FREEZE I WANT 
constexpr float FREEZE_MAX_TIME = 5.0f; // seconds

// how man times can i do it befoer i gotta wiat the recharge
constexpr float GROUND_RETURN_MAX_CHARGES = 3.0f;

// go to the ground at this speed
constexpr float GROUND_RETURN_SPEED = -150.0f;

// how long in seconds before it recharges 
// make THIS 0 MAR 8 2026 
constexpr float GROUND_RETURN_RECHARGE_TIME = 1.0f;

// i want the cooldown to be like 0.01ms bro so i set this to 0.001
inline float DASH_COOLDOWN = 0.001f;

// something so that our slopes (which are currenlty 2 halves so that its not infinite)
// doesn't just make us get stuck in the dead center btwn them 
inline float SLOPE_OVERLAP = 0.1f;

// defined in config.h but its 0.02f for now
// tune 0.01-0.05
inline float SLOPE_SKIN = 0.02f;

// radius for body part collision sweeps (arms, legs, etc.)
// larger = more solid limbs, but more collision checks
// keep single definition only
// Radius for body part collision sweeps (arms, legs, etc.)
// Increased from 0.12f to 0.25f to prevent limb spheres from catching
// on world geometry triangle edges and corners.
constexpr float BODY_SAMPLE_RADIUS = 0.25f;

// max step heihgt
// how up can u walk , like how tall can it be , the block
// feb 9 2026 doewsnt do antthing so wtv add later 
// inline float MAX_STEP_HEIGHT = 3.0f;
// 3.0f is so much so idk make smaller? 
// inline float MAX_STEP_HEIGHT = 0.1f;
// 0.1f not tall enoughso we try 0.25f mar 7 2026
inline float MAX_STEP_HEIGHT = 0.25f;
// 6 7 2026 testing 
// 6 7 2026 too much 
// inline float MAX_STEP_HEIGHT = 0.75f;

// this was for pushing me up when im on a slope? idk 
inline float SLOPE_VELOCITY_PUSHUP_MULT = 1.01f;

// this is for like 
// snapping the plr to the slope so we dont have to hold jump to stay on it 
inline float SLOPE_SNAP_DIST = 0.15f;

// push me away from walls
// take me awaayy (take me away)
// a secret plaace (away from the wall)
// a sweet escaaaaape (so i can move and not stuck in the wall)
// take me awaaayy (i want to be able to move )
// inline float SLOPE_WALL_CLEARANCE = 0.5f; // try 0.1–0.25
// try 2.5f because fuck uou (i keep getting stuck in the thing )
inline float SLOPE_WALL_CLEARANCE = 2.5f; 

// both default was 0.08f but i want lots of clearance 
inline float SLOPE_EXIT_TIMER = 0.16f;

// both default was 0.08f but i want lots of clearance 
inline float SLOPE_SUPPORT_TIMER = 0.16f;

// todo sort this bc its so big fat
// time between jumps so we dont spam the phs engine 
inline float JUMP_BUFFER_TIME = 0.12f;

// air jumps
// defined also in plauer cpp but put it here
constexpr int AIR_JUMPS_MAX = 1;

// how much we multiply current speed by with dash
// for combat system v2 mar 8 2026
// dashing into things too fast hurts u bc knockback 
// mar 8 2026 setting to 2.0f bceause i want it stronger? idk 
inline float DASH_SPEED_MULT = 1.5f;

// testing if we want infinite dashes for combat sstem v2 mar 8 2026
inline bool DASH_INFINITE = true;
