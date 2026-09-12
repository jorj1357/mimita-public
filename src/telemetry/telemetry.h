// 09 12 2026
/* purpose
* General runtime telemetry: rolling per-scope aggregates (calls/sec,
* inclusive/exclusive time, avg/max, last tick, generation/hash) and generic
* per-entity counters. Data first; visualizations consume this later.
* Units are explicit: nanoseconds internally, calls/sec and bytes/sec at edges.
* Does NOT render heat maps, own systems, or replace the debug logger.
*/
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Telemetry {

struct ScopeStats {
    std::string label;
    std::string file;
    int line = 0;
    std::uint64_t calls = 0;         // total calls since start
    std::uint64_t windowCalls = 0;   // calls since the last 1s window
    std::uint64_t callsPerSec = 0;   // calls in the last 1s window
    std::uint64_t totalNs = 0;       // inclusive total
    std::uint64_t selfNs = 0;        // exclusive total
    std::uint64_t maxNs = 0;         // max inclusive
    std::uint64_t avgNs = 0;         // inclusive total / calls
    std::uint64_t lastTick = 0;
    std::uint32_t generation = 0;
    std::string codeHash;
};

// Generic counter magnitudes for one entity. Systems increment what applies.
struct EntityCounters {
    std::uint64_t updates = 0;
    std::uint64_t physicsContacts = 0;
    std::uint64_t renderSubmissions = 0;
    std::uint64_t networkBytes = 0;
    std::uint64_t networkUpdates = 0;
    std::uint64_t lastTouchedTick = 0;
};

class Registry {
public:
    static Registry& instance();

    // Marks the current frame/tick and active code identity.
    void beginFrame(std::uint64_t tick, std::uint32_t generation,
                    const std::string& codeHash);
    // Advances the 1-second window when `dtSeconds` has elapsed.
    void endFrame(double dtSeconds);

    void record(const char* label, const char* file, int line,
                std::uint64_t inclusiveNs, std::uint64_t selfNs);

    // Per-entity counters, keyed by the caller's stable entity key.
    void addEntityCounter(std::uint64_t entityKey, const EntityCounters& delta);
    bool entityCounters(std::uint64_t entityKey, EntityCounters& out) const;

    const ScopeStats* find(const std::string& label) const;
    std::vector<ScopeStats> snapshot() const;
    std::string snapshotJson() const;
    std::string entityJson(std::uint64_t entityKey) const;

    std::size_t scopeCount() const;
    void reset();

private:
    Registry() = default;
    ScopeStats& scopeFor(const char* label, const char* file, int line);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ScopeStats> scopes_;
    std::unordered_map<std::uint64_t, EntityCounters> entityCounters_;
    std::uint64_t currentTick_ = 0;
    std::uint32_t generation_ = 0;
    std::string codeHash_;
    double windowSeconds_ = 0.0;
};

// RAII scope. Records inclusive time and, via a thread-local stack, self time.
class ScopedTimer {
public:
    ScopedTimer(const char* label, const char* file, int line);
    ~ScopedTimer();
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    const char* label_;
    const char* file_;
    int line_;
    std::uint64_t startNs_;
};

std::uint64_t monotonicNs();

} // namespace Telemetry

#define MIMITA_TELEMETRY_SCOPE(label) \
    ::Telemetry::ScopedTimer _mimita_telemetry_scope_##__LINE__(label, __FILE__, __LINE__)
