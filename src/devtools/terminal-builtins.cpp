#include "devtools/terminal.h"
#include "devtools/command-search.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

void registerHelpCommands();
void registerDebugToggleCommands();
void registerConfigCommands();

void registerTerminalBuiltins() {
    auto& term = Terminal::instance();

    registerHelpCommands();
    registerDebugToggleCommands();
    registerConfigCommands();

    term.registerCommand({
        "commands",
        "List all registered command names",
        "commands",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            t.addLog("Registered commands (" + std::to_string(t.mCommands.size()) + " total):");
            std::vector<std::string> names;
            for (const auto& pair : t.mCommands)
                names.push_back(pair.first);
            std::sort(names.begin(), names.end());
            std::string line;
            for (size_t i = 0; i < names.size(); ++i) {
                if (!line.empty()) line += "  ";
                line += names[i];
                if (line.size() > 100 || i == names.size() - 1) {
                    t.addLog("  " + line);
                    line.clear();
                }
            }
        }
    });

    term.registerCommand({
        "clear",
        "Clear the terminal scrollback",
        "clear",
        [](const std::vector<std::string>&) {
            Terminal::instance().mScrollback.clear();
        }
    });

    term.registerCommand({
        "command_stats",
        "Show command search statistics",
        "command_stats",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            t.addLog("--- Command Stats ---");
            char buf[256];
            snprintf(buf, sizeof(buf), "  registered commands: %zu", t.mCommands.size());
            t.addLog(buf);
            snprintf(buf, sizeof(buf), "  cache entries: %zu", t.mCachedCommands.size());
            t.addLog(buf);
            snprintf(buf, sizeof(buf), "  search results: %zu", t.mSearchResults.size());
            t.addLog(buf);
            snprintf(buf, sizeof(buf), "  selected result: %d", t.mSelectedResult);
            t.addLog(buf);
        }
    });

    term.registerCommand({
        "cmd",
        "Open command palette (terminal search mode)",
        "cmd",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[OK] Command palette ready — start typing to search commands");
        }
    });

    term.registerCommand({
        "palette",
        "Open command palette (terminal search mode)",
        "palette",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[OK] Command palette ready — start typing to search commands");
        }
    });
}
