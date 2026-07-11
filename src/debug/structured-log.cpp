#include "structured-log.h"
#include "debug-log.h"
#include "../config.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

// ── Singleton ───────────────────────────────────────────────

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger s;
    return s;
}

StructuredLogger::~StructuredLogger() {
    if (mInitialized) shutdown();
}

// ── Helpers ─────────────────────────────────────────────────

std::string StructuredLogger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &local);
    std::string result = buf;
    result += "." + std::to_string(ms.count());
    return result;
}

std::string StructuredLogger::runTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H%M%S", &local);
    return buf;
}

std::string StructuredLogger::categoryName(StructuredCategory cat) const {
    switch (cat) {
        case StructuredCategory::Replay:      return "REPLAY";
        case StructuredCategory::Camera:      return "CAMERA";
        case StructuredCategory::Audio:       return "AUDIO";
        case StructuredCategory::Performance: return "PERFORMANCE";
        case StructuredCategory::Collision:   return "COLLISION";
        case StructuredCategory::Gui:         return "GUI";
        case StructuredCategory::Avatar:      return "AVATAR";
        case StructuredCategory::Network:     return "NETWORK";
        case StructuredCategory::Rendering:   return "RENDERING";
        case StructuredCategory::GlbModels:   return "GLB_MODELS";
        case StructuredCategory::Executable:  return "EXECUTABLE";
    }
    return "UNKNOWN";
}

