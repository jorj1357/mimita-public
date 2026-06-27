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
    std::string targetOutfit;
};

AvatarMenuResult drawAvatarMenu(GLFWwindow* win);
void avatarMenuHandleChar(unsigned int codepoint);
void avatarMenuHandleKey(int key, int action);
void avatarMenuHandleDrop(int count, const char** paths);

// Popup text input state (shared between menu and draw)
extern bool gSavePopupOpen;
extern char gSaveNameBuf[64];
extern bool gRenamePopupOpen;
extern char gRenameBuf[64];
extern bool gDeleteConfirmOpen;
