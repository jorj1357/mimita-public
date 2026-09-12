// 09 12 2026
/* purpose
* Implements telemetry inspection commands for humans (the same data an agent
* or GUI consumes).
* Does NOT own telemetry data.
*/
#include "terminal/telemetry-commands.h"

#include "devtools/terminal.h"
#include "telemetry/telemetry.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

std::string formatTime(std::uint64_t ns)
{
    char buffer[64];
    if (ns >= 1000000ull)
        std::snprintf(buffer, sizeof(buffer), "%.2f ms", (double)ns / 1e6);
    else if (ns >= 1000ull)
        std::snprintf(buffer, sizeof(buffer), "%.2f us", (double)ns / 1e3);
    else
        std::snprintf(buffer, sizeof(buffer), "%llu ns", (unsigned long long)ns);
    return buffer;
}

} // namespace

void registerTelemetryCommands()
{
    Terminal::instance().registerCommand(
        {
            "telemetry", "Show live telemetry aggregates for instrumented scopes",
            "telemetry [top] [n]",
            [](const std::vector<std::string>& args) {
                std::size_t limit = 12;
                for (std::size_t i = 0; i < args.size(); ++i)
                    if (args[i] == "top" && i + 1 < args.size())
                        limit = (std::size_t)std::max(1, std::atoi(args[i + 1].c_str()));

                std::vector<Telemetry::ScopeStats> scopes =
                    Telemetry::Registry::instance().snapshot();
                std::sort(scopes.begin(), scopes.end(),
                          [](const Telemetry::ScopeStats& a, const Telemetry::ScopeStats& b) {
                              return a.totalNs > b.totalNs;
                          });
                if (scopes.size() > limit)
                    scopes.resize(limit);

                Terminal::instance().addLog("[TELEMETRY] scopes=" +
                    std::to_string(Telemetry::Registry::instance().scopeCount()));
                for (const auto& stats : scopes) {
                    Terminal::instance().addLog("[TELEMETRY] " + stats.label +
                        " calls/s=" + std::to_string(stats.callsPerSec) +
                        " calls=" + std::to_string(stats.calls) +
                        " avg=" + formatTime(stats.avgNs) +
                        " max=" + formatTime(stats.maxNs) +
                        " total=" + formatTime(stats.totalNs) +
                        " gen=" + std::to_string(stats.generation));
                }
            },
        },
        "2026-09-12", CommandCategory::Debug);

    Terminal::instance().registerCommand(
        {
            "telemetry_entity", "Show telemetry counters for an entity key",
            "telemetry_entity <key>",
            [](const std::vector<std::string>& args) {
                if (args.empty()) {
                    Terminal::instance().addLog("[TELEMETRY] usage: telemetry_entity <key>");
                    return;
                }
                const std::uint64_t key = std::strtoull(args[0].c_str(), nullptr, 10);
                Telemetry::EntityCounters counters;
                if (!Telemetry::Registry::instance().entityCounters(key, counters)) {
                    Terminal::instance().addLog("[TELEMETRY] no counters for entity key " +
                                                std::to_string(key));
                    return;
                }
                Terminal::instance().addLog("[TELEMETRY] entity=" + std::to_string(key) +
                    " updates=" + std::to_string(counters.updates) +
                    " contacts=" + std::to_string(counters.physicsContacts) +
                    " render=" + std::to_string(counters.renderSubmissions) +
                    " netUpdates=" + std::to_string(counters.networkUpdates) +
                    " netBytes=" + std::to_string(counters.networkBytes) +
                    " lastTick=" + std::to_string(counters.lastTouchedTick));
            },
        },
        "2026-09-12", CommandCategory::Debug);
}
