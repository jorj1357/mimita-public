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
#include "live-code/live-actor.h"
#include "live-code/live-journal.h"
#include "live-code/live-presentation.h"
#include "utils/time-format.h"

#include <cstdio>
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

    if (status.loaded) {
        ok &= check(LiveActor::available(), "actor module present", report);
        ActorStateV1 actor{};
        actor.id = 7;
        actor.kind = 1;
        actor.health = 100.0f;
        actor.maxHealth = 100.0f;
        actor.emotionConfidence = 0.5f;
        actor.emotionFear = 0.5f;
        actor.distanceToTarget = 3.0f;
        actor.tick = 123;
        ActorCommandV1 command{};
        ok &= check(LiveActor::chooseCommand(actor, command) &&
                        command.speedScale > 0.0f && command.speedScale <= 1.6f,
                    "actor chooseCommand", report);
        ok &= check(LiveActor::chooseRole(actor) != 0, "actor chooseRole", report);

        DamageNumberStyleV1 damageBase{};
        damageBase.scale = 1.0f;
        damageBase.endScale = 1.0f;
        damageBase.alpha = 1.0f;
        damageBase.lifetime = 1.0f;
        damageBase.moveSpeed = 1.0f;
        damageBase.visible = 1;
        std::snprintf(damageBase.text, sizeof(damageBase.text), "150");
        DamageNumberStyleV1 damageOut{};
        ok &= check(LivePresentation::formatDamage(damageBase, 150, 0u, damageOut) &&
                        damageOut.scale > damageBase.scale,
                    "presentation damage format", report);

        RocketTrailStyleV1 trailBase{};
        trailBase.size = 0.25f;
        trailBase.endSize = 0.8f;
        trailBase.enabled = 1;
        RocketTrailStyleV1 trailOut{};
        ok &= check(LivePresentation::rocketTrail(trailBase, trailOut) &&
                        trailOut.size > trailBase.size,
                    "presentation rocket trail", report);
    }

    HotReloadSystem::instance().unloadGameDLL();

    return ok;
}
