// 09 17 2026
/* purpose
* Implements the single authoritative debug output: one append-only JSONL file
* (`events.jsonl`) per process run under logs/yyyy-mm-dd/hhmmss/.
* Formatting, universal fields, immediate flushing, bounded duplicate
* aggregation, and hot-reloadable category levels all live here.
* The legacy Debug/terminal/printf bridges feed the same stream; there is no
* second authoritative file and no per-category .txt output.
* Does NOT own gameplay, rendering, audio, or networking.
*/

#include "structured-log.h"
#include "debug-log.h"
#include "log-manager.h"
#include "live-code/live-identity.h"
#include "../config.h"
#include "../utils/path_utils.h"
#include "../utils/time-format.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <windows.h>
#include <share.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>

namespace {

// Bound the number of live repeat buckets so low-priority spam cannot grow
// memory without limit.
constexpr std::size_t MAX_ACTIVE_BUCKETS = 512;

double steadySeconds() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8]{};
                std::snprintf(buf, sizeof(buf), "\\u%04x", c & 0xff);
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

std::mutex& logMutex() {
    static std::mutex m;
    return m;
}

} // namespace

// ── Singleton ───────────────────────────────────────────────

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger s;
    return s;
}

StructuredLogger::~StructuredLogger() {
    if (mInitialized) shutdown();
}

// ── Helpers ─────────────────────────────────────────────────

std::string StructuredLogger::categoryName(StructuredCategory cat) const {
    switch (cat) {
        case StructuredCategory::General:         return "GENERAL";
        case StructuredCategory::Glb:             return "GLB";
        case StructuredCategory::Replay:          return "REPLAY";
        case StructuredCategory::Camera:          return "CAMERA";
        case StructuredCategory::Audio:           return "AUDIO";
        case StructuredCategory::Physics:         return "PHYSICS";
        case StructuredCategory::Performance:     return "PERFORMANCE";
        case StructuredCategory::Collision:       return "COLLISION";
        case StructuredCategory::NpcCombat:       return "NPC_COMBAT";
        case StructuredCategory::NpcMovement:     return "NPC_MOVEMENT";
        case StructuredCategory::Ragdoll:         return "RAGDOLL";
        case StructuredCategory::Weapons:         return "WEAPONS";
        case StructuredCategory::Animation:       return "ANIMATION";
        case StructuredCategory::Gui:             return "GUI";
        case StructuredCategory::Avatar:          return "AVATAR";
        case StructuredCategory::Network:         return "NETWORK";
        case StructuredCategory::World:           return "WORLD";
        case StructuredCategory::Duel:            return "DUEL";
        case StructuredCategory::Auth:            return "AUTH";
        case StructuredCategory::Chat:            return "CHAT";
        case StructuredCategory::Vip:             return "VIP";
        case StructuredCategory::Rendering:       return "RENDERING";
        case StructuredCategory::GlbModels:       return "GLB_MODELS";
        case StructuredCategory::Executable:      return "EXECUTABLE";
        case StructuredCategory::GrenadeLauncher: return "GRENADE_LAUNCHER";
        case StructuredCategory::Healthbar:       return "HEALTHBAR";
        case StructuredCategory::Skybox:          return "SKYBOX";
        case StructuredCategory::ChatLayout:      return "CHAT_LAYOUT";
        case StructuredCategory::Count:           return "COUNT";
    }
    return "UNKNOWN";
}

StructuredLevel StructuredLogger::levelFromString(const std::string& s) const {
    if (s == "off")       return StructuredLevel::Off;
    if (s == "errors")    return StructuredLevel::Errors;
    if (s == "important") return StructuredLevel::Important;
    if (s == "verbose")   return StructuredLevel::Verbose;
    if (s == "trace")     return StructuredLevel::Trace;
    return StructuredLevel::Off;
}

std::string StructuredLogger::levelToString(StructuredLevel lvl) const {
    switch (lvl) {
        case StructuredLevel::Off:       return "OFF";
        case StructuredLevel::Errors:    return "ERRORS";
        case StructuredLevel::Important: return "IMPORTANT";
        case StructuredLevel::Verbose:   return "VERBOSE";
        case StructuredLevel::Trace:     return "TRACE";
    }
    return "OFF";
}

std::string StructuredLogger::debugLevelToString(debug::Level lvl) const {
    switch (lvl) {
        case debug::Level::Trace: return "TRACE";
        case debug::Level::Debug: return "DEBUG";
        case debug::Level::Info:  return "INFO";
        case debug::Level::Warn:  return "WARN";
        case debug::Level::Error: return "ERROR";
        case debug::Level::Fatal: return "FATAL";
    }
    return "INFO";
}

