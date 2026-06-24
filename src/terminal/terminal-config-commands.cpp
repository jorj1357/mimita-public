#include "devtools/terminal.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include "input/input-commands.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

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

void registerConfigCommands()
{
    auto& term = Terminal::instance();

    term.registerCommand({
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

    term.registerCommand({
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

    term.registerCommand({
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

    term.registerCommand({
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
            InputCommandSystem::instance().saveBinds("config/accounts/default.json");
            DevOverlay::instance().showNotification("Input bind: " + action + " = " + keyStr, 3.0f);
            Terminal::instance().addLog("[OK] Input bind updated: " + action + " = " + keyStr);
        }
    });

    term.registerCommand({
        "input.save",
        "Save input binds to account config",
        "input.save [account_name]",
        [](const std::vector<std::string>& args) {
            std::string account = args.empty() ? "default" : args[0];
            InputCommandSystem::instance().saveBinds("config/accounts/" + account + ".json");
            Terminal::instance().addLog("[OK] Input binds saved to account: " + account);
        }
    });

    term.registerCommand({
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
}
