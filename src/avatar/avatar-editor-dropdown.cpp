#include "avatar-editor-dropdown.h"
#include "avatar-editor-scroll.h"

#include <algorithm>
#include <cstdio>

#include "../gui/ui-system.h"
#include "../gui/ui-system-internal.h"
#include "../gui/gui-coord.h"

static void drawListBackground(float x, float y, float w, float h)
{
    UIRect screenRect = {uiScaleX(x), uiScaleY(y), uiScaleX(w), uiScaleY(h)};
    uiDrawRect(screenRect, {0.06f, 0.07f, 0.1f, 1.0f}, "dropdown-list-bg");
    uiDrawRectOutline(screenRect, {0.3f, 0.4f, 0.6f, 0.6f}, "dropdown-list-border");
}

int drawDropdown(GLFWwindow* win, DropdownState& state,
                 float x, float y, float w, float itemH,
                 const char* label, const std::vector<std::string>& items)
{
    // Display selected text
    std::string displayText = state.selectedIndex >= 0 && state.selectedIndex < (int)items.size()
        ? items[state.selectedIndex]
        : (items.empty() ? "" : items[0]);

    glm::vec4 bg{0.1f, 0.12f, 0.18f, 1.0f};
    UIRect headerRect = {x, y, w, itemH};
    UIButtonState btn = uiButton(win, displayText.c_str(), headerRect, bg, "dropdown-header");

    if (btn.clicked) {
        bool wasOpen = state.open;
        state.open = !state.open;
        if (state.open && !wasOpen) {
            state.openThisFrame = true;
            UISys::gDropdownModalActive = true;
            return -1;
        }
        if (!state.open) {
            // Just closed via header click — no selection
            UISys::gDropdownModalActive = false;
            return -1;
        }
    }

    // Draw triangle indicator
    {
        GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
        float triX = cs.designToScreenX(x + w - 18.0f);
        float triY = cs.designToScreenY(y + itemH * 0.5f);
        uiDrawTriangle(triX, triY, 5.0f, state.open, {0.6f, 0.7f, 0.9f, 0.8f}, "dropdown-arrow");
    }

    return -1;
}

int drawDropdownOverlay(GLFWwindow* win, DropdownState& state,
                         float x, float y, float w, float itemH,
                         const std::vector<std::string>& items)
{
    // Allow overlay item clicks regardless of modal state
    UISys::gDropdownModalActive = false;

    if (!state.open || items.empty())
        return -1;

    if (state.openThisFrame) {
        state.openThisFrame = false;
        return -1;
    }

    int result = -1;

    float listH = std::min((float)items.size() * itemH, 200.0f);
    float lx = x;
    float ly = y + itemH + 2.0f;

    drawListBackground(lx, ly, w, listH);

    float contentH = (float)items.size() * itemH;
    ScrollState scrollState;
    scrollState.offset = state.scrollOffset;

    beginScroll(win, {lx, ly, w, listH}, contentH, scrollState);
    state.scrollOffset = scrollState.offset;

    float iy = ly;
    for (int i = 0; i < (int)items.size(); ++i)
    {
        glm::vec4 col = (i == state.selectedIndex)
            ? glm::vec4{0.15f, 0.35f, 0.25f, 1.0f}
            : glm::vec4{0.08f, 0.09f, 0.13f, 1.0f};
        UIRect itemRect = {lx, iy, w, itemH};
        if (uiButton(win, items[i].c_str(), itemRect, col, "dropdown-item").clicked)
        {
            state.selectedIndex = i;
            state.open = false;
            UISys::gDropdownModalActive = false;
            result = i;
        }
        iy += itemH;
    }

    endScroll({lx, ly, w, listH}, contentH, scrollState);

    return result;
}
