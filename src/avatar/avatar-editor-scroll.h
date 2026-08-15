// 08 15 2026, 15 30
/* purpose
* Declares the ScrollState struct and beginScroll/endScroll helpers.
* These clip drawing to an area and draw a scrollbar.
* DOES NOT own scroll state; callers keep their own ScrollState.
*/
#pragma once

#include <GLFW/glfw3.h>
#include "../gui/ui-system.h"

// Reusable scrolling component.
// Wraps uiBeginScrollArea/uiEndScrollArea with a simpler state API.
// Use beginScroll()/endScroll() in a pair, with all draw calls between them.

struct ScrollState {
    float offset = 0.0f;
    bool dragging = false;
    float dragStartY = 0.0f;
    float dragScrollStart = 0.0f;
};

// Clips rendering to area and translates coordinates based on scroll offset.
// Must be paired with endScroll().
void beginScroll(GLFWwindow* win, UIRect area, float contentHeight, ScrollState& state);

// Ends scroll clipping and draws scrollbar.
void endScroll(UIRect area, float contentHeight, const ScrollState& state);
