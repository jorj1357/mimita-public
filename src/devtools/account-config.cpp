// C:\important\mimita-priv-v8\src\devtools\account-config.cpp
// Account config system for per-account key binds

#include "account-config.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include "config/player-settings.h"

using json = nlohmann::json;

namespace {

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
        default:
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
                return std::string(1, char('A' + (key - GLFW_KEY_A)));
            if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
                return std::string(1, char('0' + (key - GLFW_KEY_0)));
            return "UNKNOWN";
    }
}

std::string getAccountConfigPath(const std::string& account) {
    return "config/accounts/" + account + ".json";
}

void ensureConfigDir() {
    std::filesystem::create_directories("config/accounts");
}

} // namespace

bool LoadAccountConfig(const std::string& account) {
    ensureConfigDir();
    std::string path = getAccountConfigPath(account);
    
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[ACCOUNT CONFIG] No config found at %s, using defaults\n", path.c_str());
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        std::string accountName = j.value("accountName", account);
        auto bindsJson = j.value("binds", json::object());
        
        auto& bindings = const_cast<std::vector<DevBinding>&>(DevConfig::instance().bindings());
        
        // Update existing bindings or add new ones
        for (auto& [action, keyStr] : bindsJson.items()) {
            int key = keyNameToGlfw(keyStr.get<std::string>());
            if (key == -1) {
                printf("[ACCOUNT CONFIG] Unknown key '%s' for action '%s'\n", keyStr.get<std::string>().c_str(), action.c_str());
                continue;
            }
            
            bool found = false;
            for (auto& b : bindings) {
                if (b.action == action) {
                    b.key = key;
                    found = true;
                    break;
                }
            }
            if (!found) {
                DevBinding newBinding;
                newBinding.key = key;
                newBinding.action = action;
                newBinding.description = action;
                bindings.push_back(newBinding);
            }
            
            printf("[ACCOUNT CONFIG] Loaded bind: %s = %s\n", action.c_str(), keyStr.get<std::string>().c_str());
        }
        
        DevOverlay::instance().showNotification("Loaded config: " + accountName, 3.0f);
        printf("[ACCOUNT CONFIG] Loaded %zu binds from %s\n", bindsJson.size(), path.c_str());
        LoadPlayerSettings(account);
        return true;
        
    } catch (const std::exception& e) {
        printf("[ACCOUNT CONFIG] Error loading %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

bool SaveAccountConfig(const std::string& account) {
    ensureConfigDir();
    std::string path = getAccountConfigPath(account);
    
    const auto& bindings = DevConfig::instance().bindings();
    json j;
    j["accountName"] = account;
    json bindsJson = json::object();
    
    for (const auto& b : bindings) {
        bindsJson[b.action] = glfwToKeyName(b.key);
    }
    j["binds"] = bindsJson;
    
    std::ifstream existing(path);
    if (existing.is_open()) {
        try {
            json previous;
            existing >> previous;
            if (previous.contains("settings"))
                j["settings"] = previous["settings"];
        } catch (...) {}
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        printf("[ACCOUNT CONFIG] Failed to open %s for writing\n", path.c_str());
        return false;
    }
    
    file << j.dump(4);
    printf("[ACCOUNT CONFIG] Saved %zu binds to %s\n", bindings.size(), path.c_str());
    file.close();
    return SavePlayerSettings(account);
}

void CreateDefaultAccountConfig() {
    ensureConfigDir();
    std::string path = getAccountConfigPath("default");
    
    if (std::filesystem::exists(path)) {
        return; // Already exists
    }
    
    json j;
    j["accountName"] = "default";
    json bindsJson = json::object();
    bindsJson["forward"] = "W";
    bindsJson["back"] = "S";
    bindsJson["left"] = "A";
    bindsJson["right"] = "D";
    bindsJson["jump"] = "SPACE";
    bindsJson["dash"] = "LEFT_SHIFT";
    bindsJson["ground_return"] = "B";
    bindsJson["freeze"] = "G";
    j["binds"] = bindsJson;
    
    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(4);
        printf("[ACCOUNT CONFIG] Created default config at %s\n", path.c_str());
    }
}
