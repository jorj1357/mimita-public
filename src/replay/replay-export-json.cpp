// 08 16 2026, 01 35
/* purpose
* Loads hot-reloadable replay export settings and prepares export jobs.
* Resolves shipped (tools/ffmpeg.exe) or configured FFmpeg installations.
* Owns output naming, clipboard support, cancellation, and editor restoration.
* Does NOT capture framebuffer pixels or encode Media Foundation samples.
* Does NOT mix replay audio or append video outros.
* Does NOT register replay commands or gameplay key bindings.
*/
#include "replay/replay-export.h"

#include <algorithm>
#include <chrono>
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
#include "replay/replay-export-target.h"
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
extern void* sExportSubprocess;

// ---- Replay Export Config (hot-reload) ----
ReplayExportConfig gExportConfig;
static uint64_t gExportConfigLastWrite = 0;

static const char* REPLAY_EXPORT_CONFIG_PATH = "config/replayexport.json";

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
        if (j.contains("encoder"))
            loaded.encoder = j["encoder"].get<std::string>();
        if (loaded.encoder != "windows" && loaded.encoder != "ffmpeg")
            loaded.encoder = "windows";
        if (j.contains("encoderMode")) {
            loaded.encoderMode = j["encoderMode"].get<std::string>();
            if (loaded.encoderMode != "auto" && loaded.encoderMode != "discrete" &&
                loaded.encoderMode != "software")
                loaded.encoderMode = "auto";
        }
        if (j.contains("audioVolumeMultiplier"))
            loaded.audioVolumeMultiplier = j["audioVolumeMultiplier"].get<float>();
        const nlohmann::json& resolution = j.value("resolution", nlohmann::json::object());
        loaded.exportWidth = resolution.value("width", j.value("exportWidth", loaded.exportWidth));
        loaded.exportHeight = resolution.value("height", j.value("exportHeight", loaded.exportHeight));
        if (j.contains("exportBitrate"))
            loaded.exportBitrate = j["exportBitrate"].get<int>();
        if (j.contains("exportCrf"))
            loaded.exportCrf = j["exportCrf"].get<int>();

        const nlohmann::json& ui = j.value("ui", nlohmann::json::object());
        loaded.ui.chat = ui.value("chat", loaded.ui.chat);
        loaded.ui.chatBubbles = ui.value("chatBubbles", loaded.ui.chatBubbles);
        loaded.ui.replayInfo = ui.value("replayInfo", loaded.ui.replayInfo);
        loaded.ui.replayControls = ui.value("replayControls", loaded.ui.replayControls);
        loaded.ui.replayTimeline = ui.value("replayTimeline", loaded.ui.replayTimeline);
        loaded.ui.replayBrowser = ui.value("replayBrowser", loaded.ui.replayBrowser);
        loaded.ui.exportProgress = ui.value("exportProgress", loaded.ui.exportProgress);
        loaded.ui.deathScreen = ui.value("deathScreen", loaded.ui.deathScreen);
        loaded.ui.speedDisplay = ui.value("speedDisplay", loaded.ui.speedDisplay);
        loaded.ui.modeText = ui.value("modeText", loaded.ui.modeText);
        loaded.ui.playerList = ui.value("playerList", loaded.ui.playerList);
        loaded.ui.fps = ui.value("fps", loaded.ui.fps);
        loaded.ui.postFxDebug = ui.value("postFxDebug", loaded.ui.postFxDebug);
        loaded.ui.shadowDebug = ui.value("shadowDebug", loaded.ui.shadowDebug);
        loaded.ui.performanceOverlay = ui.value("performanceOverlay", loaded.ui.performanceOverlay);
        loaded.ui.devOverlay = ui.value("devOverlay", loaded.ui.devOverlay);
        loaded.ui.debugVisuals = ui.value("debugVisuals", loaded.ui.debugVisuals);
        loaded.ui.duelDebug = ui.value("duelDebug", loaded.ui.duelDebug);
        loaded.ui.npcDebug = ui.value("npcDebug", loaded.ui.npcDebug);

        const nlohmann::json& effects = j.value("effects", nlohmann::json::object());
        loaded.effects.worldDebris = effects.value("worldDebris", loaded.effects.worldDebris);
        loaded.effects.bulletHoles = effects.value("bulletHoles", loaded.effects.bulletHoles);
        loaded.effects.worldCracks = effects.value("worldCracks", loaded.effects.worldCracks);
        loaded.effects.muzzleFlash = effects.value("muzzleFlash", loaded.effects.muzzleFlash);
        loaded.effects.muzzleLighting = effects.value("muzzleLighting", loaded.effects.muzzleLighting);

        gExportConfig = loaded;
        Debug::log(Debug::Category::Replay, "[REPLAY EXPORT] config reloaded: encoder=%s encoderMode=%s volume=%.2f res=%dx%d crf=%d bitrate=%d",
                   gExportConfig.encoder.c_str(),
                   gExportConfig.encoderMode.c_str(),
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

bool saveReplayExportConfig()
{
    nlohmann::json j = {
        {"encoder", gExportConfig.encoder},
        {"encoderMode", gExportConfig.encoderMode},
        {"audioVolumeMultiplier", gExportConfig.audioVolumeMultiplier},
        {"resolution", {{"width", gExportConfig.exportWidth}, {"height", gExportConfig.exportHeight}}},
        {"exportCrf", gExportConfig.exportCrf},
        {"exportBitrate", gExportConfig.exportBitrate},
        {"ui", {
            {"chat", gExportConfig.ui.chat}, {"chatBubbles", gExportConfig.ui.chatBubbles},
            {"replayInfo", gExportConfig.ui.replayInfo}, {"replayControls", gExportConfig.ui.replayControls},
            {"replayTimeline", gExportConfig.ui.replayTimeline}, {"replayBrowser", gExportConfig.ui.replayBrowser},
            {"exportProgress", gExportConfig.ui.exportProgress}, {"deathScreen", gExportConfig.ui.deathScreen},
            {"speedDisplay", gExportConfig.ui.speedDisplay}, {"modeText", gExportConfig.ui.modeText},
            {"playerList", gExportConfig.ui.playerList}, {"fps", gExportConfig.ui.fps},
            {"postFxDebug", gExportConfig.ui.postFxDebug}, {"shadowDebug", gExportConfig.ui.shadowDebug},
            {"performanceOverlay", gExportConfig.ui.performanceOverlay}, {"devOverlay", gExportConfig.ui.devOverlay},
            {"debugVisuals", gExportConfig.ui.debugVisuals}, {"duelDebug", gExportConfig.ui.duelDebug}, {"npcDebug", gExportConfig.ui.npcDebug}}},
        {"effects", {{"worldDebris", gExportConfig.effects.worldDebris}, {"bulletHoles", gExportConfig.effects.bulletHoles},
            {"worldCracks", gExportConfig.effects.worldCracks}, {"muzzleFlash", gExportConfig.effects.muzzleFlash},
            {"muzzleLighting", gExportConfig.effects.muzzleLighting}}}
    };
    std::ofstream file(REPLAY_EXPORT_CONFIG_PATH);
    if (!file.is_open()) return false;
    file << j.dump(2) << '\n';
    return (bool)file;
}

std::string makeCmdKArgs(const std::string& cmd)
{
    return "/k \"" + cmd + "\"";
}

std::string defaultFfmpegPath()
{
    if (const char* env = std::getenv("MIMITA_FFMPEG")) {
        if (*env && std::filesystem::exists(env))
            return std::filesystem::absolute(env).make_preferred().string();
    }
    {
        std::ifstream file("config/replay/ffmpeg-path.json");
        nlohmann::json j;
        try {
            if (file.is_open() && (file >> j) && j.contains("path")) {
                std::string configured = j["path"].get<std::string>();
                if (std::filesystem::exists(configured))
                    return std::filesystem::absolute(configured).make_preferred().string();
            }
        } catch (...) {}
    }
#ifdef _WIN32
    // Shipped location: tools/ffmpeg.exe beside the game executable (release
    // layout) or beside the working directory (source checkout).
    char module[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, module, MAX_PATH) > 0) {
        std::filesystem::path exeDir = std::filesystem::path(module).parent_path();
        std::filesystem::path beside = exeDir / "tools" / "ffmpeg.exe";
        if (std::filesystem::exists(beside))
            return std::filesystem::absolute(beside).make_preferred().string();
        std::filesystem::path besideLegacy = exeDir / "ffmpeg" / "ffmpeg.exe";
        if (std::filesystem::exists(besideLegacy))
            return std::filesystem::absolute(besideLegacy).make_preferred().string();
    }
    {
        std::filesystem::path cwdTools = std::filesystem::path("tools") / "ffmpeg.exe";
        if (std::filesystem::exists(cwdTools))
            return std::filesystem::absolute(cwdTools).make_preferred().string();
    }
    char found[MAX_PATH] = {};
    if (SearchPathA(nullptr, "ffmpeg.exe", nullptr, MAX_PATH, found, nullptr) > 0)
        return found;
#endif
    return {};
}

