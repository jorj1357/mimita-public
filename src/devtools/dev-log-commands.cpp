#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>
#include "devtools/terminal.h"
#include "debug/log-manager.h"
#include "debug/structured-log.h"

namespace {
bool isEventSegment(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (name == "events.jsonl") return true;
    if (name.size() != std::string("events-000001.jsonl").size() ||
        name.rfind("events-", 0) != 0 ||
        name.compare(name.size() - 6, 6, ".jsonl") != 0)
        return false;
    for (size_t i = 7; i < 13; ++i) {
        if (name[i] < '0' || name[i] > '9') return false;
    }
    return true;
}

std::vector<std::filesystem::path> eventSegmentsInRun(
    const std::filesystem::path& current)
{
    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(current.parent_path(), ec)) {
        if (!ec && entry.is_regular_file(ec) && isEventSegment(entry.path()))
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}
}

void registerDevLogCommands()
{
    Terminal::instance().registerCommand({
        "log_info", "Print current log file info", "log_info",
        [](const std::vector<std::string>&) {
            auto& lm = LogManager::instance();
            char buf[512];
            snprintf(buf, sizeof(buf), "Current Log:\n%s\n\nLog Count:\n%d\nMax Logs:\n30",
                     lm.path().c_str(), lm.fileCount());
            Terminal::instance().addLog(buf);
            printf("[LOG INFO] Current: %s\n", lm.path().c_str());
            printf("[LOG INFO] Count: %d / 30\n", lm.fileCount());
        }
    });

    Terminal::instance().registerCommand({
        "log_open", "Report and select the active segmented events stream", "log_open",
        [](const std::vector<std::string>&) {
            auto& terminal = Terminal::instance();
            const std::string configuredPath = StructuredLogger::instance().eventsPath();
            if (configuredPath.empty()) {
                terminal.addLog("[LOG] active events.jsonl path is not initialized");
                return;
            }

            std::error_code ec;
            const std::filesystem::path path =
                std::filesystem::absolute(std::filesystem::path(configuredPath), ec);
            const auto segments = eventSegmentsInRun(path);
            if (ec || segments.empty()) {
                terminal.addLog("[LOG] active events stream was not found: " + configuredPath);
                return;
            }

            // Inspect every numbered segment so a client-only file cannot be
            // mistaken for the complete client+server session record.
            std::string line;
            long long lines = 0, invalid = 0, npcEvents = 0;
            long long deathEvents = 0, ragdollEvents = 0;
            bool sawClient = false, sawServer = false, sawStarted = false;
            std::map<std::string, int> eventCounts;
            std::map<std::string, int> categoryCounts;
            std::map<std::string, std::map<std::string, int>> processCounts;
            std::vector<std::string> startupIdentities;
            for (const auto& segment : segments) {
                std::ifstream in(segment);
                if (!in.is_open()) continue;
                while (std::getline(in, line)) {
                    if (line.empty()) continue;
                    ++lines;
                    nlohmann::json record;
                    try {
                        record = nlohmann::json::parse(line);
                    } catch (...) {
                        ++invalid;
                        continue;
                    }
                    const std::string process = record.value("process", "");
                    if (process == "client") sawClient = true;
                    else if (process == "server") sawServer = true;
                    if (!process.empty()) {
                        ++processCounts[process][record.value("event", "")];
                    }
                    const std::string event = record.value("event", "");
                    if (event == "logger.started") sawStarted = true;
                    if (event == "logger.started") {
                        const auto fields = record.value("fields", nlohmann::json::object());
                        startupIdentities.push_back(
                            "  " + record.value("process", "unknown") +
                            " pid=" + std::to_string(record.value("pid", 0ULL)) +
                            " exe=" + fields.value("executable", "(not recorded)"));
                    }
                    if (!event.empty()) {
                        ++eventCounts[event];
                        if (event.rfind("npc.", 0) == 0) ++npcEvents;
                        if (event.rfind("death.", 0) == 0) ++deathEvents;
                        if (event.rfind("ragdoll.", 0) == 0) ++ragdollEvents;
                    }
                    const std::string category = record.value("category", "");
                    if (!category.empty()) ++categoryCounts[category];
                }
            }

            std::string report;
            report += "events stream:\n" + path.parent_path().string() +
                      "\nsegments: " + std::to_string(segments.size()) + "\n\n";
            report += "processes:\n";
            report += std::string("client: ") + (sawClient ? "yes" : "no") + "\n";
            report += std::string("server: ") + (sawServer ? "yes" : "no") + "\n";
            report += std::string("logger.started: ") + (sawStarted ? "yes" : "no") +
                      "  (client and server should both appear above)\n\n";
            report += "records: " + std::to_string(lines) +
                      "  invalid_json=" + std::to_string(invalid) +
                      "  npc_events=" + std::to_string(npcEvents) +
                      "  death_events=" + std::to_string(deathEvents) +
                      "  ragdoll_events=" + std::to_string(ragdollEvents) + "\n\n";

            report += "records by process:\n";
            for (const auto& process : processCounts) {
                long long processRecords = 0;
                for (const auto& event : process.second)
                    processRecords += event.second;
                report += "  " + process.first + ": " +
                          std::to_string(processRecords) + " records\n";
            }
            report += "startup identities:\n";
            if (startupIdentities.empty()) {
                report += "  (none)\n";
            } else {
                for (const auto& identity : startupIdentities)
                    report += identity + "\n";
            }
            if (deathEvents == 0 && ragdollEvents == 0)
                report += "  WARNING: this journal currently contains no death or ragdoll events.\n";
            report += "\n";

            report += "npc events:\n";
            bool anyNpc = false;
            for (const auto& kv : eventCounts) {
                if (kv.first.rfind("npc.", 0) != 0) continue;
                report += "  " + kv.first + ": " + std::to_string(kv.second) + "\n";
                anyNpc = true;
            }
            if (!anyNpc) report += "  (none)\n";

            report += "\ncategories:\n";
            for (const auto& kv : categoryCounts)
                report += "  " + kv.first + ": " + std::to_string(kv.second) + "\n";

            terminal.addLog(report);
            printf("[LOG OPEN]\n%s", report.c_str());

            const std::string selectArgs = "/select,\"" + path.string() + "\"";
            const HINSTANCE result = ShellExecuteA(
                nullptr, "open", "explorer.exe", selectArgs.c_str(), nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32)
                terminal.addLog("[LOG] could not open Explorer for: " + path.string());
        }
    });

    Terminal::instance().registerCommand({
        "log_folder", "Open logs folder in Explorer", "log_folder",
        [](const std::vector<std::string>&) {
            ShellExecuteA(NULL, "open", "logs", NULL, NULL, SW_SHOWNORMAL);
        }
    });

    Terminal::instance().registerCommand({
        "log_flush", "Force flush log to disk", "log_flush",
        [](const std::vector<std::string>&) {
            LogManager::instance().flush();
            printf("[LOG] flushed\n");
        }
    });

    Terminal::instance().registerCommand({
        "log_test", "Write test message to log", "log_test",
        [](const std::vector<std::string>&) {
            printf("[LOG TEST] hello world\n");
            Terminal::instance().addLog("[LOG TEST] hello world");
        }
    });
}
