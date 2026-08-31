// 08 31 2026, 18 10
/* purpose
* Owns JSON-defined community server modes, weapon sets, and host defaults.
* Supplies menu dropdown strings and validated server launch selections.
* Hot-reloads onlinemodes.json and weaponsets.json without a rebuild.
* Does NOT launch servers, render UI, send network packets, or enforce combat.
* Does NOT own the existing duel-only gamemode registry.
* Does NOT store secrets or post Discord messages.
*/

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace MimitaNet {

struct CommunityMode
{
    std::string id;
    std::string name;
    std::string description;
    int teams = 0;
    int scoreLimit = 0;
};

struct CommunityWeaponSet
{
    int id = 1;
    std::string name;
    std::string description;
    std::vector<std::string> weapons;
};

class CommunityServerConfig
{
public:
    static CommunityServerConfig& instance();

    bool load(const std::string& modesPath = "config/onlinemodes.json",
              const std::string& weaponSetsPath = "config/weaponsets.json");
    bool pollReload();

    const std::vector<CommunityMode>& modes() const { return modes_; }
    const std::vector<CommunityWeaponSet>& weaponSets() const { return weaponSets_; }
    const CommunityMode* modeById(const std::string& id) const;
    const CommunityMode* modeByName(const std::string& name) const;
    const CommunityWeaponSet* weaponSetById(int id) const;
    bool weaponAllowed(int setId, const std::string& weaponId) const;

    std::string modeItems() const;
    std::string weaponSetItems() const;
    std::string weaponSetDescription(int id) const;

private:
    CommunityServerConfig() = default;
    bool loadModes(const std::string& path);
    bool loadWeaponSets(const std::string& path);
    std::filesystem::file_time_type writeTime(const std::string& path) const;

    std::string modesPath_ = "config/onlinemodes.json";
    std::string weaponSetsPath_ = "config/weaponsets.json";
    std::filesystem::file_time_type modesWriteTime_{};
    std::filesystem::file_time_type weaponSetsWriteTime_{};
    std::vector<CommunityMode> modes_;
    std::vector<CommunityWeaponSet> weaponSets_;
};

} // namespace MimitaNet
