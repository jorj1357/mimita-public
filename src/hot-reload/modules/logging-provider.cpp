// 09 24 2026
/* purpose
* Hot logging provider (Phase 2/3). Registers the overridable `log.event`
* capability so debug logging policy is edited live: filtering, event naming,
* field selection, schema/version awareness, sampling, throttling, aggregation,
* destination routing, and JSONL body construction.
* The cold kernel keeps the file handle, run directory, process identity, and
* the single atomic append; this module never opens a file.
* Does NOT own gameplay state, rendering, or transport.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-logging.h"
#include "hot-reload/hot-package.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MimitaHotLogging {
namespace {

// Ordered levels mirror the legacy structured levels so existing config values
// keep their meaning: off < errors < important < verbose < trace.
enum { L_OFF = 0, L_ERRORS = 1, L_IMPORTANT = 2, L_VERBOSE = 3, L_TRACE = 4 };

int levelFromString(const std::string& s)
{
    if (s == "off") return L_OFF;
    if (s == "errors") return L_ERRORS;
    if (s == "important") return L_IMPORTANT;
    if (s == "verbose") return L_VERBOSE;
    if (s == "trace") return L_TRACE;
    return L_OFF;
}

std::string toUpper(std::string s)
{
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    return s;
}

struct LogConfig {
    bool enabled = true;
    int defaultLevel = L_IMPORTANT;
    std::unordered_map<std::string, int> categoryLevels;  // keyed UPPERCASE
    bool destJsonl = true;
    bool destTerminal = false;
    bool destLiveCode = false;
    bool throttleEnabled = true;
    int maxPerSecond = 20;
    int duplicateWindowMs = 1000;
    float repeatWindowSeconds = 5.0f;
    bool alwaysFlush = false;
};

LogConfig g_config;
bool g_loaded = false;
std::uint64_t g_configWrite = 0;
std::string g_pendingError;
std::mutex g_mutex;

struct Bucket {
    std::string category;
    std::string event;
    std::string sample;
    std::uint64_t count = 0;
    double firstT = 0.0;
    double lastT = 0.0;
    std::uint64_t firstTick = 0;
    std::uint64_t lastTick = 0;
};
std::unordered_map<std::string, Bucket> g_buckets;

double nowSeconds()
{
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
        .count();
}

void loadConfig()
{
    std::ifstream f("config/debuglogger.json");
    if (!f.is_open()) {
        g_loaded = true;  // keep defaults / last-good
        return;
    }
    try {
        nlohmann::json j;
        f >> j;

        LogConfig cfg = g_loaded ? g_config : LogConfig{};
        if (j.contains("enabled")) cfg.enabled = j["enabled"].get<bool>();
        if (j.contains("default_level"))
            cfg.defaultLevel = levelFromString(j["default_level"].get<std::string>());

        cfg.categoryLevels.clear();
        if (j.contains("categories") && j["categories"].is_object()) {
            for (auto it = j["categories"].begin(); it != j["categories"].end(); ++it) {
                int lvl = cfg.defaultLevel;
                const auto& v = it.value();
                if (v.is_string())
                    lvl = levelFromString(v.get<std::string>());
                else if (v.is_object() && v.contains("level") && v["level"].is_string())
                    lvl = levelFromString(v["level"].get<std::string>());
                cfg.categoryLevels[toUpper(it.key())] = lvl;
            }
        }

        if (j.contains("destinations") && j["destinations"].is_object()) {
            const auto& d = j["destinations"];
            if (d.contains("events_jsonl")) cfg.destJsonl = d["events_jsonl"].get<bool>();
            if (d.contains("terminal")) cfg.destTerminal = d["terminal"].get<bool>();
            if (d.contains("live_code_file"))
                cfg.destLiveCode = d["live_code_file"].get<bool>();
        }

        if (j.contains("repeat_window_seconds"))
            cfg.repeatWindowSeconds = j["repeat_window_seconds"].get<float>();

        if (j.contains("throttling") && j["throttling"].is_object()) {
            const auto& t = j["throttling"];
            if (t.contains("enabled")) cfg.throttleEnabled = t["enabled"].get<bool>();
            if (t.contains("default_max_messages_per_second"))
                cfg.maxPerSecond = t["default_max_messages_per_second"].get<int>();
            if (t.contains("duplicate_message_window_ms"))
                cfg.duplicateWindowMs = t["duplicate_message_window_ms"].get<int>();
        }
        if (j.contains("always_flush")) cfg.alwaysFlush = j["always_flush"].get<bool>();

        g_config = cfg;
        g_loaded = true;
    } catch (const std::exception& e) {
        // Malformed update: keep the last-good config and emit one error record
        // through the append mechanism on the next flush.
        g_loaded = true;
        nlohmann::json err = nlohmann::json::object();
        err["level"] = "ERROR";
        err["category"] = "EXECUTABLE";
        err["event"] = "logger.config_error";
        err["message"] = std::string("Config parse error: ") + e.what();
        err["reason"] = "config/debuglogger.json";
        std::string d = err.dump();
        g_pendingError.assign(d.begin() + 1, d.end() - 1);
    }
}

int categoryLevel(const std::string& upperCat)
{
    auto it = g_config.categoryLevels.find(upperCat);
    return it == g_config.categoryLevels.end() ? g_config.defaultLevel : it->second;
}

int neededLevel(std::uint32_t v1level)
{
    switch (v1level) {
    case 0: return L_TRACE;
    case 1: return L_VERBOSE;
    case 2: return L_IMPORTANT;
    case 3: return L_IMPORTANT;
    case 4: return L_ERRORS;
    default: return L_ERRORS;
    }
}

const char* actorKindName(std::uint32_t kind)
{
    static const char* kKinds[] = {"none", "player", "npc", "remote", "other"};
    return kKinds[kind < 5u ? kind : 4u];
}

// Build the JSON field fragment (no outer braces). The kernel adds universal
// fields (wall_time/t/seq/run_id/pid/process), level, category, event, message,
// reason, ticks, and source, so this body carries only the typed payload.
void buildFieldsBody(const GameLogEventV1& e, std::string& out)
{
    nlohmann::json j = nlohmann::json::object();
    if (e.structSize >= sizeof(GameLogEventV1) && e.fields && e.fieldCount > 0) {
        for (std::uint32_t i = 0; i < e.fieldCount; ++i) {
            const GameLogFieldV1& f = e.fields[i];
            if ((f.flags & GAME_LOG_FIELD_FLAG_PRESENT) == 0 &&
                (f.flags & GAME_LOG_FIELD_FLAG_DEPRECATED) != 0)
                continue;
            const std::string key =
                f.name ? f.name : ("field_" + std::to_string(f.nameId));
            switch (f.type) {
            case GAME_LOG_FIELD_BOOL: j[key] = (f.intValue != 0); break;
            case GAME_LOG_FIELD_INT: j[key] = f.intValue; break;
            case GAME_LOG_FIELD_UINT: j[key] = f.uintValue; break;
            case GAME_LOG_FIELD_FLOAT: j[key] = f.doubleValue; break;
            case GAME_LOG_FIELD_STRING: j[key] = f.string; break;
            case GAME_LOG_FIELD_VEC3:
                j[key] = nlohmann::json::array({f.vector[0], f.vector[1], f.vector[2]});
                break;
            case GAME_LOG_FIELD_VEC4:
                j[key] = nlohmann::json::array(
                    {f.vector[0], f.vector[1], f.vector[2], f.vector[3]});
                break;
            default: break;
            }
        }
    }
    if (e.entityId) j["entity_id"] = e.entityId;
    if (e.actorId) j["actor_id"] = e.actorId;
    if (e.actorKind) j["actor_type"] = actorKindName(e.actorKind);
    if (e.result[0]) j["result"] = e.result;
    if (e.parentEventId) j["parent_event_id"] = e.parentEventId;
    if (e.schemaId) {
        j["schema_id"] = e.schemaId;
        j["schema_version"] = e.schemaVersion;
    }
    std::string d = j.dump();
    out.assign(d.begin() + 1, d.end() - 1);
}

std::string buildSummaryBody(const Bucket& b)
{
    nlohmann::json j = nlohmann::json::object();
    j["level"] = "INFO";
    j["category"] = b.category;
    j["event"] = b.event + ".summary";
    j["count"] = b.count;
    j["first_t"] = b.firstT;
    j["last_t"] = b.lastT;
    if (b.firstTick) j["first_tick"] = b.firstTick;
    if (b.lastTick) j["last_tick"] = b.lastTick;
    try {
        auto sample = nlohmann::json::parse("{" + b.sample + "}");
        for (auto& kv : sample.items())
            j[kv.key()] = kv.value();
    } catch (...) {
        // A malformed sample must not drop the summary; keep the counters.
    }
    std::string d = j.dump();
    return d.substr(1, d.size() - 2);
}

} // namespace

void MIMITA_GAME_CALL provideLogEvent(void* /*host*/, const GameLogEventV1* event,
                                      GameLogRecordV1* out)
{
    if (!event || !out)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    out->handled = 1;
    out->emit = 0;
    out->destinations = 0;
    out->forceFlush = 0;
    out->body = nullptr;
    out->bodyLen = 0;
    out->reserved = 0;

    if (!g_loaded)
        loadConfig();
    if (!g_config.enabled)
        return;

    const int need = neededLevel(event->level);
    const std::string cat = toUpper(event->category[0] ? event->category : "HOT");
    if (need > categoryLevel(cat))
        return;  // filtered out by hot policy

    std::uint32_t dest = 0;
    if (g_config.destJsonl) dest |= GAME_LOG_DEST_JSONL;
    if (g_config.destTerminal) dest |= GAME_LOG_DEST_TERMINAL;
    if (g_config.destLiveCode) dest |= GAME_LOG_DEST_LIVE_CODE;
    out->destinations = dest;

    static thread_local std::string body;
    buildFieldsBody(*event, body);

    const bool critical = event->level >= 4;  // error / fatal
    if (critical || g_config.repeatWindowSeconds <= 0.0f) {
        out->emit = 1;
        out->body = body.data();
        out->bodyLen = (std::uint32_t)body.size();
        out->forceFlush = (critical || g_config.alwaysFlush) ? 1u : 0u;
        return;
    }

    const std::string name = event->name[0] ? event->name : "hot.event";
    const std::string key = cat + ":" + name;
    const double now = nowSeconds();
    auto it = g_buckets.find(key);
    if (it == g_buckets.end()) {
        Bucket& b = g_buckets[key];
        b.category = cat;
        b.event = name;
        b.sample = body;
        b.count = 1;
        b.firstT = b.lastT = now;
        b.firstTick = b.lastTick = event->simulationTick;
        // First occurrence emits immediately; repeats are summarized by flush.
        out->emit = 1;
        out->body = body.data();
        out->bodyLen = (std::uint32_t)body.size();
        return;
    }
    Bucket& b = it->second;
    b.count++;
    b.lastT = now;
    b.lastTick = event->simulationTick;
    b.sample = body;  // keep the most recent representative fields
    // emit stays 0: suppressed repeat, counted for the next summary.
}

