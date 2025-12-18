// C:\important\quiet\n\mimita-public\mimita-public\src\physics\config-loader.cpp
// dec 16 2025
/*
purpose
lets us have a json file to edit so that i dont have to rebuild game over and over
*/

// #include "config-loader.h"
// #include "config.h"

// #include <fstream>
// #include <filesystem>
// #include <nlohmann/json.hpp>
// #include <cstdio>
// #include "../debug/debug-status.h"

// static std::filesystem::file_time_type lastWrite;

// void loadPhysicsConfig()
// {
//     std::ifstream f("config/physics.json");
//     if (!f.is_open()) return;

//     nlohmann::json j;
//     f >> j;

//     PHYS.gravity      = j.value("gravity", PHYS.gravity);
//     PHYS.moveSpeed    = j.value("moveSpeed", PHYS.moveSpeed);
//     PHYS.jumpStrength = j.value("jumpStrength", PHYS.jumpStrength);

//     PLAYER_HEIGHT = j.value("playerHeight", PLAYER_HEIGHT);
//     PLAYER_RADIUS = j.value("playerRadius", PLAYER_RADIUS);
//     PLAYER_CAPSULE_HALF = (PLAYER_HEIGHT * 0.5f) - PLAYER_RADIUS;

// }

// void hotReloadPhysicsConfig()
// {
//     namespace fs = std::filesystem;
//     auto t = fs::last_write_time("config/physics.json");

//     if (t != lastWrite)
//     {
//         lastWrite = t;
//         loadPhysicsConfig();
//         printf("[config] physics.json reloaded\n");
//     }
// }
