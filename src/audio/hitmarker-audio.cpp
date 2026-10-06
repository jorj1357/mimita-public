#include "audio/hitmarker-audio.h"

#include <fstream>
#include <filesystem>
#include <cmath>
#include <chrono>

#include "nlohmann/json.hpp"
#include "audio/audio.h"
#include "combat/actor-preset-weapons.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"

struct HitmarkerAudioConfig {
    bool enabled = true;
    float volumeMin = 0.40f;
    float volumeMax = 1.00f;
    float pitchMin = 0.30f;
    float pitchMax = 1.30f;
    float damageForMaxImpact = 150.0f;
    float curveExponent = 0.5f;
    float deathPitchMin = 0.06f;
    float deathPitchMax = 3.90f;
    float deathDamageForMaxEffect = 1000.0f;
    float deathVolumeMin = 0.35f;
    float deathVolumeMax = 1.50f;
    float deathKillerVolumeMultiplier = 2.0f;
    float deathObserverVolumeMultiplier = 0.5f;
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
        j = nlohmann::json::parse(file, nullptr, true, true);

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
        if (j.contains("curveExponent"))
            loaded.curveExponent = j["curveExponent"].get<float>();
        if (j.contains("deathPitchMin"))
            loaded.deathPitchMin = j["deathPitchMin"].get<float>();
        if (j.contains("deathPitchMax"))
            loaded.deathPitchMax = j["deathPitchMax"].get<float>();
        if (j.contains("deathDamageForMaxEffect"))
            loaded.deathDamageForMaxEffect = j["deathDamageForMaxEffect"].get<float>();
        if (j.contains("deathVolumeMin"))
            loaded.deathVolumeMin = j["deathVolumeMin"].get<float>();
        if (j.contains("deathVolumeMax"))
            loaded.deathVolumeMax = j["deathVolumeMax"].get<float>();
        if (j.contains("deathKillerVolumeMultiplier"))
            loaded.deathKillerVolumeMultiplier = j["deathKillerVolumeMultiplier"].get<float>();
        if (j.contains("deathObserverVolumeMultiplier"))
            loaded.deathObserverVolumeMultiplier = j["deathObserverVolumeMultiplier"].get<float>();

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
    j["curveExponent"] = gConfig.curveExponent;
    j["deathPitchMin"] = gConfig.deathPitchMin;
    j["deathPitchMax"] = gConfig.deathPitchMax;
    j["deathDamageForMaxEffect"] = gConfig.deathDamageForMaxEffect;
    j["deathVolumeMin"] = gConfig.deathVolumeMin;
    j["deathVolumeMax"] = gConfig.deathVolumeMax;
    j["deathKillerVolumeMultiplier"] = gConfig.deathKillerVolumeMultiplier;
    j["deathObserverVolumeMultiplier"] = gConfig.deathObserverVolumeMultiplier;

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
    if (!ActorPresetWeapons::hitSoundsEnabled())
        return;
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
    float exp = std::max(0.01f, gConfig.curveExponent);
    float curve = std::pow(t, exp);
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

void playDeathSoundForDamage(int damage, bool localKiller, const glm::vec3& position)
{
    if (damage <= 0)
        return;

    const float t = std::clamp(
        static_cast<float>(damage) / std::max(1.0f, gConfig.deathDamageForMaxEffect),
        0.0f, 1.0f);
    const float exp = std::max(0.01f, gConfig.curveExponent);
    const float curve = std::pow(t, exp);
    const float pitch = gConfig.deathPitchMax -
        (gConfig.deathPitchMax - gConfig.deathPitchMin) * curve;
    const float baseVolume = gConfig.deathVolumeMin +
        (gConfig.deathVolumeMax - gConfig.deathVolumeMin) * curve;
    const float roleMultiplier = localKiller
        ? gConfig.deathKillerVolumeMultiplier
        : gConfig.deathObserverVolumeMultiplier;
    const float volume = baseVolume * std::max(0.0f, roleMultiplier);

    playWorldSound("npc_death", position, volume, pitch, 45.0f);
    Debug::log(Debug::Category::Audio,
               "[DEATH AUDIO] damage=%d localKiller=%d pitch=%.2f volume=%.2f\n",
               damage, (int)localKiller, pitch, volume);
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
