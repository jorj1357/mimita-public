#pragma once

#include "dev-types.h"
#include <string>
#include <vector>

struct GLFWwindow;

class DevOverlay {
public:
    static DevOverlay& instance();
    
    void init(GLFWwindow* window);
    void update();
    void render();
    void toggle() { mVisible = !mVisible; }
    bool visible() const { return mVisible; }
    void rebuildHelpText();
    
private:
    DevOverlay() = default;
    GLFWwindow* mWindow = nullptr;
    bool mVisible = true;
    bool mF12Prev = false;
    std::string mHelpText;
    std::vector<std::string> mHelpLines;
};
