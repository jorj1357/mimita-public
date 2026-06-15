#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

struct ReplayExportJob {
    enum State { Idle, Capturing, Encoding, Done, Failed };
    State state = Idle;

    // Capture state
    std::string jsonPath;
    uint32_t totalTicks = 0;
    uint32_t capturedTicks = 0;
    int capWidth = 0;
    int capHeight = 0;

    // Encoding
    std::string ffmpegPath;
    FILE* ffmpegPipe = nullptr;

    // Output
    std::string outputPath;

    // Result
    std::string errorMsg;
    int ffmpegExitCode = -1;
    uint32_t frameWriteCount = 0;
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
