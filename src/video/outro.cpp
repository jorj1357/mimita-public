#include "video/outro.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "nlohmann/json.hpp"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "replay/replay-export.h"

struct OutroConfig {
    bool enabled = true;
    std::string outroPath = "assets/video/mimitaoutrov1.webm";
};

static OutroConfig gConfig;
static uint64_t gLastWriteTime = 0;

static const char* CONFIG_PATH = "config/video/outro.json";

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

void appendOutroToExport(const char* exportedVideoPath)
{
    if (!gConfig.enabled)
        return;
    if (!std::filesystem::exists(gConfig.outroPath))
    {
        Debug::log(Debug::Category::Replay,
                   "[OUTRO] outro file not found: %s\n", gConfig.outroPath.c_str());
        return;
    }

    Debug::log(Debug::Category::Replay,
               "[OUTRO]\n"
               "  enabled=%d\n"
               "  source=%s\n",
               (int)gConfig.enabled, gConfig.outroPath.c_str());

    // Probe durations using ffprobe (optional, for logging)
    auto probeDuration = [](const std::string& path) -> double {
        char cmd[1024];
        char tmpPath[512];
        std::snprintf(tmpPath, sizeof(tmpPath), "replays/exports/_tmp/_dur_%lld.txt",
                      (long long)std::time(nullptr));
        std::snprintf(cmd, sizeof(cmd),
                      "\"%s\" -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\" > \"%s\" 2>nul",
                      defaultFfmpegPath().c_str(), path.c_str(), tmpPath);
        std::system(cmd);
        double dur = 0.0;
        std::ifstream f(tmpPath);
        if (f.is_open()) { f >> dur; f.close(); }
        std::filesystem::remove(tmpPath);
        return dur;
    };

    double exportDuration = probeDuration(exportedVideoPath);
    double outroDuration = probeDuration(gConfig.outroPath);

    Debug::log(Debug::Category::Replay,
               "[OUTRO]\n"
               "  exportDuration=%.1f\n"
               "  outroDuration=%.1f\n",
               exportDuration, outroDuration);

    // Determine output path (replace .mp4 with -final.mp4)
    std::string inputStr(exportedVideoPath);
    std::string finalPath;
    size_t dot = inputStr.rfind('.');
    if (dot != std::string::npos)
        finalPath = inputStr.substr(0, dot) + "-final.mp4";
    else
        finalPath = inputStr + "-final.mp4";

    // Build batch script for ffmpeg concat
    std::string batDir = "replays/exports/_tmp/";
    std::error_code ec;
    std::filesystem::create_directories(batDir, ec);

    std::string batPath = batDir + "concat_outro.bat";
    std::string concatListPath = batDir + "concat_list.txt";

    // Write concat demuxer list
    {
        std::ofstream list(concatListPath);
        if (!list.is_open())
        {
            Debug::log(Debug::Category::Replay,
                       "[OUTRO] failed to create concat list: %s\n", concatListPath.c_str());
            return;
        }
        auto escapePath = [](const std::string& p) -> std::string {
            std::string r = p;
            size_t pos = 0;
            while ((pos = r.find('\'', pos)) != std::string::npos) {
                r.replace(pos, 1, "'\\''");
                pos += 4;
            }
            return r;
        };
        list << "file '" << escapePath(inputStr) << "'\n";
        list << "file '" << escapePath(gConfig.outroPath) << "'\n";
        list.close();
    }

    // Probe resolution of export to transcode outro to match
    auto probeVideoSize = [](const std::string& path) -> std::pair<int,int> {
        char cmd[1024];
        char tmpPath[512];
        std::snprintf(tmpPath, sizeof(tmpPath), "replays/exports/_tmp/_vsize_%lld.txt",
                      (long long)std::time(nullptr));
        std::snprintf(cmd, sizeof(cmd),
                      "\"%s\" -v error -select_streams v:0 -show_entries stream=width,height -of csv=s=x:p=0 \"%s\" > \"%s\" 2>nul",
                      defaultFfmpegPath().c_str(), path.c_str(), tmpPath);
        std::system(cmd);
        int w = 0, h = 0;
        std::ifstream f(tmpPath);
        if (f.is_open()) {
            std::string line;
            std::getline(f, line);
            size_t x = line.find('x');
            if (x != std::string::npos) {
                w = std::atoi(line.substr(0, x).c_str());
                h = std::atoi(line.substr(x + 1).c_str());
            }
            f.close();
        }
        std::filesystem::remove(tmpPath);
        return {w, h};
    };

    auto [expW, expH] = probeVideoSize(inputStr);
    auto [outroW, outroH] = probeVideoSize(gConfig.outroPath);

    // Build ffmpeg command: concat with filter_complex, scale outro if needed
    // Use concat filter for reliable mixed-codec concatenation
    char ffmpegCmd[4096];
    int cmdLen = 0;

    if (outroW > 0 && outroH > 0 && (outroW != expW || outroH != expH))
    {
        // Outro resolution differs: scale it to match export
        cmdLen = std::snprintf(ffmpegCmd, sizeof(ffmpegCmd),
            "@echo off\n"
            "\"%s\" -y -i \"%s\" -i \"%s\" -filter_complex "
            "\"[1:v]scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2[scaled];"
            "[0:v][0:a][scaled][1:a]concat=n=2:v=1:a=1[outv][outa]\" "
            "-map \"[outv]\" -map \"[outa]\" -c:v libx264 -preset fast -pix_fmt yuv420p -crf 18 "
            "-c:a aac -b:a 192k \"%s\" -loglevel error%s\n"
            "exit /b %%ERRORLEVEL%%\n",
            defaultFfmpegPath().c_str(),
            inputStr.c_str(), gConfig.outroPath.c_str(),
            expW, expH, expW, expH,
            finalPath.c_str(),
            isFfmpegDebugMode() ? "" : " 2>nul");
    }
    else
    {
        // Same resolution or cannot probe — concat directly
        cmdLen = std::snprintf(ffmpegCmd, sizeof(ffmpegCmd),
            "@echo off\n"
            "\"%s\" -y -i \"%s\" -i \"%s\" -filter_complex "
            "\"[0:v][0:a][1:v][1:a]concat=n=2:v=1:a=1[outv][outa]\" "
            "-map \"[outv]\" -map \"[outa]\" -c:v libx264 -preset fast -pix_fmt yuv420p -crf 18 "
            "-c:a aac -b:a 192k \"%s\" -loglevel error%s\n"
            "exit /b %%ERRORLEVEL%%\n",
            defaultFfmpegPath().c_str(),
            inputStr.c_str(), gConfig.outroPath.c_str(),
            finalPath.c_str(),
            isFfmpegDebugMode() ? "" : " 2>nul");
    }

    // Write batch file
    {
        std::ofstream bat(batPath);
        if (bat.is_open()) {
            bat << ffmpegCmd;
            bat.close();
        } else {
            Debug::log(Debug::Category::Replay,
                       "[OUTRO] failed to write batch: %s\n", batPath.c_str());
            return;
        }
    }

    Debug::log(Debug::Category::Replay,
               "[OUTRO] concat command: %s\n", ffmpegCmd);

    int result = std::system(batPath.c_str());

    // Clean up temp files
    std::filesystem::remove(batPath, ec);
    std::filesystem::remove(concatListPath, ec);

    if (result == 0 && std::filesystem::exists(finalPath))
    {
        uint64_t finalSize = std::filesystem::file_size(finalPath, ec);
        double finalDuration = probeDuration(finalPath);

        Debug::log(Debug::Category::Replay,
                   "[OUTRO]\n"
                   "  finalDuration=%.1f\n"
                   "  concat success\n",
                   finalDuration);

        // Replace original with final
        std::filesystem::rename(finalPath, inputStr, ec);
        if (ec)
        {
            Debug::log(Debug::Category::Replay,
                       "[OUTRO] rename failed: %s\n", ec.message().c_str());
        }
    }
    else
    {
        Debug::log(Debug::Category::Replay,
                   "[OUTRO] concat failed (exit=%d)\n", result);
        // Clean up finalPath if it exists
        std::filesystem::remove(finalPath, ec);
    }
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
}
