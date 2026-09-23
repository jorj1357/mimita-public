// 09 23 2026
/* purpose
* Headless self-test for the server journal correlation fields and the exit-cause
* classifier. Proves the new Fields are emitted/omitted correctly and the
* classifier distinguishes external termination, clean shutdown, non-zero exit,
// and an unobserved cause.
* Does NOT own transport or the live server loop.
*/
#include "live-code/server-journal-selftest.h"

#include <fstream>
#include <string>

#include "live-code/live-journal.h"
#include "live-code/server-exit-cause.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

bool fileContains(const std::string& path, const std::string& needle)
{
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

} // namespace

bool runServerJournalSelfTest(std::string& report)
{
    bool ok = true;
    LiveEventJournal::instance().init();
    const std::string path = LiveEventJournal::instance().path();
    ok &= check(!path.empty(), "journal opened", report);

    // ── Correlation fields are emitted ────────────────────────────────
    {
        LiveEventJournal::Fields f;
        f.tick = 4242;
        f.clientTick = 4200;
        f.connectionId = 7;
        f.requestId = 99;
        f.entityId = 12345;
        f.serverGeneration = 1;
        f.hotGeneration = 2;
        f.serverHash = "serverhash";
        f.hotHash = "hothash";
        f.result = "k";
        f.extra = "\"schema_id\":42";
        LiveEventJournal::instance().record("server.test_event", f);
    }
    ok &= check(fileContains(path, "server.test_event"), "event recorded", report);
    ok &= check(fileContains(path, "\"client_tick\":4200"), "client_tick emitted", report);
    ok &= check(fileContains(path, "\"connection_id\":7"), "connection_id emitted", report);
    ok &= check(fileContains(path, "\"request_id\":99"), "request_id emitted", report);
    ok &= check(fileContains(path, "\"entity_id\":12345"), "entity_id emitted", report);
    ok &= check(fileContains(path, "\"server_generation\":1"),
                "server_generation emitted", report);
    ok &= check(fileContains(path, "\"hot_generation\":2"), "hot_generation emitted",
                report);
    ok &= check(fileContains(path, "\"server_hash\":\"serverhash\""),
                "server_hash emitted", report);
    ok &= check(fileContains(path, "\"hot_hash\":\"hothash\""), "hot_hash emitted", report);
    ok &= check(fileContains(path, "\"schema_id\":42"), "extra fragment emitted", report);

    // ── Zero correlation ids are omitted (compact client lines) ───────
    {
        LiveEventJournal::Fields f;
        f.result = "compact";
        LiveEventJournal::instance().record("server.test_compact", f);
    }
    ok &= check(!fileContains(path, "server.test_compact") ||
                    !fileContains(path, "\"connection_id\":0"),
                "zero numeric ids are omitted", report);

    // ── Exit-cause classifier matrix ──────────────────────────────────
    using LiveCode::ServerExitCause;
    using LiveCode::classifyServerExit;
    ok &= check(classifyServerExit(false, 0, true) == ServerExitCause::ExternalTermination,
                "external termination classified", report);
    ok &= check(classifyServerExit(true, 0, false) == ServerExitCause::CleanShutdown,
                "clean shutdown classified", report);
    ok &= check(classifyServerExit(false, 3, false) == ServerExitCause::NonZeroExit,
                "non-zero exit classified", report);
    ok &= check(classifyServerExit(false, 0, false) == ServerExitCause::Unknown,
                "zero exit without clean record is not assumed clean", report);
    ok &= check(std::string(LiveCode::serverExitCauseName(ServerExitCause::Unknown)) ==
                    "unknown",
                "cause names are stable", report);

    return ok;
}