const StructuredLogConfig::CategoryConfig&
StructuredLogger::categoryConfigFor(StructuredCategory cat) const {
    switch (cat) {
        case StructuredCategory::General:         return mConfig.general;
        case StructuredCategory::Glb:             return mConfig.glb;
        case StructuredCategory::Replay:          return mConfig.replay;
        case StructuredCategory::Camera:          return mConfig.camera;
        case StructuredCategory::Audio:           return mConfig.audio;
        case StructuredCategory::Physics:         return mConfig.physics;
        case StructuredCategory::Performance:     return mConfig.performance;
        case StructuredCategory::Collision:       return mConfig.collision;
        case StructuredCategory::NpcCombat:       return mConfig.npcCombat;
        case StructuredCategory::NpcMovement:     return mConfig.npcMovement;
        case StructuredCategory::Ragdoll:         return mConfig.ragdoll;
        case StructuredCategory::Weapons:         return mConfig.weapons;
        case StructuredCategory::Animation:       return mConfig.animation;
        case StructuredCategory::Gui:             return mConfig.gui;
        case StructuredCategory::Avatar:          return mConfig.avatar;
        case StructuredCategory::Network:         return mConfig.network;
        case StructuredCategory::World:           return mConfig.world;
        case StructuredCategory::Duel:            return mConfig.duel;
        case StructuredCategory::Auth:            return mConfig.auth;
        case StructuredCategory::Chat:            return mConfig.chat;
        case StructuredCategory::Vip:             return mConfig.vip;
        case StructuredCategory::Rendering:       return mConfig.rendering;
        case StructuredCategory::GlbModels:       return mConfig.glbModels;
        case StructuredCategory::Executable:      return mConfig.executable;
        case StructuredCategory::GrenadeLauncher: return mConfig.grenadeLauncher;
        case StructuredCategory::Healthbar:       return mConfig.healthbar;
        case StructuredCategory::Skybox:          return mConfig.skybox;
        case StructuredCategory::ChatLayout:      return mConfig.chatLayout;
        case StructuredCategory::Count:           return mConfig.replay;
    }
    return mConfig.replay;
}

// ── Config loading ──────────────────────────────────────────

static StructuredLogConfig::CategoryConfig parseCategoryConfig(
    const nlohmann::json& j, StructuredLevel defaultLevel)
{
    StructuredLogConfig::CategoryConfig cfg;
    cfg.level = defaultLevel;
    cfg.fileOutput = true;
    if (j.is_string()) {
        const std::string lvl = j.get<std::string>();
        if (lvl == "off")            cfg.level = StructuredLevel::Off;
        else if (lvl == "errors")    cfg.level = StructuredLevel::Errors;
        else if (lvl == "important") cfg.level = StructuredLevel::Important;
        else if (lvl == "verbose")   cfg.level = StructuredLevel::Verbose;
        else if (lvl == "trace")     cfg.level = StructuredLevel::Trace;
        return cfg;
    }
    if (j.contains("level")) {
        std::string lvl = j["level"].get<std::string>();
        if (lvl == "off")            cfg.level = StructuredLevel::Off;
        else if (lvl == "errors")    cfg.level = StructuredLevel::Errors;
        else if (lvl == "important") cfg.level = StructuredLevel::Important;
        else if (lvl == "verbose")   cfg.level = StructuredLevel::Verbose;
        else if (lvl == "trace")     cfg.level = StructuredLevel::Trace;
    }
    if (j.contains("file_output"))
        cfg.fileOutput = j["file_output"].get<bool>();
    if (j.contains("throttle"))
        cfg.throttleSeconds = j["throttle"].get<float>();
    return cfg;
}

