// 08 15 2026, 15 30
/* purpose
* Colors tab entry point.
* Per-body-part RGB tint sliders with live preview.
* DOES NOT own avatar data.
*/
#pragma once
#include <GLFW/glfw3.h>

// Colors tab: per-part RGB tint sliders.
void drawAvatarColorsTab(GLFWwindow* win, float px, float py, float pw, float ph);
