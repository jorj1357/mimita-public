#include "terminal.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <sstream>
#include <fstream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "config.h"
#include "gui/ui-system.h"
#include "audio/audio.h"
#include "devtools/dev-config.h"
#include "devtools/dev-commands.h"
#include "devtools/dev-overlay.h"
#include "devtools/account-config.h"
#include "input/input-commands.h"
#include "npc/npc.h"
#include "camera.h"
#include "world/world.h"
#include "entities/player.h"

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

Terminal& Terminal::instance() {
    static Terminal t;
    return t;
}

void Terminal::init(GLFWwindow* window) {
    mWindow = window;
    addLog("[TERMINAL] initialized. type 'help' for commands.");

    registerCommand({
        "help",
        "List all available commands",
        "help",
        [this](const std::vector<std::string>&) {
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(mCommands.size());
            for (const auto& pair : mCommands) commands.push_back(&pair.second);
            std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                return a->name < b->name;
            });
            addLog("Available commands:");
            for (const ConsoleCommand* command : commands) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  %-24s %s", command->name.c_str(), command->description.c_str());
                addLog(buf);
            }
        }
    });

    registerCommand({
        "clear",
        "Clear the terminal scrollback",
        "clear",
        [this](const std::vector<std::string>&) {
            mScrollback.clear();
        }
    });

    registerCommand({
        "debug.collision",
        "Toggle collision debug visualization (0=off, 1=on)",
        "debug.collision <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_COLLISION = !DebugConfig::DEBUG_COLLISION;
            } else {
                DebugConfig::DEBUG_COLLISION = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_COLLISION
                ? "[OK] collision debug enabled"
                : "[OK] collision debug disabled");
        }
    });

    registerCommand({
        "debug.movement",
        "Toggle movement debug visualization (0=off, 1=on)",
        "debug.movement <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_MOVEMENT = !DebugConfig::DEBUG_MOVEMENT;
            } else {
                DebugConfig::DEBUG_MOVEMENT = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_MOVEMENT
                ? "[OK] movement debug enabled"
                : "[OK] movement debug disabled");
        }
    });

    registerCommand({
        "debug.playerarch",
        "Toggle player architecture overlay (0=off, 1=on)",
        "debug.playerarch <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_PLAYERARCH = !DebugConfig::DEBUG_PLAYERARCH;
            } else {
                DebugConfig::DEBUG_PLAYERARCH = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_PLAYERARCH
                ? "[OK] player architecture overlay enabled"
                : "[OK] player architecture overlay disabled");
        }
    });

    registerCommand({
        "debug.sound",
        "Toggle sound debug (0=off, 1=on)",
        "debug.sound <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_SOUND = !DebugConfig::DEBUG_SOUND;
            } else {
                DebugConfig::DEBUG_SOUND = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_SOUND
                ? "[OK] sound debug enabled"
                : "[OK] sound debug disabled");
        }
    });

    registerCommand({
        "debug.render",
        "Toggle render debug info (0=off, 1=on)",
        "debug.render <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_RENDER = !DebugConfig::DEBUG_RENDER;
            } else {
                DebugConfig::DEBUG_RENDER = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_RENDER
                ? "[OK] render debug enabled"
                : "[OK] render debug disabled");
        }
    });

    registerCommand({
        "debug.physics",
        "Toggle physics debug visualization (0=off, 1=on)",
        "debug.physics <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_PHYSICS = !DebugConfig::DEBUG_PHYSICS;
            } else {
                DebugConfig::DEBUG_PHYSICS = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_PHYSICS
                ? "[OK] physics debug enabled"
                : "[OK] physics debug disabled");
        }
    });

    registerCommand({
        "debug.reset",
        "Reset all debug flags to defaults",
        "debug.reset",
        [](const std::vector<std::string>&) {
            DebugConfig::ResetAll();
            Terminal::instance().addLog("[OK] all debug flags reset to defaults");
        }
    });

    // --- Input/Config Commands ---
    registerCommand({
        "controls",
        "Show current key bindings",
        "controls",
        [](const std::vector<std::string>&) {
            const auto& bindings = DevConfig::instance().bindings();
            Terminal::instance().addLog("Current bindings:");
            for (const auto& b : bindings) {
                char buf[256];
                const char* keyName = [b]() -> const char* {
                    switch (b.key) {
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
                        case GLFW_KEY_GRAVE_ACCENT: return "`";
                        default:
                            if (b.key >= GLFW_KEY_A && b.key <= GLFW_KEY_Z)
                                return (std::string(1, char('A' + (b.key - GLFW_KEY_A)))).c_str();
                            if (b.key >= GLFW_KEY_0 && b.key <= GLFW_KEY_9)
                                return (std::string(1, char('0' + (b.key - GLFW_KEY_0)))).c_str();
                            return "?";
                    }
                }();
                snprintf(buf, sizeof(buf), "  %-24s = %s  (%s)", b.action.c_str(), keyName, b.description.c_str());
                Terminal::instance().addLog(buf);
            }
        }
    });

    registerCommand({
        "bind",
        "Bind an action to a key",
        "bind <action> <key>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Terminal::instance().addLog("[ERROR] Usage: bind <action> <key>");
                Terminal::instance().addLog("Example: bind forward W");
                return;
            }
            const std::string& action = args[0];
            const std::string& keyStr = args[1];
            
            auto keyNameToGlfw = [](const std::string& name) -> int {
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
            };
            
            int key = keyNameToGlfw(keyStr);
            if (key == -1) {
                Terminal::instance().addLog("[ERROR] Unknown key: " + keyStr);
                return;
            }
            
            // Update runtime bindings in DevConfig
            auto& bindings = const_cast<std::vector<DevBinding>&>(DevConfig::instance().bindings());
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
            
            DevOverlay::instance().showNotification("Bind updated: " + action + " = " + keyStr, 3.0f);
            Terminal::instance().addLog("[OK] Bind updated: " + action + " = " + keyStr);
        }
    });

    // Input command system commands
    registerCommand({
        "input.binds",
        "Show current input command binds",
        "input.binds",
        [](const std::vector<std::string>&) {
            const auto& cmd = InputCommandSystem::instance();
            Terminal::instance().addLog("Current input binds:");
            Terminal::instance().addLog("  walkforward = " + glfwToKeyName(cmd.getKeyForAction("walkforward")));
            Terminal::instance().addLog("  walkback    = " + glfwToKeyName(cmd.getKeyForAction("walkback")));
            Terminal::instance().addLog("  walkleft    = " + glfwToKeyName(cmd.getKeyForAction("walkleft")));
            Terminal::instance().addLog("  walkright   = " + glfwToKeyName(cmd.getKeyForAction("walkright")));
            Terminal::instance().addLog("  jump          = " + glfwToKeyName(cmd.getKeyForAction("jump")));
            Terminal::instance().addLog("  dash          = " + glfwToKeyName(cmd.getKeyForAction("dash")));
            Terminal::instance().addLog("  ground_return = " + glfwToKeyName(cmd.getKeyForAction("ground_return")));
            Terminal::instance().addLog("  freeze        = " + glfwToKeyName(cmd.getKeyForAction("freeze")));
        }
    });

    registerCommand({
        "input.bind",
        "Bind an input action to a key (runtime, saved to account config)",
        "input.bind <action> <key>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Terminal::instance().addLog("[ERROR] Usage: input.bind <action> <key>");
                Terminal::instance().addLog("Actions: move_forward, move_back, move_left, move_right, jump, dash, ground_return, freeze");
                Terminal::instance().addLog("Example: input.bind jump SPACE");
                return;
            }
            const std::string& action = args[0];
            const std::string& keyStr = args[1];
            
            auto keyNameToGlfw = [](const std::string& name) -> int {
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
            };
            
            int key = keyNameToGlfw(keyStr);
            if (key == -1) {
                Terminal::instance().addLog("[ERROR] Unknown key: " + keyStr);
                return;
            }
            
            InputCommandSystem::instance().bindAction(action, key);
            DevOverlay::instance().showNotification("Input bind: " + action + " = " + keyStr, 3.0f);
            Terminal::instance().addLog("[OK] Input bind updated: " + action + " = " + keyStr);
        }
    });

    registerCommand({
        "input.save",
        "Save input binds to account config",
        "input.save [account_name]",
        [](const std::vector<std::string>& args) {
            std::string account = args.empty() ? "default" : args[0];
            InputCommandSystem::instance().saveBinds("config/accounts/" + account + ".json");
            Terminal::instance().addLog("[OK] Input binds saved to account: " + account);
        }
    });

    registerCommand({
        "input.load",
        "Load input binds from account config",
        "input.load <account_name>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: input.load <account_name>");
                return;
            }
            InputCommandSystem::instance().loadBinds("config/accounts/" + args[0] + ".json");
            Terminal::instance().addLog("[OK] Input binds loaded from account: " + args[0]);
        }
    });

    registerCommand({
        "spawn.npc",
        "Spawn NPC at cursor or position",
        "spawn.npc [cursor|<x> <y> <z>] [difficulty]",
        [](const std::vector<std::string>& args) {
            // This will be handled by the game system - we just queue the command
            // The actual spawn happens in the game loop
            extern void QueueNpcSpawnCommand(const std::vector<std::string>& args);
            QueueNpcSpawnCommand(args);
        }
    });

    registerCommand({
        "account",
        "Show current account and config",
        "account",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("Account: default");
            Terminal::instance().addLog("Config: config/accounts/default.json");
        }
    });

    registerCommand({
        "config.save",
        "Save current binds to account config",
        "config.save [account_name]",
        [](const std::vector<std::string>& args) {
            std::string account = args.empty() ? "default" : args[0];
            extern bool SaveAccountConfig(const std::string& account);
            if (SaveAccountConfig(account)) {
                Terminal::instance().addLog("[OK] Config saved for account: " + account);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to save config");
            }
        }
    });

    registerCommand({
        "config.load",
        "Load binds from account config",
        "config.load <account_name>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: config.load <account_name>");
                return;
            }
            extern bool LoadAccountConfig(const std::string& account);
            if (LoadAccountConfig(args[0])) {
                Terminal::instance().addLog("[OK] Config loaded for account: " + args[0]);
                DevOverlay::instance().showNotification("Config loaded: " + args[0], 3.0f);
            } else {
                Terminal::instance().addLog("[ERROR] Failed to load config");
            }
        }
    });
}

