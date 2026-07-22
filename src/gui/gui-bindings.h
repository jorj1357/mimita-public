#pragma once
#include <string>
#include <unordered_map>

class GuiBindings {
public:
    static GuiBindings& instance();

    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key, const std::string& fallback = "") const;
    bool has(const std::string& key) const;
    void clear();

    const std::unordered_map<std::string, std::string>& all() const { return mBindings; }

    // Focused text input ID (for routing keyboard events)
    void setFocusedId(const std::string& id) { mFocusedId = id; }
    const std::string& focusedId() const { return mFocusedId; }
    bool hasFocus(const std::string& id) const { return mFocusedId == id; }
    void clearFocus() { mFocusedId.clear(); }

private:
    GuiBindings() = default;
    std::unordered_map<std::string, std::string> mBindings;
    std::string mFocusedId;
};

// Global handlers dispatched from main loop
void guiBindingsSetWindow(GLFWwindow* win);
void guiBindingsHandleChar(unsigned int codepoint);
void guiBindingsHandleKey(int key, int action, int mods);

// Set when Enter is pressed on a text input with submitOnEnter=true.
// The menu checks this after draw to trigger the submit action.
// Binding string is the GuiBindings key (e.g. "join.code").
// Cleared after each frame by the consumer.
extern std::string gSubmittedBinding;
