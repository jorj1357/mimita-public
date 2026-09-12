// 09 12 2026
/* purpose
* Implements the live-code terminal command surface.
* Reports hot-reload generation status and requests a rollback to the previous
* active generation.
* Does NOT compile, load, or validate modules itself.
*/
#include "terminal/live-code-commands.h"

#include "devtools/terminal.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-journal.h"

#include <string>
#include <vector>

namespace {

void printStatus()
{
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    Terminal::instance().addLog("[LIVE CODE] loaded=" + std::string(status.loaded ? "yes" : "no") +
        " activeGen=" + std::to_string(status.activeGeneration) +
        " previousGen=" + std::to_string(status.previousGeneration));
    Terminal::instance().addLog("[LIVE CODE] buildRunning=" +
        std::string(status.buildRunning ? "yes" : "no") +
        " candidateReady=" + std::string(status.candidateReady ? "yes" : "no") +
        " reloadCount=" + std::to_string(status.reloadCount));
    Terminal::instance().addLog("[LIVE CODE] activeHash=" +
        (status.activeHash.empty() ? std::string("(none)") : status.activeHash));
    Terminal::instance().addLog("[LIVE CODE] observedHash=" +
        (status.observedSourceHash.empty() ? std::string("(none)") : status.observedSourceHash));
    if (!status.lastError.empty())
        Terminal::instance().addLog("[LIVE CODE] lastError=" + status.lastError);
    Terminal::instance().addLog("[LIVE CODE] journal=" + LiveEventJournal::instance().path());
}

} // namespace

void registerLiveCodeCommands()
{
    Terminal::instance().registerCommand(
        {
            "hotreload", "Inspect or roll back live replaceable code generations",
            "hotreload [status|rollback]",
            [](const std::vector<std::string>& args) {
                const std::string action = args.empty() ? "status" : args[0];
                if (action == "status") {
                    printStatus();
                    return;
                }
                if (action == "rollback") {
                    if (HotReloadSystem::instance().rollback())
                        Terminal::instance().addLog("[LIVE CODE] rollback requested");
                    else
                        Terminal::instance().addLog(
                            "[LIVE CODE] rollback unavailable: no previous generation");
                    return;
                }
                Terminal::instance().addLog(
                    "[LIVE CODE] usage: hotreload [status|rollback]");
            },
        },
        "2026-09-12", CommandCategory::Debug);
}
