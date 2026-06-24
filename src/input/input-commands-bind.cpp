#include "input-commands.h"

#include <fstream>
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

std::string glfwToKeyName(int key) {
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
