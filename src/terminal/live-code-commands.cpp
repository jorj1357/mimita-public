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
#include "live-code/live-operation-queue.h"
#include "network/server-live-collaboration.h"

#include <string>
#include <cstdint>
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

std::uint64_t parseId(const std::string& value)
{
    try { return std::stoull(value); }
    catch (...) { return 0; }
}

void printQueue()
{
    Terminal::instance().addLog("[LIVE QUEUE] pending=" +
        std::to_string(LiveCollaboration::LiveOperationQueue::instance().size()));
}

void printRevisions(const std::string& resource)
{
    const auto id = parseId(resource);
    const auto active = LiveCollaboration::ServerLiveCollaboration::instance().activeRevision(id);
    Terminal::instance().addLog("[LIVE REVISIONS] resource=" + resource +
        " active=" + std::to_string(active));
    for (const auto& revision :
         LiveCollaboration::ServerLiveCollaboration::instance().revisions(id)) {
        Terminal::instance().addLog(
            "  revision=" + std::to_string(revision.revisionId) +
            " parent=" + std::to_string(revision.parentRevisionId) +
            " hash=" + std::to_string(revision.contentHash) +
            " state=" + LiveCollaboration::revisionStateName(revision.state));
    }
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

    Terminal::instance().registerCommand(
        {
            "live", "Inspect live collaboration queue, revisions, and rollback",
            "live [queue|revisions <resourceId>|rollback <resourceId> <revisionId>]",
            [](const std::vector<std::string>& args) {
                const std::string action = args.empty() ? "queue" : args[0];
                if (action == "queue") {
                    printQueue();
                    return;
                }
                if (action == "revisions" && args.size() >= 2) {
                    printRevisions(args[1]);
                    return;
                }
                if (action == "rollback" && args.size() >= 3) {
                    const auto result = LiveCollaboration::ServerLiveCollaboration::instance()
                        .rollback(parseId(args[1]), parseId(args[2]), 0);
                    Terminal::instance().addLog(std::string("[LIVE ROLLBACK] ") +
                        (result.accepted ? "accepted" : "rejected") +
                        " reason=" + result.reason);
                    return;
                }
                Terminal::instance().addLog(
                    "[LIVE] usage: live [queue|revisions <resourceId>|rollback <resourceId> <revisionId>]");
            },
        },
        "2026-09-20", CommandCategory::Debug);
}
