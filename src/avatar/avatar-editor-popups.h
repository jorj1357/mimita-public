// 08 15 2026, 15 30
/* purpose
* Save/rename/delete popup entry point.
* Declares drawAvatarEditorPopups.
* DOES NOT own popup text state (avatar-editor.cpp does).
*/
#pragma once
#include <GLFW/glfw3.h>
#include <string>

struct AvatarEditorResult;

// Save / rename / delete confirm popups.
void drawAvatarEditorPopups(GLFWwindow* win, AvatarEditorResult& r);
