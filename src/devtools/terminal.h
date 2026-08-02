#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct GLFWwindow;
struct UITextInputState;

void registerTerminalBuiltins();

enum class CommandCategory {
    Uncategorized,
    Replay,
    NPC,
    Weapon,
    Inventory,
    Duel,
    Network,
    Debug,
    Player,
    Editor,
    UI,
    Physics
};

inline const char* categoryName(CommandCategory c) {
    switch (c) {
        case CommandCategory::Replay: return "Replay";
        case CommandCategory::NPC: return "NPC";
        case CommandCategory::Weapon: return "Weapon";
        case CommandCategory::Inventory: return "Inventory";
        case CommandCategory::Duel: return "Duel";
        case CommandCategory::Network: return "Network";
        case CommandCategory::Debug: return "Debug";
        case CommandCategory::Player: return "Player";
        case CommandCategory::Editor: return "Editor";
        case CommandCategory::UI: return "UI";
        case CommandCategory::Physics: return "Physics";
        default: return "Uncategorized";
    }
}

struct ConsoleCommand {
    std::string name;
    std::string description;
    std::string usage;
    std::function<void(const std::vector<std::string>& args)> fn;
    std::string dateAdded;
    CommandCategory category = CommandCategory::Uncategorized;
    std::vector<std::string> aliases;
};

class Terminal {
public:
    static Terminal& instance();

    void init(GLFWwindow* window);
    GLFWwindow* window() const { return mWindow; }
    void toggle();
    bool isOpen() const { return mOpen; }

    void handleChar(unsigned int codepoint);
    void handleKey(int key, int mods);
    void handleScroll(double yOffset);

    void render();

    void registerCommand(const ConsoleCommand& cmd);
    void registerCommand(const ConsoleCommand& cmd, const std::string& dateAdded);
    void registerCommand(const ConsoleCommand& cmd, CommandCategory category);
    void registerCommand(const ConsoleCommand& cmd, const std::string& dateAdded, CommandCategory category);
    void execute(const std::string& input);
    void addLog(const std::string& text);

    // Multi-step interactive input: the next submitted line is routed to the
    // callback (instead of being run as a command). Used by interactive flows
    // such as 'networkconfig save'. Pass an empty string to cancel/clear.
    void requestInput(const std::string& prompt,
                      std::function<void(const std::string&)> callback);
    bool hasPendingInput() const { return mPendingCallback != nullptr; }
    const std::string& pendingPrompt() const { return mPendingPrompt; }

    struct ReplayPickerEntry {
        std::string path;
        std::string filename;
        uint64_t fileSize = 0;
        uint32_t tickCount = 0;
        std::string dateStr;
        double durationSec = 0.0;
    };
    void startExportPicker();
    void closeExportPicker();
    bool isExportPickerActive() const { return mExportPickerActive; }
    void renderExportPicker();

private:
    bool mExportPickerActive = false;
    std::vector<ReplayPickerEntry> mExportPickerReplays;
    int mExportPickerIndex = 0;
    int mExportPickerScroll = 0;
    Terminal() = default;
    friend void registerTerminalBuiltins();
    friend void registerHelpCommands();
    friend void registerDebugToggleCommands();
    friend void registerConfigCommands();

    void executeCurrent();
    void addHistory(const std::string& input);

    // Text editing delegated to UITextInputState
    UITextInputState* textState();
    const UITextInputState* textState() const;

    GLFWwindow* mWindow = nullptr;
    bool mOpen = false;

    UITextInputState* mTextState = nullptr;
    std::string mHistorySavedLine;
    int mTabCycleIndex = -1;

    std::vector<std::string> mScrollback;
    std::vector<std::string> mHistory;
    int mHistoryIndex = -1;

    std::unordered_map<std::string, ConsoleCommand> mCommands;
    std::vector<std::string> mRegistrationOrder;

    std::string mPendingPrompt;
    std::function<void(const std::string&)> mPendingCallback;

    float mCursorBlink = 0.0f;
    int mScrollOffset = 0;

    static constexpr int MAX_SCROLLBACK = 256;
    static constexpr int MAX_HISTORY = 64;

    struct CachedCommand {
        const ConsoleCommand* cmd;
        std::string lowerName;
        std::string lowerDesc;
    };
    std::vector<CachedCommand> mCachedCommands;
    bool mCacheDirty = true;

    struct SearchResult {
        const ConsoleCommand* cmd;
        int score;
        std::vector<int> matchPositions;
    };
    std::vector<SearchResult> mSearchResults;
    std::string mLastSearchInput;
    std::string mGhostSuffix;
    int mSelectedResult = -1;

    void rebuildCache();
    void updateSearch();
    std::string computeGhostSuffix(const std::string& input) const;
    void drawAutocompleteMenu(float inputLineY, float lineHeight);
};
