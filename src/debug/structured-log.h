// 07 20 2026, 19 43
/* purpose
* Declares the structured debug logging API and configuration types.
* Provides category, level, throttling, and structured field definitions.
* Shares logger entry points used by gameplay, networking, and diagnostics.
* Does NOT own gameplay decisions, rendering, audio, or networking transport.
* Does NOT replace the lightweight Debug log facade.
* Does NOT define checker policy or task completion behavior.
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ── Structured debug logger ─────────────────────────────────
// Extends the existing Debug::log system with:
//   - hot-reloadable JSON config (config/debuglogger.json)
//   - per-category log levels (OFF/ERRORS/IMPORTANT/VERBOSE/TRACE)
//   - per-category separate log files
//   - structured fields: event ID, correlation ID, tick, frame
//   - numeric assertions with tolerance
//   - throttling and per-N-frame sampling
//   - startup metadata
//   - summary file generation

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
    bool consoleOutput = true;
    bool summaryFile = true;
    StructuredLevel defaultLevel = StructuredLevel::Off;

    struct CategoryConfig {
        StructuredLevel level = StructuredLevel::Off;
        bool fileOutput = true;
        float throttleSeconds = 0.0f; // 0 = no throttle; >0 = buffer and flush at this rate
    };

    CategoryConfig replay;
    CategoryConfig camera;
    CategoryConfig audio;
    CategoryConfig performance;
    CategoryConfig collision;
    CategoryConfig gui;
    CategoryConfig avatar;
    CategoryConfig network;
    CategoryConfig rendering;
    CategoryConfig glbModels;
    CategoryConfig executable;
    CategoryConfig grenadeLauncher;

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
    Replay,
    Camera,
    Audio,
    Performance,
    Collision,
    Gui,
    Avatar,
    Network,
    Rendering,
    GlbModels,
    Executable,
    GrenadeLauncher,
    Count
};

// ── StructuredLogger ────────────────────────────────────────
// Singleton. Owns config, category files, event counters.
class StructuredLogger {
public:
    static StructuredLogger& instance();

    // Init: read config, create log dirs, write startup metadata
    void init();
    // Shutdown: flush files, write summary, close handles
    void shutdown();
    // Poll config file for hot-reload
    void pollConfig();

    // ── Structured log entry ──────────────────────────────
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
        // Numeric fields (key, expected, actual)
        std::vector<std::string> numericKeys;
        std::vector<double> numericExpected;
        std::vector<double> numericActual;
        double tolerance = 0.0;
        std::string message;
    };

    // Write a structured entry
    void write(const Entry& e);

    // Tick: flush throttled buffers
    void tick();

    // Convenience: numeric assertion
    void assertNear(const std::string& eventId, const std::string& correlationId,
                    const std::string& reason, StructuredCategory cat,
                    const std::string& sourceFile, int sourceLine,
                    const std::string& functionName,
                    const std::string& key, double expected, double actual,
                    double tolerance, uint32_t tick = 0, uint32_t frame = 0);

    // Check if a category/level should be logged
    bool shouldLog(StructuredCategory cat, StructuredLevel level) const;

    // Get the config (for validation tolerances etc.)
    const StructuredLogConfig& config() const { return mConfig; }

    // Public accessors for performance profiler routing
    const std::string& logDir() const { return mLogDir; }
    const std::string& runId() const { return mRunId; }

private:
    StructuredLogger() = default;
    ~StructuredLogger();
    StructuredLogger(const StructuredLogger&) = delete;
    StructuredLogger& operator=(const StructuredLogger&) = delete;

    void loadConfig();
    void createLogDir();
    void openCategoryFile(StructuredCategory cat);
    void writeStartupMetadata();
    void writeSummary();
    std::string categoryName(StructuredCategory cat) const;
    StructuredLevel levelFromString(const std::string& s) const;
    std::string levelToString(StructuredLevel lvl) const;
    std::string categoryDirName(StructuredCategory cat) const;
    std::string timestamp() const;
    std::string runTimestamp() const;

    StructuredLogConfig mConfig;
    bool mInitialized = false;
    std::string mLogDir;       // logs/MM-DD-YYYY/
    std::string mRunId;        // HHMMSS used for all files this run
    uint64_t mEventCounters[(int)StructuredCategory::Count] = {};

    // Category file handles (nullptr = not open for this run)
    FILE* mCategoryFiles[(int)StructuredCategory::Count] = {};

    // Config file tracking for hot-reload
    uint64_t mConfigLastWrite = 0;
    int mConfigReloadErrors = 0;

    // ── Per-category throttled buffers ────────────────────────────
    struct ThrottledBuffer {
        std::vector<std::string> lines;     // accumulated formatted lines
        std::vector<std::string> consoleLines; // accumulated console lines
        double lastFlushTime = 0.0;
    };
    std::unordered_map<int, ThrottledBuffer> mThrottledBuffers;
    void flushThrottled(int catIdx);
};

// ── Convenience: structured log with formatted message ─────────────
// Builds an Entry with the given event/correlation IDs and writes it only if
// the category/level is enabled (avoids the variadic-macro pitfalls).
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

// Analyze a float audio buffer and return metrics.
// channels must be 1 or 2. For stereo, left/right are analyzed separately.
AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<float>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    uint32_t discontinuityThreshold = 0.8f);

// Analyze an int16 audio buffer (same interface).
AudioBufferAnalysis analyzeAudioBuffer(
    const std::vector<int16_t>& buffer, uint32_t frameCount,
    uint16_t channels, uint32_t sampleRate,
    uint32_t discontinuityThreshold = 0x7FFF);

// Convenient: log an audio buffer analysis to a category
void logAudioAnalysis(StructuredCategory cat, StructuredLevel level,
    const std::string& eventId, const std::string& correlationId,
    const std::string& stage, const AudioBufferAnalysis& analysis);

// ── Convenience macros ──────────────────────────────────────
// These capture __FILE__, __LINE__, __FUNCTION__ automatically.

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