void StructuredLogger::loadConfig() {
    const std::string configPath = "config/debuglogger.json";
    std::ifstream f(configPath);
    if (!f.is_open()) {
        return;   // defaults remain in effect
    }

    try {
        nlohmann::json j;
        f >> j;

        StructuredLogConfig cfg;

        if (j.contains("enabled"))            cfg.enabled = j["enabled"].get<bool>();
        if (j.contains("hot_reload"))         cfg.hotReload = j["hot_reload"].get<bool>();
        if (j.contains("console_output"))     cfg.consoleOutput = j["console_output"].get<bool>();
        if (j.contains("summary_file"))       cfg.summaryFile = j["summary_file"].get<bool>();
        if (j.contains("events_file"))        cfg.eventsFile = j["events_file"].get<bool>();
        if (j.contains("console_mirror")) {
            cfg.consoleMirror = j["console_mirror"].get<bool>();
            cfg.consoleOutput = cfg.consoleMirror;
        }
        if (j.contains("flush_each_event"))
            cfg.flushEachEvent = j["flush_each_event"].get<bool>();
        if (j.contains("repeat_window_seconds"))
            cfg.repeatWindowSeconds = j["repeat_window_seconds"].get<float>();
        if (j.contains("default_level"))
            cfg.defaultLevel = levelFromString(j["default_level"].get<std::string>());

        if (j.contains("categories")) {
            auto& cats = j["categories"];
            auto read = [&](const char* key, StructuredLogConfig::CategoryConfig& dst) {
                if (cats.contains(key))
                    dst = parseCategoryConfig(cats[key], cfg.defaultLevel);
            };
            read("general", cfg.general);
            read("glb", cfg.glb);
            read("replay", cfg.replay);
            read("camera", cfg.camera);
            read("audio", cfg.audio);
            read("physics", cfg.physics);
            read("performance", cfg.performance);
            read("collision", cfg.collision);
            read("npc_combat", cfg.npcCombat);
            read("npc_movement", cfg.npcMovement);
            read("ragdoll", cfg.ragdoll);
            read("weapons", cfg.weapons);
            read("animation", cfg.animation);
            read("gui", cfg.gui);
            read("avatar", cfg.avatar);
            read("network", cfg.network);
            read("world", cfg.world);
            read("duel", cfg.duel);
            read("auth", cfg.auth);
            read("chat", cfg.chat);
            read("vip", cfg.vip);
            read("rendering", cfg.rendering);
            read("glb_models", cfg.glbModels);
            read("executable", cfg.executable);
            read("grenade_launcher", cfg.grenadeLauncher);
            read("healthbar", cfg.healthbar);
            read("skybox", cfg.skybox);
            read("chat_layout", cfg.chatLayout);
        }

        if (j.contains("sampling")) {
            auto& s = j["sampling"];
            if (s.contains("default_every_n_frames"))
                cfg.sampling.defaultEveryNFrames = s["default_every_n_frames"].get<int>();
            if (s.contains("log_on_change"))
                cfg.sampling.logOnChange = s["log_on_change"].get<bool>();
            if (s.contains("minimum_numeric_change"))
                cfg.sampling.minimumNumericChange = s["minimum_numeric_change"].get<float>();
        }

        if (j.contains("throttling")) {
            auto& t = j["throttling"];
            if (t.contains("enabled"))
                cfg.throttling.enabled = t["enabled"].get<bool>();
            if (t.contains("default_max_messages_per_second"))
                cfg.throttling.defaultMaxMessagesPerSecond = t["default_max_messages_per_second"].get<int>();
            if (t.contains("duplicate_message_window_ms"))
                cfg.throttling.duplicateMessageWindowMs = t["duplicate_message_window_ms"].get<int>();
        }

        if (j.contains("replay_validation")) {
            auto& r = j["replay_validation"];
            if (r.contains("camera_position_tolerance"))
                cfg.replayValidation.cameraPositionTolerance = r["camera_position_tolerance"].get<float>();
            if (r.contains("camera_rotation_degrees_tolerance"))
                cfg.replayValidation.cameraRotationDegreesTolerance = r["camera_rotation_degrees_tolerance"].get<float>();
            if (r.contains("fov_degrees_tolerance"))
                cfg.replayValidation.fovDegreesTolerance = r["fov_degrees_tolerance"].get<float>();
            if (r.contains("quaternion_magnitude_tolerance"))
                cfg.replayValidation.quaternionMagnitudeTolerance = r["quaternion_magnitude_tolerance"].get<float>();
            if (r.contains("audio_video_sync_tolerance_ms"))
                cfg.replayValidation.audioVideoSyncToleranceMs = r["audio_video_sync_tolerance_ms"].get<float>();
        }

        mConfig = cfg;
        mCategoryLevels.clear();
        const StructuredCategory cats[28] = {
            StructuredCategory::General, StructuredCategory::Glb, StructuredCategory::Replay,
            StructuredCategory::Camera, StructuredCategory::Audio, StructuredCategory::Physics,
            StructuredCategory::Performance, StructuredCategory::Collision, StructuredCategory::NpcCombat,
            StructuredCategory::NpcMovement, StructuredCategory::Ragdoll, StructuredCategory::Weapons,
            StructuredCategory::Animation, StructuredCategory::Gui, StructuredCategory::Avatar,
            StructuredCategory::Network, StructuredCategory::World, StructuredCategory::Duel,
            StructuredCategory::Auth, StructuredCategory::Chat, StructuredCategory::Vip,
            StructuredCategory::Rendering, StructuredCategory::GlbModels, StructuredCategory::Executable,
            StructuredCategory::GrenadeLauncher, StructuredCategory::Healthbar, StructuredCategory::Skybox,
            StructuredCategory::ChatLayout};
        for (StructuredCategory c : cats)
            mCategoryLevels[categoryName(c)] = categoryConfigFor(c).level;
        mConfigReloadErrors = 0;
    }
    catch (const std::exception& e) {
        mConfigReloadErrors++;
        // Malformed config must not stop the game from logging errors: emit it.
        debug::Event ev;
        ev.category = "EXECUTABLE";
        ev.name = "logger.config_error";
        ev.level = debug::Level::Error;
        ev.message = std::string("Config parse error: ") + e.what();
        ev.reason = configPath;
        ev.sourceFile = "structured-log.cpp";
        ev.functionName = "loadConfig";
        emit(ev, true);
    }
}

