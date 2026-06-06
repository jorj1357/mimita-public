#include "dev-config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <GLFW/glfw3.h>

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

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

DevConfig& DevConfig::instance() {
    static DevConfig sInstance;
    return sInstance;
}

bool DevConfig::load(const char* path) {
    mConfigPath = path;
    mBindings.clear();
    
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[DEV CONFIG] Could not open %s, using defaults\n", path);
        return false;
    }
    
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        line = trim(line);
        
        if (line.empty() || line[0] == '#') continue;
        
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            printf("[DEV CONFIG] Line %d: malformed (no '='): %s\n", lineNum, line.c_str());
            continue;
        }
        
        std::string keyStr = trim(line.substr(0, eqPos));
        std::string rest = trim(line.substr(eqPos + 1));
        
        size_t commentPos = rest.find('#');
        std::string action, description;
        if (commentPos != std::string::npos) {
            action = trim(rest.substr(0, commentPos));
            description = trim(rest.substr(commentPos + 1));
        } else {
            action = rest;
            description = "";
        }
        
        int key = keyNameToGlfw(keyStr);
        if (key == -1) {
            printf("[DEV CONFIG] Line %d: unknown key '%s'\n", lineNum, keyStr.c_str());
            continue;
        }
        
        DevBinding binding;
        binding.key = key;
        binding.action = action;
        binding.description = description.empty() ? action : description;
        mBindings.push_back(binding);
        
        printf("[DEV CONFIG] Bound %s (0x%X) -> %s : %s\n", keyStr.c_str(), key, action.c_str(), binding.description.c_str());
    }
    
    printf("[DEV CONFIG] Loaded %zu bindings from %s\n", mBindings.size(), path);
    return true;
}

void DevConfig::reload() {
    if (!mConfigPath.empty()) {
        load(mConfigPath.c_str());
    }
}

const DevBinding* DevConfig::findByKey(int key) const {
    for (const auto& b : mBindings) {
        if (b.key == key) return &b;
    }
    return nullptr;
}

const DevBinding* DevConfig::findByAction(const std::string& action) const {
    for (const auto& b : mBindings) {
        if (b.action == action) return &b;
    }
    return nullptr;
}
