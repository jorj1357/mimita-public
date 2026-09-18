// 09 17 2026
/* purpose
* Declares the single authoritative debug output API.
* One append-only JSONL stream (`events.jsonl`) per process run is the complete
* debug record; the legacy Debug/terminal/printf paths bridge into it.
* Provides the generic subsystem-neutral `debug::logEvent` API so any system
* (collision, GUI, audio, weapons, animation, poses, networking, server/client
* state, NPCs, players, actors, hot reload, future systems) can emit events
* without adding new logger functions. Also keeps the legacy StructuredLogger
* surface compiling while call sites migrate gradually.
* Does NOT own gameplay decisions, rendering, audio, or networking transport.
* Does NOT define checker policy or task completion behavior.
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdarg>
#include <cstdio>
#include <nlohmann/json.hpp>

// ── Generic event API (the authoritative surface) ───────────────────────────
namespace debug {

enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

struct Event {
    std::string category;   // e.g. "COLLISION"
    std::string name;       // e.g. "collision.resolved"
    Level level = Level::Info;
    std::string message;
    std::string reason;
    std::string correlationId;

    nlohmann::json fields = nlohmann::json::object();

    uint64_t frame = 0;
    uint64_t simulationTick = 0;
    uint64_t serverTick = 0;
    uint64_t clientTick = 0;
    uint64_t durationUs = 0;

    // Caller-provided aggregation key. When empty the logger derives a stable
    // key from category + name + stable identity fields.
    std::string aggregationKey;

    // Provenance, filled automatically by MIMITA_EVENT; callers normally leave
    // these empty.
    std::string sourceFile;
    int sourceLine = 0;
    std::string functionName;
};

// Emit one event into the process run's events.jsonl stream.
void logEvent(const Event& event);

    // Force any pending repeat-bucket summaries to flush.
    void flushEvents();


// True when the logger is initialized and the category/level is enabled.
bool eventsEnabled(const std::string& category, Level level);

// The active events.jsonl path (empty when not initialized).
std::string eventsPath();

} // namespace debug

// Convenience macro that captures provenance automatically.
#define MIMITA_EVENT(ev) do { \
    ::debug::Event _ev__ = (ev); \
    _ev__.sourceFile = __FILE__; \
    _ev__.sourceLine = __LINE__; \
    _ev__.functionName = __FUNCTION__; \
    ::debug::logEvent(_ev__); \
} while (0)

// ── Legacy structured levels/categories (compatibility) ─────────────────────
enum class StructuredLevel {
    Off = 0,
    Errors,
    Important,
    Verbose,
    Trace
};

struct StructuredLogConfig {
    bool enabled = true;
    bool hotReload = true;
    // Legacy name kept for compatibility; maps to console_mirror.
    bool consoleOutput = false;
    bool summaryFile = false;
    bool eventsFile = true;
    bool consoleMirror = false;
    bool flushEachEvent = true;
    float repeatWindowSeconds = 1.0f;
    StructuredLevel defaultLevel = StructuredLevel::Off;

    struct CategoryConfig {
        StructuredLevel level = StructuredLevel::Off;
        bool fileOutput = true;
        float throttleSeconds = 0.0f;
    };

    CategoryConfig general;
    CategoryConfig glb;
    CategoryConfig replay;
    CategoryConfig camera;
    CategoryConfig audio;
    CategoryConfig physics;
    CategoryConfig performance;
    CategoryConfig collision;
    CategoryConfig npcCombat;
    CategoryConfig npcMovement;
    CategoryConfig ragdoll;
    CategoryConfig weapons;
    CategoryConfig animation;
    CategoryConfig gui;
    CategoryConfig avatar;
    CategoryConfig network;
    CategoryConfig world;
    CategoryConfig duel;
    CategoryConfig auth;
    CategoryConfig chat;
    CategoryConfig vip;
    CategoryConfig rendering;
    CategoryConfig glbModels;
    CategoryConfig executable;
    CategoryConfig grenadeLauncher;
    CategoryConfig healthbar;
    CategoryConfig skybox;
    CategoryConfig chatLayout;

    struct Sampling {
        int defaultEveryNFrames = 60;
        bool logOnChange = true;
        float minimumNumericChange = 0.001f;
    } sampling;

    struct Throttling {
        bool enabled = true;
        int defaultMaxMessagesPerSecond = 20;
        int duplicateMessageWindowMs = 1000;
    } throttling;

    struct ReplayValidation {
        float cameraPositionTolerance = 0.05f;
        float cameraRotationDegreesTolerance = 0.25f;
        float fovDegreesTolerance = 0.1f;
        float quaternionMagnitudeTolerance = 0.0001f;
        float audioVideoSyncToleranceMs = 50.0f;
    } replayValidation;
};

enum class StructuredCategory {
    General,
    Glb,
    Replay,
    Camera,
    Audio,
    Physics,
    Performance,
    Collision,
    NpcCombat,
    NpcMovement,
    Ragdoll,
    Weapons,
    Animation,
    Gui,
    Avatar,
    Network,
    World,
    Duel,
    Auth,
    Chat,
    Vip,
    Rendering,
    GlbModels,
    Executable,
    GrenadeLauncher,
    Healthbar,
    Skybox,
    ChatLayout,
    Count
};

// ── StructuredLogger (compatibility facade) ─────────────────────────────────
// Owns config, the single events.jsonl handle, sequence, and repeat buckets.
// The legacy Entry/write path now emits one JSONL record per message.
class StructuredLogger {
public:
    static StructuredLogger& instance();

    // Init: read config, create the run directory, open events.jsonl, emit
    // `logger.started`, flush.
    void init();
    // Shutdown: flush repeat buckets, emit `logger.stopped`, flush, close.
    void shutdown();
    // Poll the config file for hot-reload.
    void pollConfig();

    // ── Legacy structured log entry ───────────────────────
    struct Entry {
        StructuredCategory category;
        StructuredLevel level;
        std::string eventId;
        std::string correlationId;
        std::string reason;
        std::string sourceFile;
        int sourceLine;
        std::string functionName;
        uint32_t tick = 0;
        uint32_t frame = 0;
        std::vector<std::string> numericKeys;
        std::vector<double> numericExpected;
        std::vector<double> numericActual;
        double tolerance = 0.0;
        std::string message;
    };

    void write(const Entry& e);

    void writeFormatted(StructuredCategory category, StructuredLevel level,
                        const char* sourceFile, int sourceLine,
                        const char* functionName, const char* format, ...);
    void writeVFormatted(StructuredCategory category, StructuredLevel level,
                         const char* sourceFile, int sourceLine,
                         const char* functionName, const char* format, va_list args);

    // Tick: flush time-expired repeat buckets.
    void tick();

    void assertNear(const std::string& eventId, const std::string& correlationId,
                    const std::string& reason, StructuredCategory cat,
                    const std::string& sourceFile, int sourceLine,
                    const std::string& functionName,
                    const std::string& key, double expected, double actual,
                    double tolerance, uint32_t tick = 0, uint32_t frame = 0);

    bool shouldLog(StructuredCategory cat, StructuredLevel level) const;

    const StructuredLogConfig& config() const { return mConfig; }

    const std::string& logDir() const { return mLogDir; }
    const std::string& runId() const { return mRunId; }

    // The authoritative JSONL path for this run.
    const std::string& eventsPath() const { return mEventsPath; }

    // ── Internal writer surface used by debug::logEvent ─────
    // Emits one JSONL record with universal fields and aggregation. Public so
    // the free `debug::logEvent` function can reach it without friendship.
    void emit(const debug::Event& event, bool forceNoAggregate = false);

    // Flush every pending repeat bucket (used by debug::flushEvents).
    void flushAllBuckets();

    // Category/level gate over the string-keyed config map.
    bool categoryEnabled(const std::string& category, debug::Level level) const;

    // ── Repeat aggregation state ────────────────────────────
    struct RepeatBucket {
        bool active = false;
        std::string key;
        std::string category;
        std::string event;
        nlohmann::json sample = nlohmann::json::object();
        uint64_t count = 0;
        double firstTime = 0.0;
        double lastTime = 0.0;
        uint64_t firstTick = 0;
        uint64_t lastTick = 0;
    };

private:
    StructuredLogger() = default;
    ~StructuredLogger();
    StructuredLogger(const StructuredLogger&) = delete;
    StructuredLogger& operator=(const StructuredLogger&) = delete;

    void loadConfig();
    void createRunDir();
    std::string categoryName(StructuredCategory cat) const;
    StructuredLevel levelFromString(const std::string& s) const;
    std::string levelToString(StructuredLevel lvl) const;
    std::string debugLevelToString(debug::Level lvl) const;
    const StructuredLogConfig::CategoryConfig& categoryConfigFor(StructuredCategory cat) const;
    void writeLine(const std::string& json);
    void flushBucket(RepeatBucket& bucket);
    std::string buildRecord(const debug::Event& event) const;

    StructuredLogConfig mConfig;
    bool mInitialized = false;
    std::string mLogDir;       // logs/yyyy-mm-dd/hhmmss/
    std::string mRunId;        // hhmmss used for this run
    std::string mEventsPath;   // <mLogDir>/events.jsonl
    FILE* mEventsFile = nullptr;
    uint64_t mSequence = 0;
    double mStartTime = 0.0;

    // String-keyed category levels from config (uppercase keys).
    std::unordered_map<std::string, StructuredLevel> mCategoryLevels;

    // Active repeat buckets, bounded. Keyed by aggregation key.
    std::unordered_map<std::string, RepeatBucket> mBuckets;

    uint64_t mConfigLastWrite = 0;
    int mConfigReloadErrors = 0;
};

// ── Convenience: structured log with formatted message ─────────────
inline void logStructured(StructuredCategory cat, StructuredLevel level,
                          const std::string& eventId,
                          const std::string& correlationId,
                          const std::string& reason,
                          const std::string& message)
{
    if (!StructuredLogger::instance().shouldLog(cat, level))
        return;
    StructuredLogger::Entry e;
    e.category = cat;
    e.level = level;
    e.eventId = eventId;
    e.correlationId = correlationId;
    e.reason = reason;
    e.sourceFile = "";
    e.sourceLine = 0;
    e.functionName = "";
    e.message = message;
    StructuredLogger::instance().write(e);
}

// ── Audio buffer analysis ───────────────────────────────────
struct AudioBufferAnalysis {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    uint64_t frameCount = 0;
    uint64_t sampleCount = 0;
    double durationSec = 0.0;
    double minSample = 0.0;
    double maxSample = 0.0;
    double mean = 0.0;
    double rms = 0.0;
    double peak = 0.0;
    uint64_t zeroCount = 0;
    uint64_t nanCount = 0;
    uint64_t infCount = 0;
    uint64_t clipCount = 0;
    uint64_t discontinuityCount = 0;
    double largestDiscontinuity = 0.0;
    std::vector<double> firstSamples;
    std::vector<double> lastSamples;
};

AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<float>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    uint32_t discontinuityThreshold = 0.8f);

AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<int16_t>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    uint32_t discontinuityThreshold = 0x7FFF);

void logAudioAnalysis(StructuredCategory cat, StructuredLevel level,
    const std::string& eventId, const std::string& correlationId,
    const std::string& stage, const AudioBufferAnalysis& analysis);

// ── Convenience macros (legacy) ─────────────────────────────

#define MIMITA_LOG(cat, level, eventId, correlationId, reason, ...) do { \
    if (::StructuredLogger::instance().shouldLog(cat, level)) { \
        ::StructuredLogger::Entry _e__; \
        _e__.category = cat; \
        _e__.level = level; \
        _e__.eventId = eventId; \
        _e__.correlationId = correlationId; \
        _e__.reason = reason; \
        _e__.sourceFile = __FILE__; \
        _e__.sourceLine = __LINE__; \
        _e__.functionName = __FUNCTION__; \
        char _msg__[1024] = {}; \
        std::snprintf(_msg__, sizeof(_msg__), ##__VA_ARGS__); \
        _e__.message = _msg__; \
        ::StructuredLogger::instance().write(_e__); \
    } \
} while(0)

#define MIMITA_ASSERT_NEAR(cat, eventId, corrId, reason, key, expected, actual, tolerance) \
    ::StructuredLogger::instance().assertNear( \
        eventId, corrId, reason, cat, \
        __FILE__, __LINE__, __FUNCTION__, \
        key, (double)(expected), (double)(actual), (double)(tolerance))

#define DBG(cat, ...) \
    ::StructuredLogger::instance().writeFormatted( \
        ::StructuredCategory::cat, ::StructuredLevel::Verbose, \
        __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
