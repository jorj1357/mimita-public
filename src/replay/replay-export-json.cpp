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
#include "replay/replay-editor.h"
#include "video/outro.h"
#include <nlohmann/json.hpp>
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "render/post-fx.h"
#include "audio/audio-codec.h"
#include "replay/replay-export.h"
#include "debug/structured-log.h"
#include "gui/hud/player-nameplates.h"

void encodeReplayToMp4();

extern ReplayExportJob gJob;

// ---- Replay Export Config (hot-reload) ----
ReplayExportConfig gExportConfig;
static uint64_t gExportConfigLastWrite = 0;

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

        ReplayExportConfig loaded;
        if (j.contains("audioVolumeMultiplier"))
            loaded.audioVolumeMultiplier = j["audioVolumeMultiplier"].get<float>();
        if (j.contains("exportWidth"))
            loaded.exportWidth = j["exportWidth"].get<int>();
        if (j.contains("exportHeight"))
            loaded.exportHeight = j["exportHeight"].get<int>();
        if (j.contains("exportBitrate"))
            loaded.exportBitrate = j["exportBitrate"].get<int>();
        if (j.contains("exportCrf"))
            loaded.exportCrf = j["exportCrf"].get<int>();

        gExportConfig = loaded;
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] config reloaded: volume=%.2f res=%dx%d crf=%d bitrate=%d",
                   gExportConfig.audioVolumeMultiplier,
                   gExportConfig.exportWidth, gExportConfig.exportHeight,
                   gExportConfig.exportCrf, gExportConfig.exportBitrate);
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] config reload failed: %s", e.what());
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

    if (wt != gExportConfigLastWrite)
    {
        gExportConfigLastWrite = wt;
        reloadReplayExportConfig();
    }
}

// Rename accessors to match new struct
float getReplayExportAudioVolume() { return gExportConfig.audioVolumeMultiplier; }

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

    replayExportDebugOpen();
    resetHealthbarCounters();
    gRplxImpactWorldCount = 0;
    gRplxHitBurstCount = 0;
    gRplxDebrisBlockCount = 0;
    gRplxEffectDuplicateCount = 0;

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

    // Auto-load editor with keyframes if .rple.json exists
    {
        bool editorLoaded = gReplayEditor.load(jsonPath);
        printf("[RPLX] currentReplayPath=%s\n", jsonPath.c_str());
        if (editorLoaded) {
            auto& ed = gReplayEditor;
            printf("[RPLX] editorProjectPath=%s\n", ed.editPath().c_str());
            printf("[RPLX] hasActiveReplayEditor=1\n");
            printf("[RPLX] loadedKeyframes campos=%d cammode=%d pbspeed=%d\n",
                   ed.cameraKeyframeCount(), ed.cameraModeKeyframeCount(),
                   ed.timeKeyframeCount());

            // Save full editor state before forcing freecam for export
            gJob.editorWasFreecam = ed.freecam;
            gJob.savedFreecamPos[0] = ed.freecamPos.x;
            gJob.savedFreecamPos[1] = ed.freecamPos.y;
            gJob.savedFreecamPos[2] = ed.freecamPos.z;
            gJob.savedFreecamRot[0] = ed.freecamRot.x;
            gJob.savedFreecamRot[1] = ed.freecamRot.y;
            gJob.savedFreecamRot[2] = ed.freecamRot.z;
            gJob.savedFreecamRot[3] = ed.freecamRot.w;
            gJob.savedFreecamRoll = ed.freecamRoll;
            gJob.savedFreecamFov = ed.freecamFov;
            std::strncpy(gJob.savedCameraMode,
                REPLAY_PLAYER.cameraController().modeName(),
                sizeof(gJob.savedCameraMode) - 1);
            gJob.savedCameraMode[sizeof(gJob.savedCameraMode) - 1] = '\0';

            // Force freecam mode if camera keyframes exist so export uses editor camera path
            if (ed.cameraKeyframeCount() > 0) {
                ed.freecam = true;
                REPLAY_PLAYER.cameraController().setMode("freecam");
                printf("[RPLX] exportUsesReplayEditorCamera=1\n");
            } else {
                printf("[RPLX] exportUsesReplayEditorCamera=0 reason=no_camera_keyframes\n");
            }
        } else {
            printf("[RPLX] hasActiveReplayEditor=0\n");
            printf("[RPLX] exportUsesReplayEditorCamera=0 reason=no_editor_project\n");
        }
    }

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

    // Use configured export resolution if caller passed 0
    if (renderWidth <= 0 || renderHeight <= 0) {
        renderWidth = gExportConfig.exportWidth;
        renderHeight = gExportConfig.exportHeight;
    }
    int captureW = renderWidth;
    int captureH = renderHeight;
    {
        GLint vp[4] = {};
        glGetIntegerv(GL_VIEWPORT, vp);
        if (vp[2] > 0 && vp[3] > 0) {
            captureW = std::min(vp[2], renderWidth);
            captureH = std::min(vp[3], renderHeight);
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
    printf("[RPLX] config resolution: %dx%d CRF=%d bitrate=%d\n",
           gExportConfig.exportWidth, gExportConfig.exportHeight,
           gExportConfig.exportCrf, gExportConfig.exportBitrate);
    printf("[RPLX] total ticks to render: %u\n", totalTicks);

    gJob.state = ReplayExportJob::Capturing;
    gJob.jsonPath = jsonPath;
    gJob.totalTicks = totalTicks;
    gJob.capturedTicks = 0;
    gJob.exportTick = 0.0f;
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

    {
        StructuredLogger::Entry e;
        e.category = StructuredCategory::Replay;
        e.level = StructuredLevel::Important;
        e.eventId = "REPLAY_EXPORT_STARTED";
        e.correlationId = "REPLAY_EXPORT";
        e.reason = "Replay export started";
        e.sourceFile = __FILE__;
        e.sourceLine = __LINE__;
        e.functionName = __FUNCTION__;
        e.numericKeys = {"totalTicks", "videoWidth", "videoHeight"};
        e.numericExpected = {(double)totalTicks, (double)captureW, (double)captureH};
        e.numericActual = e.numericExpected;
        StructuredLogger::instance().write(e);
    }

    return true;
}

void restoreReplayExportEditorState()
{
    if (!gReplayEditor.isLoaded())
        return;

    // Restore position/rotation regardless of whether freecam was on before
    gReplayEditor.freecamPos = glm::vec3(
        gJob.savedFreecamPos[0],
        gJob.savedFreecamPos[1],
        gJob.savedFreecamPos[2]);
    gReplayEditor.freecamRot = glm::quat(
        gJob.savedFreecamRot[3],  // w
        gJob.savedFreecamRot[0],  // x
        gJob.savedFreecamRot[1],  // y
        gJob.savedFreecamRot[2]); // z
    gReplayEditor.freecamRoll = gJob.savedFreecamRoll;
    gReplayEditor.freecamFov = gJob.savedFreecamFov;

    if (!gJob.editorWasFreecam) {
        gReplayEditor.freecam = false;
        gReplayEditor.mPrevCameraMode.clear();
        std::string savedMode(gJob.savedCameraMode);
        if (!savedMode.empty())
            REPLAY_PLAYER.cameraController().setMode(savedMode);
    }

    // Overwrite any autosaved state that may have been written during export
    gReplayEditor.saveEdit();
    gReplayEditor.saveSession();
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
    restoreReplayExportEditorState();
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
