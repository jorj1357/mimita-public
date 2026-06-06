#include "dev-overlay.h"
#include "dev-config.h"
#include "gui/ui-system.h"
#include <GLFW/glfw3.h>
#include <sstream>
#include <cstdio>

DevOverlay& DevOverlay::instance() {
    static DevOverlay sInstance;
    return sInstance;
}

void DevOverlay::init(GLFWwindow* window) {
    mWindow = window;
    rebuildHelpText();
    printf("[DEV OVERLAY] Initialized\n");
}

void DevOverlay::rebuildHelpText() {
    const auto& bindings = DevConfig::instance().bindings();
    mHelpLines.clear();
    
    for (const auto& b : bindings) {
        char line[128];
        snprintf(line, sizeof(line), "%s = %s", 
                 [b]() -> const char* {
                     switch (b.key) {
                         case GLFW_KEY_F1: return "F1";
                         case GLFW_KEY_F2: return "F2";
                         case GLFW_KEY_F3: return "F3";
                         case GLFW_KEY_F4: return "F4";
                         case GLFW_KEY_F5: return "F5";
                         case GLFW_KEY_F6: return "F6";
                         case GLFW_KEY_F7: return "F7";
                         case GLFW_KEY_F8: return "F8";
                         case GLFW_KEY_F9: return "F9";
                         case GLFW_KEY_F10: return "F10";
                         case GLFW_KEY_F11: return "F11";
                         case GLFW_KEY_F12: return "F12";
                         case GLFW_KEY_ESCAPE: return "ESC";
                         case GLFW_KEY_TAB: return "TAB";
                         case GLFW_KEY_SPACE: return "SPACE";
                         case GLFW_KEY_ENTER: return "ENTER";
                         case GLFW_KEY_GRAVE_ACCENT: return "`";
                         default:
                             if (b.key >= GLFW_KEY_A && b.key <= GLFW_KEY_Z)
                                 return (std::string(1, char('A' + (b.key - GLFW_KEY_A)))).c_str();
                             if (b.key >= GLFW_KEY_0 && b.key <= GLFW_KEY_9)
                                 return (std::string(1, char('0' + (b.key - GLFW_KEY_0)))).c_str();
                             return "?";
                     }
                 }(),
                 b.description.c_str());
        mHelpLines.push_back(line);
    }
    
    mHelpLines.push_back("F12 = Reload config");
    printf("[DEV OVERLAY] Rebuilt help text (%zu lines)\n", mHelpLines.size());
}

void DevOverlay::update() {
    if (!mWindow) return;
    
    bool f12Down = glfwGetKey(mWindow, GLFW_KEY_F12) == GLFW_PRESS;
    if (f12Down && !mF12Prev) {
        DevConfig::instance().reload();
        rebuildHelpText();
    }
    mF12Prev = f12Down;
}

void DevOverlay::render() {
    if (!mVisible || !mWindow) return;
    
    uiBeginFrame(mWindow, "dev-overlay");
    
    const float lineHeight = 18.0f;
    const float padding = 10.0f;
    const float width = 380.0f;
    const float height = padding * 2 + 24.0f + mHelpLines.size() * lineHeight;
    
    uiDrawRect({padding, padding, width, height}, {0.0f, 0.0f, 0.0f, 0.75f}, "dev-overlay-bg");
    uiDrawRectOutline({padding, padding, width, height}, {1.0f, 1.0f, 0.5f, 0.8f}, "dev-overlay-border");
    
    uiDrawText("DEV CONTROLS", padding + 10, padding + 8, 0.38f, {1.0f, 1.0f, 0.5f, 1.0f});
    
    float y = padding + 30;
    for (const auto& line : mHelpLines) {
        uiDrawText(line.c_str(), padding + 10, y, 0.30f, {0.85f, 0.95f, 1.0f, 1.0f});
        y += lineHeight;
    }
    
    uiEndFrame();
}