// ── Run directory / file ────────────────────────────────────

void StructuredLogger::createRunDir() {
    mRunId = MiMitaTime::utcCompactStamp();
    std::string relPath = "logs/" + MiMitaTime::utcDateFolder() + "/" + mRunId;

    mLogDir = relPath;
    std::string exeDir = getExecutableDirectory();
    if (!exeDir.empty()) {
        std::string candidate = exeDir + relPath;
        std::error_code ec;
        std::filesystem::create_directories(candidate, ec);
        if (!ec)
            mLogDir = candidate;
    }
    std::error_code ec;
    std::filesystem::create_directories(mLogDir, ec);

    mEventsPath = mLogDir + "/events.jsonl";
}

// ── Raw writer ──────────────────────────────────────────────

void StructuredLogger::writeLine(const std::string& json) {
    if (!mEventsFile) return;
    std::fwrite(json.data(), 1, json.size(), mEventsFile);
    std::fputc('\n', mEventsFile);
    if (mConfig.flushEachEvent)
        std::fflush(mEventsFile);
}

// ── Record building ─────────────────────────────────────────

std::string StructuredLogger::buildRecord(const debug::Event& event) const {
    std::string out;
    out.reserve(256 + event.message.size());

    out += "{\"wall_time\":\"";
    out += MiMitaTime::utcIso8601Millis();
    out += "\",\"t\":";

    char num[64];
    std::snprintf(num, sizeof(num), "%.3f", steadySeconds());
    out += num;

    out += ",\"seq\":";
    out += std::to_string(mSequence);

    out += ",\"run_id\":\"";
    out += escapeJson(mRunId);
    out += "\",\"pid\":";
    out += std::to_string(LiveIdentity::pid());

    out += ",\"process\":\"";
    out += escapeJson(LiveIdentity::process());
    out += '"';

    out += ",\"level\":\"";
    out += debugLevelToString(event.level);
    out += '"';

    out += ",\"category\":\"";
    out += escapeJson(event.category);
    out += '"';

    out += ",\"event\":\"";
    out += escapeJson(event.name);
    out += '"';

    auto appendStr = [&](const char* key, const std::string& value) {
        if (value.empty()) return;
        out += ",\"";
        out += key;
        out += "\":\"";
        out += escapeJson(value);
        out += '"';
    };
    auto appendTick = [&](const char* key, uint64_t value) {
        if (value == 0) return;
        out += ",\"";
        out += key;
        out += "\":";
        out += std::to_string(value);
    };

    appendStr("message", event.message);
    appendStr("reason", event.reason);
    appendStr("correlation_id", event.correlationId);
    appendTick("frame", event.frame);
    appendTick("tick", event.simulationTick);
    appendTick("server_tick", event.serverTick);
    appendTick("client_tick", event.clientTick);
    appendTick("duration_us", event.durationUs);

    if (!event.sourceFile.empty()) {
        appendStr("source", event.sourceFile);
        if (event.sourceLine > 0) {
            out += ",\"line\":";
            out += std::to_string(event.sourceLine);
        }
    }
    appendStr("func", event.functionName);

    // Caller fields, appended last. Empty object adds nothing. Universal keys
    // are already written above, so they are skipped here: every record must
    // carry each key exactly once.
    if (event.fields.is_object()) {
        for (auto it = event.fields.begin(); it != event.fields.end(); ++it) {
            const std::string& k = it.key();
            if (k == "wall_time" || k == "t" || k == "seq" || k == "run_id" ||
                k == "pid" || k == "process" || k == "level" || k == "category" ||
                k == "event" || k == "message" || k == "reason" ||
                k == "correlation_id" || k == "frame" || k == "tick" ||
                k == "server_tick" || k == "client_tick" || k == "duration_us" ||
                k == "source" || k == "line" || k == "func")
                continue;
            out += ",\"";
            out += escapeJson(k);
            out += "\":";
            out += it.value().dump();
        }
    } else if (!event.fields.is_null()) {
        out += ",\"fields\":";
        out += event.fields.dump();
    }

    out += '}';
    return out;
}

