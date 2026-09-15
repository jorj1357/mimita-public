// 09 15 2026
/* purpose
* Implements the headless generation switch tick-domain mapping self-test:
* delta-based mapping from authoritative server tick space to client-local
* simulation tick space, including due-in-the-past and prediction-lead cases.
* Does NOT own rendering/presentation or the transport.
*/
#include "hot-reload/generation-switch-mapping-selftest.h"

#include <string>

#include "hot-reload/generation-switch-mapping.h"

using namespace MimitaRuntime;

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runGenerationSwitchMappingSelfTest(std::string& report)
{
    bool ok = true;

    // Server at tick 1000 sends SWITCH at 1030; client local sim at 1004 when it
    // observes server tick 1000 (prediction lead of 4). Boundary = 1004 + 30.
    ok &= check(mapServerSwitchTickToClientLocal(1000, 1004, 1030) == 1034,
                "delta mapping accounts for the client prediction lead", report);

    // No lead (client local == observed server tick).
    ok &= check(mapServerSwitchTickToClientLocal(1000, 1000, 1030) == 1030,
                "delta mapping with zero lead equals the server delta", report);

    // Switch tick already in the past relative to observed server tick: due now.
    ok &= check(mapServerSwitchTickToClientLocal(1035, 1040, 1030) == 1040,
                "already-due switch maps to the current local tick", report);

    // Exact boundary.
    ok &= check(mapServerSwitchTickToClientLocal(1000, 1004, 1000) == 1004,
                "switch at the observed tick maps to the receipt local tick",
                report);

    // Large forecast: mapping preserves the full delta.
    ok &= check(mapServerSwitchTickToClientLocal(500, 12, 800) == 312,
                "mapping preserves the full server delta", report);

    // Deterministic repeat.
    ok &= check(mapServerSwitchTickToClientLocal(500, 12, 800) ==
                    mapServerSwitchTickToClientLocal(500, 12, 800),
                "mapping is deterministic", report);

    return ok;
}