void Terminal::toggle() {
    mOpen = !mOpen;
    if (mOpen) {
        mInputLine.clear();
        mHistoryIndex = -1;
        printf("[TERMINAL] opened\n");
    } else {
        printf("[TERMINAL] closed\n");
    }
}

void Terminal::addLog(const std::string& text) {
    mScrollback.push_back(text);
    if ((int)mScrollback.size() > MAX_SCROLLBACK)
        mScrollback.erase(mScrollback.begin());
}

void Terminal::addHistory(const std::string& input) {
    if (input.empty())
        return;
    if (!mHistory.empty() && mHistory.back() == input)
        return;
    mHistory.push_back(input);
    if ((int)mHistory.size() > MAX_HISTORY)
        mHistory.erase(mHistory.begin());
    mHistoryIndex = -1;
}

void Terminal::executeCurrent() {
    std::string input = mInputLine;
    addLog("] " + input);
    addHistory(input);
    execute(input);
    mInputLine.clear();
    mHistoryIndex = -1;
}

void Terminal::execute(const std::string& input) {
    std::istringstream iss(input);
    std::string cmdName;
    iss >> cmdName;
    if (cmdName.empty())
        return;

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg)
        args.push_back(arg);

    auto it = mCommands.find(cmdName);
    if (it == mCommands.end()) {
        addLog("[ERROR] unknown command: " + cmdName);
        addLog("type 'help' for a list of commands");
        return;
    }

    try {
        it->second.fn(args);
    } catch (const std::exception& e) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[ERROR] %s: %s", cmdName.c_str(), e.what());
        addLog(buf);
    }
}

