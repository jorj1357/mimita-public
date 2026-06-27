#include "avatar-editor-dropdown.h"
#include "avatar-editor-scroll.h"

#include <algorithm>
#include <cstdio>

#include "../gui/ui-system.h"
#include "../gui/gui-coord.h"

int drawDropdown(GLFWwindow* win, DropdownState& state,
                 float x, float y, float w, float itemH,
                 const char* label, const std::vector<std::string>& items)
{
    int result = -1;

    // Label
    if (label)
        uiDrawText(label, uiScaleX(x), uiScaleY(y - itemH - 4.0f), 0.32f, {0.7f, 0.8f, 0.9f, 1.0f});

    // Selected item display
    std::string displayText = state.selectedIndex >= 0 && state.selectedIndex < (int)items.size()
        ? items[state.selectedIndex]
        : "None";
    glm::vec4 bg = state.open ? glm::vec4{0.15f, 0.2f, 0.3f, 1.0f} : glm::vec4{0.1f, 0.12f, 0.18f, 1.0f};
    UIRect headerRect = {x, y, w, itemH};
    UIButtonState btn = uiButton(win, displayText.c_str(), headerRect, bg, "dropdown-header");
    if (btn.clicked)
        state.open = !state.open;

    if (!state.open)
        return -1;

    // Open dropdown list
    float listH = std::min((float)items.size() * itemH, 200.0f);
    UIRect listArea = {x, y + itemH + 2.0f, w, listH};
    UIRect listAreaScreen = {uiScaleX(listArea.x), uiScaleY(listArea.y),
                              uiScaleX(listArea.w), uiScaleY(listArea.h)};
    uiDrawRect(listAreaScreen, {0.06f, 0.07f, 0.1f, 1.0f}, "dropdown-list-bg");

    float contentH = (float)items.size() * itemH;
    ScrollState scrollState;
    scrollState.offset = state.scrollOffset;

    beginScroll(win, listArea, contentH, scrollState);
    state.scrollOffset = scrollState.offset;

    float iy = listArea.y;
    for (int i = 0; i < (int)items.size(); ++i)
    {
        glm::vec4 col = (i == state.selectedIndex)
            ? glm::vec4{0.15f, 0.35f, 0.25f, 1.0f}
            : glm::vec4{0.08f, 0.09f, 0.13f, 1.0f};
        UIRect itemRect = {listArea.x, iy, w, itemH};
        if (uiButton(win, items[i].c_str(), itemRect, col, "dropdown-item").clicked)
        {
            state.selectedIndex = i;
            state.open = false;
            result = i;
        }
        iy += itemH;
    }

    endScroll(listArea, contentH, scrollState);

    return result;
}
