// 08 20 2026, 00 00
/* purpose
* Declares the config-owned allowlist and key mapping for duel weapons.
* Supports safe hot reload while the game is running.
* Provides the single query owner used by weapon equipping and input.
* Does NOT own weapon definitions, duel state, or weapon firing.
* Does NOT mutate player inventory or network state.
*/
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

class DuelWeaponPool {
public:
    static DuelWeaponPool& instance();
    void load(const std::string& path);
    void pollReload();
    bool isEquippable(const std::string& weaponId) const;
    int nativeSlotForDuelSlot(int duelSlot) const;
    int duelSlotForNativeSlot(int nativeSlot) const;

private:
    DuelWeaponPool() = default;
    std::unordered_set<std::string> mEquippable;
    std::unordered_map<int, int> mDuelToNative;
    std::unordered_map<int, int> mNativeToDuel;
    std::string mPath;
    std::filesystem::file_time_type mLastWrite{};
};
