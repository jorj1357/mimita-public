#include "video/outro.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "nlohmann/json.hpp"
#include "debug/debug-log.h"
#include "devtools/terminal.h"

static const char* CONFIG_PATH = "config/video/outro.json";

struct OutroConfig {
    bool enabled = true;
    std::string outroPath = "assets/video/mimitaoutrov1.webm";
};

static OutroConfig gConfig;
static uint64_t gLastWriteTime = 0;

// Default ffmpeg path matching replay-export.cpp
static std::string ffmpegPath()
{
    return "C:\\important\\ffmpeg-2025-11-17-git-e94439e49b-full_build\\bin\\ffmpeg.exe";
}

static uint64_t fileWriteTime(const char* path)
{
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    return ft.time_since_epoch().count();
}

static void reloadConfig()
{
    std::ifstream file(CONFIG_PATH);
    if (!file.is_open())
        return;

    try
    {
        nlohmann::json j;
        file >> j;

        OutroConfig loaded;
        if (j.contains("enabled"))
            loaded.enabled = j["enabled"].get<bool>();
        if (j.contains("outroPath"))
            loaded.outroPath = j["outroPath"].get<std::string>();

        gConfig = loaded;
        Debug::log(Debug::Category::Replay,
                   "[OUTRO] config reloaded: enabled=%d path=%s\n",
                   (int)gConfig.enabled, gConfig.outroPath.c_str());
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Replay,
                   "[OUTRO] config reload failed: %s\n", e.what());
    }
}

static void saveConfig()
{
    nlohmann::json j;
    j["enabled"] = gConfig.enabled;
    j["outroPath"] = gConfig.outroPath;

    std::ofstream file(CONFIG_PATH);
    if (file.is_open())
        file << j.dump(4) << std::endl;
}

void pollOutroConfig()
{
    static double elapsed = 0.0;
    elapsed += 1.0 / 60.0;
    if (elapsed < 0.25)
        return;
    elapsed = 0.0;

    uint64_t wt = fileWriteTime(CONFIG_PATH);
    if (wt == 0)
        return;

    if (wt != gLastWriteTime)
    {
        gLastWriteTime = wt;
        reloadConfig();
    }
}

static double probeDuration(const std::string& path)
{
    char cmd[1024];
    char tmpPath[512];
    std::snprintf(tmpPath, sizeof(tmpPath), "replays/exports/_tmp/_dur_%lld.txt",
                  (long long)std::time(nullptr));
    std::snprintf(cmd, sizeof(cmd),
                  "\"%s\" -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\" > \"%s\" 2>nul",
                  ffmpegPath().c_str(), path.c_str(), tmpPath);
    std::system(cmd);
    double dur = 0.0;
    std::ifstream f(tmpPath);
    if (f.is_open()) { f >> dur; f.close(); }
    std::filesystem::remove(tmpPath);
    return dur;
}

