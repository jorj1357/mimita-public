#include "input-commands.h"
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
    mWindow = window;
    setupDefaultBinds();
    printf("[INPUT COMMANDS] Initialized\n");
}

void InputCommandSystem::setupDefaultBinds() {
    // Clear existing
    mActionToKey.clear();
    mKeyToAction.clear();
    mActionStates.clear();
    
    // Default binds
    bindAction("walkforward", GLFW_KEY_W);
    bindAction("walkback", GLFW_KEY_S);
    bindAction("walkleft", GLFW_KEY_A);
    bindAction("walkright", GLFW_KEY_D);
    bindAction("jump", GLFW_KEY_SPACE);
    bindAction("dash", GLFW_KEY_LEFT_SHIFT);
    bindAction("ground_return", GLFW_KEY_B);
    bindAction("freeze", GLFW_KEY_G);
    
    // Initialize states
    mActionStates["walkforward"] = {};
    mActionStates["walkback"] = {};
    mActionStates["walkleft"] = {};
    mActionStates["walkright"] = {};
    mActionStates["jump"] = {};
    mActionStates["dash"] = {};
    mActionStates["ground_return"] = {};
    mActionStates["freeze"] = {};
}

void InputCommandSystem::update(float dt) {
    (void)dt;
    if (!mWindow) return;
    
    for (auto& [actionName, state] : mActionStates) {
        auto it = mActionToKey.find(actionName);
        if (it == mActionToKey.end()) continue;
        
        int key = it->second;
        bool currDown = mKeyboardEnabled && glfwGetKey(mWindow, key) == GLFW_PRESS;
        bool prevDown = mPrevKeyStates[key];
        
        state.pressed = currDown && !prevDown;
        state.released = !currDown && prevDown;
        state.held = currDown;
        state.value = currDown ? 1.0f : 0.0f;

        auto pulse = mPendingPulses.find(actionName);
        if (pulse != mPendingPulses.end() && pulse->second) {
            state.pressed = true;
            state.held = true;
            state.value = 1.0f;
            pulse->second = false;
        }
        
        mPrevKeyStates[key] = currDown;
    }
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
    return {0.0f, 0.0f}; // Will be computed in pollInput using camera
}

bool InputCommandSystem::isJumpHeld() const {
    return getState("jump").held;
}

bool InputCommandSystem::isDashPressed() const {
    return getState("dash").pressed;
}

bool InputCommandSystem::isGroundReturnPressed() const {
    return getState("ground_return").pressed;
}

bool InputCommandSystem::isFreezeHeld() const {
    return getState("freeze").held;
}

void InputCommandSystem::bindAction(const std::string& actionName, int key) {
    // Remove old binding for this key
    auto oldActionIt = mKeyToAction.find(key);
    if (oldActionIt != mKeyToAction.end()) {
        mActionToKey.erase(oldActionIt->second);
    }
    
    // Remove old binding for this action
    auto oldKeyIt = mActionToKey.find(actionName);
    if (oldKeyIt != mActionToKey.end()) {
        mKeyToAction.erase(oldKeyIt->second);
    }
    
    // Add new binding
    mActionToKey[actionName] = key;
    mKeyToAction[key] = actionName;
    
    // Ensure state exists
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
    json j;
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
    if (str == "freeze") return InputAction::Freeze;
    return InputAction::Count;
}
