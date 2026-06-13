#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct GLFWwindow;

struct ConsoleCommand {
    std::string name;
    std::string description;
    std::string usage;
    std::function<void(const std::vector<std::string>& args)> fn;
    std::string dateAdded;  // "YYYY-MM-DD" or empty — stored separately
};

class Terminal {
public:
    static Terminal& instance();

    void init(GLFWwindow* window);
    void toggle();
    bool isOpen() const { return mOpen; }

    void handleChar(unsigned int codepoint);
    void handleKey(int key, int mods);
    void handleScroll(double yOffset);

    void render();

    void registerCommand(const ConsoleCommand& cmd);
    void registerCommand(const ConsoleCommand& cmd, const std::string& dateAdded);
    void execute(const std::string& input);
    void addLog(const std::string& text);

private:
    Terminal() = default;

    void executeCurrent();
    void addHistory(const std::string& input);

    GLFWwindow* mWindow = nullptr;
    bool mOpen = false;

    std::string mInputLine;
    std::vector<std::string> mScrollback;
    std::vector<std::string> mHistory;
    int mHistoryIndex = -1;

    std::unordered_map<std::string, ConsoleCommand> mCommands;

    float mCursorBlink = 0.0f;
    int mScrollOffset = 0;

    static constexpr int MAX_SCROLLBACK = 256;
    static constexpr int MAX_HISTORY = 64;
};
