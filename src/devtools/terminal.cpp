#include "terminal.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>

#include "config.h"
#include "devtools/dev-config.h"
#include "devtools/dev-overlay.h"
#include "devtools/account-config.h"
#include "input/input-commands.h"
#include "physics/config.h"
#include "replay/replay.h"

Terminal& Terminal::instance() {
    static Terminal t;
    return t;
}

void Terminal::init(GLFWwindow* window) {
    mWindow = window;
    addLog("[TERMINAL] initialized. type 'help' for commands.");

    registerTerminalBuiltins();

    registerTerminalBuiltins();

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
        "npc_spawnpoint",
        "Set the global NPC spawn point",
        "npc_spawnpoint <x> <y> <z>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 3) {
                Terminal::instance().addLog(
                    "Current spawn point: (" +
                    std::to_string(npcSpawnPoint.x) + ", " +
                    std::to_string(npcSpawnPoint.y) + ", " +
                    std::to_string(npcSpawnPoint.z) + ")");
                return;
            }
            npcSpawnPoint.x = std::stof(args[0]);
            npcSpawnPoint.y = std::stof(args[1]);
            npcSpawnPoint.z = std::stof(args[2]);
            Terminal::instance().addLog(
                "[NPC] Spawn point set to (" +
                std::to_string(npcSpawnPoint.x) + ", " +
                std::to_string(npcSpawnPoint.y) + ", " +
                std::to_string(npcSpawnPoint.z) + ")");
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
        mCursorPos = 0;
        mSelectionStart = -1;
        mHistoryIndex = -1;
        mHistorySavedLine.clear();
        mScrollOffset = 0;
        mTabCycleIndex = -1;
        printf("[TERMINAL] opened\n");
    } else {
        printf("[TERMINAL] closed\n");
    }
}

void Terminal::addLog(const std::string& text) {
    // Split multi-line strings into separate entries so each entry is exactly one visual line.
    // Otherwise uiDrawText's internal newline spacing (fontLineHeight*scale) differs from the
    // terminal's per-entry y advancement (lineHeight=22), causing overlapping text.
    size_t pos = 0, next;
    while ((next = text.find('\n', pos)) != std::string::npos) {
        std::string line = text.substr(pos, next - pos);
        if (!line.empty())
            mScrollback.push_back(line);
        pos = next + 1;
    }
    if (pos < text.size())
        mScrollback.push_back(text.substr(pos));
    if ((int)mScrollback.size() > MAX_SCROLLBACK)
        mScrollback.erase(mScrollback.begin(), mScrollback.begin() + ((int)mScrollback.size() - MAX_SCROLLBACK));
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

void Terminal::startExportPicker() {
    mExportPickerReplays.clear();
    mExportPickerIndex = 0;
    mExportPickerScroll = 0;

    std::vector<std::string> paths;
    std::error_code ec;
    const std::filesystem::path baseDir = "replays";
    if (std::filesystem::exists(baseDir, ec)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir, ec)) {
            if (ec || !entry.is_regular_file()) continue;
            const std::string name = entry.path().filename().string();
            if (name.find("-validation") != std::string::npos) continue;
            if ((name.size() > 5 && name.rfind(".json") == name.size() - 5) ||
                (name.size() > 11 && name.rfind(".mclip.json") == name.size() - 11)) {
                paths.push_back(entry.path().string());
            }
        }
    }

    // Sort by modified time ascending (oldest first)
    std::sort(paths.begin(), paths.end(), [](const std::string& a, const std::string& b) {
        return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
    });

    for (const std::string& p : paths) {
        ReplayPickerEntry e;
        e.path = p;
        e.filename = std::filesystem::path(p).filename().string();
        e.fileSize = std::filesystem::file_size(p, ec);

        // Try to load clip info for tick count
        ReplayClip clip;
        if (clip.load(p)) {
            e.tickCount = clip.header.tickCount;
            e.durationSec = (double)e.tickCount / 60.0;
        }

        // Format date
        auto ft = std::filesystem::last_write_time(p, ec);
        if (!ec) {
            auto sft = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ft - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t t = std::chrono::system_clock::to_time_t(sft);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M %p", &tm);
            e.dateStr = buf;
        }

        mExportPickerReplays.push_back(e);
    }

    addLog("[REPLAY PICKER] found " + std::to_string(mExportPickerReplays.size()) + " replays");
    if (!mExportPickerReplays.empty())
        mExportPickerIndex = (int)mExportPickerReplays.size() - 1; // start at newest
    mExportPickerActive = true;
    mExportPickerScroll = std::max(0, (int)mExportPickerReplays.size() - 20);
}

void Terminal::closeExportPicker() {
    mExportPickerActive = false;
    mExportPickerReplays.clear();
    mExportPickerIndex = 0;
    mExportPickerScroll = 0;
}

void Terminal::executeCurrent() {
    std::string input = mInputLine;
    addLog("] " + input);
    addHistory(input);

    // Multi-command: split on ';'
    size_t start = 0;
    while (start < input.size()) {
        while (start < input.size() && input[start] == ' ') start++;
        size_t end = input.find(';', start);
        if (end == std::string::npos) end = input.size();
        std::string cmd = input.substr(start, end - start);
        // Trim trailing spaces
        while (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();
        if (!cmd.empty()) {
            execute(cmd);
        }
        start = end + 1;
    }

    mInputLine.clear();
    mCursorPos = 0;
    mSelectionStart = -1;
    mHistoryIndex = -1;
    mHistorySavedLine.clear();
    mTabCycleIndex = -1;
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
    for (const std::string& alias : cmd.aliases)
    {
        ConsoleCommand a = cmd;
        a.name = alias;
        a.aliases.clear();
        mCommands[alias] = a;
        mRegistrationOrder.push_back(alias);
    }
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
    for (const std::string& alias : cmd.aliases)
    {
        ConsoleCommand a = cmd;
        a.name = alias;
        a.aliases.clear();
        a.dateAdded = dateAdded;
        mCommands[alias] = a;
        mRegistrationOrder.push_back(alias);
    }
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
    for (const std::string& alias : cmd.aliases)
    {
        ConsoleCommand a = cmd;
        a.name = alias;
        a.aliases.clear();
        a.category = category;
        mCommands[alias] = a;
        mRegistrationOrder.push_back(alias);
    }
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
    for (const std::string& alias : cmd.aliases)
    {
        ConsoleCommand a = cmd;
        a.name = alias;
        a.aliases.clear();
        a.dateAdded = dateAdded;
        a.category = category;
        mCommands[alias] = a;
        mRegistrationOrder.push_back(alias);
    }
}
