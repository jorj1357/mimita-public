// C:\important\go away v5\s\mimita-v5\src\physics\physics.h

#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "entities/player.h"
#include "world/world.h"   // <-- ADD THIS
#include "map/map_common.h"

// const World& world, not const Mesh& map
// this is defined here and in phsics.cpp need to pick 1 prob this one
void updatePhysics(
    Player& player, 
    const World& world, 
    GLFWwindow* win, 
    float dt, 
    const Camera& cam
);
