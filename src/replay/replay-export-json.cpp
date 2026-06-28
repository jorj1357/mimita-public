#include "replay/replay-export.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "replay/replay.h"
#include "video/outro.h"
#include <nlohmann/json.hpp>
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "render/post-fx.h"
#include "audio/audio-codec.h"

void encodeReplayToMp4();

extern ReplayExportJob gJob;

// ---- Replay Export Audio Config (hot-reload) ----
struct ReplayExportAudioConfig {
    float audioVolumeMultiplier = 0.8f;
};

ReplayExportAudioConfig gAudioConfig;
static uint64_t gAudioConfigLastWrite = 0;

static const char* REPLAY_EXPORT_CONFIG_PATH = "config/replay/replay-export.json";

static uint64_t cfgFileWriteTime(const char* path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return ft.time_since_epoch().count();
}

static void reloadReplayExportConfig()
{
    std::ifstream file(REPLAY_EXPORT_CONFIG_PATH);
    if (!file.is_open())
        return;

    try
    {
        nlohmann::json j;
        file >> j;

        ReplayExportAudioConfig loaded;
        if (j.contains("audioVolumeMultiplier"))
            loaded.audioVolumeMultiplier = j["audioVolumeMultiplier"].get<float>();

        gAudioConfig = loaded;
        Debug::log(Debug::Category::Replay, "[REPLAY AUDIO] config reloaded audioVolumeMultiplier=%.2f", gAudioConfig.audioVolumeMultiplier);
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY AUDIO] config reload failed: %s", e.what());
    }
}

void pollReplayExportConfig()
{
    static double elapsed = 0.0;
    elapsed += 1.0 / 60.0;
    if (elapsed < 0.25)
        return;
    elapsed = 0.0;

    uint64_t wt = cfgFileWriteTime(REPLAY_EXPORT_CONFIG_PATH);
    if (wt == 0)
        return;

    if (wt != gAudioConfigLastWrite)
    {
        gAudioConfigLastWrite = wt;
        reloadReplayExportConfig();
    }
}

std::string makeCmdKArgs(const std::string& cmd)
{
    return "/k \"" + cmd + "\"";
}

std::string defaultFfmpegPath()
{
    return "C:\\important\\ffmpeg-2025-11-17-git-e94439e49b-full_build\\bin\\ffmpeg.exe";
}

static std::string sanitizeFilenameWindows(const std::string& name)
{
    const std::string invalidChars = "<>:\"/\\|?*\n\r\t";
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (invalidChars.find(c) == std::string::npos && (unsigned char)c >= 32)
            result += c;
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
        result.pop_back();
    if (result.empty())
        result = "replay";
    return result;
}