void copyTextToClipboard(const std::string& text)
{
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (memory) {
        void* target = GlobalLock(memory);
        if (target) {
            std::memcpy(target, text.c_str(), text.size() + 1);
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_TEXT, memory)) GlobalFree(memory);
        } else GlobalFree(memory);
    }
    CloseClipboard();
#else
    (void)text;
#endif
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

bool startReplayExport(const std::string& jsonPath, int renderWidth, int renderHeight,
                       bool restoreLiveOnFinish)
{
    if (gJob.state == ReplayExportJob::Capturing || gJob.state == ReplayExportJob::Encoding)
    {
        return false;
    }

    if (!std::filesystem::exists(jsonPath))
    {
        gJob.state = ReplayExportJob::Failed;
        gJob.errorMsg = "Replay file not found:\n" + jsonPath;
        return false;
    }

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-PRESS] startReplayExport: clip=%s %dx%d\n",
        jsonPath.c_str(), renderWidth, renderHeight);
    return spawnExportSubprocess(jsonPath, renderWidth, renderHeight);
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
    replayExportTargetDestroy();
    // Kill the subprocess if it's still running
    if (sExportSubprocess) {
        TerminateProcess((HANDLE)sExportSubprocess, 1);
        CloseHandle((HANDLE)sExportSubprocess);
        sExportSubprocess = nullptr;
    }
    if (gJob.state == ReplayExportJob::Capturing && gJob.rawFile)
    {
        fclose(gJob.rawFile);
        gJob.rawFile = nullptr;
    }
    if (!gJob.rawTempPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(gJob.rawTempPath, ec);
    }
    cancelMfReplayExport(gJob.mfWriter);
    cancelReplayFfmpegEncode();
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

