#pragma once

#include <string>
#include <vector>
#include <GLFW/glfw3.h>

// Reusable dropdown widget.
// State maintained by caller via DropdownState.
struct DropdownState {
    bool open = false;
    int selectedIndex = -1;
    float scrollOffset = 0.0f;
    bool openThisFrame = false; // true on the frame the dropdown was just opened
};

// Draw a dropdown at the given position.
// items: list of strings to display.
// Returns the index of the newly-selected item, or -1 if no change.
int drawDropdown(GLFWwindow* win, DropdownState& state,
                 float x, float y, float w, float itemH,
                 const char* label, const std::vector<std::string>& items);

// Draw only the overlay part of a dropdown (item list) — no header.
// Used for z-ordering: call this after all normal elements are drawn.
// Returns the index of the newly-selected item, or -1.
int drawDropdownOverlay(GLFWwindow* win, DropdownState& state,
                         float x, float y, float w, float itemH,
                         const std::vector<std::string>& items);
