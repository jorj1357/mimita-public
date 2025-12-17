// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics-debug-movement.h
// dec 17 2025
/**
 * purpose
 * header for debug movement 
 * so we dont have to walk slow everwhere 
 */

#pragma once
#include "camera.h"

struct Player;
struct GLFWwindow;
class Camera;

void applyDebugMovement(Player& p, GLFWwindow* win, const Camera& cam, float dt);