std::string StructuredLogger::categoryDirName(StructuredCategory cat) const {
    switch (cat) {
        case StructuredCategory::Replay:      return "Replay";
        case StructuredCategory::Camera:      return "Camera";
        case StructuredCategory::Audio:       return "Audio";
        case StructuredCategory::Performance: return "Performance";
        case StructuredCategory::Collision:   return "Collisions";
        case StructuredCategory::Gui:         return "GUI";
        case StructuredCategory::Avatar:      return "Avatar";
        case StructuredCategory::Network:     return "Network";
        case StructuredCategory::Rendering:   return "Rendering";
        case StructuredCategory::GlbModels:   return "GLBModels";
        case StructuredCategory::Executable:  return "Executable";
    }
    return "Unknown";
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

// ── Config loading ──────────────────────────────────────────

static StructuredLogConfig::CategoryConfig parseCategoryConfig(
    const nlohmann::json& j, StructuredLevel defaultLevel)
{
    StructuredLogConfig::CategoryConfig cfg;
    cfg.level = defaultLevel;
    cfg.fileOutput = true;
    if (j.contains("level")) {
        std::string lvl = j["level"].get<std::string>();
        if (lvl == "off")       cfg.level = StructuredLevel::Off;
        else if (lvl == "errors")    cfg.level = StructuredLevel::Errors;
        else if (lvl == "important") cfg.level = StructuredLevel::Important;
        else if (lvl == "verbose")   cfg.level = StructuredLevel::Verbose;
        else if (lvl == "trace")     cfg.level = StructuredLevel::Trace;
    }
    if (j.contains("file_output"))
        cfg.fileOutput = j["file_output"].get<bool>();
    return cfg;
}

void StructuredLogger::loadConfig() {
    const std::string configPath = "config/debuglogger.json";
    std::ifstream f(configPath);
    if (!f.is_open()) {
        Debug::log(Debug::Category::General,
            "[STRUCTURED_LOG] Config not found: %s\n", configPath.c_str());
        return;
    }

    try {
        nlohmann::json j;
        f >> j;

        StructuredLogConfig cfg;

        if (j.contains("enabled"))          cfg.enabled = j["enabled"].get<bool>();
        if (j.contains("hot_reload"))       cfg.hotReload = j["hot_reload"].get<bool>();
        if (j.contains("console_output"))   cfg.consoleOutput = j["console_output"].get<bool>();
        if (j.contains("summary_file"))     cfg.summaryFile = j["summary_file"].get<bool>();
        if (j.contains("default_level"))
            cfg.defaultLevel = levelFromString(j["default_level"].get<std::string>());

        if (j.contains("categories")) {
            auto& cats = j["categories"];
            if (cats.contains("replay"))
                cfg.replay = parseCategoryConfig(cats["replay"], cfg.defaultLevel);
            if (cats.contains("camera"))
                cfg.camera = parseCategoryConfig(cats["camera"], cfg.defaultLevel);
            if (cats.contains("audio"))
                cfg.audio = parseCategoryConfig(cats["audio"], cfg.defaultLevel);
            if (cats.contains("performance"))
                cfg.performance = parseCategoryConfig(cats["performance"], cfg.defaultLevel);
            if (cats.contains("collision"))
                cfg.collision = parseCategoryConfig(cats["collision"], cfg.defaultLevel);
            if (cats.contains("gui"))
                cfg.gui = parseCategoryConfig(cats["gui"], cfg.defaultLevel);
            if (cats.contains("avatar"))
                cfg.avatar = parseCategoryConfig(cats["avatar"], cfg.defaultLevel);
            if (cats.contains("network"))
                cfg.network = parseCategoryConfig(cats["network"], cfg.defaultLevel);
            if (cats.contains("rendering"))
                cfg.rendering = parseCategoryConfig(cats["rendering"], cfg.defaultLevel);
            if (cats.contains("glb_models"))
                cfg.glbModels = parseCategoryConfig(cats["glb_models"], cfg.defaultLevel);
            if (cats.contains("executable"))
                cfg.executable = parseCategoryConfig(cats["executable"], cfg.defaultLevel);
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
        mConfigReloadErrors = 0;
    }
    catch (const std::exception& e) {
        mConfigReloadErrors++;
        Debug::error(Debug::Category::General,
            "[STRUCTURED_LOG] Config parse error: %s (file: %s, errors=%d)\n",
            e.what(), configPath.c_str(), mConfigReloadErrors);
    }
}

// ── Log directory ───────────────────────────────────────────

void StructuredLogger::createLogDir() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%m-%d-%Y", &local);
    mLogDir = "logs/" + std::string(dateBuf);
    std::error_code ec;
    std::filesystem::create_directories(mLogDir, ec);
}

// ── Category file ───────────────────────────────────────────

void StructuredLogger::openCategoryFile(StructuredCategory cat) {
    int idx = (int)cat;
    if (idx < 0 || idx >= 11) return;
    if (mCategoryFiles[idx]) return;

    auto& catCfg = [&]() -> const StructuredLogConfig::CategoryConfig& {
        switch (cat) {
            case StructuredCategory::Replay:      return mConfig.replay;
            case StructuredCategory::Camera:      return mConfig.camera;
            case StructuredCategory::Audio:       return mConfig.audio;
            case StructuredCategory::Performance: return mConfig.performance;
            case StructuredCategory::Collision:   return mConfig.collision;
            case StructuredCategory::Gui:         return mConfig.gui;
            case StructuredCategory::Avatar:      return mConfig.avatar;
            case StructuredCategory::Network:     return mConfig.network;
            case StructuredCategory::Rendering:   return mConfig.rendering;
            case StructuredCategory::GlbModels:   return mConfig.glbModels;
            case StructuredCategory::Executable:  return mConfig.executable;
        }
        return mConfig.replay;
    }();

    if (!catCfg.fileOutput) return;

    std::string fileName = categoryDirName(cat) + "_log_" + mRunId + ".txt";
    std::string filePath = mLogDir + "/" + fileName;

    FILE* f = fopen(filePath.c_str(), "w");
    if (f) {
        mCategoryFiles[idx] = f;
        // Write file header
        fprintf(f, "==================================================\n");
        fprintf(f, " %s LOG\n", categoryName(cat).c_str());
        fprintf(f, " Start: %s\n", timestamp().c_str());
        fprintf(f, " Run ID: %s\n", mRunId.c_str());
        fprintf(f, "==================================================\n\n");
        fflush(f);
    }
}

// ── Startup metadata ────────────────────────────────────────

void StructuredLogger::writeStartupMetadata() {
    // Write to executable log if enabled
    if (mCategoryFiles[(int)StructuredCategory::Executable]) {
        FILE* f = mCategoryFiles[(int)StructuredCategory::Executable];
        fprintf(f, "--- Startup Metadata ---\n");
        fprintf(f, " Timestamp: %s\n", timestamp().c_str());
        fprintf(f, " Run ID: %s\n", mRunId.c_str());
        fprintf(f, " Log directory: %s\n", mLogDir.c_str());
        fprintf(f, " Enabled categories:\n");
        for (int i = 0; i < 11; i++) {
            if (mCategoryFiles[i]) {
                StructuredCategory cat = (StructuredCategory)i;
                auto& cfg = [&]() -> const StructuredLogConfig::CategoryConfig& {
                    switch (cat) {
                        case StructuredCategory::Replay:      return mConfig.replay;
                        case StructuredCategory::Camera:      return mConfig.camera;
                        case StructuredCategory::Audio:       return mConfig.audio;
                        case StructuredCategory::Performance: return mConfig.performance;
                        case StructuredCategory::Collision:   return mConfig.collision;
                        case StructuredCategory::Gui:         return mConfig.gui;
                        case StructuredCategory::Avatar:      return mConfig.avatar;
                        case StructuredCategory::Network:     return mConfig.network;
                        case StructuredCategory::Rendering:   return mConfig.rendering;
                        case StructuredCategory::GlbModels:   return mConfig.glbModels;
                        case StructuredCategory::Executable:  return mConfig.executable;
                    }
                    return mConfig.replay;
                }();
                fprintf(f, "   %-12s level=%-10s file=%s\n",
                    categoryName(cat).c_str(),
                    levelToString(cfg.level).c_str(),
                    cfg.fileOutput ? "yes" : "no");
            }
        }
        fprintf(f, "--- End Startup Metadata ---\n\n");
        fflush(f);
    }
}

// ── Summary file ────────────────────────────────────────────

void StructuredLogger::writeSummary() {
    if (!mConfig.summaryFile) return;

    std::string summaryPath = mLogDir + "/Summary_" + mRunId + ".txt";
    FILE* sf = fopen(summaryPath.c_str(), "w");
    if (!sf) return;

    fprintf(sf, "==================================================\n");
    fprintf(sf, " STRUCTURED LOG SUMMARY\n");
    fprintf(sf, " Run ID: %s\n", mRunId.c_str());
    fprintf(sf, " Generated: %s\n", timestamp().c_str());
    fprintf(sf, "==================================================\n\n");

    // Collect content from all category files
    for (int i = 0; i < 11; i++) {
        if (!mCategoryFiles[i]) continue;
        StructuredCategory cat = (StructuredCategory)i;

        // Flush category file first
        if (mCategoryFiles[i]) {
            fflush(mCategoryFiles[i]);
        }

        std::string fileName = categoryDirName(cat) + "_log_" + mRunId + ".txt";
        std::string filePath = mLogDir + "/" + fileName;

        fprintf(sf, "\n");
        fprintf(sf, "==================================================\n");
        fprintf(sf, " %s LOG\n", categoryName(cat).c_str());
        fprintf(sf, " Source: %s\n", filePath.c_str());
        fprintf(sf, "==================================================\n\n");

        // Copy file contents
        FILE* cf = fopen(filePath.c_str(), "r");
        if (cf) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), cf)) > 0) {
                fwrite(buf, 1, n, sf);
            }
            fclose(cf);
        }
    }

    fclose(sf);
}

