#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

struct GLFWwindow;

enum class InputAction {
    MoveForward,
    MoveBack,
    MoveLeft,
    MoveRight,
    Jump,
    Dash,
    GroundReturn,
    Freeze,
    Count
};

struct InputCommandState {
    bool pressed = false;
    bool held = false;
    bool released = false;
    float value = 0.0f; // For analog inputs
};

class InputCommandSystem {
public:
    static InputCommandSystem& instance();
    
    void init(GLFWwindow* window);
    void update(float dt);
    void pulseAction(const std::string& actionName);
    void setKeyboardEnabled(bool enabled) { mKeyboardEnabled = enabled; }
    
    // Query command state
    const InputCommandState& getState(InputAction action) const;
    const InputCommandState& getState(const std::string& actionName) const;
    
    // Get combined movement vector from commands
    glm::vec2 getMoveVector() const;
    bool isJumpHeld() const;
    bool isDashPressed() const;
    bool isGroundReturnPressed() const;
    bool isFreezeHeld() const;
    
    // Bind action to key (runtime)
    void bindAction(const std::string& actionName, int key);
    void bindAction(InputAction action, int key);
    
    // Get key for action
    int getKeyForAction(InputAction action) const;
    int getKeyForAction(const std::string& actionName) const;
    
    // Load/save binds from JSON
    void loadBinds(const std::string& path);
    void saveBinds(const std::string& path) const;
    
    // Default binds
    void setupDefaultBinds();
    
private:
    InputCommandSystem() = default;
    
    GLFWwindow* mWindow = nullptr;
    std::unordered_map<std::string, InputCommandState> mActionStates;
    std::unordered_map<std::string, int> mActionToKey;
    std::unordered_map<int, std::string> mKeyToAction;
    bool mPrevKeyStates[512] = {false};
    std::unordered_map<std::string, bool> mPendingPulses;
    bool mKeyboardEnabled = true;
};

const char* inputActionToString(InputAction action);
InputAction stringToInputAction(const std::string& str);