void MIMITA_GAME_CALL flushTick(void* host, std::uint64_t /*tick*/, float /*dt*/)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    GameLogAppendFn append = nullptr;
    if (ctx && ctx->resolveCapability)
        append = reinterpret_cast<GameLogAppendFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_LOG_APPEND));

    // Hot-reloadable config: reload only when the file changed.
    std::error_code ec;
    auto wt = std::filesystem::last_write_time("config/debuglogger.json", ec);
    if (!ec) {
        const std::uint64_t c = (std::uint64_t)wt.time_since_epoch().count();
        std::lock_guard<std::mutex> lock(g_mutex);
        if (c != g_configWrite || !g_loaded) {
            g_configWrite = c;
            loadConfig();
        }
    }

    std::string pending;
    std::vector<std::pair<std::string, std::string>> summaries;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_pendingError.empty())
            pending.swap(g_pendingError);
        const double now = nowSeconds();
        for (auto it = g_buckets.begin(); it != g_buckets.end();) {
            Bucket& b = it->second;
            if (now - b.firstT >= (double)g_config.repeatWindowSeconds) {
                if (b.count > 1)
                    summaries.push_back({it->first, buildSummaryBody(b)});
                it = g_buckets.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (!append)
        return;
    for (auto& s : summaries)
        append(host, s.second.data(), (std::uint32_t)s.second.size(), 0u);
    if (!pending.empty())
        append(host, pending.data(), (std::uint32_t)pending.size(), 1u);
}

namespace {

const GameCapabilityDescriptorV1 kLogEventProvider{
    GAME_CAP_LOG_EVENT, gameHash("sig.log.event.v1"), 0,
    reinterpret_cast<void*>(&provideLogEvent), "logging.provider"};

void MIMITA_GAME_CALL flushSystem(void* host, std::uint64_t tick, float dt)
{
    flushTick(host, tick, dt);
}

const GameSystemDescriptorV1 kFlushSystem{
    gameHash("logging.flush"), gameHash("logging.60"), 100, 0, flushSystem,
    "logging.flush"};

const MimitaHotPackage::CapabilityRegistrar s_logEventProvider{kLogEventProvider};
const MimitaHotPackage::SystemRegistrar s_flushSystem{kFlushSystem};
const MimitaHotPackage::CapabilityRequirementRegistrar s_logAppendRequirement{
    GAME_CAP_LOG_APPEND, gameHash("sig.log.append.v1"), 0};

} // namespace
} // namespace MimitaHotLogging

#endif // MIMITA_GAME_DLL
