#include "gui-bindings.h"
#include "gui-layout.h"
#include <cstdio>
#include <cstring>

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

void guiBindingsHandleChar(unsigned int codepoint)
{
    GuiBindings& b = GuiBindings::instance();
    const std::string& fid = b.focusedId();
    if (fid.empty())
        return;
    // Find the binding key for this input element
    std::string bindingKey = b.get(fid + ".binding");
    if (bindingKey.empty())
        return;
    // Append character to binding value (limit to 64 chars)
    std::string val = b.get(bindingKey);
    if (val.size() < 64 && codepoint >= 32 && codepoint <= 126)
    {
        val += (char)codepoint;
        b.set(bindingKey, val);
    }
}

void guiBindingsHandleKey(int key, int action)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;
    GuiBindings& b = GuiBindings::instance();
    const std::string& fid = b.focusedId();
    if (fid.empty())
        return;
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
        // Tab moves to next input — handled by menu if desired, otherwise clear focus
        b.clearFocus();
    }
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
    {
        // Enter submits — clear focus so it can be processed as button click too
        b.clearFocus();
    }
}