// ── Aggregation ─────────────────────────────────────────────

void StructuredLogger::flushBucket(RepeatBucket& bucket) {
    if (!bucket.active)
        return;
    if (bucket.count > 1) {
        nlohmann::json summary = bucket.sample;
        summary["count"] = bucket.count;
        summary["first_t"] = bucket.firstTime;
        summary["last_t"] = bucket.lastTime;
        if (bucket.firstTick != 0)
            summary["first_tick"] = bucket.firstTick;
        if (bucket.lastTick != 0)
            summary["last_tick"] = bucket.lastTick;

        // Rename the event to the summary variant, then emit once.
        debug::Event ev;
        ev.category = bucket.category;
        ev.name = bucket.event + ".summary";
        ev.level = debug::Level::Info;
        ev.fields = summary;
        if (summary.contains("message") && summary["message"].is_string())
            ev.message = summary["message"].get<std::string>();
        if (summary.contains("reason") && summary["reason"].is_string())
            ev.reason = summary["reason"].get<std::string>();
        mSequence++;
        writeLine(buildRecord(ev));
    } else if (bucket.count == 1) {
        // Never collapse a lone event; re-emit the representative as-is.
        debug::Event ev;
        ev.category = bucket.category;
        ev.name = bucket.event;
        ev.level = debug::Level::Info;
        ev.fields = bucket.sample;
        if (bucket.sample.contains("message") && bucket.sample["message"].is_string())
            ev.message = bucket.sample["message"].get<std::string>();
        if (bucket.sample.contains("reason") && bucket.sample["reason"].is_string())
            ev.reason = bucket.sample["reason"].get<std::string>();
        mSequence++;
        writeLine(buildRecord(ev));
    }
    bucket = RepeatBucket{};
}

void StructuredLogger::flushAllBuckets() {
    for (auto& kv : mBuckets)
        flushBucket(kv.second);
    mBuckets.clear();
}

// ── Emit ────────────────────────────────────────────────────

void StructuredLogger::emit(const debug::Event& event, bool forceNoAggregate) {
    std::lock_guard<std::mutex> lock(logMutex());
    if (!mInitialized || !mConfig.enabled || !mEventsFile)
        return;
    if (!categoryEnabled(event.category, event.level))
        return;

    // Errors and fatal events bypass aggregation entirely.
    const bool bypassAggregate = forceNoAggregate ||
        event.level == debug::Level::Error ||
        event.level == debug::Level::Fatal;

    if (bypassAggregate) {
        mSequence++;
        writeLine(buildRecord(event));
        return;
    }

    // Build the aggregation key: caller key wins, else category + name + the
    // caller's stable identity fields (never timestamps/seq/positions).
    std::string key = event.aggregationKey;
    if (key.empty()) {
        key = event.category + ":" + event.name;
        if (event.fields.is_object()) {
            for (auto it = event.fields.begin(); it != event.fields.end(); ++it) {
                if (it.value().is_number_integer() || it.value().is_string()) {
                    key += "|";
                    key += it.key();
                    key += "=";
                    key += it.value().dump();
                }
            }
        }
    }

    // Record the representative snapshot under a stable key. Universal keys
    // (category/event/level and the universal field set) are carried separately
    // by the summary record, so they are not copied into the sample fields.
    nlohmann::json sample = nlohmann::json::object();
    if (!event.message.empty()) sample["message"] = event.message;
    if (!event.reason.empty())   sample["reason"] = event.reason;
    if (event.fields.is_object()) {
        for (auto it = event.fields.begin(); it != event.fields.end(); ++it) {
            const std::string& k = it.key();
            if (k == "category" || k == "event" || k == "level" ||
                k == "wall_time" || k == "t" || k == "seq")
                continue;
            sample[k] = it.value();
        }
    }

    const double now = steadySeconds();
    auto it = mBuckets.find(key);
    if (it == mBuckets.end()) {
        // Bound memory: if at capacity, flush the oldest bucket first.
        if (mBuckets.size() >= MAX_ACTIVE_BUCKETS) {
            auto oldest = mBuckets.begin();
            for (auto b = mBuckets.begin(); b != mBuckets.end(); ++b)
                if (b->second.lastTime < oldest->second.lastTime) oldest = b;
            flushBucket(oldest->second);
            mBuckets.erase(oldest);
        }
        RepeatBucket& b = mBuckets[key];
        b.active = true;
        b.key = key;
        b.category = event.category;
        b.event = event.name;
        b.sample = sample;
        b.count = 1;
        b.firstTime = now;
        b.lastTime = now;
        b.firstTick = event.simulationTick;
        b.lastTick = event.simulationTick;
        return;
    }

    RepeatBucket& b = it->second;
    b.count++;
    b.lastTime = now;
    b.lastTick = event.simulationTick;
    b.sample = sample;   // keep the most recent representative values

    if (now - b.firstTime >= (double)mConfig.repeatWindowSeconds) {
        flushBucket(b);
        mBuckets.erase(it);
    }
}

