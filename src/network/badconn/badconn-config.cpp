// 07 31 2026, 15 30
/* purpose
* Loads and validates the badconn preset block from config/networkingconfig.json
* into runtime preset structs (single source of truth with the networking config).
* Warns on unknown fields, skips invalid presets, and fails safe with no impairments.
* Keeps per-preset parsing isolated so one bad preset never blocks the rest.
* Does NOT own packet processing, queues, or the simulator state.
*/

#include "network/badconn/badconn.h"
#include "network/badconn/badconn-internal.h"
#include "network/badconn/badconn-registry.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <fstream>

namespace badconn {

namespace {

void warnInvalidField(const char* file, const char* presetId,
                      const char* block, const char* field, const char* detail)
{
    Debug::warn(Debug::Category::Networking,
                "[BADCONN CFG] invalid %s.%s in %s preset '%s' (%s)\n",
                block, field, file, presetId, detail);
}

void parseDirection(const nlohmann::json& block, const char* file,
                    const char* presetId, const char* blockName, Direction& out)
{
    out = Direction::Both;
    if (!block.contains("direction"))
        return;
    if (!block["direction"].is_string())
    {
        warnInvalidField(file, presetId, blockName, "direction",
                         "must be \"both\", \"in\", or \"out\" — defaulting to both");
        return;
    }
    const std::string value = block["direction"];
    if (value == "both")
        out = Direction::Both;
    else if (value == "in")
        out = Direction::In;
    else if (value == "out")
        out = Direction::Out;
    else
        warnInvalidField(file, presetId, blockName, "direction",
                         "unknown value — defaulting to both");
}

void warnUnknownKeys(const nlohmann::json& block, const char* file,
                     const char* presetId, const char* blockName,
                     const std::vector<std::string>& known)
{
    for (auto it = block.begin(); it != block.end(); ++it)
    {
        bool match = false;
        for (const std::string& key : known)
        {
            if (it.key() == key)
            {
                match = true;
                break;
            }
        }
        if (match)
            continue;
        badConnWarnUnknownKey(file, presetId,
                              (std::string(blockName) + "." + it.key()).c_str());
    }
}

void parseSeed(const nlohmann::json& presetJson, const char* file,
               const char* presetId, uint32_t& seed)
{
    if (!presetJson.contains("seed"))
        return;
    const nlohmann::json& value = presetJson["seed"];
    if (value.is_number_unsigned())
        seed = value.get<uint32_t>();
    else if (value.is_number_integer() && value.get<int64_t>() > 0)
        seed = static_cast<uint32_t>(value.get<int64_t>());
    else
        warnInvalidField(file, presetId, "preset", "seed",
                         "must be a positive integer — defaulting to 0 (random)");
}

} // namespace

bool loadLatencyBlock(const nlohmann::json& block, const char* file,
                      const char* presetId, BadConnPreset& preset)
{
    if (!block.is_object())
    {
        warnInvalidField(file, presetId, "latency", "block", "must be an object");
        return false;
    }
    warnUnknownKeys(block, file, presetId, "latency",
                    {"direction", "min_ms", "max_ms", "base_ms", "jitter_ms"});

    if (!block.contains("min_ms") && !block.contains("max_ms") &&
        !block.contains("base_ms"))
    {
        preset.latency.enabled = false;
        return true;
    }

    parseDirection(block, file, presetId, "latency", preset.latency.direction);

    const int minMs = block.value("min_ms", 0);
    const int maxMs = block.value("max_ms", 0);
    if (minMs < 0 || maxMs < 0 || minMs > maxMs || maxMs > 10000)
    {
        warnInvalidField(file, presetId, "latency", "min_ms/max_ms",
                         "need 0 <= min_ms <= max_ms <= 10000");
        return false;
    }

    const int baseMs = block.value("base_ms", 0);
    const int jitterMs = block.value("jitter_ms", 0);
    if (baseMs < 0 || baseMs > 10000 || jitterMs < 0 || jitterMs > 2000)
    {
        warnInvalidField(file, presetId, "latency", "base_ms/jitter_ms",
                         "need 0 <= base_ms <= 10000 and 0 <= jitter_ms <= 2000");
        return false;
    }

    preset.latency.enabled = true;
    preset.latency.minMs = minMs;
    preset.latency.maxMs = maxMs;
    preset.latency.baseMs = baseMs;
    preset.latency.jitterMs = jitterMs;
    return true;
}

bool loadLossBlock(const nlohmann::json& block, const char* file,
                   const char* presetId, BadConnPreset& preset)
{
    if (!block.is_object())
    {
        warnInvalidField(file, presetId, "loss", "block", "must be an object");
        return false;
    }
    warnUnknownKeys(block, file, presetId, "loss",
                    {"direction", "min_percent", "max_percent",
                     "burst_percent", "burst_probability"});

    if (!block.contains("min_percent") && !block.contains("max_percent"))
    {
        preset.loss.enabled = false;
        return true;
    }

    parseDirection(block, file, presetId, "loss", preset.loss.direction);

    const float minPercent = block.value("min_percent", 0.0f);
    const float maxPercent = block.value("max_percent", 0.0f);
    if (minPercent < 0.0f || maxPercent < 0.0f ||
        minPercent > maxPercent || maxPercent > 100.0f)
    {
        warnInvalidField(file, presetId, "loss", "min_percent/max_percent",
                         "need 0 <= min_percent <= max_percent <= 100");
        return false;
    }

    const float burstPercent = block.value("burst_percent", 0.0f);
    const float burstProbability = block.value("burst_probability", 0.0f);
    if (burstPercent < 0.0f || burstPercent > 100.0f ||
        burstProbability < 0.0f || burstProbability > 1.0f)
    {
        warnInvalidField(file, presetId, "loss", "burst_percent/burst_probability",
                         "need 0 <= burst_percent <= 100 and 0 <= burst_probability <= 1");
        return false;
    }

    preset.loss.enabled = true;
    preset.loss.minPercent = minPercent;
    preset.loss.maxPercent = maxPercent;
    preset.loss.burstPercent = burstPercent;
    preset.loss.burstProbability = burstProbability;
    return true;
}

bool loadReorderBlock(const nlohmann::json& block, const char* file,
                      const char* presetId, BadConnPreset& preset)
{
    if (!block.is_object())
    {
        warnInvalidField(file, presetId, "reorder", "block", "must be an object");
        return false;
    }
    warnUnknownKeys(block, file, presetId, "reorder",
                    {"direction", "min_percent", "max_percent", "window"});

    if (!block.contains("min_percent") && !block.contains("max_percent"))
    {
        preset.reorder.enabled = false;
        return true;
    }

    parseDirection(block, file, presetId, "reorder", preset.reorder.direction);

    const float minPercent = block.value("min_percent", 0.0f);
    const float maxPercent = block.value("max_percent", 0.0f);
    if (minPercent < 0.0f || maxPercent < 0.0f ||
        minPercent > maxPercent || maxPercent > 100.0f)
    {
        warnInvalidField(file, presetId, "reorder", "min_percent/max_percent",
                         "need 0 <= min_percent <= max_percent <= 100");
        return false;
    }

    int window = block.value("window", 8);
    preset.reorder.window = std::clamp(window, 2, 64);

    preset.reorder.enabled = true;
    preset.reorder.minPercent = minPercent;
    preset.reorder.maxPercent = maxPercent;
    return true;
}

bool loadBlackoutBlock(const nlohmann::json& block, const char* file,
                       const char* presetId, BadConnPreset& preset)
{
    if (!block.is_object())
    {
        warnInvalidField(file, presetId, "blackout", "block", "must be an object");
        return false;
    }
    warnUnknownKeys(block, file, presetId, "blackout",
                    {"enabled", "start_probability_per_second", "min_ms",
                     "max_ms", "cooldown_ms", "direction", "release_queued"});

    const bool enabled = block.value("enabled", false);
    preset.blackout.enabled = enabled;
    if (!enabled)
        return true;

    parseDirection(block, file, presetId, "blackout", preset.blackout.direction);

    const double probability = block.value("start_probability_per_second", 0.0);
    if (probability < 0.0 || probability > 1.0)
    {
        warnInvalidField(file, presetId, "blackout", "start_probability_per_second",
                         "need 0 <= probability <= 1");
        return false;
    }

    const int minMs = block.value("min_ms", 0);
    const int maxMs = block.value("max_ms", 0);
    if (minMs < 0 || maxMs < 0 || minMs > maxMs)
    {
        warnInvalidField(file, presetId, "blackout", "min_ms/max_ms",
                         "need 0 <= min_ms <= max_ms");
        return false;
    }

    const int cooldownMs = block.value("cooldown_ms", 0);
    if (cooldownMs < 0)
    {
        warnInvalidField(file, presetId, "blackout", "cooldown_ms",
                         "must be >= 0");
        return false;
    }

    preset.blackout.startProbabilityPerSecond = probability;
    preset.blackout.minMs = minMs;
    preset.blackout.maxMs = maxMs;
    preset.blackout.cooldownMs = cooldownMs;
    preset.blackout.releaseQueued = block.value("release_queued", false);
    return true;
}

bool loadConfig(const std::string& path)
{
    std::vector<BadConnPreset> loaded;
    loaded.reserve(16);

    try
    {
        std::ifstream stream(path);
        if (!stream.is_open())
        {
            Debug::warn(Debug::Category::Networking,
                        "[BADCONN CFG] cannot open %s — no presets active\n",
                        path.c_str());
            applyLoadedPresets(std::move(loaded));
            return false;
        }

        nlohmann::json root;
        stream >> root;

        if (!root.is_object())
        {
            Debug::warn(Debug::Category::Networking,
                        "[BADCONN CFG] %s root is not an object — no presets active\n",
                        path.c_str());
            applyLoadedPresets(std::move(loaded));
            return false;
        }

        // Badconn presets live inside the shared networkingconfig.json under
        // "badconn.presets"; fall back to the old top-level "presets" shape.
        const nlohmann::json* presetsJson = nullptr;
        if (root.contains("badconn") && root["badconn"].is_object() &&
            root["badconn"].contains("presets") &&
            root["badconn"]["presets"].is_object())
        {
            presetsJson = &root["badconn"]["presets"];
        }
        else if (root.contains("presets") && root["presets"].is_object())
        {
            presetsJson = &root["presets"];
        }
        if (!presetsJson)
        {
            Debug::warn(Debug::Category::Networking,
                        "[BADCONN CFG] %s missing badconn.presets object — no presets active\n",
                        path.c_str());
            applyLoadedPresets(std::move(loaded));
            return false;
        }

        const int version = root.value("version", 0);
        if (version != 1)
        {
            Debug::warn(Debug::Category::Networking,
                        "[BADCONN CFG] %s unsupported version %d — no presets active\n",
                        path.c_str(), version);
            applyLoadedPresets(std::move(loaded));
            return false;
        }

        for (auto it = presetsJson->begin(); it != presetsJson->end(); ++it)
        {
            const std::string id = it.key();
            const nlohmann::json& presetJson = it.value();
            try
            {
                if (!presetJson.is_object())
                {
                    Debug::warn(Debug::Category::Networking,
                                "[BADCONN CFG] preset '%s' in %s is not an object — skipped\n",
                                id.c_str(), path.c_str());
                    continue;
                }

                BadConnPreset preset;
                preset.id = id;
                preset.name = presetJson.value("name", "Preset " + id);
                parseSeed(presetJson, path.c_str(), id.c_str(), preset.seed);

                std::vector<std::string> presetKeys = {"name", "seed"};
                for (const BadConnImpairmentHandler& handler : badConnImpairmentHandlers())
                    presetKeys.push_back(handler.fieldName);
                warnUnknownKeys(presetJson, path.c_str(), id.c_str(), "preset",
                                presetKeys);

                bool valid = true;
                for (const BadConnImpairmentHandler& handler : badConnImpairmentHandlers())
                {
                    if (!presetJson.contains(handler.fieldName))
                        continue;
                    if (!handler.load(presetJson[handler.fieldName], path.c_str(),
                                      id.c_str(), preset))
                    {
                        valid = false;
                        break;
                    }
                }

                if (!valid)
                {
                    Debug::warn(Debug::Category::Networking,
                                "[BADCONN CFG] preset '%s' in %s skipped (invalid fields)\n",
                                id.c_str(), path.c_str());
                    continue;
                }

                loaded.push_back(std::move(preset));
            }
            catch (const std::exception& presetError)
            {
                Debug::warn(Debug::Category::Networking,
                            "[BADCONN CFG] preset '%s' in %s parse error: %s — skipped\n",
                            id.c_str(), path.c_str(), presetError.what());
            }
        }

        Debug::warn(Debug::Category::Networking,
                    "[BADCONN CFG] loaded %zu presets from %s\n",
                    loaded.size(), path.c_str());
        applyLoadedPresets(std::move(loaded));
        return true;
    }
    catch (const std::exception& error)
    {
        Debug::warn(Debug::Category::Networking,
                    "[BADCONN CFG] parse error in %s: %s — no presets active\n",
                    path.c_str(), error.what());
        applyLoadedPresets(std::move(loaded));
        return false;
    }
}

} // namespace badconn
