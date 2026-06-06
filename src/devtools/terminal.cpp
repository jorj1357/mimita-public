#include "terminal.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <sstream>
#include <GLFW/glfw3.h>

#include "config.h"
#include "gui/ui-system.h"
#include "audio/audio.h"

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
            std::string list;
            for (const auto& pair : mCommands) {
                list += pair.second.name + "\n";
            }
            addLog("Available commands:");
            for (const auto& pair : mCommands) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  %-24s %s", pair.second.name.c_str(), pair.second.description.c_str());
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
    } else if (key == GLFW_KEY_ESCAPE) {
        toggle();
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
