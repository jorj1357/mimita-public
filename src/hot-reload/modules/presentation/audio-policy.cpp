// 09 23 2026
/* purpose
* Hot audio-policy owner. Loads the logical-sound recipe snapshot from
* config/audio-recipes.json (hot, last-valid preserved on parse failure) and
* converts a recipe key plus plain-data overrides into one GameAudioCommandV2
* through the stable audio.play capability. Also handles the generic sound-only
* `audio.fact` event so cold callers can request a recipe sound with a fallback.
* Does NOT own the device, mixer, decoded resources, or the voice table.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/hot-audio-policy.h"

#include "hot-reload/hot-package.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

using AudioPlayFn = void (MIMITA_GAME_CALL *)(void*,
                                              const GameAudioCommandV1*);

constexpr int kMaxRecipeSounds = 8;
constexpr int kMaxRecipes = 64;
const char* kRecipePath = "config/audio-recipes.json";

struct Recipe {
    std::uint64_t key = 0;
    int soundCount = 0;
    char sounds[kMaxRecipeSounds][64] = {};
    float volBase = 1.0f, volMin = 1.0f, volMax = 1.0f;
    float pitchBase = 1.0f, pitchMin = 1.0f, pitchMax = 1.0f;
    float maxDistance = 30.0f;
    float falloffStart = 2.0f;
    std::uint32_t category = GAME_AUDIO_CATEGORY_IMPACTS;
    std::uint32_t spatialMode = 1;
    bool loop = false;
    std::uint32_t priority = 0;
    std::uint32_t interruption = GAME_AUDIO_INTERRUPT_OVERLAP;
    bool deterministic = false;
    bool repeatAllowed = true;
    std::uint32_t cooldownMs = 0;
};

std::vector<Recipe> g_recipes;
std::filesystem::file_time_type g_lastWrite;
bool g_loaded = false;
std::uint64_t g_requestId = 0;

std::string lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::uint32_t categoryFromName(const std::string& raw)
{
    const std::string s = lower(raw);
    if (s == "movement") return GAME_AUDIO_CATEGORY_MOVEMENT;
    if (s == "ui") return GAME_AUDIO_CATEGORY_UI;
    if (s == "weapons") return GAME_AUDIO_CATEGORY_WEAPONS;
    if (s == "npc") return GAME_AUDIO_CATEGORY_NPC;
    if (s == "impacts") return GAME_AUDIO_CATEGORY_IMPACTS;
    if (s == "ambient") return GAME_AUDIO_CATEGORY_AMBIENT;
    if (s == "music") return GAME_AUDIO_CATEGORY_MUSIC;
    if (s == "notification") return GAME_AUDIO_CATEGORY_NOTIFICATION;
    if (s == "editor") return GAME_AUDIO_CATEGORY_EDITOR;
    if (s == "debug") return GAME_AUDIO_CATEGORY_DEBUG;
    return GAME_AUDIO_CATEGORY_IMPACTS;
}

std::uint32_t spatialFromName(const std::string& raw)
{
    const std::string s = lower(raw);
    if (s == "local") return 0;
    if (s == "listener") return 2;
    return 1;   // world
}

std::uint32_t interruptionFromName(const std::string& raw)
{
    const std::string s = lower(raw);
    if (s == "reject") return GAME_AUDIO_INTERRUPT_REJECT;
    if (s == "replace_oldest") return GAME_AUDIO_INTERRUPT_REPLACE_OLDEST;
    if (s == "replace_same_slot") return GAME_AUDIO_INTERRUPT_REPLACE_SAME_SLOT;
    if (s == "restart") return GAME_AUDIO_INTERRUPT_RESTART;
    return GAME_AUDIO_INTERRUPT_OVERLAP;
}

// base defaults lo/hi; min/max override the jitter range when present.
void readRange(const nlohmann::json& node, float& base, float& lo, float& hi)
{
    if (!node.is_object())
        return;
    if (node.contains("base"))
        base = node["base"].get<float>();
    lo = hi = base;
    if (node.contains("min"))
        lo = node["min"].get<float>();
    if (node.contains("max"))
        hi = node["max"].get<float>();
}

bool loadRecipes()
{
    std::ifstream file(kRecipePath);
    if (!file.is_open())
        return false;
    try {
        const nlohmann::json root =
            nlohmann::json::parse(file, nullptr, true, true);
        if (!root.is_object())
            return false;
        std::vector<Recipe> next;
        next.reserve(16);
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (!it.value().is_object())
                continue;
            if (static_cast<int>(next.size()) >= kMaxRecipes)
                break;
            const nlohmann::json& node = it.value();
            Recipe r;
            r.key = gameHash(it.key().c_str());
            if (node.contains("sounds") && node["sounds"].is_array()) {
                for (const auto& s : node["sounds"]) {
                    if (r.soundCount >= kMaxRecipeSounds)
                        break;
                    if (!s.is_string())
                        continue;
                    std::snprintf(r.sounds[r.soundCount], sizeof(r.sounds[0]),
                                  "%s", s.get<std::string>().c_str());
                    ++r.soundCount;
                }
            }
            if (r.soundCount == 0)
                continue;
            readRange(node.value("volume", nlohmann::json::object()),
                      r.volBase, r.volMin, r.volMax);
            readRange(node.value("pitch", nlohmann::json::object()),
                      r.pitchBase, r.pitchMin, r.pitchMax);
            if (node.contains("maxDistance"))
                r.maxDistance = node["maxDistance"].get<float>();
            if (node.contains("falloffStart"))
                r.falloffStart = node["falloffStart"].get<float>();
            if (node.contains("category"))
                r.category =
                    categoryFromName(node["category"].get<std::string>());
            if (node.contains("spatialMode"))
                r.spatialMode =
                    spatialFromName(node["spatialMode"].get<std::string>());
            if (node.contains("loop"))
                r.loop = node["loop"].get<bool>();
            if (node.contains("priority"))
                r.priority = node["priority"].get<std::uint32_t>();
            if (node.contains("overlap"))
                r.interruption =
                    interruptionFromName(node["overlap"].get<std::string>());
            if (node.contains("repeatAllowed"))
                r.repeatAllowed = node["repeatAllowed"].get<bool>();
            if (node.contains("cooldownMs"))
                r.cooldownMs = node["cooldownMs"].get<std::uint32_t>();
            if (node.contains("selection"))
                r.deterministic =
                    node["selection"].get<std::string>() == "deterministic";
            next.push_back(r);
        }
        if (next.empty())
            return false;
        g_recipes = std::move(next);
        g_loaded = true;
        return true;
    } catch (const std::exception& e) {
        std::printf("[AUDIO POLICY] recipe parse failed: %s (keeping last valid)\n",
                    e.what());
        return false;
    }
}

void pollRecipes()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point nextCheck;
    const auto now = Clock::now();
    if (now < nextCheck)
        return;
    nextCheck = now + std::chrono::milliseconds(250);

    std::error_code ec;
    const auto wt = std::filesystem::last_write_time(kRecipePath, ec);
    if (ec)
        return;
    if (!g_loaded || wt != g_lastWrite) {
        g_lastWrite = wt;
        loadRecipes();
    }
}

const Recipe* findRecipe(std::uint64_t key)
{
    for (const Recipe& r : g_recipes)
        if (r.key == key)
            return &r;
    return nullptr;
}

std::uint32_t mixSeed(std::uint64_t a, std::uint64_t b, std::uint64_t c)
{
    std::uint64_t x = a * 0x9E3779B97F4A7C15ull;
    x ^= b + 0x165667B19E3779F9ull + (x << 6) + (x >> 2);
    x ^= c + 0x27D4EB2F165667C5ull + (x << 6) + (x >> 2);
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    return static_cast<std::uint32_t>(x);
}

float unitFloat(std::uint32_t h)
{
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

void MIMITA_GAME_CALL onAudioFact(void* host, const GameEventV1* event)
{
    auto* ctx = static_cast<GameplayContextV1*>(host);
    auto* fact = event ? static_cast<GameAudioFactV1*>(event->payload) : nullptr;
    if (!ctx || !fact)
        return;
    HotAudioOverrideV1 ov{};
    ov.volumeScale = fact->volumeScale > 0.0f ? fact->volumeScale : 1.0f;
    ov.pitchScale = fact->pitchScale > 0.0f ? fact->pitchScale : 1.0f;
    ov.volumeBase = fact->volumeBase;
    ov.pitchBase = fact->pitchBase;
    ov.seed = fact->seed;
    ov.sound = fact->sound[0] != '\0' ? fact->sound : nullptr;
    if (hotEmitRecipeSound(ctx, fact->recipeKey, fact->position,
                           fact->ownerEntity, fact->spatial != 0, &ov))
        fact->handled = 1u;
}

const MimitaHotPackage::EventRegistrar s_audioFact{
    {GAME_EVENT_AUDIO_FACT, gameHash("audio.fact.v2"), 0, onAudioFact,
     "hot.audio-policy"}};

} // namespace

bool hotEmitRecipeSound(GameplayContextV1* ctx, std::uint64_t recipeKey,
                        const float position[3], std::uint64_t ownerEntity,
                        bool spatial, const HotAudioOverrideV1* overrides)
{
    if (!ctx || !ctx->resolveCapability)
        return false;
    pollRecipes();
    const Recipe* r = findRecipe(recipeKey);
    const char* explicitSound =
        (overrides && overrides->sound && overrides->sound[0])
            ? overrides->sound
            : nullptr;
    // A recipe or an explicit logical sound is required.
    if (!r && !explicitSound)
        return false;
    auto audio = reinterpret_cast<AudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!audio)
        return false;

    const std::uint64_t seed =
        (overrides && overrides->seed) ? overrides->seed : ctx->tick;
    const std::uint32_t h = mixSeed(recipeKey, ownerEntity, seed);
    const std::uint32_t variant =
        (r && r->soundCount > 0)
            ? (h >> 4) % static_cast<std::uint32_t>(r->soundCount)
            : 0u;

    // Defaults when no recipe matched: generic impact policy.
    float volBase = r ? r->volBase : 1.0f;
    float volMin = r ? r->volMin : 1.0f;
    float volMax = r ? r->volMax : 1.0f;
    float pitchBase = r ? r->pitchBase : 1.0f;
    float pitchMin = r ? r->pitchMin : 1.0f;
    float pitchMax = r ? r->pitchMax : 1.0f;

    float volume = volMin + (volMax - volMin) * unitFloat(h);
    float pitch =
        pitchMin + (pitchMax - pitchMin) * unitFloat(h * 2654435761u + 1u);
    (void)volBase;
    (void)pitchBase;
    if (overrides) {
        if (overrides->volumeBase >= 0.0f)
            volume = overrides->volumeBase;
        if (overrides->pitchBase >= 0.0f)
            pitch = overrides->pitchBase;
        volume *= overrides->volumeScale;
        pitch *= overrides->pitchScale;
    }

    GameAudioCommandV1 cmd{};
    cmd.commandVersion = GAME_AUDIO_COMMAND_VERSION;
    cmd.structSize = sizeof(GameAudioCommandV1);
    std::snprintf(cmd.sound, sizeof(cmd.sound), "%s",
                  explicitSound ? explicitSound
                                : (r ? r->sounds[variant] : ""));
    cmd.position[0] = position ? position[0] : 0.0f;
    cmd.position[1] = position ? position[1] : 0.0f;
    cmd.position[2] = position ? position[2] : 0.0f;
    cmd.volume = volume;
    cmd.pitch = pitch;
    cmd.maxDistance = r ? r->maxDistance : 40.0f;
    cmd.spatial = spatial ? 1u : 0u;
    cmd.spatialMode = r ? r->spatialMode : 1u;
    cmd.category = r ? r->category : GAME_AUDIO_CATEGORY_IMPACTS;
    cmd.ownerEntity = ownerEntity;
    cmd.op = GAME_AUDIO_PLAY_ONESHOT;
    cmd.loop = (r && r->loop) ? 1u : 0u;
    cmd.priority = r ? r->priority : 0u;
    cmd.interruption =
        r ? r->interruption : GAME_AUDIO_INTERRUPT_OVERLAP;
    cmd.seed = h;
    cmd.requestId = ++g_requestId;
    cmd.hotGeneration = ctx->generation;
    cmd.falloffStart = r ? r->falloffStart : 2.0f;
    audio(ctx->host, &cmd);
    return true;
}

bool hotAudioRecipeExists(std::uint64_t recipeKey)
{
    pollRecipes();
    return findRecipe(recipeKey) != nullptr;
}

std::uint32_t hotAudioRecipeCount()
{
    pollRecipes();
    return static_cast<std::uint32_t>(g_recipes.size());
}

#endif // MIMITA_GAME_DLL
