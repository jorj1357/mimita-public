#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// ── Replay export configuration (hot-reloaded from config/replay/replay-export.json)

struct ReplayExportConfig {
    float audioVolumeMultiplier = 0.8f;
    int exportWidth = 1920;
    int exportHeight = 1080;
    int exportBitrate = 0;
    int exportCrf = 18;
};

extern ReplayExportConfig gExportConfig;

struct ReplayExportJob {
    enum State { Idle, Capturing, Encoding, Done, Failed };
    State state = Idle;

    // Capture state
    std::string jsonPath;
    uint32_t totalTicks = 0;
    uint32_t capturedTicks = 0;
    float exportTick = 0.0f;       // fractional tick position (speed-aware)
    int capWidth = 0;
    int capHeight = 0;

    // Desired output dimensions (may differ from capture dimensions)
    int outputWidth = 0;
    int outputHeight = 0;

    // Encoding
    std::string ffmpegPath;
    std::string rawTempPath;
    FILE* rawFile = nullptr;

    // Output
    std::string outputPath;

    // Result
    std::string errorMsg;
    int ffmpegExitCode = -1;
    uint32_t frameWriteCount = 0;
    uint64_t rawFileBytes = 0;
    uint64_t mp4FileBytes = 0;
    float progress() const;
};

std::string defaultFfmpegPath();
bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight);
void updateReplayExport();
bool isReplayExportActive();
float getReplayExportProgress();
std::string getReplayExportResultPath();
std::string getReplayExportStatusText();
void cancelReplayExport();
void openReplayFolder();
const ReplayExportJob& getReplayExportJob();
void setFfmpegDebugMode(bool enabled);
bool isFfmpegDebugMode();
std::string generateExportOutputPath();
std::string makeCmdKArgs(const std::string& cmd);
void pollReplayExportConfig();
float getReplayExportAudioVolume();

// Batch export
void updateReplayBatchExport();
bool isReplayBatchExportActive();

// Export debug counter accessors (defined in engine-tick-camera.cpp)
extern int gRplxImpactWorldCount;
extern int gRplxHitBurstCount;
extern int gRplxDebrisBlockCount;
extern int gRplxEffectDuplicateCount;

// Debug log for replay export diagnostics
void replayExportDebugOpen();
void replayExportDebugClose();
extern FILE* gReplayExportDebugFile;
#define RPLXDEBUG(...) do { if (gReplayExportDebugFile) { fprintf(gReplayExportDebugFile, __VA_ARGS__); fflush(gReplayExportDebugFile); } } while(0)
