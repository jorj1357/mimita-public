// 09 12 2026
/* purpose
* Implements the telemetry registry and scoped timer.
* Does NOT render or own systems.
*/
#include "telemetry/telemetry.h"

#include <chrono>

#include <nlohmann/json.hpp>

namespace Telemetry {

namespace {

struct ScopeFrame {
    std::uint64_t startNs = 0;
    std::uint64_t childNs = 0;
};

constexpr int kMaxStack = 64;
thread_local ScopeFrame gStack[kMaxStack];
thread_local int gDepth = 0;

std::string scopeKey(const char* label, const char* file, int line)
{
    std::string key = label ? label : "scope";
    key += '@';
    key += file ? file : "";
    key += ':';
    key += std::to_string(line);
    return key;
}

} // namespace

std::uint64_t monotonicNs()
{
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

Registry& Registry::instance()
{
    static Registry registry;
    return registry;
}

void Registry::beginFrame(std::uint64_t tick, std::uint32_t generation,
                          const std::string& codeHash)
{
    std::lock_guard<std::mutex> lock(mutex_);
    currentTick_ = tick;
    generation_ = generation;
    codeHash_ = codeHash;
}

void Registry::endFrame(double dtSeconds)
{
    std::lock_guard<std::mutex> lock(mutex_);
    windowSeconds_ += dtSeconds;
    if (windowSeconds_ < 1.0)
        return;
    for (auto& entry : scopes_) {
        entry.second.callsPerSec = entry.second.windowCalls;
        entry.second.windowCalls = 0;
    }
    windowSeconds_ = 0.0;
}

ScopeStats& Registry::scopeFor(const char* label, const char* file, int line)
{
    const std::string key = scopeKey(label, file, line);
    auto it = scopes_.find(key);
    if (it != scopes_.end())
        return it->second;
    ScopeStats stats;
    stats.label = label ? label : "scope";
    stats.file = file ? file : "";
    stats.line = line;
    stats.generation = generation_;
    stats.codeHash = codeHash_;
    return scopes_.emplace(key, std::move(stats)).first->second;
}

void Registry::record(const char* label, const char* file, int line,
                      std::uint64_t inclusiveNs, std::uint64_t selfNs)
{
    if (!label || !*label)
        return;
    std::lock_guard<std::mutex> lock(mutex_);
    ScopeStats& stats = scopeFor(label, file, line);
    ++stats.calls;
    ++stats.windowCalls;
    stats.totalNs += inclusiveNs;
    stats.selfNs += selfNs;
    if (inclusiveNs > stats.maxNs)
        stats.maxNs = inclusiveNs;
    stats.avgNs = stats.calls ? stats.totalNs / stats.calls : 0;
    stats.lastTick = currentTick_;
    stats.generation = generation_;
    stats.codeHash = codeHash_;
}

void Registry::addEntityCounter(std::uint64_t entityKey, const EntityCounters& delta)
{
    std::lock_guard<std::mutex> lock(mutex_);
    EntityCounters& counters = entityCounters_[entityKey];
    counters.updates += delta.updates;
    counters.physicsContacts += delta.physicsContacts;
    counters.renderSubmissions += delta.renderSubmissions;
    counters.networkBytes += delta.networkBytes;
    counters.networkUpdates += delta.networkUpdates;
    if (delta.lastTouchedTick > counters.lastTouchedTick)
        counters.lastTouchedTick = delta.lastTouchedTick;
}

bool Registry::entityCounters(std::uint64_t entityKey, EntityCounters& out) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entityCounters_.find(entityKey);
    if (it == entityCounters_.end())
        return false;
    out = it->second;
    return true;
}

const ScopeStats* Registry::find(const std::string& label) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : scopes_)
        if (entry.second.label == label)
            return &entry.second;
    return nullptr;
}

std::vector<ScopeStats> Registry::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ScopeStats> out;
    out.reserve(scopes_.size());
    for (const auto& entry : scopes_)
        out.push_back(entry.second);
    return out;
}

std::string Registry::snapshotJson() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json array = nlohmann::json::array();
    for (const auto& entry : scopes_) {
        const ScopeStats& stats = entry.second;
        nlohmann::json item;
        item["label"] = stats.label;
        item["calls"] = stats.calls;
        item["calls_per_sec"] = stats.callsPerSec;
        item["total_ns"] = stats.totalNs;
        item["self_ns"] = stats.selfNs;
        item["avg_ns"] = stats.avgNs;
        item["max_ns"] = stats.maxNs;
        item["last_tick"] = stats.lastTick;
        item["generation"] = stats.generation;
        item["code_hash"] = stats.codeHash;
        array.push_back(std::move(item));
    }
    return array.dump();
}

std::string Registry::entityJson(std::uint64_t entityKey) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entityCounters_.find(entityKey);
    if (it == entityCounters_.end())
        return "{}";
    const EntityCounters& counters = it->second;
    nlohmann::json item;
    item["updates"] = counters.updates;
    item["physics_contacts"] = counters.physicsContacts;
    item["render_submissions"] = counters.renderSubmissions;
    item["network_bytes"] = counters.networkBytes;
    item["network_updates"] = counters.networkUpdates;
    item["last_touched_tick"] = counters.lastTouchedTick;
    return item.dump();
}

std::size_t Registry::scopeCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return scopes_.size();
}

void Registry::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    scopes_.clear();
    entityCounters_.clear();
    windowSeconds_ = 0.0;
}

ScopedTimer::ScopedTimer(const char* label, const char* file, int line)
    : label_(label)
    , file_(file)
    , line_(line)
    , startNs_(monotonicNs())
{
    if (gDepth < kMaxStack) {
        gStack[gDepth].startNs = startNs_;
        gStack[gDepth].childNs = 0;
        ++gDepth;
    }
}

ScopedTimer::~ScopedTimer()
{
    const std::uint64_t endNs = monotonicNs();
    if (gDepth <= 0)
        return;
    --gDepth;
    const ScopeFrame frame = gStack[gDepth];
    const std::uint64_t inclusive = endNs - frame.startNs;
    const std::uint64_t self = inclusive > frame.childNs ? inclusive - frame.childNs : 0;
    if (gDepth > 0)
        gStack[gDepth - 1].childNs += inclusive;
    Registry::instance().record(label_, file_, line_, inclusive, self);
}

} // namespace Telemetry
