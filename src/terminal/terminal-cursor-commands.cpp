// 08 03 2026, 15 00
/* purpose
* Registers the uc / lc terminal commands for cursor lock control.
* Both route through the shared MouseLock state so the gameplay lock flag,
* the cursor mode, and the on-screen indicator stay consistent.
* Does NOT own the gameplay mouse-lock state or render UI.
*/
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "input/mouse-lock.h"

void registerCursorCommands()
{
    Terminal::instance().registerCommand({
        "uc", "Unlock cursor - free the cursor to click UI buttons",
        "uc",
        [](const std::vector<std::string>&) {
            GLFWwindow* win = Terminal::instance().window();
            if (win) {
                MouseLock::set(win, false);
                Terminal::instance().addLog("[CURSOR] unlocked");
            }
        }
    });

    Terminal::instance().registerCommand({
        "lc", "Lock cursor - return to normal weapon-aiming mode",
        "lc",
        [](const std::vector<std::string>&) {
            GLFWwindow* win = Terminal::instance().window();
            if (win) {
                MouseLock::set(win, true);
                Terminal::instance().addLog("[CURSOR] locked");
            }
        }
    });
}
