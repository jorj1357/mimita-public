// 08 16 2026, 01 35
/* purpose
* Declares replay MP4 export configuration, job state, and backend APIs.
* Supports Windows Media Foundation by default and optional FFmpeg exports.
* Exposes clip completion, Explorer selection, and export lifecycle helpers.
* Does NOT capture gameplay state or render replay frames itself.
* Does NOT own replay recording or editor command registration.
* Does NOT bundle or download an external encoder.
*/
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// ── Replay export configuration (hot-reloaded from config/replay/replay-export.json)

struct ReplayExportConfig {
    std::string encoder = "windows";
    // encoderMode: "auto" (default; integrated GPU encoder first, software
    // fallback, never the discrete GPU) | "discrete" | "software"
    std::string encoderMode = "auto";
    float audioVolumeMultiplier = 0.8f;
    int exportWidth = 1920;
    int exportHeight = 1080;
    int exportBitrate = 0;
    int exportCrf = 18;
};

// Per-export-frame timing buckets (ms). Accumulated by the export capture path
// across replay seek/update, replay effects/audio, render, and readback/encode.
struct ReplayExportFrameTimings {
    double seekMs = 0.0;
    double updateMs = 0.0;
    double weaponEventsMs = 0.0;
    double audioEventsMs = 0.0;
    double renderMs = 0.0;
    double readPixelsMs = 0.0;
    double copyMs = 0.0;
    double encoderMs = 0.0;
    double waitMs = 0.0;
    double totalMs = 0.0;
};

// Timing state (one export at a time; written on the main thread only).
extern ReplayExportFrameTimings gExportFrameTimings;
extern ReplayExportFrameTimings gExportTimingTotals;
extern uint32_t gExportTimingFrames;
double replayExportNowSec();
void replayExportTimingFrameBegin();
void replayExportTimingFrameEnd();
void replayExportTimingReset();
void replayExportTimingLogSummary();

// Gates verbose per-frame export logs off by default.
extern bool gReplayExportVerbose;

struct MfMp4Writer;

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
    MfMp4Writer* mfWriter = nullptr;
    bool restoreLiveOnFinish = false;
    bool clipExport = false;
    double startTimeSec = 0.0;
    bool mfExportOk = false;
    bool mfOutroMissing = false;

    // Reused pixel buffers (avoids a fresh allocation every exported frame).
    std::vector<uint8_t> pixelBuffer;
    std::vector<uint8_t> flipBuffer;

    // Editor state before export (restored after to prevent corruption)
    bool editorWasFreecam = false;
    float savedFreecamPos[3];
    float savedFreecamRot[4];
    float savedFreecamRoll = 0.0f;
    float savedFreecamFov = 70.0f;
    char savedCameraMode[64];

    // Output
    std::string outputPath;

    // Result
    std::string errorMsg;
    int ffmpegExitCode = -1;
    void* ffmpegProcess = nullptr;
    void* ffmpegLogHandle = nullptr;
    std::string ffmpegBatchPath;
    std::string ffmpegLogPath;
    std::string ffmpegWavPath;
    bool ffmpegWithAudio = false;
    uint32_t frameWriteCount = 0;
    uint64_t rawFileBytes = 0;
    uint64_t mp4FileBytes = 0;
    float progress() const;
};

std::string defaultFfmpegPath();
bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight,
                       bool restoreLiveOnFinish = false);
void updateReplayExport();
bool isReplayExportActive();
float getReplayExportProgress();
std::string getReplayExportResultPath();
std::string getReplayExportStatusText();
void cancelReplayExport();
void restoreReplayExportEditorState();
void openReplayFolder();
void openLastReplayExport();
void copyTextToClipboard(const std::string& text);
void finishReplayExport(bool success, const std::string& error = {});
const ReplayExportJob& getReplayExportJob();
void setFfmpegDebugMode(bool enabled);
bool isFfmpegDebugMode();
std::string generateExportOutputPath();
std::string makeCmdKArgs(const std::string& cmd);
void pollReplayExportConfig();
float getReplayExportAudioVolume();
bool buildReplayExportAudio(const std::string& wavPath, uint32_t totalTicks);
bool writeReplayExportWav(const std::string& path, const int16_t* samples,
                          size_t sampleCount, uint32_t sampleRate,
                          uint16_t channels = 2);
void pollReplayFfmpegEncode();
void cancelReplayFfmpegEncode();

bool startMfReplayExport(MfMp4Writer*& writer, const std::string& outputPath,
                         int width, int height, int bitrate, std::string& error);
bool mfReplayInitReady(MfMp4Writer* writer);
bool mfReplayInitSucceeded(MfMp4Writer* writer);
std::string mfReplayEncoderMode(MfMp4Writer* writer);
bool writeMfReplayVideoFrame(MfMp4Writer* writer, const uint8_t* rgbBottomUp,
                             int sourceWidth, int sourceHeight, uint32_t frameIndex,
                             bool* accepted, std::string& error);
void finishMfReplayExport(MfMp4Writer*& writer, const std::string& wavPath,
                          const std::string& outroPath);
bool pollMfReplayExport(MfMp4Writer*& writer, bool& ok, bool& outroMissing,
                        std::string& error);
void cancelMfReplayExport(MfMp4Writer*& writer);
void exportMfDiag();

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