// ── Public free API ─────────────────────────────────────────

namespace debug {

void logEvent(const Event& event) {
    StructuredLogger::instance().emit(event);
}

void flushEvents() {
    StructuredLogger::instance().flushAllBuckets();
}

std::string eventsPath() {
    return StructuredLogger::instance().eventsPath();
}

bool eventsEnabled(const std::string& category, Level level) {
    return StructuredLogger::instance().categoryEnabled(category, level);
}

} // namespace debug

// ── Category/level gate ─────────────────────────────────────

bool StructuredLogger::categoryEnabled(const std::string& category,
                                       debug::Level level) const {
    if (!mConfig.enabled)
        return false;
    auto it = mCategoryLevels.find(category);
    StructuredLevel configured = StructuredLevel::Off;
    if (it != mCategoryLevels.end())
        configured = it->second;
    else
        configured = mConfig.defaultLevel;

    // Map the requested debug level onto the legacy ordered levels.
    StructuredLevel needed;
    switch (level) {
        case debug::Level::Trace: needed = StructuredLevel::Trace; break;
        case debug::Level::Debug: needed = StructuredLevel::Verbose; break;
        case debug::Level::Info:  needed = StructuredLevel::Important; break;
        case debug::Level::Warn:  needed = StructuredLevel::Important; break;
        case debug::Level::Error: needed = StructuredLevel::Errors; break;
        case debug::Level::Fatal: needed = StructuredLevel::Errors; break;
    }
    return (int)needed <= (int)configured;
}

bool StructuredLogger::shouldLog(StructuredCategory cat, StructuredLevel level) const {
    if (!mConfig.enabled) return false;
    return (int)level <= (int)categoryConfigFor(cat).level;
}

// ── Init / Shutdown ─────────────────────────────────────────

void StructuredLogger::init() {
    if (mInitialized) return;

    loadConfig();
    if (!mConfig.enabled) return;

    createRunDir();

    // Open with Windows share flags so VSCode, rg, and Get-Content -Wait can
    // read the file while the game writes.
    mEventsFile = _fsopen(mEventsPath.c_str(), "a", _SH_DENYNO);
    if (!mEventsFile) {
        mInitialized = false;
        return;
    }
    mInitialized = true;
    mStartTime = steadySeconds();

    debug::Event started;
    started.category = "LOGGER";
    started.name = "logger.started";
    started.level = debug::Level::Info;
    started.message = "events jsonl opened";
    started.fields = {
        {"path", mEventsPath},
        {"run_id", mRunId},
        {"pid", (uint64_t)LiveIdentity::pid()},
        {"process", LiveIdentity::process()},
    };
    emit(started, true);
    std::fflush(mEventsFile);
}

void StructuredLogger::shutdown() {
    if (!mInitialized) return;

    flushAllBuckets();

    debug::Event stopped;
    stopped.category = "LOGGER";
    stopped.name = "logger.stopped";
    stopped.level = debug::Level::Info;
    stopped.message = "events jsonl closing";
    stopped.fields = {
        {"path", mEventsPath},
        {"run_id", mRunId},
    };
    emit(stopped, true);

    if (mEventsFile) {
        std::fflush(mEventsFile);
        std::fclose(mEventsFile);
        mEventsFile = nullptr;
    }
    mInitialized = false;

    // Keep a discoverable pointer to the latest run's events file.
    std::error_code ec;
    std::filesystem::create_directories("logs", ec);
    FILE* f = std::fopen("logs/latest-log-path.txt", "w");
    if (f) {
        std::fprintf(f, "%s\n", mEventsPath.c_str());
        std::fclose(f);
    }
}

// ── Config polling (hot-reload) ─────────────────────────────

void StructuredLogger::pollConfig() {
    if (!mConfig.hotReload) return;

    const std::string configPath = "config/debuglogger.json";
    std::error_code ec;
    auto wt = std::filesystem::last_write_time(configPath, ec);
    if (ec) return;

    uint64_t wtCount = wt.time_since_epoch().count();
    if (wtCount != mConfigLastWrite) {
        mConfigLastWrite = wtCount;

        const bool wasInitialized = mInitialized;
        const bool wasEnabled = mConfig.enabled;
        loadConfig();

        // If the run file was not open because the logger was disabled at
        // startup, honor a config change that enables it live.
        if (!wasInitialized && mConfig.enabled)
            init();

        if (mInitialized) {
            debug::Event ev;
            ev.category = "LOGGER";
            ev.name = "logger.config_reloaded";
            ev.level = debug::Level::Info;
            ev.message = "debuglogger.json hot-reloaded";
            ev.fields = {{"enabled", mConfig.enabled}};
            emit(ev, true);
        } else if (wasEnabled && !mConfig.enabled) {
            // Nothing to emit into; the logger is intentionally off.
        }
    }
}

