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
#include "gui-render/gui-rect.h"
#include "gui-scale.h"
#include <cstdio>

bool guiButton(
    GLFWwindow* win,
    const char* text,
    float x,
    float y,
    float w,
    float h,
    glm::vec4 color
)
{
    printf("[GUI BUTTON] begin: %s\n", text);

    GuiScale s = guiGetScale(win);

    x *= s.scaleX;
    y *= s.scaleY;
    w *= s.scaleX;
    h *= s.scaleY;

    printf("[GUI BUTTON] scaled rect: x=%f y=%f w=%f h=%f\n", x, y, w, h);

    // new mouse debug printing mar 14 2026 test 
    double mx = 0.0;
    double my = 0.0;

    glfwGetCursorPos(win, &mx, &my);

    int width, height;
    glfwGetWindowSize(win, &width, &height);

    my = height - my;

    printf("[GUI BUTTON] mouse: %f %f\n", mx, my);

    bool hover =
        mx >= x && mx <= x + w &&
        my >= y && my <= y + h;

    printf("[GUI BUTTON] hover: %s -> %d\n", text, (int)hover);

    glm::vec4 drawColor = color;
    if (hover)
    {
        drawColor *= 1.2f;
        printf("[GUI BUTTON] hover highlight\n");
    }

    drawGuiRect(x, y, w, h, drawColor);

    static bool prevMouseDown = false;
    bool mouseDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool clickEdge = mouseDown && !prevMouseDown;
    prevMouseDown = mouseDown;

    if (hover && clickEdge)
    {
        printf("[GUI BUTTON] CLICKED: %s\n", text);
        return true;
    }

    printf("[GUI BUTTON] end: %s\n", text);
    return false;
}