void Terminal::registerCommand(const ConsoleCommand& cmd) {
    mCommands[cmd.name] = cmd;
}

void Terminal::handleChar(unsigned int codepoint) {
    if (!mOpen) return;
    if (codepoint >= 32 && codepoint <= 126) {
        mInputLine += (char)codepoint;
    }
}

void Terminal::handleKey(int key, int mods) {
    if (!mOpen) return;

    (void)mods;

    if (key == GLFW_KEY_ENTER) {
        executeCurrent();
    } else if (key == GLFW_KEY_BACKSPACE) {
        if (!mInputLine.empty())
            mInputLine.pop_back();
    } else if (key == GLFW_KEY_UP) {
        if (!mHistory.empty()) {
            if (mHistoryIndex == -1)
                mHistoryIndex = (int)mHistory.size() - 1;
            else if (mHistoryIndex > 0)
                mHistoryIndex--;
            mInputLine = mHistory[mHistoryIndex];
        }
    } else if (key == GLFW_KEY_DOWN) {
        if (mHistoryIndex >= 0 && mHistoryIndex < (int)mHistory.size() - 1) {
            mHistoryIndex++;
            mInputLine = mHistory[mHistoryIndex];
        } else {
            mHistoryIndex = -1;
            mInputLine.clear();
        }
    } else if (key == GLFW_KEY_TAB) {
        // simple tab completion: find first matching command
        if (!mInputLine.empty()) {
            std::string prefix = mInputLine;
            std::string match;
            for (const auto& pair : mCommands) {
                if (pair.first.find(prefix) == 0) {
                    if (match.empty()) {
                        match = pair.first;
                    } else {
                        // multiple matches, show them
                        match.clear();
                        addLog("] " + mInputLine);
                        for (const auto& p : mCommands) {
                            if (p.first.find(prefix) == 0)
                                addLog("  " + p.first);
                        }
                        return;
                    }
                }
            }
            if (!match.empty()) {
                mInputLine = match + " ";
            }
        }
    }
}