void StructuredLogger::tick() {
    if (!mInitialized || !mConfig.enabled) return;
    const double now = steadySeconds();
    std::lock_guard<std::mutex> lock(logMutex());
    for (auto it = mBuckets.begin(); it != mBuckets.end();) {
        RepeatBucket& b = it->second;
        if (b.active && now - b.firstTime >= (double)mConfig.repeatWindowSeconds) {
            flushBucket(b);
            it = mBuckets.erase(it);
        } else {
            ++it;
        }
    }
}

// ── Legacy Entry bridge (one JSONL record per message) ──────

void StructuredLogger::write(const Entry& e) {
    if (!mInitialized || !mConfig.enabled) return;
    if (!shouldLog(e.category, e.level)) return;

    // Map the legacy structured level onto the generic level so the record
    // reads consistently with debug::logEvent output.
    debug::Level level = debug::Level::Debug;
    switch (e.level) {
        case StructuredLevel::Off:       return;
        case StructuredLevel::Errors:    level = debug::Level::Error; break;
        case StructuredLevel::Important: level = debug::Level::Info; break;
        case StructuredLevel::Verbose:   level = debug::Level::Debug; break;
        case StructuredLevel::Trace:     level = debug::Level::Trace; break;
    }

    debug::Event ev;
    ev.category = categoryName(e.category);
    ev.name = e.eventId.empty() ? "log" : e.eventId;
    ev.level = level;
    ev.message = e.message;
    ev.reason = e.reason;
    ev.correlationId = e.correlationId;
    ev.simulationTick = e.tick;
    ev.frame = e.frame;
    ev.sourceFile = e.sourceFile;
    ev.sourceLine = e.sourceLine;
    ev.functionName = e.functionName;

    if (!e.numericKeys.empty()) {
        nlohmann::json fields = nlohmann::json::object();
        for (size_t i = 0; i < e.numericKeys.size(); ++i) {
            const double expected = i < e.numericExpected.size() ? e.numericExpected[i] : 0.0;
            const double actual = i < e.numericActual.size() ? e.numericActual[i] : 0.0;
            fields[e.numericKeys[i] + "_expected"] = expected;
            fields[e.numericKeys[i] + "_actual"] = actual;
            fields[e.numericKeys[i] + "_difference"] = actual - expected;
        }
        ev.fields = std::move(fields);
    }

    // A numeric assertion failure is an error and must never be aggregated.
    const bool assertionFailed = !e.numericKeys.empty() && e.tolerance > 0.0 &&
        std::fabs((e.numericActual.empty() ? 0.0 : e.numericActual[0]) -
                  (e.numericExpected.empty() ? 0.0 : e.numericExpected[0])) > e.tolerance;
    emit(ev, assertionFailed);
}

void StructuredLogger::writeFormatted(StructuredCategory category, StructuredLevel level,
                                      const char* sourceFile, int sourceLine,
                                      const char* functionName, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    writeVFormatted(category, level, sourceFile, sourceLine, functionName, format, args);
    va_end(args);
}

void StructuredLogger::writeVFormatted(StructuredCategory category, StructuredLevel level,
                                       const char* sourceFile, int sourceLine,
                                       const char* functionName, const char* format, va_list args)
{
    if (!mInitialized || !mConfig.enabled || !shouldLog(category, level)) return;

    char message[4096] = {};
    va_list copy;
    va_copy(copy, args);
    std::vsnprintf(message, sizeof(message), format ? format : "", copy);
    va_end(copy);

    Entry e;
    e.category = category;
    e.level = level;
    e.eventId = "log";
    e.sourceFile = sourceFile ? sourceFile : "?";
    e.sourceLine = sourceLine;
    e.functionName = functionName ? functionName : "?";
    e.message = message;
    write(e);
}

void StructuredLogger::assertNear(
    const std::string& eventId, const std::string& correlationId,
    const std::string& reason, StructuredCategory cat,
    const std::string& sourceFile, int sourceLine,
    const std::string& functionName,
    const std::string& key, double expected, double actual,
    double tolerance, uint32_t tick, uint32_t frame)
{
    if (!mInitialized || !mConfig.enabled) return;
    if (!shouldLog(cat, StructuredLevel::Trace)) return;

    Entry e;
    e.category = cat;
    e.level = std::fabs(actual - expected) > tolerance
        ? StructuredLevel::Errors : StructuredLevel::Verbose;
    e.eventId = eventId;
    e.correlationId = correlationId;
    e.reason = reason;
    e.sourceFile = sourceFile;
    e.sourceLine = sourceLine;
    e.functionName = functionName;
    e.tick = tick;
    e.frame = frame;
    e.numericKeys.push_back(key);
    e.numericExpected.push_back(expected);
    e.numericActual.push_back(actual);
    e.tolerance = tolerance;

    write(e);
}

