#pragma once
#include <string>

// Initialize Win32 drag-drop target (call once after window creation)
void initAvatarDropTarget(void* hwnd);
void shutdownAvatarDropTarget();

// Query current hover state (for UI display)
const std::string& getDropHoverPath();
bool isDropHoverActive();
