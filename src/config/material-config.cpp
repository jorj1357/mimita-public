// 2026-09-28
/* purpose
* Implement the universal material table loader + hot reload.
* Reads config/materials.json; malformed JSON keeps the previous valid table.
* Does NOT own object behavior or damage authority.
*/

#include "config/material-config.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"

using json = nlohmann::json;

namespace MimitaImpact {
namespace {

std::filesystem::file_time_type getLastWrite(const std::string& path)
{
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type{} : t;
}

float readFloat(const json& j, const char* key, float fallback)
{
    if (j.contains(key) && j[key].is_number())
        return j[key].get<float>();
    return fallback;
}

} // anonymous namespace

uint32_t materialIdForName(const std::string& name)
{
    if (name.empty())
        return 0;
    uint32_t hash = 2166136261u;
    for (unsigned char c : name)
    {
        hash ^= (uint32_t)c;
        hash *= 16777619u;
    }
    return hash == 0 ? 1u : hash;
}

MaterialConfig& MaterialConfig::instance()
{
    static MaterialConfig config;
    return config;
}

const MaterialDefinition& MaterialConfig::fallback() const
{
    return mFallback;
}

bool MaterialConfig::load(const std::string& path)
{
    mPath = path;
    const auto writeTime = getLastWrite(mPath);

    std::ifstream file(mPath);
    if (!file.is_open())
    {
        mLastWrite = writeTime;
        Debug::warn(Debug::Category::General,
            "[MATERIAL CONFIG] Missing %s; using built-in defaults\n", mPath.c_str());
        return false;
    }

    try
    {
        json root = json::parse(file, nullptr, true, true);
        std::unordered_map<uint32_t, MaterialDefinition> nextById;

        if (root.contains("materials") && root["materials"].is_object())
        {
            for (auto it = root["materials"].begin(); it != root["materials"].end(); ++it)
            {
                const json& m = it.value();
                if (!m.is_object())
                    continue;
                MaterialDefinition def;
                def.id = m.value("id", it.key());
                def.density = readFloat(m, "density", def.density);
                def.strength = readFloat(m, "strength", def.strength);
                def.cutResistance = readFloat(m, "cutResistance", def.cutResistance);
                def.fractureThreshold = readFloat(m, "fractureThreshold", def.fractureThreshold);
                def.surfaceHardness = readFloat(m, "surfaceHardness", def.surfaceHardness);
                def.holeEnergyScale = readFloat(m, "holeEnergyScale", def.holeEnergyScale);
                def.maxCutRadius = readFloat(m, "maxCutRadius", def.maxCutRadius);
                nextById[materialIdForName(def.id)] = def;
            }
        }

        if (nextById.empty())
        {
            mLastWrite = writeTime;
            Debug::warn(Debug::Category::General,
                "[MATERIAL CONFIG] %s has no materials; keeping previous table\n",
                mPath.c_str());
            return false;
        }

        mById = std::move(nextById);
        mLastWrite = writeTime;
        ++mRevision;
        Debug::log(Debug::Category::General,
            "[MATERIAL CONFIG] Loaded %zu materials from %s (revision=%u)\n",
            mById.size(), mPath.c_str(), mRevision);
        return true;
    }
    catch (const std::exception& e)
    {
        mLastWrite = writeTime;
        Debug::error(Debug::Category::General,
            "[MATERIAL CONFIG] Error loading %s: %s. Keeping previous table\n",
            mPath.c_str(), e.what());
        return false;
    }
}

bool MaterialConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{} || writeTime == mLastWrite)
        return false;
    return load(mPath);
}

void MaterialConfig::reloadNow()
{
    load(mPath);
}

const MaterialDefinition& MaterialConfig::find(uint32_t materialId) const
{
    const auto it = mById.find(materialId);
    return it == mById.end() ? mFallback : it->second;
}

const MaterialDefinition& MaterialConfig::findByName(const std::string& name) const
{
    return find(materialIdForName(name));
}

} // namespace MimitaImpact
