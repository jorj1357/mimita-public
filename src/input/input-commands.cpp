#include "input-commands.h"
#include "gui/ui-system.h"
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "devtools/terminal.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

InputCommandSystem& InputCommandSystem::instance() {
    static InputCommandSystem sInstance;
    return sInstance;
}

void InputCommandSystem::init(GLFWwindow* window) {
    if (mWindow && !mActionToKey.empty()) {
        mWindow = window;
        return;
    }
    mWindow = window;

    // Install GLFW key callback for reliable edge capture
    glfwSetKeyCallback(window, keyCallback);

    mFocused = true;
    setupDefaultBinds();
    printf("[INPUT COMMANDS] Initialized with callback\n");
}

// GLFW key callback — fires on every press/release, not just polling frames.
// This callback overwrites glfwSetKeyCallback from main.cpp, so it MUST forward
// to Terminal, sign-in, server-info, and online-menu key handlers.
void InputCommandSystem::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    auto& sys = instance();

    if (key >= 0 && key < 512) {
        double now = glfwGetTime();
        if (action == GLFW_PRESS) {
            sys.mKeyPressTime[key] = now;
        } else if (action == GLFW_RELEASE) {
            sys.mKeyReleaseTime[key] = now;
        }
    }

    // Forward to terminal and menu handlers (these were previously called from
    // the main.cpp key callback before it was overwritten)
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        Terminal::instance().handleKey(key, mods);
        signInMenuHandleKey(key, action);
        serverInfoMenuHandleKey(key, action);
        onlineMenuHandleKey(key, action);
    }
}

void InputCommandSystem::setupDefaultBinds() {
    mActionToKey.clear();
    mKeyToAction.clear();
    mActionStates.clear();

    bindAction("walkforward", GLFW_KEY_W);
    bindAction("walkback", GLFW_KEY_S);
    bindAction("walkleft", GLFW_KEY_A);
    bindAction("walkright", GLFW_KEY_D);
    bindAction("jump", GLFW_KEY_SPACE);
    bindAction("dash", GLFW_KEY_LEFT_SHIFT);
    bindAction("down_dash", GLFW_KEY_Q);
    bindAction("freeze", GLFW_KEY_E);
    bindAction("reload", GLFW_KEY_R);

    mActionStates["walkforward"] = {};
    mActionStates["walkback"] = {};
    mActionStates["walkleft"] = {};
    mActionStates["walkright"] = {};
    mActionStates["jump"] = {};
    mActionStates["dash"] = {};
    mActionStates["down_dash"] = {};
    mActionStates["freeze"] = {};
    mActionStates["reload"] = {};
}

void InputCommandSystem::update(float dt) {
    (void)dt;
    if (!mWindow) return;

    mCurrentTime = glfwGetTime();

    // Track focus: when unfocused, held keys stay as they were (don't force-release)
    mFocused = glfwGetWindowAttrib(mWindow, GLFW_FOCUSED) != 0;

    for (auto& [actionName, state] : mActionStates) {
        auto it = mActionToKey.find(actionName);
        if (it == mActionToKey.end()) continue;

        int key = it->second;

        // Use callback timestamps for press/release edges instead of polling,
        // fall back to glfwGetKey for held state (which works across frames)
        bool currDown = mKeyboardEnabled && glfwGetKey(mWindow, key) == GLFW_PRESS;
        bool prevDown = mPrevKeyStates[key];

        // Callback-based press detection: a press occurred if the callback
        // timestamp is more recent than the last poll timestamp
        double lastPollTime = mCurrentTime - dt;
        bool callbackPress = mKeyPressTime[key] > lastPollTime && mKeyPressTime[key] > mKeyReleaseTime[key];
        bool callbackRelease = mKeyReleaseTime[key] > lastPollTime && mKeyReleaseTime[key] > mKeyPressTime[key];

        // Prefer callback edges, fall back to polling edges for safety
        bool edgePressed = callbackPress || (currDown && !prevDown);
        bool edgeReleased = callbackRelease || (!currDown && prevDown);

        state.pressed = edgePressed;
        state.released = edgeReleased;
        state.held = currDown;
        state.value = currDown ? 1.0f : 0.0f;

        // Pending pulses (from terminal commands)
        auto pulse = mPendingPulses.find(actionName);
        if (pulse != mPendingPulses.end() && pulse->second) {
            state.pressed = true;
            state.held = true;
            state.value = 1.0f;
            pulse->second = false;
        }

        mPrevKeyStates[key] = currDown;
    }

    // Fill input buffers for critical movement actions
    // A buffered action persists for bufferSeconds so quick presses survive frame drops.
    if (getState("jump").pressed) {
        mJumpBuffer.active = true;
        mJumpBuffer.pressTime = mCurrentTime;
    }
    if (getState("dash").pressed) {
        mDashBuffer.active = true;
        mDashBuffer.pressTime = mCurrentTime;
    }
    if (getState("down_dash").pressed) {
        mDownDashBuffer.active = true;
        mDownDashBuffer.pressTime = mCurrentTime;
    }

    // Expire stale buffered actions
    if (mJumpBuffer.active && (mCurrentTime - mJumpBuffer.pressTime) > mJumpBuffer.bufferSeconds)
        mJumpBuffer.active = false;
    if (mDashBuffer.active && (mCurrentTime - mDashBuffer.pressTime) > mDashBuffer.bufferSeconds)
        mDashBuffer.active = false;
    if (mDownDashBuffer.active && (mCurrentTime - mDownDashBuffer.pressTime) > mDownDashBuffer.bufferSeconds)
        mDownDashBuffer.active = false;
}

