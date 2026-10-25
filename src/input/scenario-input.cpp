#include "input/scenario-input.h"

#include "input/input-frame.h"

#include <GLFW/glfw3.h>

namespace {
bool gActive = false;
const InputFrame* gFrame = nullptr;
}

namespace ScenarioInput {

void setActive(bool active)
{
    gActive = active;
}

bool active()
{
    return gActive;
}

void setFrame(const InputFrame* frame)
{
    gFrame = frame;
}

const InputFrame* frame()
{
    return gFrame;
}

bool mouseButton(GLFWwindow* window, int button)
{
    if (gActive && gFrame)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            return gFrame->primaryHeld;
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            return gFrame->secondaryHeld;
        return false;
    }
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool keyDown(GLFWwindow* window, int key)
{
    if (gActive && gFrame)
    {
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        {
            const int slot = key == GLFW_KEY_0 ? 0 : key - GLFW_KEY_0;
            return (gFrame->slotHeldMask & (1u << slot)) != 0;
        }
        if (key == GLFW_KEY_Q)
            return gFrame->shieldHeld;
        if (key == GLFW_KEY_E)
            return gFrame->grabHeld;
        if (key == GLFW_KEY_SPACE)
            return gFrame->jump;
        return false;
    }
    return glfwGetKey(window, key) == GLFW_PRESS;
}

void clear()
{
    gActive = false;
    gFrame = nullptr;
}

} // namespace ScenarioInput
