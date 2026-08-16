// 08 16 2026, 01 35
/* purpose
* Probes and remuxes the optional replay outro through a resolved FFmpeg install.
* Uses the replay exporter encoder resolver so developer paths stay portable.
* Captures child-process output for actionable replay diagnostics.
* Does NOT bundle, download, or install FFmpeg for players.
* Does NOT own replay capture, clip commands, or Media Foundation export.
* Does NOT modify source replay JSON or gameplay state.
*/
#include "video/outro.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

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

static std::string ffprobeExe()
{
    std::filesystem::path ffmpeg = defaultFfmpegPath();
    if (ffmpeg.empty()) return {};
    return (ffmpeg.parent_path() / "ffprobe.exe").string();
}

static std::string absPath(const std::string& path)
{
    std::error_code ec;
    auto p = std::filesystem::absolute(path, ec);
    if (ec) return path;
    return p.string();
}

// Execute a Windows process with CreateProcessW and capture stdout.
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
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
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

static double probeDuration(const std::string& path)
{
    std::string absInput = absPath(path);
    std::string exePath = ffprobeExe();

    std::string args;
    {
        char buf[2048];
        std::snprintf(buf, sizeof(buf),
                      "-v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"",
                      absInput.c_str());
        args = buf;
    }

    Debug::log(Debug::Category::Replay, "[OUTRO CMD] ffprobe exe=%s\n", exePath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO CMD] ffprobe args=%s\n", args.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO CMD] ffprobe exe exists=%d\n", (int)std::filesystem::exists(exePath));

    std::string stdoutBuf;
    int exitCode = runProcessCaptureStdout(exePath, args, stdoutBuf);

    Debug::log(Debug::Category::Replay, "[OUTRO CMD] ffprobe exit=%d\n", exitCode);
    Debug::log(Debug::Category::Replay, "[OUTRO PROBE] raw output=%s\n", stdoutBuf.c_str());

    // Trim whitespace
    while (!stdoutBuf.empty() && (stdoutBuf.back() == '\n' || stdoutBuf.back() == '\r' || stdoutBuf.back() == ' '))
        stdoutBuf.pop_back();

    double dur = 0.0;
    if (!stdoutBuf.empty())
        dur = std::atof(stdoutBuf.c_str());

    Debug::log(Debug::Category::Replay, "[OUTRO PROBE] parsed duration=%.1f\n", dur);
    return dur;
}

static bool runFfmpeg(const std::string& args, int& outExitCode, std::string& outStdout)
{
    outExitCode = runProcessCaptureStdout(ffmpegExe(), args, outStdout);
    return outExitCode == 0;
}

static bool probeResolution(const std::string& path, int& outW, int& outH)
{
    char buf[2048];
    std::snprintf(buf, sizeof(buf),
                  "-v error -select_streams v:0 -show_entries stream=width,height -of csv=s=x:p=1 \"%s\"",
                  path.c_str());
    std::string args = buf;
    std::string stdoutBuf;
    int exitCode = runProcessCaptureStdout(ffprobeExe(), args, stdoutBuf);
    outW = 0; outH = 0;
    if (exitCode == 0)
    {
        size_t x = stdoutBuf.find('x');
        if (x != std::string::npos)
        {
            outW = std::atoi(stdoutBuf.substr(0, x).c_str());
            outH = std::atoi(stdoutBuf.substr(x + 1).c_str());
        }
    }
    return outW > 0 && outH > 0;
}