// ── Init / Shutdown ─────────────────────────────────────────

void StructuredLogger::init() {
    if (mInitialized) return;

    mRunId = runTimestamp();
    loadConfig();
    if (!mConfig.enabled) return;

    createLogDir();

    // Open category files for enabled categories
    for (int i = 0; i < 11; i++) {
        openCategoryFile((StructuredCategory)i);
    }

    writeStartupMetadata();
    mInitialized = true;

    Debug::log(Debug::Category::General,
        "[STRUCTURED_LOG] Initialized: dir=%s run=%s\n",
        mLogDir.c_str(), mRunId.c_str());
}

void StructuredLogger::shutdown() {
    if (!mInitialized) return;

    writeSummary();

    for (int i = 0; i < 11; i++) {
        if (mCategoryFiles[i]) {
            fprintf(mCategoryFiles[i], "\n--- End of log ---\n");
            fclose(mCategoryFiles[i]);
            mCategoryFiles[i] = nullptr;
        }
    }

    mInitialized = false;
    Debug::log(Debug::Category::General,
        "[STRUCTURED_LOG] Shutdown complete: dir=%s run=%s\n",
        mLogDir.c_str(), mRunId.c_str());
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

        // Save old config to compare level changes
        StructuredLogConfig oldCfg = mConfig;
        loadConfig();

        // Re-open category files if level/file_output changed
        if (mInitialized) {
            for (int i = 0; i < 11; i++) {
                StructuredCategory cat = (StructuredCategory)i;
                auto& newCfg = [&]() -> const StructuredLogConfig::CategoryConfig& {
                    switch (cat) {
                        case StructuredCategory::Replay:      return mConfig.replay;
                        case StructuredCategory::Camera:      return mConfig.camera;
                        case StructuredCategory::Audio:       return mConfig.audio;
                        case StructuredCategory::Performance: return mConfig.performance;
                        case StructuredCategory::Collision:   return mConfig.collision;
                        case StructuredCategory::Gui:         return mConfig.gui;
                        case StructuredCategory::Avatar:      return mConfig.avatar;
                        case StructuredCategory::Network:     return mConfig.network;
                        case StructuredCategory::Rendering:   return mConfig.rendering;
                        case StructuredCategory::GlbModels:   return mConfig.glbModels;
                        case StructuredCategory::Executable:  return mConfig.executable;
                    }
                    return mConfig.replay;
                }();

                if (newCfg.fileOutput && !mCategoryFiles[i]) {
                    openCategoryFile(cat);
                } else if (!newCfg.fileOutput && mCategoryFiles[i]) {
                    fclose(mCategoryFiles[i]);
                    mCategoryFiles[i] = nullptr;
                }
            }
        }

        Debug::log(Debug::Category::General,
            "[STRUCTURED_LOG] Config hot-reloaded\n");
    }
}

// ── Should Log ──────────────────────────────────────────────