void appendOutroToFinishedMp4(const char* replayMp4Path)
{
    std::string replayPath(replayMp4Path);
    std::string outroPath = gConfig.outroPath;
    std::error_code ec;
    bool hardFail = false;

    // ---- INPUT ----
    bool replayExists = std::filesystem::exists(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] input path=%s\n", replayPath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] input exists=%d\n", (int)replayExists);
    if (!replayExists) { Debug::log(Debug::Category::Replay, "[OUTRO TEST] input not found\n"); return; }

    uint64_t replaySize = std::filesystem::file_size(replayPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] input size=%llu\n", (unsigned long long)replaySize);
    if (replaySize == 0) { Debug::log(Debug::Category::Replay, "[OUTRO TEST] input empty\n"); return; }

    double replayDuration = probeDuration(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] input duration=%.1f\n", replayDuration);

    // ---- OUTRO ----
    bool outroExists = std::filesystem::exists(outroPath);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] outro path=%s\n", outroPath.c_str());
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] outro exists=%d\n", (int)outroExists);
    if (!outroExists) { Debug::log(Debug::Category::Replay, "[OUTRO TEST] outro not found, skipping\n"); return; }

    uint64_t outroSize = std::filesystem::file_size(outroPath, ec);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] outro size=%llu\n", (unsigned long long)outroSize);

    double outroDuration = probeDuration(outroPath);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] outro duration=%.1f\n", outroDuration);

    // ---- BUILD OUTPUT PATH ----
    std::string outputPath;
    {
        size_t dot = replayPath.rfind('.');
        if (dot != std::string::npos)
            outputPath = replayPath.substr(0, dot) + "-with-outro.mp4";
        else
            outputPath = replayPath + "-with-outro.mp4";
    }

    // ---- FFMPEG CONCAT ----
    std::string batDir = "replays/exports/_tmp/";
    std::filesystem::create_directories(batDir, ec);

    std::string batPath = batDir + "outro_append.bat";
    std::string stderrPath = "replays/exports/outro_ffmpeg_stderr.txt";

    char ffmpegCmd[4096];
    std::snprintf(ffmpegCmd, sizeof(ffmpegCmd),
        "@echo off\n"
        "\"%s\" -y -i \"%s\" -i \"%s\" -filter_complex "
        "\"[0:v][0:a][1:v][1:a]concat=n=2:v=1:a=1[outv][outa]\" "
        "-map \"[outv]\" -map \"[outa]\" -c:v libx264 -preset fast -pix_fmt yuv420p -crf 18 "
        "-c:a aac -b:a 192k \"%s\" 2>\"%s\"\n"
        "exit /b %%ERRORLEVEL%%\n",
        ffmpegPath().c_str(),
        replayPath.c_str(), outroPath.c_str(),
        outputPath.c_str(),
        stderrPath.c_str());

    {
        std::ofstream bat(batPath);
        if (!bat.is_open()) { Debug::log(Debug::Category::Replay, "[OUTRO TEST] failed to write batch\n"); return; }
        bat << ffmpegCmd;
        bat.close();
    }

    int result = std::system(batPath.c_str());
    std::filesystem::remove(batPath, ec);

    Debug::log(Debug::Category::Replay, "[OUTRO TEST] ffmpeg exit code=%d\n", result);

    // Dump stderr on failure
    if (result != 0)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] ffmpeg stderr dumped to %s\n", stderrPath.c_str());
        std::ifstream stderrFile(stderrPath);
        if (stderrFile.is_open())
        {
            std::string line;
            int lineCount = 0;
            while (std::getline(stderrFile, line) && lineCount < 20)
            {
                Debug::log(Debug::Category::Replay, "[OUTRO TEST] stderr: %s\n", line.c_str());
                ++lineCount;
            }
            stderrFile.close();
        }
        hardFail = true;
    }

    // ---- VERIFY OUTPUT ----
    bool outputExists = std::filesystem::exists(outputPath);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] output exists=%d\n", (int)outputExists);

    if (!outputExists)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] output file missing\n");
        hardFail = true;
    }

    uint64_t outputSize = 0;
    double outputDuration = 0.0;
    if (outputExists)
    {
        outputSize = std::filesystem::file_size(outputPath, ec);
        outputDuration = probeDuration(outputPath);
    }
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] output size=%llu\n", (unsigned long long)outputSize);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] output duration=%.1f\n", outputDuration);

    // ---- VALIDATION ----
    if (outputDuration <= replayDuration)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] FAILED duration did not increase (%.1f <= %.1f)\n",
                   outputDuration, replayDuration);
        hardFail = true;
    }

    if (outputSize <= replaySize)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] FAILED size did not increase (%llu <= %llu)\n",
                   (unsigned long long)outputSize, (unsigned long long)replaySize);
        hardFail = true;
    }

    double expectedDuration = replayDuration + outroDuration;
    if (outputDuration < expectedDuration - 0.5)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] FAILED duration too short (%.1f < %.1f - 0.5)\n",
                   outputDuration, expectedDuration);
        hardFail = true;
    }

    if (hardFail)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] HARD FAIL\n");
        std::filesystem::remove(outputPath, ec);
        return;
    }

    // ---- REPLACE ORIGINAL ----
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] replacing original\n");
    std::filesystem::rename(outputPath, replayPath, ec);
    if (ec)
    {
        Debug::log(Debug::Category::Replay, "[OUTRO TEST] rename failed: %s\n", ec.message().c_str());
        return;
    }

    double finalDuration = probeDuration(replayPath);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] final duration=%.1f\n", finalDuration);
    Debug::log(Debug::Category::Replay, "[OUTRO TEST] PASS\n");
}

