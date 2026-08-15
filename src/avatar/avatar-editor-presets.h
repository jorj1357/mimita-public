// 08 15 2026, 15 30
/* purpose
* Presets tab entry point.
* Save/load named looks.
* DOES NOT own preset storage (AvatarSystem does).
*/
#pragma once
#include <GLFW/glfw3.h>

// Presets tab: save / load look presets.
void drawAvatarPresetsTab(GLFWwindow* win, float px, float py, float pw, float ph);
