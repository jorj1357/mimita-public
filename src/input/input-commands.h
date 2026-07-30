#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>

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
    DownDash,
    Count
};

struct InputCommandState {
    bool pressed = false;
    bool held = false;
    bool released = false;
    float value = 0.0f;
};

// A buffered action event persists briefly so quick presses aren't lost
// between frames or during frame drops.
struct BufferedAction {
    bool active = false;
    double pressTime = 0.0;
    double bufferSeconds = 0.15; // 150ms buffer
};

class InputCommandSystem {
public:
    static InputCommandSystem& instance();

    void init(GLFWwindow* window);
    void update(float dt);
    void pulseAction(const std::string& actionName);
    void setKeyboardEnabled(bool enabled);

    // GLFW key callback — captures every key event, not just polling snapshots
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Query command state
    const InputCommandState& getState(InputAction action) const;
    const InputCommandState& getState(const std::string& actionName) const;

    // Get combined movement vector from commands
    glm::vec2 getMoveVector() const;
    float getMovementHoldDuration() const;
    bool isJumpHeld() const;
    bool isDashPressed() const;
    bool isGroundReturnPressed() const;
    bool isDownDashPressed() const;
    bool isFreezeHeld() const;

    // Buffered action queries — return true if action was pressed within buffer window
    bool consumeBufferedJump();
    bool consumeBufferedDash();
    bool consumeBufferedDownDash();

    // Debug
    bool inputDebug() const { return mInputDebug; }
    void setInputDebug(bool enabled) { mInputDebug = enabled; }
    void drawInputDebug();

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
    bool mFocused = true;

    // Per-key callback timestamps (for accurate edge detection)
    double mKeyPressTime[512] = {0.0};
    double mKeyReleaseTime[512] = {0.0};
    double mCurrentTime = 0.0;

    // Buffered actions — survive frame drops, focus loss, etc.
    BufferedAction mJumpBuffer;
    BufferedAction mDashBuffer;
    BufferedAction mDownDashBuffer;

    // Debug
    bool mInputDebug = false;
};

const char* inputActionToString(InputAction action);
InputAction stringToInputAction(const std::string& str);