void appendOutroToFinishedMp4(const char* replayMp4Path)
{
    std::string replayPath = absPath(replayMp4Path);
    std::string outroPath = absPath(gOutroConfig.outroPath);
    std::error_code ec;
    bool hardFail = false;

    if (replayPath.find("-with-outro") != std::string::npos)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO] already appended, skipping\n");
        return;
    }

    bool replayExists = std::filesystem::exists(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input path=%s\n", replayPath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input exists=%d\n", (int)replayExists);
    if (!replayExists) { Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input not found\n"); return; }

    uint64_t replaySize = std::filesystem::file_size(replayPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input size=%llu\n", (unsigned long long)replaySize);
    if (replaySize == 0) { Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input empty\n"); return; }

    double replayDuration = probeDuration(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO] replay duration=%.1f\n", replayDuration);

    int replayW = 0, replayH = 0;
    probeResolution(replayPath, replayW, replayH);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] input resolution=%dx%d\n", replayW, replayH);

    bool outroExists = std::filesystem::exists(outroPath);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro path=%s\n", outroPath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro exists=%d\n", (int)outroExists);
    if (!outroExists) { Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro not found\n"); return; }

    uint64_t outroSize = std::filesystem::file_size(outroPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] outro size=%llu\n", (unsigned long long)outroSize);

    double outroDuration = probeDuration(outroPath);
    Debug::log(Debug::Category::Replay, "[OUTRO] outro duration=%.1f\n", outroDuration);

    double expectedDuration = replayDuration + outroDuration;
    Debug::log(Debug::Category::Replay, "[OUTRO] expected duration=%.1f\n", expectedDuration);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] expected duration=%.1f + %.1f = %.1f\n",
               replayDuration, outroDuration, expectedDuration);

    if (replayDuration <= 0.0 || outroDuration <= 0.0)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] FAILED duration probe (input=%.1f outro=%.1f)\n",
                   replayDuration, outroDuration);
        return;
    }

    std::string tmpDir = absPath("replays/exports/_tmp");
    std::filesystem::create_directories(tmpDir, ec);

    std::string outputPath;
    {
        size_t dot = replayPath.rfind('.');
        if (dot != std::string::npos)
            outputPath = replayPath.substr(0, dot) + "-with-outro.mp4";
        else
            outputPath = replayPath + "-with-outro.mp4";
    }

    std::string normalizedOutro = tmpDir + "\\outro_normalized.mp4";

    char stage1Args[4096];
    std::snprintf(stage1Args, sizeof(stage1Args),
        "-y -i \"%s\" -c:v libx264 -preset fast -pix_fmt yuv420p -crf 18 "
        "-c:a aac -b:a 192k "
        "-vf \"scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2\" "
        "-loglevel error \"%s\"",
        outroPath.c_str(), replayW, replayH, replayW, replayH, normalizedOutro.c_str());

    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] stage1 args=%s\n", stage1Args);

    int stage1Exit = 0;
    std::string stage1Out;
    bool stage1Ok = runFfmpeg(stage1Args, stage1Exit, stage1Out);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] stage1 exit=%d\n", stage1Exit);

    if (!stage1Ok)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] stage1 failed (normalize)\n");
        return;
    }

    double normDur = probeDuration(normalizedOutro);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] normalized duration=%.1f\n", normDur);

    std::string concatListPath = tmpDir + "\\concat_list.txt";
    {
        std::ofstream list(concatListPath);
        if (!list.is_open())
        {
            Debug::log(Debug::Category::Replay, "[OUTRO APPEND] failed to write concat list\n");
            return;
        }
        list << "file '" << replayPath << "'\n";
        list << "file '" << normalizedOutro << "'\n";
        list.close();
    }

    char stage2Args[4096];
    std::snprintf(stage2Args, sizeof(stage2Args),
        "-y -f concat -safe 0 -i \"%s\" -c copy -bsf:v h264_mp4toannexb -loglevel error \"%s\"",
        concatListPath.c_str(), outputPath.c_str());

    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] stage2 args=%s\n", stage2Args);

    int stage2Exit = 0;
    std::string stage2Out;
    bool stage2Ok = runFfmpeg(stage2Args, stage2Exit, stage2Out);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] stage2 exit=%d\n", stage2Exit);

    if (!stage2Ok)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] stage2 failed (concat)\n");
        hardFail = true;
    }

    std::filesystem::remove(normalizedOutro, ec);
    std::filesystem::remove(concatListPath, ec);

    bool outputExists = std::filesystem::exists(outputPath);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] output exists=%d\n", (int)outputExists);

    if (!outputExists)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] output file missing\n");
        hardFail = true;
    }

    uint64_t outputSize = 0;
    double outputDuration = 0.0;
    if (outputExists)
    {
        outputSize = std::filesystem::file_size(outputPath, ec);
        outputDuration = probeDuration(outputPath);
    }
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] output size=%llu\n", (unsigned long long)outputSize);
    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] output duration=%.1f\n", outputDuration);

    if (outputDuration <= replayDuration)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] FAILED duration did not increase (%.1f <= %.1f)\n",
                   outputDuration, replayDuration);
        hardFail = true;
    }

    if (outputSize <= replaySize)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] FAILED size did not increase (%llu <= %llu)\n",
                   (unsigned long long)outputSize, (unsigned long long)replaySize);
        hardFail = true;
    }

    if (outputDuration < expectedDuration - 0.5)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] FAILED duration too short (%.1f < %.1f - 0.5)\n",
                   outputDuration, expectedDuration);
        hardFail = true;
    }

    if (hardFail)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] HARD FAIL\n");
        return;
    }

    Debug::log(Debug::Category::Replay, "[OUTRO APPEND] replacing original\n");
    std::filesystem::rename(outputPath, replayPath, ec);
    if (ec)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO APPEND] rename failed: %s\n", ec.message().c_str());
        return;
    }

    double finalDuration = probeDuration(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO] final duration=%.1f\n", finalDuration);
    if (std::fabs(finalDuration - expectedDuration) < 0.5)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO] PASS\n");
    }
    else
    {
        Debug::log(Debug::Category::Replay, "[OUTRO] FAILED duration mismatch (expected=%.1f actual=%.1f)\n",
                   expectedDuration, finalDuration);
    }
}
