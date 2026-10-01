// 2026-10-01
/* purpose
* Own the destruction budgets and fracture tuning loaded from
* config/destructible-world.json. Mirrors the MaterialConfig singleton pattern
* (load + pollReload; malformed JSON keeps the previous valid values).
* Gameplay reads these values; it does NOT hardcode per-cut budgets elsewhere.
* Does NOT own cut sizing, geometry, damage authority, or rendering.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "impact/destructible-geometry.h"

namespace MimitaImpact {

class DestructibleWorldConfig
{
public:
    static DestructibleWorldConfig& instance();

    bool load(const std::string& path = "config/destructible-world.json");
    bool pollReload();
    void reloadNow();

    // Bounded destruction work per fixed tick (impact-system flush).
    uint32_t maxCutsPerEntityPerTick() const { return mMaxCutsPerEntityPerTick; }
    float cutBudgetMsPerTick() const { return mCutBudgetMsPerTick; }

    // Safety cap on generated triangles per entity.
    size_t maxTrianglesPerEntity() const { return mMaxTrianglesPerEntity; }

    // Fracture trigger + budget.
    const FractureTuning& fractureTuning() const { return mFractureTuning; }

    uint32_t revision() const { return mRevision; }

private:
    DestructibleWorldConfig() = default;

    uint32_t mMaxCutsPerEntityPerTick = 8;
    float mCutBudgetMsPerTick = 2.0f;
    size_t mMaxTrianglesPerEntity = 120000;
    FractureTuning mFractureTuning;
    std::string mPath = "config/destructible-world.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mHasWriteTime = false;
    uint32_t mRevision = 0;
};

} // namespace MimitaImpact