std::string generateExportOutputPath()
{
    namespace fs = std::filesystem;
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    char dateDir[32];
    std::strftime(dateDir, sizeof(dateDir), "%m-%d-%Y", &localTime);

    char timeFile[64];
    std::strftime(timeFile, sizeof(timeFile), "%H-%M-%S-clip-duel.mp4", &localTime);

    const fs::path exportDir = fs::path("replays") / "exports" / dateDir;
    std::error_code ec;
    fs::create_directories(exportDir, ec);
    if (ec) {
    }

    std::string baseFile = timeFile;
    fs::path path = exportDir / baseFile;
    int attempt = 1;
    while (fs::exists(path, ec)) {
        std::string stem = baseFile;
        size_t dot = stem.rfind('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        std::string numbered = stem + "_" + std::to_string(attempt) + ".mp4";
        path = exportDir / numbered;
        attempt++;
    }
    std::string result = path.string();
    return result;
}

bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight)
{
    if (gJob.state == ReplayExportJob::Capturing || gJob.state == ReplayExportJob::Encoding)
    {
        return false;
    }
    gJob = ReplayExportJob{};

    if (!std::filesystem::exists(jsonPath))
    {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay file not found:\n" + jsonPath;
        return false;
    }

    ReplayClip clip;
    if (!clip.load(jsonPath))
    {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay clip:\n" + jsonPath;
        return false;
    }

    uint32_t totalTicks = clip.header.tickCount;
    if (totalTicks == 0)
    {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay has no frames.";
        return false;
    }

    printf("[RPLX] loading replay JSON...\n");
    ReplayClip loadCheck;
    bool parseOk = loadCheck.load(jsonPath);
    printf("[RPLX] json parse: %s\n", parseOk ? "success" : "FAIL");
    printf("[RPLX] replay tick count: %u\n", loadCheck.header.tickCount);
    if (!loadCheck.frames.empty()) {
        printf("[RPLX] first tick: %u\n", loadCheck.frames.front().tick);
        printf("[RPLX] last tick: %u\n", loadCheck.frames.back().tick);
    }
    printf("[RPLX] sound event count: %zu\n", loadCheck.soundEvents.size());
    if (!loadCheck.sceneFrames.empty()) {
        size_t maxActors = 0;
        for (auto& sf : loadCheck.sceneFrames)
            if (sf.actors.size() > maxActors) maxActors = sf.actors.size();
        printf("[RPLX] player/npc count (max in any frame): %zu\n", maxActors);
    }

    if (!REPLAY_PLAYER.loadFromJSON(jsonPath)) {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Failed to load replay into player:\n" + jsonPath;
        printf("[RPLX ERROR] loadFromJSON failed for: %s\n", jsonPath.c_str());
        return false;
    }
    REPLAY_PLAYER.beginPlayback();
    REPLAY_PLAYER.seekToTick(0);

    std::string ffmpeg = defaultFfmpegPath();
    printf("[RPLX] ffmpeg path: %s\n", ffmpeg.c_str());
    printf("[RPLX] ffmpeg exists: %s\n", std::filesystem::exists(ffmpeg) ? "yes" : "no");
    if (!std::filesystem::exists(ffmpeg))
    {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "FFmpeg not found:\n" + ffmpeg;
        return false;
    }

    std::string outputPath = generateExportOutputPath();

    std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
    if (!std::filesystem::exists(outDir))
    {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec) {
            gJob.state = ReplayExportJob::Failed;
            gJob.errorMsg = "Cannot create output directory:\n" + outDir.string();
            return false;
        }
    }

    // ROOT CAUSE FIX: Query actual framebuffer dimensions for capture.
    int captureW = renderWidth;
    int captureH = renderHeight;
    {
        GLint vp[4] = {};
        glGetIntegerv(GL_VIEWPORT, vp);
        if (vp[2] > 0 && vp[3] > 0) {
            captureW = vp[2];
            captureH = vp[3];
        }
    }

    namespace fs = std::filesystem;
    std::string rawTempDir = (fs::path("replays") / "exports" / "_tmp").string();
    {
        std::error_code ec;
        fs::create_directories(rawTempDir, ec);
    }
    std::string rawTempPath = (fs::path("replays") / "exports" / "_tmp" / "export_raw.rgb").string();

    FILE* rawFile = fopen(rawTempPath.c_str(), "wb");
    if (!rawFile) {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Cannot create temp raw file:\n" + rawTempPath;
        return false;
    }

    printf("[RPLX] output path: %s\n", outputPath.c_str());
    printf("[RPLX] capture resolution: %dx%d\n", captureW, captureH);
    printf("[RPLX] total ticks to render: %u\n", totalTicks);

    gJob.state = ReplayExportJob::Capturing;
    gJob.jsonPath = jsonPath;
    gJob.totalTicks = totalTicks;
    gJob.capturedTicks = 0;
    gJob.capWidth = captureW;
    gJob.capHeight = captureH;
    gJob.outputWidth = renderWidth;
    gJob.outputHeight = renderHeight;
    gJob.ffmpegPath = ffmpeg;
    gJob.rawTempPath = rawTempPath;
    gJob.rawFile = rawFile;
    gJob.outputPath = outputPath;
    gJob.ffmpegExitCode = -1;
    gJob.errorMsg.clear();
    gJob.frameWriteCount = 0;
    gJob.rawFileBytes = 0;
    gJob.mp4FileBytes = 0;
    return true;
}

float getReplayExportAudioVolume()
{
    return gAudioConfig.audioVolumeMultiplier;
}

void cancelReplayExport()
{
    if (gJob.state == ReplayExportJob::Capturing && gJob.rawFile)
    {
        fclose(gJob.rawFile);
        gJob.rawFile = nullptr;
    }
    if (!gJob.rawTempPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(gJob.rawTempPath, ec);
    }
    gJob = ReplayExportJob{};
}

void openReplayFolder()
{
    std::string path = "replays\\exports";
    std::string cmd = "explorer.exe \"" + path + "\"";
    std::thread([cmd]() {
        std::system(cmd.c_str());
    }).detach();
    Debug::log(Debug::Category::Replay, "[REPLAY] Opened replays folder");
}
