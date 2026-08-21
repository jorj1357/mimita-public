// 08 20 2026, 12 00
/* purpose
* Declares the image editor tab for selected avatar faces.
* Exposes shared PNG assignment and transform controls.
* DOES NOT choose body parts or own the avatar preview.
*/
#pragma once

#include <GLFW/glfw3.h>

void drawAvatarImageEditorTab(GLFWwindow* win, float px, float py, float pw, float ph);