// ── Audio buffer analysis ───────────────────────────────────

#include <cmath>
#include <limits>
#include <cfloat>

template<typename T>
static AudioBufferAnalysis analyzeAudioBufferImpl(
    const std::vector<T>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    double threshold)
{
    AudioBufferAnalysis a;
    a.sampleRate = sampleRate;
    a.channels = channels;
    a.frameCount = frameCount;
    a.sampleCount = buffer.size();
    a.durationSec = frameCount > 0 ? (double)frameCount / (double)sampleRate : 0.0;

    uint64_t totalSamples = buffer.size();
    if (totalSamples == 0 || frameCount == 0 || channels == 0) return a;

    a.minSample = std::numeric_limits<double>::max();
    a.maxSample = -std::numeric_limits<double>::max();
    double sum = 0.0;
    double sumSq = 0.0;

    std::vector<double> prevSample(channels, 0.0);
    std::vector<bool> firstSamplePerCh(channels, true);

    for (uint64_t i = 0; i < totalSamples; i++) {
        double s = (double)buffer[i];
        int ch = (int)(i % channels);

        if (std::numeric_limits<T>::is_integer) {
            double maxVal = (double)std::numeric_limits<T>::max();
            s = s / maxVal;
        }

        if (std::isnan(s)) { a.nanCount++; continue; }
        if (std::isinf(s)) { a.infCount++; continue; }

        if (s < a.minSample) a.minSample = s;
        if (s > a.maxSample) a.maxSample = s;
        sum += s;
        sumSq += s * s;

        if (std::fabs(s) <= 0.0001) a.zeroCount++;
        if (std::fabs(s) >= 1.0) a.clipCount++;

        if (!firstSamplePerCh[ch]) {
            double delta = std::fabs(s - prevSample[ch]);
            if (delta > threshold) {
                a.discontinuityCount++;
                if (delta > a.largestDiscontinuity)
                    a.largestDiscontinuity = delta;
            }
        }
        prevSample[ch] = s;
        firstSamplePerCh[ch] = false;

        if (i < 16) a.firstSamples.push_back(s);
        if (i >= totalSamples - 16) a.lastSamples.push_back(s);
    }

    uint64_t validSamples = totalSamples - a.nanCount - a.infCount;
    if (validSamples > 0) {
        a.mean = sum / (double)validSamples;
        a.rms = std::sqrt(sumSq / (double)validSamples);
    }
    a.peak = std::max(std::fabs(a.minSample), std::fabs(a.maxSample));

    return a;
}

AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<float>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    uint32_t discontinuityThreshold)
{
    return analyzeAudioBufferImpl<float>(buffer, frameCount, channels, sampleRate, (double)discontinuityThreshold);
}

AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<int16_t>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    uint32_t discontinuityThreshold)
{
    return analyzeAudioBufferImpl<int16_t>(buffer, frameCount, channels, sampleRate, (double)discontinuityThreshold);
}

void logAudioAnalysis(StructuredCategory cat, StructuredLevel level,
    const std::string& eventId, const std::string& correlationId,
    const std::string& stage, const AudioBufferAnalysis& a)
{
    if (!StructuredLogger::instance().shouldLog(cat, level)) return;

    StructuredLogger::Entry e;
    e.category = cat;
    e.level = level;
    e.eventId = eventId;
    e.correlationId = correlationId;
    e.reason = "Audio buffer analysis: " + stage;
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = __FUNCTION__;

    e.numericKeys = {
        "sampleRate", "channels", "frameCount", "durationSec",
        "minSample", "maxSample", "mean", "rms", "peak",
        "zeroCount", "nanCount", "infCount", "clipCount",
        "discontinuityCount", "largestDiscontinuity"
    };
    e.numericExpected = {
        (double)a.sampleRate, (double)a.channels, (double)a.frameCount, a.durationSec,
        a.minSample, a.maxSample, a.mean, a.rms, a.peak,
        (double)a.zeroCount, (double)a.nanCount, (double)a.infCount, (double)a.clipCount,
        (double)a.discontinuityCount, a.largestDiscontinuity
    };
    e.numericActual = e.numericExpected;
    e.tolerance = 0.0;

    StructuredLogger::instance().write(e);
}
