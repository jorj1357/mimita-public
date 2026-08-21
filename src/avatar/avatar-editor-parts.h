// 08 20 2026, 12 00
/* purpose
* Draws the avatar editor's multi-face part picker tab.
* Keeps body-part and face selection shared with image and cosmetic tabs.
* Supports Select all and per-part/per-face checkboxes.
* DOES NOT edit avatar textures or cosmetic data.
*/
#pragma once

#include <GLFW/glfw3.h>

void drawAvatarPartPickerTab(GLFWwindow* win, float px, float py, float pw, float ph);
