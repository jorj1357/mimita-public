// C:\important\go away v5\s\mimita-v5\src\physics\config.h

#pragma once

// all tweakable constants live here

/*
------------------------------------------
MIMITA SIZES V1 AS OF NOV 6 2025
------------------------------------------

roblox

csgo

tf2

blender
1 unit = 1 meter

mimita
1.0f = 1 meter

------------------------------------------
EXTRA TODO NOV 6 2025
------------------------------------------

make gravity like this
so that its meters and kilometers etc
// Example in Mimita
float gravity = -9.81f;   // meters per second²
float playerHeight = 1.8f; // 1.8 meters tall
float playerSpeed = 5.0f;  // 5 m/s walking

also todo nov 6 2025
scaling from blender/other programs to mimita
float blenderToMimitaScale = 1.0f; // default
// if it looks too big,
// try 0.01f (common for FBX),
// but for glTF keep 1.0

glm::vec3 scaledPos = (
originalPos * blenderToMimitaScale
);

*/

/*
todo nov 6 2025 942pm
still tweaking
idk
need fix later
*/

/**
 * todo dec 12 2025 is this even what actual
 * ly does anthing? we go wa faster than 20 i think  idk
 */

struct PhysicsConfig {
    float gravity = -25.0f;
    float moveSpeed = 20.0f;
    float jumpStrength = 10.0f;
    float deathHeight = -50.0f;
    float respawnDelay = 1.0f;
};

// player_radius is in physics as of dec 16 2025 but we duplicate code here bc idk 
constexpr float PLAYER_RADIUS = 0.35f;

// global config instance
inline PhysicsConfig PHYS;
