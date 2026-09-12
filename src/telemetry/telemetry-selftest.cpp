// 09 12 2026
/* purpose
* Implements the telemetry self-test.
* Does NOT own systems.
*/
#include "telemetry/telemetry-selftest.h"

#include "telemetry/telemetry.h"

#include <string>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runTelemetrySelfTest(std::string& report)
{
    bool ok = true;
    Telemetry::Registry& registry = Telemetry::Registry::instance();
    registry.reset();
    registry.beginFrame(1000, 7, "hash-abc");

    for (int i = 0; i < 3; ++i) {
        MIMITA_TELEMETRY_SCOPE("TelemetrySelfTestScope");
    }

    const Telemetry::ScopeStats* scope = registry.find("TelemetrySelfTestScope");
    ok &= check(scope != nullptr, "scope recorded", report);
    if (scope) {
        ok &= check(scope->calls == 3, "call count increments", report);
        ok &= check(scope->maxNs > 0 && scope->avgNs <= scope->maxNs,
                    "timing nonnegative and avg<=max", report);
        ok &= check(scope->generation == 7 && scope->codeHash == "hash-abc",
                    "generation/hash attached", report);
        ok &= check(scope->lastTick == 1000, "last tick attached", report);
    }

    Telemetry::EntityCounters delta;
    delta.updates = 5;
    delta.renderSubmissions = 2;
    delta.lastTouchedTick = 1000;
    registry.addEntityCounter(42, delta);
    Telemetry::EntityCounters counters;
    ok &= check(registry.entityCounters(42, counters) && counters.updates == 5 &&
                    counters.renderSubmissions == 2,
                "entity counters update", report);

    const std::string json = registry.snapshotJson();
    ok &= check(json.find("TelemetrySelfTestScope") != std::string::npos,
                "snapshot json retrievable", report);

    registry.endFrame(1.5);
    const Telemetry::ScopeStats* windowed = registry.find("TelemetrySelfTestScope");
    ok &= check(windowed != nullptr && windowed->callsPerSec == 3,
                "one-second window computes calls/sec", report);

    registry.reset();
    return ok;
}
