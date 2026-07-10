#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include "devtools/terminal.h"

void registerCursorCommands()
{
    Terminal::instance().registerCommand({
        "uc", "Unlock cursor — free the cursor to click UI buttons",
        "uc",
        [](const std::vector<std::string>&) {
            GLFWwindow* win = Terminal::instance().window();
            if (win) {
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                Terminal::instance().addLog("[CURSOR] unlocked");
            }
        }
    });

    Terminal::instance().registerCommand({
        "lc", "Lock cursor — return to normal weapon-aiming mode",
        "lc",
        [](const std::vector<std::string>&) {
            GLFWwindow* win = Terminal::instance().window();
            if (win) {
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                Terminal::instance().addLog("[CURSOR] locked");
            }
        }
    });
}
