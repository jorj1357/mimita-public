#pragma once

#include <string>
#include <vector>

struct GLFWwindow;

class DevOverlay {
public:
    static DevOverlay& instance();
    
    void init(GLFWwindow* window);
    void update(float dt);
    void render();
    
    // Show temporary notification (auto-fades)
    void showNotification(const std::string& message, float duration = 3.0f);
    
private:
    DevOverlay() = default;
    GLFWwindow* mWindow = nullptr;
    
    struct Notification {
        std::string message;
        float timer = 0.0f;
        float duration = 3.0f;
    };
    std::vector<Notification> mNotifications;
};
