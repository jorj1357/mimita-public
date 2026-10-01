// 2026-10-01
/* purpose
* Implement the destruction-budget / fracture-tuning loader + hot reload.
* Reads config/destructible-world.json; malformed JSON keeps the previous
* valid values. Does NOT own cut sizing or gameplay behavior.
*/

#include "impact/destructible-world-config.h"

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

} // anonymous namespace

DestructibleWorldConfig& DestructibleWorldConfig::instance()
{
    static DestructibleWorldConfig config;
    return config;
}

bool DestructibleWorldConfig::load(const std::string& path)
{
    mPath = path;
    const auto writeTime = getLastWrite(mPath);

    std::ifstream file(mPath);
    if (!file.is_open())
    {
        mLastWrite = writeTime;
        mHasWriteTime = true;
        Debug::warn(Debug::Category::General,
            "[DESTRUCTIBLE CONFIG] Missing %s; using built-in defaults\n",
            mPath.c_str());
        return false;
    }

    try
    {
        json root = json::parse(file, nullptr, true, true);
        const json& destruction =
            root.contains("destruction") && root["destruction"].is_object()
                ? root["destruction"] : json::object();
        const json& fracture =
            root.contains("fracture") && root["fracture"].is_object()
                ? root["fracture"] : json::object();

        if (destruction.contains("maxCutsPerEntityPerTick") &&
            destruction["maxCutsPerEntityPerTick"].is_number_unsigned())
            mMaxCutsPerEntityPerTick =
                destruction["maxCutsPerEntityPerTick"].get<uint32_t>();
        if (destruction.contains("cutBudgetMsPerTick") &&
            destruction["cutBudgetMsPerTick"].is_number())
            mCutBudgetMsPerTick =
                destruction["cutBudgetMsPerTick"].get<float>();
        if (destruction.contains("maxTrianglesPerEntity") &&
            destruction["maxTrianglesPerEntity"].is_number_unsigned())
            mMaxTrianglesPerEntity =
                destruction["maxTrianglesPerEntity"].get<size_t>();

        if (fracture.contains("enabled") && fracture["enabled"].is_boolean())
            mFractureTuning.enabled = fracture["enabled"].get<bool>();
        if (fracture.contains("minPieceVolumeFraction") &&
            fracture["minPieceVolumeFraction"].is_number())
            mFractureTuning.minPieceVolumeFraction =
                fracture["minPieceVolumeFraction"].get<float>();
        if (fracture.contains("maxRemainingFraction") &&
            fracture["maxRemainingFraction"].is_number())
            mFractureTuning.maxRemainingFraction =
                fracture["maxRemainingFraction"].get<float>();
        if (fracture.contains("comOffsetFraction") &&
            fracture["comOffsetFraction"].is_number())
            mFractureTuning.comOffsetFraction =
                fracture["comOffsetFraction"].get<float>();
        if (fracture.contains("maxFragmentsPerEvent") &&
            fracture["maxFragmentsPerEvent"].is_number_unsigned())
            mFractureTuning.maxFragmentsPerEvent =
                fracture["maxFragmentsPerEvent"].get<uint32_t>();

        mLastWrite = writeTime;
        mHasWriteTime = true;
        ++mRevision;
        Debug::log(Debug::Category::General,
            "[DESTRUCTIBLE CONFIG] Loaded %s (cuts=%u budget=%.2fms tris=%zu "
            "fragments=%u) revision=%u\n",
            mPath.c_str(), mMaxCutsPerEntityPerTick, mCutBudgetMsPerTick,
            mMaxTrianglesPerEntity, mFractureTuning.maxFragmentsPerEvent,
            mRevision);
        return true;
    }
    catch (const std::exception& e)
    {
        mLastWrite = writeTime;
        mHasWriteTime = true;
        Debug::error(Debug::Category::General,
            "[DESTRUCTIBLE CONFIG] Error loading %s: %s. Keeping previous values\n",
            mPath.c_str(), e.what());
        return false;
    }
}

bool DestructibleWorldConfig::pollReload()
{
    const auto writeTime = getLastWrite(mPath);
    if (writeTime == std::filesystem::file_time_type{})
        return false;
    if (mHasWriteTime && writeTime == mLastWrite)
        return false;
    return load(mPath);
}

void DestructibleWorldConfig::reloadNow()
{
    load(mPath);
}

} // namespace MimitaImpact