bool StructuredLogger::shouldLog(StructuredCategory cat, StructuredLevel level) const {
    if (!mConfig.enabled) return false;

    const auto& catCfg = [&]() -> const StructuredLogConfig::CategoryConfig& {
        switch (cat) {
            case StructuredCategory::Replay:      return mConfig.replay;
            case StructuredCategory::Camera:      return mConfig.camera;
            case StructuredCategory::Audio:       return mConfig.audio;
            case StructuredCategory::Performance: return mConfig.performance;
            case StructuredCategory::Collision:   return mConfig.collision;
            case StructuredCategory::Gui:         return mConfig.gui;
            case StructuredCategory::Avatar:      return mConfig.avatar;
            case StructuredCategory::Network:     return mConfig.network;
            case StructuredCategory::Rendering:   return mConfig.rendering;
            case StructuredCategory::GlbModels:   return mConfig.glbModels;
            case StructuredCategory::Executable:  return mConfig.executable;
        }
        return mConfig.replay;
    }();

    return (int)level <= (int)catCfg.level;
}

// ── Write entry ─────────────────────────────────────────────

void StructuredLogger::write(const Entry& e) {
    if (!mInitialized || !mConfig.enabled) return;
    if (!shouldLog(e.category, e.level)) return;

    int idx = (int)e.category;
    if (idx < 0 || idx >= 11) return;
    FILE* f = mCategoryFiles[idx];
    if (!f) return;

    uint64_t& counter = mEventCounters[idx];
    counter++;

    // Build structured log line
    char buf[4096];
    int pos = 0;

    // Header
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "[%s]\n", timestamp().c_str());
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "Level: %s\n", levelToString(e.level).c_str());
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "Category: %s\n", categoryName(e.category).c_str());
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "Event ID: %s_%06llu\n", categoryName(e.category).c_str(),
        (unsigned long long)counter);
    if (!e.correlationId.empty())
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "Correlation ID: %s\n", e.correlationId.c_str());
    if (!e.reason.empty())
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "Reason: %s\n", e.reason.c_str());
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "Source: %s\n", e.sourceFile.c_str());
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "Line: %d\n", e.sourceLine);
    pos += std::snprintf(buf + pos, sizeof(buf) - pos,
        "Function: %s\n", e.functionName.c_str());
    if (e.tick > 0)
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "Tick: %u\n", e.tick);
    if (e.frame > 0)
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "Frame: %u\n", e.frame);

    // Numeric fields
    for (size_t i = 0; i < e.numericKeys.size(); i++) {
        const std::string& key = e.numericKeys[i];
        double expected = i < e.numericExpected.size() ? e.numericExpected[i] : 0.0;
        double actual = i < e.numericActual.size() ? e.numericActual[i] : 0.0;

        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "  %s:\n", key.c_str());
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "    Expected: %.6f\n", expected);
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "    Actual:   %.6f\n", actual);
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "    Difference: %.6f\n", actual - expected);

        if (e.tolerance > 0.0) {
            double diff = std::fabs(actual - expected);
            pos += std::snprintf(buf + pos, sizeof(buf) - pos,
                "    Tolerance: %.6f\n", e.tolerance);
            pos += std::snprintf(buf + pos, sizeof(buf) - pos,
                "    Status: %s\n", diff <= e.tolerance ? "PASS" : "FAIL");
        }
    }

    if (!e.message.empty())
        pos += std::snprintf(buf + pos, sizeof(buf) - pos,
            "Message: %s\n", e.message.c_str());

    pos += std::snprintf(buf + pos, sizeof(buf) - pos, "\n");

    fprintf(f, "%s", buf);
    fflush(f);

    // Console output
    if (mConfig.consoleOutput) {
        printf("[%s][%s] %s", categoryName(e.category).c_str(),
               levelToString(e.level).c_str(), e.reason.c_str());
        if (!e.numericKeys.empty()) {
            for (size_t i = 0; i < e.numericKeys.size(); i++) {
                double expected = i < e.numericExpected.size() ? e.numericExpected[i] : 0.0;
                double actual = i < e.numericActual.size() ? e.numericActual[i] : 0.0;
                printf(" %s: exp=%.4f act=%.4f diff=%.4f",
                       e.numericKeys[i].c_str(), expected, actual, actual - expected);
            }
        }
        printf("\n");
    }
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

    // Analyze samples per channel
    std::vector<double> prevSample(channels, 0.0);
    std::vector<bool> firstSamplePerCh(channels, true);

    for (uint64_t i = 0; i < totalSamples; i++) {
        double s = (double)buffer[i];
        int ch = (int)(i % channels);

        // Convert int16 if needed (normalize to [-1, 1])
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

        // Discontinuity detection (per-channel, independent)
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

        // Record first/last 16 samples
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

// ── Numeric assertion ───────────────────────────────────────

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
