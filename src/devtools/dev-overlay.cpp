#include "dev-overlay.h"
#include "gui/ui-system.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <algorithm>

DevOverlay& DevOverlay::instance() {
    static DevOverlay sInstance;
    return sInstance;
}

void DevOverlay::init(GLFWwindow* window) {
    mWindow = window;
    printf("[DEV OVERLAY] Initialized\n");
}

void DevOverlay::showNotification(const std::string& message, float duration) {
    Notification n;
    n.message = message;
    n.timer = duration;
    n.duration = duration;
    mNotifications.push_back(n);
    printf("[DEV OVERLAY] Notification: %s\n", message.c_str());
}

void DevOverlay::update(float dt) {
    if (!mWindow) return;
    
    for (auto& n : mNotifications) {
        n.timer -= dt;
    }
    mNotifications.erase(
        std::remove_if(mNotifications.begin(), mNotifications.end(),
            [](const Notification& n) { return n.timer <= 0.0f; }),
        mNotifications.end()
    );
}

void DevOverlay::render() {
    if (!mWindow || mNotifications.empty()) return;
    
    uiBeginFrame(mWindow, "dev-overlay-notifications");
    
    const float lineHeight = 24.0f;
    const float padding = 12.0f;
    const float width = 420.0f;
    const float startY = 60.0f;
    
    float y = startY;
    for (const auto& n : mNotifications) {
        float alpha = std::min(1.0f, n.timer / 0.5f); // Fade out in last 0.5s
        float bgAlpha = 0.85f * alpha;
        
        float textWidth = n.message.size() * 10.0f; // approximate
        float height = lineHeight + padding * 0.5f;
        
        uiDrawRect({padding, y, width, height}, {0.0f, 0.0f, 0.0f, bgAlpha}, "dev-notif-bg");
        uiDrawRectOutline({padding, y, width, height}, {1.0f, 1.0f, 0.5f, 0.8f * alpha}, "dev-notif-border");
        
        uiDrawText(n.message.c_str(), padding + 10, y + 6, 0.35f, {1.0f, 1.0f, 0.5f, alpha});
        
        y += height + 6.0f;
    }
    
    uiEndFrame();
}