// 09 12 2026
/* purpose
* Implements the headless live-code self-test.
* Proves UTC formatting, SHA-256 hashing, JSONL journal writing, and the
* GameAPI load + ABI + self-test path that runs before any graphics startup.
* Does NOT own gameplay or the hot-reload activation policy.
*/
#include "live-code/live-code-selftest.h"

#include "hot-reload/hot-reload-system.h"
#include "debug/structured-log.h"
#include "live-code/code-hash.h"
#include "live-code/live-actor.h"
#include "live-code/live-behavior.h"
#include "live-code/live-gameplay.h"
#include "live-code/live-journal.h"
#include "live-code/live-presentation.h"
#include "utils/time-format.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
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

    // ── Events JSONL self-test ──────────────────────────────────────────────
    // One events.jsonl, one clean record per message, no category .txt, every
    // line independent JSON, strictly increasing seq, UTC Z timestamps, and
    // bounded duplicate aggregation.
    {
        StructuredLogger::instance().init();
        const std::string eventsPath = StructuredLogger::instance().eventsPath();
        ok &= check(!eventsPath.empty(), "events.jsonl created", report);
        ok &= check(eventsPath.find("events.jsonl") != std::string::npos,
                    "events.jsonl path is jsonl", report);
        {
            // The run directory must contain only the JSONL stream.
            const std::filesystem::path runDir =
                std::filesystem::path(eventsPath).parent_path();
            bool onlyJsonl = true;
            std::error_code dirEc;
            for (const auto& entry :
                 std::filesystem::directory_iterator(runDir, dirEc)) {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().filename().string() != "events.jsonl")
                    onlyJsonl = false;
            }
            ok &= check(onlyJsonl, "no category .txt files", report);
        }

        // 500 identical events must collapse to one summary with count 500.
        for (int i = 0; i < 500; ++i) {
            debug::Event ev;
            ev.category = "LEGACY";
            ev.name = "selftest.repeat";
            ev.level = debug::Level::Info;
            ev.message = "repeated";
            ev.aggregationKey = "SELFTEST:repeat";
            debug::logEvent(ev);
        }
        debug::Event other;
        other.category = "NETWORK";
        other.name = "selftest.keychange";
        other.level = debug::Level::Info;
        other.message = "key change flushes the prior bucket";
        other.aggregationKey = "SELFTEST:keychange";
        debug::logEvent(other);
        debug::flushEvents();

        // Errors must never be aggregated away.
        debug::Event err;
        err.category = "EXECUTABLE";
        err.name = "selftest.error";
        err.level = debug::Level::Error;
        err.message = "error must stand alone";
        debug::logEvent(err);

        StructuredLogger::instance().shutdown();

        std::ifstream events(eventsPath);
        uint64_t lastSeq = 0;
        bool seqMonotonic = true;
        bool allJson = true;
        bool utcZ = true;
        bool sawSummary500 = false;
        bool sawError = false;
        uint32_t lineCount = 0;
        if (events.is_open()) {
            while (std::getline(events, line)) {
                if (line.empty())
                    continue;
                ++lineCount;
                allJson &= line.front() == '{' && line.back() == '}';
                try {
                    const nlohmann::json record = nlohmann::json::parse(line);
                    const uint64_t seq = record.value("seq", (uint64_t)0);
                    if (seq <= lastSeq)
                        seqMonotonic = false;
                    lastSeq = seq;
                    const std::string wall = record.value("wall_time", std::string());
                    if (wall.empty() || wall.back() != 'Z')
                        utcZ = false;
                    if (record.value("event", std::string()) == "selftest.repeat.summary" &&
                        record.value("count", 0) == 500)
                        sawSummary500 = true;
                    if (record.value("event", std::string()) == "selftest.error")
                        sawError = record.value("level", std::string()) == "ERROR";
                } catch (...) {
                    allJson = false;
                }
            }
        }
        ok &= check(lineCount > 0, "events.jsonl has records", report);
        ok &= check(allJson, "every line is valid JSON", report);
        ok &= check(seqMonotonic, "seq strictly increases", report);
        ok &= check(utcZ, "timestamps are UTC with Z", report);
        ok &= check(sawSummary500, "500 repeats -> one summary count=500", report);
        ok &= check(sawError, "errors are never aggregated", report);
    }

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

        RocketFlightStateV1 flightState{};
        RocketFlightParamsV1 flightBase{};
        flightBase.speedScale = 1.0f;
        flightBase.lifetime = 5.0f;
        RocketFlightParamsV1 flightOut{};
        // Assert the policy is callable and returns a usable result. Do NOT pin
        // the tuned output: the developer edits this policy live on purpose.
        ok &= check(LiveGameplay::rocketFlight(flightState, flightBase, flightOut) &&
                        std::isfinite(flightOut.speedScale) && flightOut.speedScale > 0.0f,
                    "gameplay rocket flight params", report);

        // Generic behavior path: the kernel emits a damage-policy event and the
        // hot behavior must handle it. Do NOT pin the returned damage value.
        DamagePolicyV1 policy{};
        policy.baseDamage = 123;
        policy.outDamage = 123;
        policy.source = GAME_DAMAGE_SOURCE_EXPLOSION;
        const bool handled = LiveBehavior::dispatchDamagePolicy(policy, 42);
        ok &= check(handled && policy.handled == 1 &&
                        std::isfinite((float)policy.outDamage) && policy.outDamage >= 0,
                    "hot damage policy dispatch", report);

        // ── Hot-module log capability round-trip ───────────────────────────
        // Proves the exact path the collision package uses: a hot caller
        // resolves `log.event` through the gameplay context and the record
        // lands in the same events.jsonl, so collision diagnostics are
        // observable live while the game runs.
        StructuredLogger::instance().init();
        const std::string capEventsPath = StructuredLogger::instance().eventsPath();
        bool emitted = false;
        if (GameplayContextV1* ctx = LiveBehavior::hostContext(1)) {
            auto logFn = reinterpret_cast<GameLogEventFn>(
                ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
            if (logFn) {
                GameLogEventV1 ev{};
                ev.level = 2u;
                ev.simulationTick = 1u;
                // NETWORK is enabled at "important" by default, so this proves
                // delivery independently of the COLLISION level setting.
                std::snprintf(ev.category, sizeof(ev.category), "NETWORK");
                std::snprintf(ev.name, sizeof(ev.name), "selftest.capability_log");
                std::snprintf(ev.message, sizeof(ev.message),
                              "hot log capability round-trip");
                std::snprintf(ev.result, sizeof(ev.result), "ok");
                logFn(ctx->host, &ev);
                emitted = true;
            }
        }
        StructuredLogger::instance().shutdown();
        ok &= check(emitted, "log.event capability resolves", report);
        bool sawCapabilityLog = false;
        std::ifstream probe(capEventsPath);
        while (std::getline(probe, line)) {
            if (line.find("selftest.capability_log") != std::string::npos)
                sawCapabilityLog = true;
        }
        ok &= check(sawCapabilityLog, "hot capability log reached events.jsonl",
                    report);
    }

    HotReloadSystem::instance().unloadGameDLL();

    return ok;
}
