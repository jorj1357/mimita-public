// 08 03 2026, 15 00
/* purpose
* Implements the gameplay mouse-lock state.
* Tracks a persistent lock flag and applies GLFW_CURSOR_DISABLED / NORMAL to
* the window so other subsystems (combat, net, overlays) can query the source
* of truth instead of each deciding cursor behavior independently.
* Does NOT fire weapons, render UI, or read the game state.
*/
#include "input/mouse-lock.h"
#include "debug/structured-log.h"

#include <nlohmann/json.hpp>

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
    if (!win) return;

    const int requestedMode = on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    const int modeBefore = glfwGetInputMode(win, GLFW_CURSOR);
    glfwSetInputMode(win, GLFW_CURSOR, requestedMode);
    const int modeAfter = glfwGetInputMode(win, GLFW_CURSOR);

    auto& logger = StructuredLogger::instance();
    if (logger.shouldLog(StructuredCategory::Gui, StructuredLevel::Important)) {
        nlohmann::json fields = {
            {"locked", gGameplayMouseLocked},
            {"requested_mode", requestedMode},
            {"mode_before", modeBefore},
            {"mode_after", modeAfter},
            {"mode_applied", modeAfter == requestedMode}
        };
        logger.writeEvent(StructuredCategory::Gui, StructuredLevel::Important,
                          "cursor.mouse_lock_set", "CURSOR_MOUSE_LOCK",
                          "MouseLock::set applied a gameplay cursor request", 0,
                          fields, __FILE__, __LINE__, __FUNCTION__);
    }
}

void toggle(GLFWwindow* win)
{
    set(win, !gGameplayMouseLocked);
}

} // namespace MouseLock
