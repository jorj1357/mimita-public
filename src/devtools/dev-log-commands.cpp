#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include "devtools/terminal.h"
#include "debug/log-manager.h"

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
        "log_open", "Open current log in default editor", "log_open",
        [](const std::vector<std::string>&) {
            std::string p = LogManager::instance().path();
            if (!p.empty()) {
                ShellExecuteA(NULL, "open", p.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
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