void Terminal::render() {
    if (!mOpen || !mWindow) return;

    uiBeginFrame(mWindow, "terminal");

    float fbW = uiScreenW();
    float fbH = uiScreenH();

    // Fullscreen semi-transparent black background
    uiDrawRect({0, 0, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.92f}, "terminal-bg");

    // Red accent line at top
    uiDrawRect({0, 0, fbW, 3}, {0.85f, 0.05f, 0.05f, 0.9f}, "terminal-accent");

    // Draw scrollback
    float lineHeight = 22.0f;
    float inputLineY = fbH - 40.0f;
    float startY = inputLineY - 12.0f - lineHeight;

    int visibleLines = (int)(startY / lineHeight);
    int scrollStart = std::max(0, (int)mScrollback.size() - visibleLines);

    float y = startY - lineHeight * ((int)mScrollback.size() - scrollStart);
    for (int i = scrollStart; i < (int)mScrollback.size(); i++) {
        const std::string& line = mScrollback[i];
        glm::vec4 color = {0.7f, 0.8f, 0.9f, 1.0f};
        if (line.find("[OK]") == 0)
            color = {0.2f, 1.0f, 0.3f, 1.0f};
        else if (line.find("[ERROR]") == 0)
            color = {1.0f, 0.2f, 0.2f, 1.0f};
        else if (line.find("] ") == 0)
            color = {1.0f, 0.85f, 0.3f, 1.0f};
        else if (line.find("[TERMINAL]") == 0)
            color = {0.6f, 0.6f, 1.0f, 1.0f};

        uiDrawText(line.c_str(), 16.0f, y, 0.38f, color);
        y += lineHeight;
    }

    // Input line background
    uiDrawRect({0, inputLineY - 6.0f, fbW, 36.0f}, {0.08f, 0.08f, 0.08f, 0.95f}, "terminal-input-bg");
    uiDrawRect({0, inputLineY - 6.0f, fbW, 1}, {0.85f, 0.05f, 0.05f, 0.7f}, "terminal-input-accent");

    // Prompt
    std::string prompt = "] " + mInputLine;

    // Blinking cursor
    mCursorBlink += 0.05f;
    bool cursorVisible = fmodf(mCursorBlink, 1.0f) < 0.6f;
    if (cursorVisible)
        prompt += "_";

    uiDrawText(prompt.c_str(), 16.0f, inputLineY, 0.42f, {0.95f, 0.95f, 0.95f, 1.0f});

    uiEndFrame();
}
