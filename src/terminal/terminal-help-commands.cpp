#include "devtools/terminal.h"
#include "devtools/command-search.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

void registerHelpCommands()
{
    auto& term = Terminal::instance();

    term.registerCommand({
        "help",
        "Show command info. help <name> for details, help <category> to filter.",
        "help [name|category]",
        [](const std::vector<std::string>& args) {
            auto& t = Terminal::instance();
            if (args.empty()) {
                std::vector<const ConsoleCommand*> commands;
                commands.reserve(t.mCommands.size());
                for (const auto& pair : t.mCommands) commands.push_back(&pair.second);
                std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                    return a->name < b->name;
                });
                t.addLog("Available commands (help <name> for details, help <Category> to filter):");
                std::string lastCat;
                for (const ConsoleCommand* cmd : commands) {
                    std::string cat = categoryName(cmd->category);
                    if (cat != lastCat) {
                        char hdr[64];
                        snprintf(hdr, sizeof(hdr), "--- %s ---", cat.c_str());
                        t.addLog(hdr);
                        lastCat = cat;
                    }
                    char buf[256];
                    snprintf(buf, sizeof(buf), "  %-24s %s", cmd->name.c_str(), cmd->description.c_str());
                    t.addLog(buf);
                }
                return;
            }
            std::string a = args[0];
            std::string aLower = a;
            std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
            auto it = t.mCommands.find(a);
            if (it != t.mCommands.end()) {
                const ConsoleCommand& cmd = it->second;
                t.addLog("--- " + cmd.name + " ---");
                t.addLog("  Description: " + cmd.description);
                t.addLog("  Usage: " + cmd.usage);
                t.addLog("  Category: " + std::string(categoryName(cmd.category)));
                if (!cmd.dateAdded.empty())
                    t.addLog("  Added: " + cmd.dateAdded);
                return;
            }
            for (int c = (int)CommandCategory::Uncategorized; c <= (int)CommandCategory::UI; ++c) {
                std::string cn = categoryName((CommandCategory)c);
                std::transform(cn.begin(), cn.end(), cn.begin(), ::tolower);
                if (cn == aLower) {
                    std::string filterCat = categoryName((CommandCategory)c);
                    std::vector<const ConsoleCommand*> commands;
                    commands.reserve(t.mCommands.size());
                    for (const auto& pair : t.mCommands) commands.push_back(&pair.second);
                    std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                        return a->name < b->name;
                    });
                    t.addLog("--- " + filterCat + " ---");
                    for (const ConsoleCommand* cmd : commands) {
                        if (categoryName(cmd->category) == filterCat) {
                            char buf[256];
                            snprintf(buf, sizeof(buf), "  %-24s %s", cmd->name.c_str(), cmd->description.c_str());
                            t.addLog(buf);
                        }
                    }
                    return;
                }
            }
            t.rebuildCache();
            MatchResult mr;
            std::vector<const ConsoleCommand*> fuzzyResults;
            for (const auto& cc : t.mCachedCommands) {
                if (fuzzyMatch(aLower, cc.lowerName, mr) > 0)
                    fuzzyResults.push_back(cc.cmd);
            }
            std::sort(fuzzyResults.begin(), fuzzyResults.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                return a->name < b->name;
            });
            if (!fuzzyResults.empty()) {
                t.addLog("No exact match found for \"" + a + "\". Did you mean:");
                for (size_t i = 0; i < std::min(fuzzyResults.size(), (size_t)20); i++) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "  %-24s %s",
                             fuzzyResults[i]->name.c_str(),
                             fuzzyResults[i]->description.c_str());
                    t.addLog(buf);
                }
                if (fuzzyResults.size() > 20) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "  ... and %zu more", fuzzyResults.size() - 20);
                    t.addLog(buf);
                }
            } else {
                t.addLog("Unknown command or category: " + a + ". Try 'help' for a list.");
            }
        }
    }, CommandCategory::UI);

    term.registerCommand({
        "help2",
        "List commands grouped by category (newest at bottom of each)",
        "help2",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            std::unordered_map<std::string, std::vector<const ConsoleCommand*>> byCat;
            std::vector<std::string> catOrder;
            for (const auto& pair : t.mCommands) {
                std::string cat = categoryName(pair.second.category);
                if (byCat.find(cat) == byCat.end()) catOrder.push_back(cat);
                byCat[cat].push_back(&pair.second);
            }
            for (auto& kv : byCat) {
                std::sort(kv.second.begin(), kv.second.end(),
                    [&t](const ConsoleCommand* a, const ConsoleCommand* b) {
                        auto ia = std::find(t.mRegistrationOrder.begin(), t.mRegistrationOrder.end(), a->name);
                        auto ib = std::find(t.mRegistrationOrder.begin(), t.mRegistrationOrder.end(), b->name);
                        return (ia != t.mRegistrationOrder.end() && ib != t.mRegistrationOrder.end()) ? ia < ib : a->name < b->name;
                    });
            }
            t.addLog("=== COMMANDS BY CATEGORY ===");
            for (const auto& cat : catOrder) {
                char hdr[64];
                snprintf(hdr, sizeof(hdr), "--- %s ---", cat.c_str());
                t.addLog(hdr);
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
                    t.addLog(buf);
                }
            }
        }
    }, CommandCategory::UI);

    term.registerCommand({
        "help_recent",
        "Show last 20 added commands",
        "help_recent",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(t.mCommands.size());
            for (const auto& pair : t.mCommands) {
                if (!pair.second.dateAdded.empty())
                    commands.push_back(&pair.second);
            }
            std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                if (a->dateAdded != b->dateAdded)
                    return a->dateAdded > b->dateAdded;
                return a->name < b->name;
            });
            int count = 0;
            t.addLog("=== RECENTLY ADDED COMMANDS ===");
            for (const ConsoleCommand* command : commands) {
                if (count >= 20) break;
                char buf[256];
                snprintf(buf, sizeof(buf), "  [NEW] %-20s %s  (added: %s)",
                         command->name.c_str(), command->description.c_str(),
                         command->dateAdded.c_str());
                t.addLog(std::string(buf));
                count++;
            }
        }
    });

    term.registerCommand({
        "help_today",
        "Show commands added in the current session",
        "help_today",
        [](const std::vector<std::string>&) {
            auto& t = Terminal::instance();
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(t.mCommands.size());
            for (const auto& pair : t.mCommands) {
                if (!pair.second.dateAdded.empty())
                    commands.push_back(&pair.second);
            }
            std::sort(commands.begin(), commands.end(), [](const ConsoleCommand* a, const ConsoleCommand* b) {
                if (a->dateAdded != b->dateAdded)
                    return a->dateAdded > b->dateAdded;
                return a->name < b->name;
            });
            t.addLog("=== ALL DATED COMMANDS ===");
            for (const ConsoleCommand* command : commands) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  [%s] %-20s %s",
                         command->dateAdded.c_str(), command->name.c_str(),
                         command->description.c_str());
                t.addLog(std::string(buf));
            }
        }
    });

    term.registerCommand({
        "help_since",
        "Show commands added after a date (e.g. help_since 2026-06-10)",
        "help_since <YYYY-MM-DD>",
        [](const std::vector<std::string>& args) {
            auto& t = Terminal::instance();
            std::string since = args.empty() ? "2026-06-01" : args[0];
            std::vector<const ConsoleCommand*> commands;
            commands.reserve(t.mCommands.size());
            for (const auto& pair : t.mCommands) {
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
            t.addLog(header);
            for (const ConsoleCommand* command : commands) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  [NEW] %-20s %s  (added: %s)",
                         command->name.c_str(), command->description.c_str(),
                         command->dateAdded.c_str());
                t.addLog(std::string(buf));
            }
            if (commands.empty()) {
                char buf[128];
                snprintf(buf, sizeof(buf), "  (no commands found since %s)", since.c_str());
                t.addLog(buf);
            }
        }
    });
}
