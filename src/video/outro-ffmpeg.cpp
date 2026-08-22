// 08 16 2026, 01 35
/* purpose
* Appends the optional replay outro with a single ffmpeg command (filter_complex
* concat). The replay and outro are re-encoded together in one pass; no ffprobe.
* Uses the replay exporter encoder resolver so shipped paths stay portable.
* Captures child-process output for actionable replay diagnostics.
* Does NOT bundle, download, or install FFmpeg for players.
* Does NOT own replay capture, clip commands, or Media Foundation export.
* Does NOT modify source replay JSON or gameplay state.
*/
#include "video/outro.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "video/outro.h"
#include "debug/debug-log.h"
#include "replay/replay-export.h"

extern OutroConfig gOutroConfig;

static std::string ffmpegExe()
{
    return defaultFfmpegPath();
}

static std::string absPath(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::absolute(path, ec);
    if (ec) return path;
    return p.string();
}

// Execute a Windows process with CreateProcessW and capture stdout+stderr.
// Returns exit code. Sets stdoutBuf to captured output.
// Does NOT use cmd.exe — no shell interpretation.
static int runProcessCaptureStdout(const std::string& exePath, const std::string& args, std::string& stdoutBuf)
{
    stdoutBuf.clear();

    // Build command line for CreateProcessW: executable + args
    std::wstring cmdLine = L"\"" + std::wstring(exePath.begin(), exePath.end()) + L"\" " + std::wstring(args.begin(), args.end());

    // Create pipes for stdout
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 65536))
    {
        Debug::log(Debug::Category::Replay, "[OUTRO CMD] CreatePipe failed\n");
        return -1;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFOW si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe; // merge stderr (ffmpeg info/progress) into the capture
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    // Mutable copy for CreateProcessW
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    BOOL created = CreateProcessW(
        NULL,
        mutableCmd.data(),
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(hWritePipe);

    if (!created)
    {
        DWORD err = GetLastError();
        Debug::log(Debug::Category::Replay, "[OUTRO CMD] CreateProcessW failed (error=%lu)\n", (unsigned long)err);
        CloseHandle(hReadPipe);
        return -1;
    }

    // Read stdout
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        buf[bytesRead] = '\0';
        stdoutBuf += buf;
    }

    CloseHandle(hReadPipe);

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}

static bool runFfmpeg(const std::string& args, int& outExitCode, std::string& outStdout)
{
    outExitCode = runProcessCaptureStdout(ffmpegExe(), args, outStdout);
    return outExitCode == 0;
}

// ffprobe-free video size probe: run `ffmpeg -i <path>` and parse the first
// `Stream #0:0: Video: ..., WxH` dimension from its output (stderr merged in).
bool probeVideoSizeViaFfmpeg(const std::string& path, int& outW, int& outH)
{
    std::string exe = ffmpegExe();
    if (exe.empty()) return false;
    std::string args = "-hide_banner -i \"" + absPath(path) + "\"";
    std::string buf;
    int code = runProcessCaptureStdout(exe, args, buf);
    (void)code; // ffmpeg -i with no output exits non-zero; info is in the buffer

    outW = 0; outH = 0;
    size_t videoPos = buf.find("Video:");
    if (videoPos == std::string::npos)
        return false;
    for (size_t i = videoPos; i < buf.size(); ++i) {
        if (!std::isdigit((unsigned char)buf[i]))
            continue;
        size_t start = i;
        while (i < buf.size() && std::isdigit((unsigned char)buf[i])) ++i;
        if (i >= buf.size() || buf[i] != 'x' || i + 1 >= buf.size() ||
            !std::isdigit((unsigned char)buf[i + 1]))
            continue;
        size_t hend = i + 1;
        while (hend < buf.size() && std::isdigit((unsigned char)buf[hend])) ++hend;
        int w = std::atoi(buf.substr(start, i - start).c_str());
        int h = std::atoi(buf.substr(i + 1, hend - (i + 1)).c_str());
        if (w > 0 && h > 0) { outW = w; outH = h; return true; }
    }
    return false;
}

