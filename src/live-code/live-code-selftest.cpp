// 09 12 2026
/* purpose
* Implements the headless live-code self-test.
* Proves UTC formatting, SHA-256 hashing, JSONL journal writing, and the
* GameAPI load + ABI + self-test path that runs before any graphics startup.
* Does NOT own gameplay or the hot-reload activation policy.
*/
#include "live-code/live-code-selftest.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/code-hash.h"
#include "live-code/live-journal.h"
#include "utils/time-format.h"

#include <fstream>
#include <string>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runLiveCodeSelfTest(std::string& report)
{
    bool ok = true;

    const std::string timestamp = MiMitaTime::utcIso8601Millis();
    ok &= check(timestamp.size() == 24 && timestamp[10] == 'T' &&
                    timestamp.back() == 'Z',
                "utc millisecond format", report);

    const std::string emptyHash = LiveCodeHash::sha256Bytes("", 0);
    ok &= check(
        emptyHash ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "sha256 empty string", report);

    LiveEventJournal::instance().init();
    const std::string journalPath = LiveEventJournal::instance().path();
    ok &= check(LiveEventJournal::instance().active(), "journal active", report);

    LiveEventJournal::Fields fields;
    fields.result = "selftest";
    LiveEventJournal::instance().record("test_finished", fields);

    std::ifstream journal(journalPath);
    std::string lastLine;
    std::string line;
    while (std::getline(journal, line)) {
        if (!line.empty())
            lastLine = line;
    }
    ok &= check(lastLine.find("\"type\":\"test_finished\"") != std::string::npos,
                "journal line written", report);
    ok &= check(lastLine.find("\"ts_utc\":") != std::string::npos,
                "journal utc field", report);
    ok &= check(!lastLine.empty() && lastLine.front() == '{' && lastLine.back() == '}',
                "journal valid JSON object", report);
    LiveEventJournal::instance().shutdown();

    HotReloadSystem::instance().startup();
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    report += "  activeGeneration=" + std::to_string(status.activeGeneration) + "\n";
    report += "  activeHash=" + status.activeHash + "\n";
    report += "  reloadCount=" + std::to_string(status.reloadCount) + "\n";
    ok &= check(status.loaded, "GameAPI load + ABI + self-test", report);
    HotReloadSystem::instance().unloadGameDLL();

    return ok;
}
