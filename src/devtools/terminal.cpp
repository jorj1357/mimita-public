#include "terminal.h"
#include "command-search.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <sstream>
#include <fstream>
#include <cmath>
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
        "Show command info. help <name> for details, help <category> to filter.",
        "help [name|category]",
        [this](const std::vector<std::string>& args) {
            if (args.empty()) {
                std::vector<const ConsoleCommand*> commands;
                commands.reserve(mCommands.size());
                for (const auto& pair : mCommands) commands.push_back(&pair.second);
                std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                    return a->name < b->name;
                });
                addLog("Available commands (help <name> for details, help <Category> to filter):");
                std::string lastCat;
                for (const ConsoleCommand* cmd : commands) {
                    std::string cat = categoryName(cmd->category);
                    if (cat != lastCat) {
                        char hdr[64];
                        snprintf(hdr, sizeof(hdr), "--- %s ---", cat.c_str());
                        addLog(hdr);
                        lastCat = cat;
                    }
                    char buf[256];
                    snprintf(buf, sizeof(buf), "  %-24s %s", cmd->name.c_str(), cmd->description.c_str());
                    addLog(buf);
                }
                return;
            }
            std::string a = args[0];
            std::string aLower = a;
            std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
            auto it = mCommands.find(a);
            if (it != mCommands.end()) {
                const ConsoleCommand& cmd = it->second;
                addLog("--- " + cmd.name + " ---");
                addLog("  Description: " + cmd.description);
                addLog("  Usage: " + cmd.usage);
                addLog("  Category: " + std::string(categoryName(cmd.category)));
                if (!cmd.dateAdded.empty())
                    addLog("  Added: " + cmd.dateAdded);
                return;
            }
            for (int c = (int)CommandCategory::Uncategorized; c <= (int)CommandCategory::UI; ++c) {
                std::string cn = categoryName((CommandCategory)c);
                std::transform(cn.begin(), cn.end(), cn.begin(), ::tolower);
                if (cn == aLower) {
                    std::string filterCat = categoryName((CommandCategory)c);
                    std::vector<const ConsoleCommand*> commands;
                    commands.reserve(mCommands.size());
                    for (const auto& pair : mCommands) commands.push_back(&pair.second);
                    std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                        return a->name < b->name;
                    });
                    addLog("--- " + filterCat + " ---");
                    for (const ConsoleCommand* cmd : commands) {
                        if (categoryName(cmd->category) == filterCat) {
                            char buf[256];
                            snprintf(buf, sizeof(buf), "  %-24s %s", cmd->name.c_str(), cmd->description.c_str());
                            addLog(buf);
                        }
                    }
                    return;
                }
            }
            // Fuzzy search for matching commands
            rebuildCache();
            MatchResult mr;
            std::vector<const ConsoleCommand*> fuzzyResults;
            for (const auto& cc : mCachedCommands) {
                if (fuzzyMatch(aLower, cc.lowerName, mr) > 0)
                    fuzzyResults.push_back(cc.cmd);
            }
            std::sort(fuzzyResults.begin(), fuzzyResults.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                return a->name < b->name;
            });
            if (!fuzzyResults.empty()) {
                addLog("No exact match found for \"" + a + "\". Did you mean:");
                for (size_t i = 0; i < std::min(fuzzyResults.size(), (size_t)20); i++) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "  %-24s %s",
                             fuzzyResults[i]->name.c_str(),
                             fuzzyResults[i]->description.c_str());
                    addLog(buf);
                }
                if (fuzzyResults.size() > 20) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "  ... and %zu more", fuzzyResults.size() - 20);
                    addLog(buf);
                }
            } else {
                addLog("Unknown command or category: " + a + ". Try 'help' for a list.");
            }
        }
    }, CommandCategory::UI);

    registerCommand({
        "help2",
        "List commands grouped by category (newest at bottom of each)",
        "help2",
        [this](const std::vector<std::string>&) {
            // Collect commands by category
            std::unordered_map<std::string, std::vector<const ConsoleCommand*>> byCat;
            std::vector<std::string> catOrder;
            for (const auto& pair : mCommands) {
                std::string cat = categoryName(pair.second.category);
                if (byCat.find(cat) == byCat.end()) catOrder.push_back(cat);
                byCat[cat].push_back(&pair.second);
            }
            for (auto& kv : byCat) {
                // Sort each category by registration order (newest last)
                std::sort(kv.second.begin(), kv.second.end(),
                    [this](const ConsoleCommand* a, const ConsoleCommand* b) {
                        auto ia = std::find(mRegistrationOrder.begin(), mRegistrationOrder.end(), a->name);
                        auto ib = std::find(mRegistrationOrder.begin(), mRegistrationOrder.end(), b->name);
                        return (ia != mRegistrationOrder.end() && ib != mRegistrationOrder.end()) ? ia < ib : a->name < b->name;
                    });
            }
            addLog("=== COMMANDS BY CATEGORY ===");
            for (const auto& cat : catOrder) {
                char hdr[64];
                snprintf(hdr, sizeof(hdr), "--- %s ---", cat.c_str());
                addLog(hdr);
                for (const ConsoleCommand* cmd : byCat[cat]) {
                    char buf[256];
                    if (!cmd->dateAdded.empty()) {
                        snprintf(buf, sizeof(buf), "  %-24s %s  (%s)",
                                 cmd->name.c_str(), cmd->description.c_str(),
                                 cmd->dateAdded.c_str());
                    } else {
                        snprintf(buf, sizeof(buf), "  %-24s %s",
                                 cmd->name.c_str(), cmd->description.c_str());
                    }
                    addLog(buf);
                }
            }
        }
    }, CommandCategory::UI);

    registerCommand({
        "help_recent",
        "Show last 20 added commands",
        "help_recent",
        [this](const std::vector<std::string>&) {
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(mCommands.size());
            for (const auto& pair : mCommands) {
                if (!pair.second.dateAdded.empty())
                    commands.push_back(&pair.second);
            }
            std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                if (a->dateAdded != b->dateAdded)
                    return a->dateAdded > b->dateAdded; // newest first
                return a->name < b->name;
            });
            int count = 0;
            addLog("=== RECENTLY ADDED COMMANDS ===");
            for (const ConsoleCommand* command : commands) {
                if (count >= 20) break;
                char buf[256];
                snprintf(buf, sizeof(buf), "  [NEW] %-20s %s  (added: %s)",
                         command->name.c_str(), command->description.c_str(),
                         command->dateAdded.c_str());
                addLog(std::string(buf));
                count++;
            }
        }
    });

    registerCommand({
        "help_today",
        "Show commands added in the current session",
        "help_today",
        [this](const std::vector<std::string>&) {
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(mCommands.size());
            for (const auto& pair : mCommands) {
                if (!pair.second.dateAdded.empty())
                    commands.push_back(&pair.second);
            }
            std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                if (a->dateAdded != b->dateAdded)
                    return a->dateAdded > b->dateAdded;
                return a->name < b->name;
            });
            addLog("=== ALL DATED COMMANDS ===");
            for (const ConsoleCommand* command : commands) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  [%s] %-20s %s",
                         command->dateAdded.c_str(), command->name.c_str(),
                         command->description.c_str());
                addLog(std::string(buf));
            }
        }
    });

    registerCommand({
        "help_since",
        "Show commands added after a date (e.g. help_since 2026-06-10)",
        "help_since <YYYY-MM-DD>",
        [this](const std::vector<std::string>& args) {
            std::string since = args.empty() ? "2026-06-01" : args[0];
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(mCommands.size());
            for (const auto& pair : mCommands) {
                if (!pair.second.dateAdded.empty() && pair.second.dateAdded >= since)
                    commands.push_back(&pair.second);
            }
            std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                if (a->dateAdded != b->dateAdded)
                    return a->dateAdded > b->dateAdded;
                return a->name < b->name;
            });
            char header[128];
            snprintf(header, sizeof(header), "=== COMMANDS ADDED SINCE %s ===", since.c_str());
            addLog(header);
            for (const ConsoleCommand* command : commands) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  [NEW] %-20s %s  (added: %s)",
                         command->name.c_str(), command->description.c_str(),
                         command->dateAdded.c_str());
                addLog(std::string(buf));
            }
            if (commands.empty()) {
                char buf[128];
                snprintf(buf, sizeof(buf), "  (no commands found since %s)", since.c_str());
                addLog(buf);
            }
        }
    });

    registerCommand({
        "commands",
        "List all registered command names",
        "commands",
        [this](const std::vector<std::string>&) {
            addLog("Registered commands (" + std::to_string(mCommands.size()) + " total):");
            std::vector<std::string> names;
            for (const auto& pair : mCommands)
                names.push_back(pair.first);
            std::sort(names.begin(), names.end());
            std::string line;
            for (size_t i = 0; i < names.size(); ++i) {
                if (!line.empty()) line += "  ";
                line += names[i];
                if (line.size() > 100 || i == names.size() - 1) {
                    addLog("  " + line);
                    line.clear();
                }
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
        "debug.npc",
        "Toggle NPC physics logs and visualization (0=off, 1=on)",
        "debug.npc <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC = !DebugConfig::DEBUG_NPC;
            } else {
                DebugConfig::DEBUG_NPC = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_NPC
                ? "[OK] NPC physics debug enabled"
                : "[OK] NPC physics debug disabled");
        }
    });

    registerCommand({
        "godball_debug",
        "Toggle godball collision debug visualization (0=off, 1=on)",
        "godball_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_GODBALL = !DebugConfig::DEBUG_GODBALL;
            } else {
                DebugConfig::DEBUG_GODBALL = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_GODBALL
                ? "[OK] godball debug enabled"
                : "[OK] godball debug disabled");
        }
    });

    registerCommand({
        "godball_hitstop_debug",
        "Toggle godball hitstop/slowmo on impact (0=off, 1=on)",
        "godball_hitstop_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_GODBALL_HITSTOP = !DebugConfig::DEBUG_GODBALL_HITSTOP;
            } else {
                DebugConfig::DEBUG_GODBALL_HITSTOP = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_GODBALL_HITSTOP
                ? "[OK] godball hitstop enabled"
                : "[OK] godball hitstop disabled");
        }
    });

    registerCommand({
        "npc_combat_debug",
        "Toggle NPC combat debug logging (0=off, 1=on)",
        "npc_combat_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_NPC_COMBAT = !DebugConfig::DEBUG_NPC_COMBAT;
            } else {
                DebugConfig::DEBUG_NPC_COMBAT = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_NPC_COMBAT
                ? "[OK] NPC combat debug enabled"
                : "[OK] NPC combat debug disabled");
        }
    });

    registerCommand({
        "anim_debug_arms",
        "Toggle arm animation debug logging (0=off, 1=on)",
        "anim_debug_arms <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_ANIM_ARMS = !DebugConfig::DEBUG_ANIM_ARMS;
            } else {
                DebugConfig::DEBUG_ANIM_ARMS = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_ANIM_ARMS
                ? "[OK] arm animation debug enabled"
                : "[OK] arm animation debug disabled");
        }
    });

    registerCommand({
        "swordsword_debug",
        "Toggle swordsword debug visualization (0=off, 1=on)",
        "swordsword_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_SWORDSWORD = !DebugConfig::DEBUG_SWORDSWORD;
            } else {
                DebugConfig::DEBUG_SWORDSWORD = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_SWORDSWORD
                ? "[OK] swordsword debug enabled"
                : "[OK] swordsword debug disabled");
        }
    });

    registerCommand({
        "animation_debug",
        "Toggle animation debug logging (0=off, 1=on)",
        "animation_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                DebugConfig::DEBUG_ANIMATION = !DebugConfig::DEBUG_ANIMATION;
            } else {
                DebugConfig::DEBUG_ANIMATION = args[0] != "0";
            }
            Terminal::instance().addLog(
                DebugConfig::DEBUG_ANIMATION
                ? "[OK] animation debug enabled"
                : "[OK] animation debug disabled");
        }
    });

    registerCommand({
        "animation_dump_pose",
        "Dump all body part perfectPose and physical pose to console once",
        "animation_dump_pose",
        [](const std::vector<std::string>&) {
            DebugConfig::DEBUG_ANIMATION = true;
            Terminal::instance().addLog("[OK] animation dump queued (will print next frame)");
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
            InputCommandSystem::instance().saveBinds("config/accounts/default.json");
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
        "npc_spawn_legacy",
        "Legacy NPC spawn position command",
        "npc_spawn_legacy [cursor|<x> <y> <z>] [difficulty]",
        [](const std::vector<std::string>& args) {
            extern void QueueNpcSpawnCommand(const std::vector<std::string>& args);
            QueueNpcSpawnCommand(args);
        }
    });

    registerCommand({
        "npc_spawn_training",
        "Spawn a training NPC",
        "npc_spawn_training [cursor|<x> <y> <z>] [difficulty]",
        [](const std::vector<std::string>& args) {
            extern void QueueNpcTrainingSpawnCommand(const std::vector<std::string>& args);
            QueueNpcTrainingSpawnCommand(args);
        }
    });

    registerCommand({
        "npc_spawn_training_mode",
        "Set training NPC mode: 0=idle, 1=flee, 2=attack",
        "npc_spawn_training_mode <0|1|2>",
        [](const std::vector<std::string>& args) {
            extern int gNpcTrainingMode;
            if (args.empty()) {
                Terminal::instance().addLog("Current training mode: " + std::to_string(gNpcTrainingMode));
                return;
            }
            int mode = std::clamp(std::stoi(args[0]), 0, 2);
            gNpcTrainingMode = mode;
            Terminal::instance().addLog("Training mode set to " + std::to_string(mode));
        }
    });

    registerCommand({
        "npc_training_health",
        "Set training NPC health (applies to future spawns)",
        "npc_training_health <number>",
        [](const std::vector<std::string>& args) {
            extern int gNpcTrainingHealth;
            if (args.empty()) {
                Terminal::instance().addLog("Current training health: " + std::to_string(gNpcTrainingHealth));
                return;
            }
            int hp = std::max(1, std::stoi(args[0]));
            gNpcTrainingHealth = hp;
            Terminal::instance().addLog("Training health set to " + std::to_string(hp));
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

    registerCommand({
        "command_stats",
        "Show command search statistics",
        "command_stats",
        [this](const std::vector<std::string>&) {
            addLog("--- Command Stats ---");
            char buf[256];
            snprintf(buf, sizeof(buf), "  registered commands: %zu", mCommands.size());
            addLog(buf);
            snprintf(buf, sizeof(buf), "  cache entries: %zu", mCachedCommands.size());
            addLog(buf);
            snprintf(buf, sizeof(buf), "  search results: %zu", mSearchResults.size());
            addLog(buf);
            snprintf(buf, sizeof(buf), "  selected result: %d", mSelectedResult);
            addLog(buf);
        }
    });

    registerCommand({
        "cmd",
        "Open command palette (terminal search mode)",
        "cmd",
        [this](const std::vector<std::string>&) {
            addLog("[OK] Command palette ready — start typing to search commands");
        }
    });

    registerCommand({
        "palette",
        "Open command palette (terminal search mode)",
        "palette",
        [this](const std::vector<std::string>&) {
            addLog("[OK] Command palette ready — start typing to search commands");
        }
    });
}

void Terminal::toggle() {
    mOpen = !mOpen;
    if (mOpen) {
        mInputLine.clear();
        mHistoryIndex = -1;
        mScrollOffset = 0;
        printf("[TERMINAL] opened\n");
    } else {
        printf("[TERMINAL] closed\n");
    }
}

void Terminal::addLog(const std::string& text) {
    mScrollback.push_back(text);
    if ((int)mScrollback.size() > MAX_SCROLLBACK)
        mScrollback.erase(mScrollback.begin());
    if (mScrollOffset > 0)
        mScrollOffset = std::min(mScrollOffset + 1, std::max(0, (int)mScrollback.size() - 1));
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
    if (mCommands.find(cmd.name) != mCommands.end()) {
        std::string msg = "[DUPLICATE COMMAND] " + cmd.name;
        addLog(msg);
        printf("%s\n", msg.c_str());
        return;
    }
    mCommands[cmd.name] = cmd;
    mRegistrationOrder.push_back(cmd.name);
    mCacheDirty = true;
}

void Terminal::registerCommand(const ConsoleCommand& cmd, const std::string& dateAdded) {
    if (mCommands.find(cmd.name) != mCommands.end()) {
        addLog("[DUPLICATE COMMAND] " + cmd.name);
        return;
    }
    mCommands[cmd.name] = cmd;
    mCommands[cmd.name].dateAdded = dateAdded;
    mRegistrationOrder.push_back(cmd.name);
    mCacheDirty = true;
}

void Terminal::registerCommand(const ConsoleCommand& cmd, CommandCategory category) {
    if (mCommands.find(cmd.name) != mCommands.end()) {
        addLog("[DUPLICATE COMMAND] " + cmd.name);
        return;
    }
    mCommands[cmd.name] = cmd;
    mCommands[cmd.name].category = category;
    mRegistrationOrder.push_back(cmd.name);
    mCacheDirty = true;
}

void Terminal::registerCommand(const ConsoleCommand& cmd, const std::string& dateAdded, CommandCategory category) {
    if (mCommands.find(cmd.name) != mCommands.end()) {
        addLog("[DUPLICATE COMMAND] " + cmd.name);
        return;
    }
    mCommands[cmd.name] = cmd;
    mCommands[cmd.name].dateAdded = dateAdded;
    mCommands[cmd.name].category = category;
    mRegistrationOrder.push_back(cmd.name);
    mCacheDirty = true;
}

void Terminal::rebuildCache() {
    if (!mCacheDirty) return;
    mCachedCommands.clear();
    mCachedCommands.reserve(mCommands.size());
    for (const auto& pair : mCommands) {
        CachedCommand cc;
        cc.cmd = &pair.second;
        cc.lowerName = pair.first;
        std::transform(cc.lowerName.begin(), cc.lowerName.end(), cc.lowerName.begin(), ::tolower);
        cc.lowerDesc = pair.second.description;
        std::transform(cc.lowerDesc.begin(), cc.lowerDesc.end(), cc.lowerDesc.begin(), ::tolower);
        mCachedCommands.push_back(cc);
    }
    mCacheDirty = false;
}

void Terminal::updateSearch() {
    if (mInputLine == mLastSearchInput && !mCacheDirty) return;
    mLastSearchInput = mInputLine;
    rebuildCache();

    mSearchResults.clear();
    mGhostSuffix.clear();
    mSelectedResult = -1;

    if (mInputLine.empty()) {
        return;
    }

    // Only autocomplete the first word (no spaces = typing command name)
    if (mInputLine.find(' ') != std::string::npos) {
        mSelectedResult = -1;
        return;
    }

    std::string inputLower = mInputLine;
    std::transform(inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower);

    MatchResult mr;
    for (const auto& cc : mCachedCommands) {
        int score = fuzzyMatch(inputLower, cc.lowerName, mr);
        if (score > 0) {
            SearchResult sr;
            sr.cmd = cc.cmd;
            sr.score = score;
            sr.matchPositions = std::move(mr.positions);
            mSearchResults.push_back(std::move(sr));
        }
        // Also search description for lower priority matches
        if (score == 0 && cc.lowerDesc.find(inputLower) != std::string::npos) {
            SearchResult sr;
            sr.cmd = cc.cmd;
            sr.score = 1;
            mSearchResults.push_back(std::move(sr));
        }
    }

    std::sort(mSearchResults.begin(), mSearchResults.end(),
        [](const SearchResult& a, const SearchResult& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.cmd->name < b.cmd->name;
        });

    if (mSearchResults.size() > 20)
        mSearchResults.resize(20);

    if (mSelectedResult >= (int)mSearchResults.size())
        mSelectedResult = mSearchResults.empty() ? -1 : 0;

    mGhostSuffix = computeGhostSuffix(inputLower);
}

std::string Terminal::computeGhostSuffix(const std::string& inputLower) const {
    if (mSearchResults.empty() || inputLower.empty())
        return {};

    const std::string& bestName = mSearchResults[0].cmd->name;
    if (inputLower.size() >= bestName.size())
        return {};

    // Only show ghost if input is a case-insensitive prefix of the best match
    for (size_t i = 0; i < inputLower.size(); i++) {
        if (inputLower[i] != std::tolower(bestName[i]))
            return {};
    }

    return bestName.substr(inputLower.size());
}

void Terminal::drawAutocompleteMenu(float inputLineY, float lineHeight) {
    if (mSearchResults.empty() || mInputLine.empty())
        return;

    float fbW = uiScreenW();
    float itemH = lineHeight;
    int maxVisible = 12;
    int numShow = std::min((int)mSearchResults.size(), maxVisible);
    float menuH = itemH * numShow + 4;
    float menuX = 16.0f;
    float menuY = inputLineY - 8.0f - menuH;
    float menuW = fbW - 32.0f;

    // Menu background
    uiDrawRect({menuX, menuY, menuW, menuH}, {0.06f, 0.06f, 0.06f, 0.95f}, "autocomplete-bg");
    uiDrawRect({menuX, menuY, menuW, 1}, {0.85f, 0.05f, 0.05f, 0.7f}, "autocomplete-accent");

    float descColorMulti = 0.4f;

    for (int i = 0; i < numShow; i++) {
        const SearchResult& sr = mSearchResults[i];
        float y = menuY + 2.0f + itemH * i;

        if (i == mSelectedResult) {
            uiDrawRect({menuX + 2, y, menuW - 4, itemH}, {0.25f, 0.25f, 0.35f, 0.8f}, "autocomplete-sel");
        }

        // Draw command name with highlighted matching positions
        const std::string& name = sr.cmd->name;
        glm::vec4 nameColor = {0.95f, 0.95f, 0.95f, 1.0f};
        glm::vec4 hlColor = {1.0f, 0.9f, 0.3f, 1.0f};

        if (sr.matchPositions.empty()) {
            uiDrawText(name.c_str(), menuX + 8, y, 0.35f, nameColor);
        } else {
            // Draw full name in base color, then overlay matched chars in highlight
            uiDrawText(name.c_str(), menuX + 8, y, 0.35f, nameColor);
            for (int pos : sr.matchPositions) {
                if (pos < (int)name.size()) {
                    std::string prefix = name.substr(0, pos);
                    float cx = menuX + 8 + uiMeasureText(prefix.c_str(), 0.35f);
                    char ch[2] = { name[pos], '\0' };
                    uiDrawText(ch, cx, y, 0.35f, hlColor);
                }
            }
        }

        // Draw description
        if (!sr.cmd->description.empty()) {
            float nameW = uiMeasureText(name.c_str(), 0.35f) + 12;
            std::string desc = sr.cmd->description;
            float maxDescW = menuW - nameW - 24;
            if (maxDescW > 40) {
                // Truncate if too long
                float descW = uiMeasureText(desc.c_str(), 0.30f);
                if (descW > maxDescW) {
                    while (!desc.empty() && uiMeasureText((desc + "...").c_str(), 0.30f) > maxDescW)
                        desc.pop_back();
                    desc += "...";
                }
                uiDrawText(desc.c_str(), menuX + 8 + nameW, y, 0.30f,
                          {descColorMulti, descColorMulti, descColorMulti + 0.25f, 0.8f});
            }
        }
    }

    if ((int)mSearchResults.size() > maxVisible) {
        char more[64];
        snprintf(more, sizeof(more), "... %zu more", mSearchResults.size() - (size_t)maxVisible);
        uiDrawText(more, menuX + 8, menuY + menuH, 0.30f, {0.5f, 0.5f, 0.5f, 0.7f});
    }
}

void Terminal::handleChar(unsigned int codepoint) {
    if (!mOpen) return;
    if (codepoint >= 32 && codepoint <= 126) {
        mInputLine += (char)codepoint;
        mSelectedResult = -1;
    }
}

void Terminal::handleKey(int key, int mods) {
    if (!mOpen) return;

    updateSearch();

    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_V) {
        const char* clip = glfwGetClipboardString(mWindow);
        if (clip) mInputLine += clip;
        return;
    }
    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_C) {
        if (!mInputLine.empty())
            glfwSetClipboardString(mWindow, mInputLine.c_str());
        return;
    }
    if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_X) {
        if (!mInputLine.empty()) {
            glfwSetClipboardString(mWindow, mInputLine.c_str());
            mInputLine.clear();
        }
        return;
    }

    if (key == GLFW_KEY_ENTER) {
        if (!mSearchResults.empty()) {
            int idx = mSelectedResult >= 0 ? mSelectedResult : 0;
            mInputLine = mSearchResults[idx].cmd->name;
        }
        executeCurrent();
    } else if (key == GLFW_KEY_BACKSPACE) {
        if (!mInputLine.empty()) {
            mInputLine.pop_back();
            mSelectedResult = -1;
        }
    } else if ((mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_UP) {
        mScrollOffset = std::min(mScrollOffset + 1, std::max(0, (int)mScrollback.size() - 1));
    } else if ((mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 1);
    } else if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_UP) {
        mScrollOffset = std::min(mScrollOffset + 10, std::max(0, (int)mScrollback.size() - 1));
    } else if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 10);
    } else if (key == GLFW_KEY_PAGE_UP) {
        mScrollOffset = std::min(mScrollOffset + 10, std::max(0, (int)mScrollback.size() - 1));
    } else if (key == GLFW_KEY_PAGE_DOWN) {
        mScrollOffset = std::max(0, mScrollOffset - 10);
    } else if (key == GLFW_KEY_UP) {
        if (!mSearchResults.empty()) {
            if (mSelectedResult < 0)
                mSelectedResult = 0;
            else if (mSelectedResult > 0)
                mSelectedResult--;
        } else if (!mHistory.empty()) {
            if (mHistoryIndex == -1)
                mHistoryIndex = (int)mHistory.size() - 1;
            else if (mHistoryIndex > 0)
                mHistoryIndex--;
            mInputLine = mHistory[mHistoryIndex];
        }
    } else if (key == GLFW_KEY_DOWN) {
        if (!mSearchResults.empty()) {
            if (mSelectedResult < (int)mSearchResults.size() - 1)
                mSelectedResult++;
            else
                mSelectedResult = 0;
        } else if (mHistoryIndex >= 0 && mHistoryIndex < (int)mHistory.size() - 1) {
            mHistoryIndex++;
            mInputLine = mHistory[mHistoryIndex];
        } else {
            mHistoryIndex = -1;
            mInputLine.clear();
        }
    } else if (key == GLFW_KEY_TAB) {
        if (!mSearchResults.empty() && !mInputLine.empty()) {
            int idx = mSelectedResult >= 0 ? mSelectedResult : 0;
            if (idx < (int)mSearchResults.size()) {
                mInputLine = mSearchResults[idx].cmd->name;
                mLastSearchInput = mInputLine;
            }
        }
    }
}