void registerOutroCommands()
{
    Terminal::instance().registerCommand({
        "outro", "Enable or disable outro appending (1=on, 0=off)", "outro <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    std::string("[OUTRO] enabled=") + (gConfig.enabled ? "1" : "0"));
                return;
            }
            gConfig.enabled = args[0] != "0";
            saveConfig();
            Terminal::instance().addLog(
                std::string("[OUTRO] enabled=") + (gConfig.enabled ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "outro_status", "Print outro configuration", "outro_status",
        [](const std::vector<std::string>&) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "[OUTRO] enabled=%d path=%s",
                     (int)gConfig.enabled, gConfig.outroPath.c_str());
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "outro_test", "Append outro to an existing MP4", "outro_test [<path>]",
        [](const std::vector<std::string>& args) {
            std::string mp4Path;
            if (!args.empty())
            {
                mp4Path = args[0];
            }
            else
            {
                // Find latest exported MP4
                namespace fs = std::filesystem;
                fs::path exportDir = "replays/exports";
                if (!fs::exists(exportDir))
                {
                    Terminal::instance().addLog("[OUTRO] no exports directory found");
                    return;
                }
                std::vector<fs::path> candidates;
                for (auto& entry : fs::recursive_directory_iterator(exportDir))
                {
                    if (entry.path().extension() == ".mp4" &&
                        entry.path().filename().string().find("-with-outro") == std::string::npos)
                    {
                        candidates.push_back(entry.path());
                    }
                }
                if (candidates.empty())
                {
                    Terminal::instance().addLog("[OUTRO] no exported MP4s found in replays/exports/");
                    return;
                }
                std::sort(candidates.begin(), candidates.end(),
                    [](const fs::path& a, const fs::path& b) {
                        return fs::last_write_time(a) > fs::last_write_time(b);
                    });
                mp4Path = candidates[0].string();
            }

            Terminal::instance().addLog("[OUTRO] appending outro to: " + mp4Path);
            if (!gConfig.enabled)
            {
                Terminal::instance().addLog("[OUTRO] outro is disabled, enabling temporarily");
                gConfig.enabled = true;
            }

            // Capture durations before
            double beforeDuration = probeDuration(mp4Path);
            uint64_t beforeSize = 0;
            {
                std::error_code ec2;
                beforeSize = std::filesystem::file_size(mp4Path, ec2);
            }

            appendOutroToFinishedMp4(mp4Path.c_str());

            // Verify after
            double afterDuration = probeDuration(mp4Path);
            uint64_t afterSize = 0;
            {
                std::error_code ec2;
                afterSize = std::filesystem::file_size(mp4Path, ec2);
            }

            if (afterDuration > beforeDuration && afterSize > beforeSize)
            {
                Terminal::instance().addLog("[OUTRO] outro_test complete — file grew from " +
                    std::to_string(beforeSize) + " to " + std::to_string(afterSize) + " bytes, duration " +
                    std::to_string(beforeDuration) + "s -> " + std::to_string(afterDuration) + "s");
            }
            else
            {
                Terminal::instance().addLog("[OUTRO] outro_test FAILED — file did not grow");
            }
        }
    });

    Terminal::instance().registerCommand({
        "outro_verify", "Print latest export file info", "outro_verify",
        [](const std::vector<std::string>&) {
            namespace fs = std::filesystem;
            fs::path exportDir = "replays/exports";
            if (!fs::exists(exportDir))
            {
                Terminal::instance().addLog("[OUTRO] no exports directory");
                return;
            }
            std::vector<fs::path> candidates;
            for (auto& entry : fs::recursive_directory_iterator(exportDir))
            {
                if (entry.path().extension() == ".mp4" &&
                    entry.path().filename().string().find("-with-outro") == std::string::npos)
                {
                    candidates.push_back(entry.path());
                }
            }
            if (candidates.empty())
            {
                Terminal::instance().addLog("[OUTRO] no MP4 files found");
                return;
            }
            std::sort(candidates.begin(), candidates.end(),
                [](const fs::path& a, const fs::path& b) {
                    return fs::last_write_time(a) > fs::last_write_time(b);
                });
            fs::path latest = candidates[0];
            uint64_t size = fs::file_size(latest);
            double dur = probeDuration(latest.string());

            char buf[512];
            snprintf(buf, sizeof(buf),
                     "[OUTRO] latest=%s\n"
                     "[OUTRO]   duration=%.1f s\n"
                     "[OUTRO]   size=%llu bytes",
                     latest.filename().string().c_str(), dur, (unsigned long long)size);
            Terminal::instance().addLog(buf);
        }
    });
}
