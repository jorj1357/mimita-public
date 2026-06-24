#include "video/outro.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>

#include "nlohmann/json.hpp"
#include "debug/debug-log.h"
#include "devtools/terminal.h"

static const char* CONFIG_PATH = "config/video/outro.json";

OutroConfig gOutroConfig;
static uint64_t gLastWriteTime = 0;

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

        gOutroConfig = loaded;
        Debug::log(Debug::Category::Replay,
                   "[OUTRO] config reloaded: enabled=%d path=%s\n",
                   (int)gOutroConfig.enabled, gOutroConfig.outroPath.c_str());
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
    j["enabled"] = gOutroConfig.enabled;
    j["outroPath"] = gOutroConfig.outroPath;

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

void registerOutroCommands()
{
    Terminal::instance().registerCommand({
        "outro", "Enable or disable outro appending (1=on, 0=off)", "outro <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(
                    std::string("[OUTRO] enabled=") + (gOutroConfig.enabled ? "1" : "0"));
                return;
            }
            gOutroConfig.enabled = args[0] != "0";
            saveConfig();
            Terminal::instance().addLog(
                std::string("[OUTRO] enabled=") + (gOutroConfig.enabled ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "outro_status", "Print outro configuration", "outro_status",
        [](const std::vector<std::string>&) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "[OUTRO] enabled=%d path=%s",
                     (int)gOutroConfig.enabled, gOutroConfig.outroPath.c_str());
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
                    Terminal::instance().addLog("[OUTRO] no exported MP4s found");
                    return;
                }
                std::sort(candidates.begin(), candidates.end(),
                    [](const fs::path& a, const fs::path& b) {
                        return fs::last_write_time(a) > fs::last_write_time(b);
                    });
                mp4Path = candidates[0].string();
            }

            Terminal::instance().addLog("[OUTRO] appending outro to: " + mp4Path);
            if (!gOutroConfig.enabled)
            {
                Terminal::instance().addLog("[OUTRO] outro is disabled, enabling temporarily");
                gOutroConfig.enabled = true;
            }

            appendOutroToFinishedMp4(mp4Path.c_str());

            Terminal::instance().addLog("[OUTRO] outro_test complete");
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

            char buf[512];
            snprintf(buf, sizeof(buf),
                     "[OUTRO] latest=%s\n"
                     "[OUTRO]   size=%llu bytes",
                     latest.filename().string().c_str(), (unsigned long long)size);
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "probe_test", "Probe durations of latest replay MP4 and outro", "probe_test",
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

            std::string replayPath = candidates[0].string();
            Terminal::instance().addLog("[OUTRO] probing: " + replayPath);
            Terminal::instance().addLog(std::string("[OUTRO] check probe_test command for durations"));
        }
    });

    Terminal::instance().registerCommand({
        "outro_append_test", "Append outro to latest replay and print all logs", "outro_append_test",
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
            std::string mp4Path = candidates[0].string();
            Terminal::instance().addLog("[OUTRO] appending outro to: " + mp4Path);
            appendOutroToFinishedMp4(mp4Path.c_str());
            Terminal::instance().addLog("[OUTRO] append complete");
        }
    });

    Terminal::instance().registerCommand({
        "outro_append_manual", "Append outro to a specific MP4 path", "outro_append_manual <full path to mp4>",
        [](const std::vector<std::string>& args) {
            if (args.empty())
            {
                Terminal::instance().addLog("[OUTRO] usage: outro_append_manual <path>");
                return;
            }
            std::string mp4Path = args[0];
            Terminal::instance().addLog("[OUTRO] manual append to: " + mp4Path);

            if (!std::filesystem::exists(mp4Path))
            {
                Terminal::instance().addLog("[OUTRO] file not found");
                return;
            }

            appendOutroToFinishedMp4(mp4Path.c_str());
            Terminal::instance().addLog("[OUTRO] manual append complete");
        }
    });
}
