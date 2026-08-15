// 08 15 2026, 15 30
/* purpose
* PNG library panel entry point.
* Draws the thumbnail grid and handles selection state.
* DOES NOT import PNGs (that happens in the drop handler).
*/
#pragma once
#include <GLFW/glfw3.h>

// PNG library panel (left). Draws the thumbnail grid; click selects.
void drawAvatarLibrary(GLFWwindow* win, float px, float py, float pw, float ph);
