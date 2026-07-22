#include "gui-bindings.h"
#include "gui-layout.h"
#include "ui-text-input.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <GLFW/glfw3.h>

std::string gSubmittedBinding;

GuiBindings& GuiBindings::instance()
{
    static GuiBindings bindings;
    return bindings;
}

void GuiBindings::set(const std::string& key, const std::string& value)
{
    mBindings[key] = value;
}

std::string GuiBindings::get(const std::string& key, const std::string& fallback) const
{
    auto it = mBindings.find(key);
    if (it != mBindings.end())
        return it->second;
    return fallback;
}

bool GuiBindings::has(const std::string& key) const
{
    return mBindings.find(key) != mBindings.end();
}

void GuiBindings::clear()
{
    mBindings.clear();
    mFocusedId.clear();
}

// ─── Global input dispatch ───────────────────────────────────────────────

// External globals for text-input states (defined in gui-element-render.cpp)
extern std::unordered_map<std::string, UITextInputState> gTextInputStates;

static GLFWwindow* gBindingsWindow = nullptr;

void guiBindingsSetWindow(GLFWwindow* win)
{
    gBindingsWindow = win;
}

void guiBindingsHandleChar(unsigned int codepoint)
{
    GuiBindings& b = GuiBindings::instance();
    const std::string& fid = b.focusedId();
    if (fid.empty())
        return;

    // Find the active UITextInputState and delegate to it
    auto it = gTextInputStates.find(fid);
    if (it != gTextInputStates.end())
    {
        UITextInputOptions opts;
        opts.maxLength = 200;
        // Sync binding if changed
        std::string bindingKey = b.get(fid + ".binding");
        if (!bindingKey.empty())
        {
            // Keep text state in sync with binding value (initial load)
            if (it->second.value.empty() && !b.get(bindingKey).empty())
                it->second.value = b.get(bindingKey);
        }
        uiTextInputHandleChar(it->second, codepoint, opts);
        // Sync back to binding
        if (!bindingKey.empty())
            b.set(bindingKey, it->second.value);
        return;
    }

    // Fallback: legacy direct binding append
    std::string bindingKey = b.get(fid + ".binding");
    if (bindingKey.empty())
        return;
    std::string val = b.get(bindingKey);
    if (val.size() < 200 && codepoint >= 32 && codepoint <= 126)
    {
        val += (char)codepoint;
        b.set(bindingKey, val);
    }
}

void guiBindingsHandleKey(int key, int action, int mods)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;
    GuiBindings& b = GuiBindings::instance();
    const std::string& fid = b.focusedId();
    if (fid.empty())
        return;

    // Find the active UITextInputState and delegate to it
    auto it = gTextInputStates.find(fid);
    if (it != gTextInputStates.end())
    {
        UITextInputOptions opts;
        opts.maxLength = 200;
        if (!uiTextInputHandleKey(gBindingsWindow, it->second, key, action, mods, opts))
        {
            // If the key wasn't consumed, check for escape/tab/enter for focus management
            if (key == GLFW_KEY_ESCAPE)
            {
                it->second.focused = false;
                b.clearFocus();
            }
            else if (key == GLFW_KEY_TAB)
            {
                it->second.focused = false;
                b.clearFocus();
            }
            else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
            {
                it->second.focused = false;
                b.clearFocus();
                if (it->second.submitOnEnter)
                {
                    std::string bindingKey = b.get(fid + ".binding");
                    if (!bindingKey.empty())
                        gSubmittedBinding = bindingKey;
                }
            }
        }
        // Sync back to binding
        std::string bindingKey = b.get(fid + ".binding");
        if (!bindingKey.empty())
            b.set(bindingKey, it->second.value);
        return;
    }

    // Fallback: legacy behavior
    std::string bindingKey = b.get(fid + ".binding");
    if (bindingKey.empty())
        return;
    if (key == GLFW_KEY_BACKSPACE)
    {
        std::string val = b.get(bindingKey);
        if (!val.empty())
        {
            val.pop_back();
            b.set(bindingKey, val);
        }
    }
    else if (key == GLFW_KEY_ESCAPE)
    {
        b.clearFocus();
    }
    else if (key == GLFW_KEY_TAB)
    {
        b.clearFocus();
    }
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
    {
        b.clearFocus();
    }
}
