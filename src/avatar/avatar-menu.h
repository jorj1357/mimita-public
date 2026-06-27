#pragma once
#include <GLFW/glfw3.h>
#include <string>

struct AvatarMenuResult {
    bool goBack = false;
    bool goApply = false;
    bool goSave = false;
    bool savePopupOpen = false;
    bool renamePopupOpen = false;
    bool deleteConfirmOpen = false;
    std::string targetOutfit; // for rename/delete
};

AvatarMenuResult drawAvatarMenu(GLFWwindow* win);
void avatarMenuHandleChar(unsigned int codepoint);
void avatarMenuHandleKey(int key, int action);

// Drag & drop support
void avatarMenuHandleDrop(int count, const char** paths);
