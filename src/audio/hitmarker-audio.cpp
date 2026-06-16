#include "audio/hitmarker-audio.h"

#include <fstream>
#include <filesystem>
#include <cmath>
#include <chrono>

#include "nlohmann/json.hpp"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"

struct HitmarkerAudioConfig {
    bool enabled = true;
    float volumeMin = 0.40f;
    float volumeMax = 1.00f;
    float pitchMin = 0.70f;
    float pitchMax = 1.30f;
    float damageForMaxImpact = 100.0f;
};

static HitmarkerAudioConfig gConfig;
static uint64_t gLastWriteTime = 0;
static double gLastSoundTime = 0.0;

static const char* CONFIG_PATH = "config/audio/hitmarker.json";

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

        HitmarkerAudioConfig loaded;
        if (j.contains("enabled"))
            loaded.enabled = j["enabled"].get<bool>();
        if (j.contains("volumeMin"))
            loaded.volumeMin = j["volumeMin"].get<float>();
        if (j.contains("volumeMax"))
            loaded.volumeMax = j["volumeMax"].get<float>();
        if (j.contains("pitchMin"))
            loaded.pitchMin = j["pitchMin"].get<float>();
        if (j.contains("pitchMax"))
            loaded.pitchMax = j["pitchMax"].get<float>();
        if (j.contains("damageForMaxImpact"))
            loaded.damageForMaxImpact = j["damageForMaxImpact"].get<float>();

        gConfig = loaded;
        Debug::log(Debug::Category::Audio,
                   "[HITMARKER AUDIO] config reloaded: enabled=%d\n", (int)gConfig.enabled);
    }
    catch (const std::exception& e)
    {
        Debug::log(Debug::Category::Audio,
                   "[HITMARKER AUDIO] config reload failed: %s\n", e.what());
    }
}

static void saveConfig()
{
    nlohmann::json j;
    j["enabled"] = gConfig.enabled;
    j["volumeMin"] = gConfig.volumeMin;
    j["volumeMax"] = gConfig.volumeMax;
    j["pitchMin"] = gConfig.pitchMin;
    j["pitchMax"] = gConfig.pitchMax;
    j["damageForMaxImpact"] = gConfig.damageForMaxImpact;

    std::ofstream file(CONFIG_PATH);
    if (file.is_open())
        file << j.dump(4) << std::endl;
}

void pollHitmarkerAudioConfig()
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

void playHitmarkerSound(int damage)
{
    if (!gConfig.enabled)
        return;

    double now = 0.0;
    {
        using namespace std::chrono;
        now = duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    // Rate limit: max 1 sound per 50ms
    if (now - gLastSoundTime < 0.05)
        return;
    gLastSoundTime = now;

    float t = std::clamp((float)damage / gConfig.damageForMaxImpact, 0.0f, 1.0f);
    float curve = std::sqrt(t);
    float pitch = gConfig.pitchMax - (gConfig.pitchMax - gConfig.pitchMin) * curve;
    float volume = gConfig.volumeMin + (gConfig.volumeMax - gConfig.volumeMin) * curve;

    playSoundPitched("hitmarker1", volume, pitch);

    Debug::log(Debug::Category::Audio,
               "[HITMARKER AUDIO]\n"
               "  damage=%d\n"
               "  pitch=%.2f\n"
               "  volume=%.2f\n",
               damage, pitch, volume);
}

void registerHitmarkerAudioCommands()
{
    Terminal::instance().registerCommand({
        "hitmarker_audio_reload", "Reload config/audio/hitmarker.json", "hitmarker_audio_reload",
        [](const std::vector<std::string>&) {
            reloadConfig();
            Terminal::instance().addLog("[HITMARKER AUDIO] config reloaded");
        }
    });

    Terminal::instance().registerCommand({
        "hitmarker_audio_test", "Play hitmarker sound with specified damage", "hitmarker_audio_test <damage>",
        [](const std::vector<std::string>& args) {
            int damage = args.empty() ? 25 : std::stoi(args[0]);
            playHitmarkerSound(damage);
            Terminal::instance().addLog("[HITMARKER AUDIO] test damage=" + std::to_string(damage));
        }
    });
}
