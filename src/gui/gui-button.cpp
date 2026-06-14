// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-button.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * guiButton(args)
 *
 * this file DOES:
 * - handle hover
 * - handle click
 * - draw button background
 *
 * this file DOES NOT:
 * - draw text yet
 * - decide menu transitions
 */

#include "gui-button.h"
#include "gui-scale.h"
#include "gui/ui-system.h"
#include <cstdio>

bool guiButton(
    GLFWwindow* win,
    const char* text,
    float x,
    float y,
    float w,
    float h,
    glm::vec4 color,
    const char* id
)
{
    GuiScale s = guiGetScale(win);

    x *= s.scaleX;
    y *= s.scaleY;
    w *= s.scaleX;
    h *= s.scaleY;

    UIButtonState state = uiButton(win, text, {x, y, w, h}, color, id);
    return state.clicked;
}
