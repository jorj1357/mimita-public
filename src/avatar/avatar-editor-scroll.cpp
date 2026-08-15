// 08 15 2026, 15 30
/* purpose
* Wraps uiBeginScrollArea/uiEndScrollArea with a simpler ScrollState API.
* DOES NOT own scroll state; callers pass their own ScrollState in.
*/
#include "avatar-editor-scroll.h"

#include <algorithm>
#include "../gui/ui-system.h"
#include "../gui/gui-coord.h"

void beginScroll(GLFWwindow* win, UIRect area, float contentHeight, ScrollState& state)
{
    UIScrollState uis;
    uis.scrollY = state.offset;
    uis.dragging = state.dragging;
    uis.dragStartY = state.dragStartY;
    uis.dragScrollStart = state.dragScrollStart;

    uiBeginScrollArea(win, area, contentHeight, uis);

    state.offset = uis.scrollY;
    state.dragging = uis.dragging;
    state.dragStartY = uis.dragStartY;
    state.dragScrollStart = uis.dragScrollStart;
}

void endScroll(UIRect area, float contentHeight, const ScrollState& state)
{
    UIScrollState uis;
    uis.scrollY = state.offset;
    uis.dragging = state.dragging;
    uis.dragStartY = state.dragStartY;
    uis.dragScrollStart = state.dragScrollStart;

    uiEndScrollArea(area, contentHeight, uis);
}
