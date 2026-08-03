// 08 03 2026, 15 00
/* purpose
* Implements the gameplay mouse-lock state.
* Tracks a persistent lock flag and applies GLFW_CURSOR_DISABLED / NORMAL to
* the window so other subsystems (combat, net, overlays) can query the source
* of truth instead of each deciding cursor behavior independently.
* Does NOT fire weapons, render UI, or read the game state.
*/
#include "input/mouse-lock.h"

namespace MouseLock {

namespace {
bool gGameplayMouseLocked = true;
}

bool locked()
{
    return gGameplayMouseLocked;
}

void set(GLFWwindow* win, bool on)
{
    gGameplayMouseLocked = on;
    if (win)
        glfwSetInputMode(win, GLFW_CURSOR,
            on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void toggle(GLFWwindow* win)
{
    set(win, !gGameplayMouseLocked);
}

} // namespace MouseLock
