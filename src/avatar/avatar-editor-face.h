// 08 15 2026, 15 30
/* purpose
* Faces tab entry point.
* Body part + face side picker, PNG assign, transform sliders.
* DOES NOT own avatar data or atlas baking.
*/
#pragma once
#include <GLFW/glfw3.h>

// Faces tab: body-part + face-side picker, PNG assign, transform sliders.
void drawAvatarFacesTab(GLFWwindow* win, float px, float py, float pw, float ph);
