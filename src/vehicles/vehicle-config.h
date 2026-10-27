// 2026-10-27
/* purpose
* Load and hot-reload vehicle templates from config/vehicles/*.json.
* Retains last valid values on parse failure, like other JSON config owners.
* Does NOT spawn vehicles, simulate them, or own rendering.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "vehicles/vehicle-types.h"

class VehicleConfigRegistry {
public:
    static VehicleConfigRegistry& instance();

    bool loadDirectory(const std::string& directory);
    bool pollReload();
    const VehicleSpec* find(const std::string& id) const;
    std::vector<std::string> ids() const;

    uint64_t revision() const { return mRevision; }
    const std::string& directory() const { return mDirectory; }

private:
    VehicleConfigRegistry() = default;

    bool loadFile(const std::string& path, VehicleSpec& out) const;

    std::unordered_map<std::string, VehicleSpec> mSpecs;
    std::unordered_map<std::string, int64_t> mFileTimes;
    std::string mDirectory = "config/vehicles";
    uint64_t mRevision = 0;
};
