#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include "devtools/terminal.h"
#include "debug/log-manager.h"
#include "debug/structured-log.h"

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
        "log_open", "Select the active events.jsonl in Windows Explorer", "log_open",
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
            if (ec || !std::filesystem::is_regular_file(path, ec)) {
                terminal.addLog("[LOG] active events.jsonl was not found: " + configuredPath);
                return;
            }

            const std::string selectArgs = "/select,\"" + path.string() + "\"";
            const HINSTANCE result = ShellExecuteA(
                nullptr, "open", "explorer.exe", selectArgs.c_str(), nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32) {
                terminal.addLog("[LOG] could not open Explorer for: " + path.string());
                return;
            }
            terminal.addLog("[LOG] selected active events.jsonl: " + path.string());
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