void Terminal::handleScroll(double yOffset)
{
    if (!mOpen)
        return;
    int lines = (int)std::round(std::fabs(yOffset) * 3.0);
    if (lines < 1) lines = 1;
    if (yOffset > 0.0)
        mScrollOffset = std::min(mScrollOffset + lines, std::max(0, (int)mScrollback.size() - 1));
    else if (yOffset < 0.0)
        mScrollOffset = std::max(0, mScrollOffset - lines);
}

void Terminal::render() {
    if (!mOpen || !mWindow) return;

    updateSearch();

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
    int endExclusive = std::max(0, (int)mScrollback.size() - mScrollOffset);
    int scrollStart = std::max(0, endExclusive - visibleLines);

    float y = startY - lineHeight * (endExclusive - scrollStart - 1);
    for (int i = scrollStart; i < endExclusive; i++) {
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
    if (mScrollOffset > 0) {
        char scrollText[64];
        snprintf(scrollText, sizeof(scrollText), "[SCROLLBACK: %d lines above newest]", mScrollOffset);
        uiDrawText(scrollText, fbW - 330.0f, 20.0f, 0.30f, {1.0f, 0.8f, 0.25f, 1.0f});
    }

    // Autocomplete menu (draw before input line background)
    drawAutocompleteMenu(inputLineY, lineHeight);

    // Input line background
    uiDrawRect({0, inputLineY - 6.0f, fbW, 36.0f}, {0.08f, 0.08f, 0.08f, 0.95f}, "terminal-input-bg");
    uiDrawRect({0, inputLineY - 6.0f, fbW, 1}, {0.85f, 0.05f, 0.05f, 0.7f}, "terminal-input-accent");

    // Prompt text
    float promptX = 16.0f;
    std::string prompt = "] " + mInputLine;
    uiDrawText(prompt.c_str(), promptX, inputLineY, 0.42f, {0.95f, 0.95f, 0.95f, 1.0f});

    // Ghost text (inline autocomplete)
    float inputEndX = promptX + uiMeasureText(prompt.c_str(), 0.42f);
    if (!mGhostSuffix.empty()) {
        uiDrawText(mGhostSuffix.c_str(), inputEndX, inputLineY, 0.42f, {0.4f, 0.4f, 0.45f, 0.7f});
    }

    // Blinking cursor
    mCursorBlink += 0.05f;
    bool cursorVisible = fmodf(mCursorBlink, 1.0f) < 0.6f;
    if (cursorVisible) {
        uiDrawText("_", inputEndX, inputLineY, 0.42f, {0.95f, 0.95f, 0.95f, 1.0f});
    }

    uiEndFrame();
}