// ── Subprocess export ─────────────────────────────────────────────────
// Spawns a separate mimita.exe process to run the replay export in the
// background. The main game keeps playing; the subprocess handles
// rendering + encoding + outro independently.

bool spawnExportSubprocess(const std::string& clipPath, int width, int height)
{
    if (sExportSubprocess) {
        Debug::log(Debug::Category::Replay,
            "[REPLAY] spawnExportSubprocess: already running\n");
        return false;
    }

    if (!std::filesystem::exists(clipPath)) {
        Debug::log(Debug::Category::Replay,
            "[REPLAY] spawnExportSubprocess: clip not found: %s\n", clipPath.c_str());
        return false;
    }

    // Resolve the exe path
    char exePathBuf[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH)) {
        Debug::log(Debug::Category::Replay,
            "[REPLAY] spawnExportSubprocess: GetModuleFileName failed\n");
        return false;
    }

    // Generate output path
    std::string outputPath = generateExportOutputPath();

    // Build command line
    std::string cmd = std::string(exePathBuf) +
        " --export-replay \"" + clipPath +
        "\" --output \"" + outputPath +
        "\" --width " + std::to_string(width) +
        " --height " + std::to_string(height);
    if (gReplayExportVerbose)
        cmd += " --replay-export-verbose";

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SPAWN-REQUEST] clip=%s output=%s %dx%d\n",
        clipPath.c_str(), outputPath.c_str(), width, height);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                        nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi))
    {
        DWORD err = GetLastError();
        Debug::error(Debug::Category::Replay,
            "[EXPORT-SPAWN-FAILED] CreateProcess error=%lu\n",
            (unsigned long)err);
        return false;
    }

    sExportSubprocess = pi.hProcess;
    gJob.outputPath = outputPath;
    gJob.startTimeSec = replayExportNowSec();

    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SPAWN-SUCCESS] pid=%lu exe=%s\n",
        pi.dwProcessId, exePathBuf);
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SPAWN-SUCCESS] cmdline=%s\n", cmd.c_str());
    Debug::warn(Debug::Category::Replay,
        "[EXPORT-SUBPROCESS-LOG] child log will appear in logs/ as ReplayExport_log_*.txt\n");

    CloseHandle(pi.hThread); // We only need the process handle
    return true;
}