bool appendOutroToFinishedMp4(const char* replayMp4Path, int replayW, int replayH, bool hasAudio)
{
    std::string replayPath = absPath(replayMp4Path);
    std::string outroPath = absPath(gOutroConfig.outroPath);
    std::error_code ec;

    if (replayPath.find("-with-outro") != std::string::npos)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO] already appended, skipping\n");
        return true;
    }

    bool replayExists = std::filesystem::exists(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input path=%s\n", replayPath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input exists=%d\n", (int)replayExists);
    if (!replayExists) { Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input not found\n"); return false; }

    uint64_t replaySize = std::filesystem::file_size(replayPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input size=%llu\n", (unsigned long long)replaySize);
    if (replaySize == 0) { Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input empty\n"); return false; }

    if (replayW <= 0 || replayH <= 0)
    {
        // Manual/dev commands have no export job: probe the replay resolution
        // via `ffmpeg -i` (no ffprobe shipped).
        if (!probeVideoSizeViaFfmpeg(replayPath, replayW, replayH))
        {
            Debug::log(Debug::Category::Replay, "[OUTRO APPEND] cannot determine replay resolution\n");
            return false;
        }
    }
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input resolution=%dx%d audio=%d\n",
               replayW, replayH, (int)hasAudio);

    bool outroExists = std::filesystem::exists(outroPath);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro path=%s\n", outroPath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro exists=%d\n", (int)outroExists);
    if (!outroExists) { Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro not found\n"); return false; }

    std::string outputPath;
    {
        size_t dot = replayPath.rfind('.');
        if (dot != std::string::npos)
            outputPath = replayPath.substr(0, dot) + "-with-outro.mp4";
        else
            outputPath = replayPath + "-with-outro.mp4";
    }

    // Single-pass append: normalize the outro to the replay's geometry/format,
    // then concat both in one ffmpeg run. The replay is re-encoded at CRF 18
    // (visually identical); no ffprobe, no concat demuxer, no bitstream filters.
    std::string scalePad =
        "scale=" + std::to_string(replayW) + ":" + std::to_string(replayH) +
        ":force_original_aspect_ratio=decrease,pad=" + std::to_string(replayW) + ":" +
        std::to_string(replayH) + ":(ow-iw)/2:(oh-ih)/2";
    std::string filter;
    std::string mapArgs;
    if (hasAudio)
    {
        filter =
            "[0:v]setpts=PTS-STARTPTS,fps=60,format=yuv420p[v0];"
            "[0:a]asetpts=PTS-STARTPTS,aformat=sample_rates=48000:channel_layouts=stereo[a0];"
            "[1:v]" + scalePad + ",setsar=1,fps=60,format=yuv420p,setpts=PTS-STARTPTS[v1];"
            "[1:a]aresample=48000,aformat=sample_rates=48000:channel_layouts=stereo,asetpts=PTS-STARTPTS[a1];"
            "[v0][a0][v1][a1]concat=n=2:v=1:a=1[v][a]";
        mapArgs = "-map \"[v]\" -map \"[a]\"";
    }
    else
    {
        filter =
            "[0:v]setpts=PTS-STARTPTS,fps=60,format=yuv420p[v0];"
            "[1:v]" + scalePad + ",setsar=1,fps=60,format=yuv420p,setpts=PTS-STARTPTS[v1];"
            "[v0][v1]concat=n=2:v=1:a=0[v]";
        mapArgs = "-map \"[v]\" -an";
    }

    char args[8192];
    std::snprintf(args, sizeof(args),
        "-y -i \"%s\" -i \"%s\" -filter_complex \"%s\" %s "
        "-c:v libx264 -preset fast -pix_fmt yuv420p -crf 18 -c:a aac -b:a 192k "
        "-loglevel error \"%s\"",
        replayPath.c_str(), outroPath.c_str(), filter.c_str(), mapArgs.c_str(),
        outputPath.c_str());

    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] single-cmd args=%s\n", args);

    int exitCode = 0;
    std::string cmdOut;
    bool ok = runFfmpeg(args, exitCode, cmdOut);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] single-cmd exit=%d\n", exitCode);

    if (!ok || !std::filesystem::exists(outputPath, ec))
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] FAILED (exit=%d output_exists=%d)\n",
                   exitCode, (int)std::filesystem::exists(outputPath, ec));
        return false;
    }

    uint64_t outputSize = std::filesystem::file_size(outputPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] output size=%llu (input=%llu)\n",
               (unsigned long long)outputSize, (unsigned long long)replaySize);
    if (outputSize <= replaySize)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] FAILED size did not increase (%llu <= %llu)\n",
                   (unsigned long long)outputSize, (unsigned long long)replaySize);
        return false;
    }

    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] replacing original\n");
    std::filesystem::remove(replayPath, ec);
    if (ec) {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] could not remove intermediate: %s\n", ec.message().c_str());
        return false;
    }
    std::filesystem::rename(outputPath, replayPath, ec);
    if (ec)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] rename failed: %s\n", ec.message().c_str());
        return false;
    }

    Debug::log(Debug::Category::Replay, "[OUTRO] PASS (outro appended, final=%s)\n", replayPath.c_str());
    return true;
}
