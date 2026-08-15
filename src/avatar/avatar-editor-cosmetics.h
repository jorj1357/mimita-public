// 08 15 2026, 15 30
/* purpose
* Cosmetics tab entry point and dropdown overlay pass.
* Slot dropdowns + placement sliders for GLB cosmetics.
* DOES NOT load GLB meshes; CosmeticSystem does that.
*/
#pragma once
#include <GLFW/glfw3.h>

// Cosmetics tab: slot dropdowns + placement sliders. Overlay renders the
// open dropdown list on top of everything (call after all panels).
void drawAvatarCosmeticsTab(GLFWwindow* win, float px, float py, float pw, float ph);
void drawAvatarCosmeticsOverlay(GLFWwindow* win);
