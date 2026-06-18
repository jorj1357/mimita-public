#pragma once
#include <GLFW/glfw3.h>

struct AvatarMenuResult {
    bool goBack = false;
    bool goApply = false;
    bool goSave = false;
};

AvatarMenuResult drawAvatarMenu(GLFWwindow* win);
void avatarMenuHandleChar(unsigned int codepoint);
void avatarMenuHandleKey(int key, int action);
