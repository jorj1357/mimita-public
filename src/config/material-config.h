// 2026-09-28
/* purpose
* Own the universal material table (config/materials.json) used by the impact
* and destructible-geometry systems. Mirrors the existing gameplay-config
* singleton pattern (load + pollReload, malformed JSON keeps the last value).
* Does NOT own object behavior, damage authority, or per-weapon data.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "impact/impact-event.h"

namespace MimitaImpact {

// Stable 32-bit identity for a material name (FNV-1a). 0 is reserved for
// "unknown/default". Using a hash keeps runtime + network ids stable across
// config reloads and file ordering.
uint32_t materialIdForName(const std::string& name);

class MaterialConfig
{
public:
    static MaterialConfig& instance();

    bool load(const std::string& path = "config/materials.json");
    bool pollReload();
    void reloadNow();

    // Returns the definition for a material id, or the fallback definition when
    // the id is unknown. Never returns null.
    const MaterialDefinition& find(uint32_t materialId) const;
    const MaterialDefinition& findByName(const std::string& name) const;
    const MaterialDefinition& fallback() const;

    uint32_t revision() const { return mRevision; }

private:
    MaterialConfig() = default;

    std::unordered_map<uint32_t, MaterialDefinition> mById;
    MaterialDefinition mFallback;
    std::string mPath = "config/materials.json";
    std::filesystem::file_time_type mLastWrite{};
    bool mHasWriteTime = false;
    uint32_t mRevision = 0;
};

} // namespace MimitaImpact