bool InputCommandSystem::consumeBufferedJump() {
    if (mJumpBuffer.active) {
        mJumpBuffer.active = false;
        return true;
    }
    return getState("jump").pressed;
}

bool InputCommandSystem::consumeBufferedDash() {
    if (mDashBuffer.active) {
        mDashBuffer.active = false;
        return true;
    }
    return getState("dash").pressed;
}

bool InputCommandSystem::consumeBufferedDownDash() {
    if (mDownDashBuffer.active) {
        mDownDashBuffer.active = false;
        return true;
    }
    return getState("down_dash").pressed;
}

void InputCommandSystem::pulseAction(const std::string& actionName) {
    if (mActionStates.find(actionName) == mActionStates.end())
        mActionStates[actionName] = {};
    mPendingPulses[actionName] = true;
}

const InputCommandState& InputCommandSystem::getState(InputAction action) const {
    return getState(inputActionToString(action));
}

const InputCommandState& InputCommandSystem::getState(const std::string& actionName) const {
    static InputCommandState empty;
    auto it = mActionStates.find(actionName);
    return it != mActionStates.end() ? it->second : empty;
}

glm::vec2 InputCommandSystem::getMoveVector() const {
    return {0.0f, 0.0f};
}

bool InputCommandSystem::isJumpHeld() const {
    return getState("jump").held;
}

bool InputCommandSystem::isDashPressed() const {
    return getState("dash").pressed || mDashBuffer.active;
}

bool InputCommandSystem::isGroundReturnPressed() const {
    return getState("ground_return").pressed;
}

bool InputCommandSystem::isDownDashPressed() const {
    return getState("down_dash").pressed || mDownDashBuffer.active;
}

bool InputCommandSystem::isFreezeHeld() const {
    return getState("freeze").held;
}

