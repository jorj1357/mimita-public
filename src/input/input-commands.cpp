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

static int keyNameToGlfw(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "F1") return GLFW_KEY_F1;
    if (upper == "F2") return GLFW_KEY_F2;
    if (upper == "F3") return GLFW_KEY_F3;
    if (upper == "F4") return GLFW_KEY_F4;
    if (upper == "F5") return GLFW_KEY_F5;
    if (upper == "F6") return GLFW_KEY_F6;
    if (upper == "F7") return GLFW_KEY_F7;
    if (upper == "F8") return GLFW_KEY_F8;
    if (upper == "F9") return GLFW_KEY_F9;
    if (upper == "F10") return GLFW_KEY_F10;
    if (upper == "F11") return GLFW_KEY_F11;
    if (upper == "F12") return GLFW_KEY_F12;
    if (upper == "ESCAPE" || upper == "ESC") return GLFW_KEY_ESCAPE;
    if (upper == "TAB") return GLFW_KEY_TAB;
    if (upper == "SPACE") return GLFW_KEY_SPACE;
    if (upper == "ENTER") return GLFW_KEY_ENTER;
    if (upper == "BACKSPACE") return GLFW_KEY_BACKSPACE;
    if (upper == "DELETE" || upper == "DEL") return GLFW_KEY_DELETE;
    if (upper == "INSERT" || upper == "INS") return GLFW_KEY_INSERT;
    if (upper == "HOME") return GLFW_KEY_HOME;
    if (upper == "END") return GLFW_KEY_END;
    if (upper == "PAGEUP" || upper == "PGUP") return GLFW_KEY_PAGE_UP;
    if (upper == "PAGEDOWN" || upper == "PGDOWN") return GLFW_KEY_PAGE_DOWN;
    if (upper == "UP") return GLFW_KEY_UP;
    if (upper == "DOWN") return GLFW_KEY_DOWN;
    if (upper == "LEFT") return GLFW_KEY_LEFT;
    if (upper == "RIGHT") return GLFW_KEY_RIGHT;
    if (upper == "GRAVE_ACCENT" || upper == "`" || upper == "~") return GLFW_KEY_GRAVE_ACCENT;
    if (upper == "LEFT_SHIFT") return GLFW_KEY_LEFT_SHIFT;
    if (upper == "RIGHT_SHIFT") return GLFW_KEY_RIGHT_SHIFT;
    if (upper == "LEFT_CONTROL") return GLFW_KEY_LEFT_CONTROL;
    if (upper == "RIGHT_CONTROL") return GLFW_KEY_RIGHT_CONTROL;
    if (upper == "LEFT_ALT") return GLFW_KEY_LEFT_ALT;
    if (upper == "RIGHT_ALT") return GLFW_KEY_RIGHT_ALT;
    if (upper.size() == 1) {
        char c = upper[0];
        if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
    }
    if (upper.rfind("KEY_", 0) == 0) {
        std::string keyPart = upper.substr(4);
        if (keyPart.size() == 1) {
            char c = keyPart[0];
            if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
            if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
        }
    }
    return -1;
}

static std::string glfwToKeyName(int key) {
    switch (key) {
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
        case GLFW_KEY_ESCAPE: return "ESCAPE";
        case GLFW_KEY_TAB: return "TAB";
        case GLFW_KEY_SPACE: return "SPACE";
        case GLFW_KEY_ENTER: return "ENTER";
        case GLFW_KEY_BACKSPACE: return "BACKSPACE";
        case GLFW_KEY_DELETE: return "DELETE";
        case GLFW_KEY_INSERT: return "INSERT";
        case GLFW_KEY_HOME: return "HOME";
        case GLFW_KEY_END: return "END";
        case GLFW_KEY_PAGE_UP: return "PAGEUP";
        case GLFW_KEY_PAGE_DOWN: return "PAGEDOWN";
        case GLFW_KEY_UP: return "UP";
        case GLFW_KEY_DOWN: return "DOWN";
        case GLFW_KEY_LEFT: return "LEFT";
        case GLFW_KEY_RIGHT: return "RIGHT";
        case GLFW_KEY_GRAVE_ACCENT: return "GRAVE_ACCENT";
        case GLFW_KEY_LEFT_SHIFT: return "LEFT_SHIFT";
        case GLFW_KEY_RIGHT_SHIFT: return "RIGHT_SHIFT";
        case GLFW_KEY_LEFT_CONTROL: return "LEFT_CONTROL";
        case GLFW_KEY_RIGHT_CONTROL: return "RIGHT_CONTROL";
        case GLFW_KEY_LEFT_ALT: return "LEFT_ALT";
        case GLFW_KEY_RIGHT_ALT: return "RIGHT_ALT";
        default:
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
                return std::string(1, char('A' + (key - GLFW_KEY_A)));
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
                return std::string(1, char('0' + (key - GLFW_KEY_0)));
            return "UNKNOWN";
    }
}

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

void InputCommandSystem::loadBinds(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[INPUT COMMANDS] Could not open %s, using defaults\n", path.c_str());
        return;
    }

    try {
        json j;
        file >> j;

        auto binds = j.value("binds", json::object());
        for (auto& [action, keyStr] : binds.items()) {
            int key = keyNameToGlfw(keyStr.get<std::string>());
            if (key != -1) {
                bindAction(action, key);
            }
        }

        printf("[INPUT COMMANDS] Loaded binds from %s\n", path.c_str());
    } catch (const std::exception& e) {
        printf("[INPUT COMMANDS] Error loading %s: %s\n", path.c_str(), e.what());
    }
}

void InputCommandSystem::saveBinds(const std::string& path) const {
    json j = json::object();
    {
        std::ifstream existing(path);
        if (existing.is_open()) {
            try { existing >> j; } catch (...) { j = json::object(); }
        }
    }
    json bindsJson = json::object();

    for (const auto& [action, key] : mActionToKey) {
        bindsJson[action] = glfwToKeyName(key);
    }
    j["binds"] = bindsJson;

    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(4);
        printf("[INPUT COMMANDS] Saved binds to %s\n", path.c_str());
    }
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