void InputCommandSystem::drawInputDebug()
{
    if (!mInputDebug) return;

    float x = 10.0f;
    float y = 80.0f;
    float lineH = 16.0f;
    auto& cs = GuiCoordinateSystem::instance();

    auto drawLine = [&](const char* text, glm::vec4 color = {1,1,1,1}) {
        uiDrawText(text, x, y, 0.26f, color);
        y += lineH;
    };

    char buf[256];
    y = 80.0f;

    // Draw key states for movement-critical keys
    struct KeyDebug {
        const char* name;
        int glfwKey;
    };
    KeyDebug keys[] = {
        {"W", GLFW_KEY_W}, {"A", GLFW_KEY_A}, {"S", GLFW_KEY_S}, {"D", GLFW_KEY_D},
        {"Space", GLFW_KEY_SPACE}, {"Shift", GLFW_KEY_LEFT_SHIFT},
        {"Q", GLFW_KEY_Q}, {"E", GLFW_KEY_E}, {"R", GLFW_KEY_R}
    };

    uiDrawRect({(float)x, (float)y - 4, 380.0f, (float)(sizeof(keys)/sizeof(keys[0]) * lineH + lineH + 20)},
               {0.0f, 0.0f, 0.0f, 0.75f}, "input-debug-bg");

    snprintf(buf, sizeof(buf), "Frame: %d  Focused: %s",
             (int)(mCurrentTime * 60.0), mFocused ? "yes" : "no");
    drawLine(buf, {0.3f, 1.0f, 0.5f, 1});

    for (auto& k : keys) {
        bool held = glfwGetKey(mWindow, k.glfwKey) == GLFW_PRESS;
        bool pressed = mKeyPressTime[k.glfwKey] > mKeyReleaseTime[k.glfwKey] &&
                       (mCurrentTime - mKeyPressTime[k.glfwKey]) < 0.1;
        snprintf(buf, sizeof(buf), "%s: held=%s  pressed=%s  callback=%.3fs",
                 k.name, held ? "YES" : "no", pressed ? "YES" : "no",
                 mKeyPressTime[k.glfwKey]);
        glm::vec4 col = held ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(0.7f,0.7f,0.7f,1);
        drawLine(buf, col);
    }

    // Buffer states
    y += 4.0f;
    snprintf(buf, sizeof(buf), "JumpBuffer: %s  (%.0fms)",
             mJumpBuffer.active ? "ACTIVE" : "idle",
             mJumpBuffer.active ? (mCurrentTime - mJumpBuffer.pressTime) * 1000.0 : 0.0);
    drawLine(buf, mJumpBuffer.active ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(0.7f,0.7f,0.7f,1));

    snprintf(buf, sizeof(buf), "DashBuffer: %s  (%.0fms)",
             mDashBuffer.active ? "ACTIVE" : "idle",
             mDashBuffer.active ? (mCurrentTime - mDashBuffer.pressTime) * 1000.0 : 0.0);
    drawLine(buf, mDashBuffer.active ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(0.7f,0.7f,0.7f,1));

    snprintf(buf, sizeof(buf), "DownDashBuffer: %s  (%.0fms)",
             mDownDashBuffer.active ? "ACTIVE" : "idle",
             mDownDashBuffer.active ? (mCurrentTime - mDownDashBuffer.pressTime) * 1000.0 : 0.0);
    drawLine(buf, mDownDashBuffer.active ? glm::vec4(0.3f,1,0.3f,1) : glm::vec4(0.7f,0.7f,0.7f,1));
}

std::string glfwToKeyName(int key);

void InputCommandSystem::bindAction(const std::string& actionName, int key) {
    auto oldActionIt = mKeyToAction.find(key);
    if (oldActionIt != mKeyToAction.end()) {
        mActionToKey.erase(oldActionIt->second);
    }

    auto oldKeyIt = mActionToKey.find(actionName);
    if (oldKeyIt != mActionToKey.end()) {
        mKeyToAction.erase(oldKeyIt->second);
    }

    mActionToKey[actionName] = key;
    mKeyToAction[key] = actionName;

    if (mActionStates.find(actionName) == mActionStates.end()) {
        mActionStates[actionName] = {};
    }

    printf("[INPUT COMMANDS] Bound %s = %s\n", actionName.c_str(), glfwToKeyName(key).c_str());
}

void InputCommandSystem::bindAction(InputAction action, int key) {
    bindAction(inputActionToString(action), key);
}

int InputCommandSystem::getKeyForAction(InputAction action) const {
    return getKeyForAction(inputActionToString(action));
}

int InputCommandSystem::getKeyForAction(const std::string& actionName) const {
    auto it = mActionToKey.find(actionName);
    return it != mActionToKey.end() ? it->second : -1;
}

const char* inputActionToString(InputAction action) {
    switch (action) {
        case InputAction::MoveForward: return "walkforward";
        case InputAction::MoveBack: return "walkback";
        case InputAction::MoveLeft: return "walkleft";
        case InputAction::MoveRight: return "walkright";
        case InputAction::Jump: return "jump";
        case InputAction::Dash: return "dash";
        case InputAction::GroundReturn: return "ground_return";
        case InputAction::Freeze: return "freeze";
        case InputAction::DownDash: return "down_dash";
        default: return "unknown";
    }
}

InputAction stringToInputAction(const std::string& str) {
    if (str == "walkforward" || str == "move_forward") return InputAction::MoveForward;
    if (str == "walkback" || str == "move_back") return InputAction::MoveBack;
    if (str == "walkleft" || str == "move_left") return InputAction::MoveLeft;
    if (str == "walkright" || str == "move_right") return InputAction::MoveRight;
    if (str == "jump") return InputAction::Jump;
    if (str == "dash") return InputAction::Dash;
    if (str == "ground_return") return InputAction::GroundReturn;
    if (str == "down_dash") return InputAction::DownDash;
    if (str == "freeze") return InputAction::Freeze;
    return InputAction::Count;
}
