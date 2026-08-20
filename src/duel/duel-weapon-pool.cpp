// 08 20 2026, 00 00
/* purpose
* Loads and hot-reloads the config-owned duel weapon allowlist and key order.
* Keeps the last valid configuration when a file is missing or malformed.
* Exposes fast queries for equip validation and duel key remapping.
* Does NOT register weapons or decide whether a duel is currently active.
* Does NOT own player inventory, firing, or network authority.
*/
#include "duel/duel-weapon-pool.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include "debug/debug-log.h"
#include "combat/weapon-registry.h"

using json = nlohmann::json;

DuelWeaponPool& DuelWeaponPool::instance() { static DuelWeaponPool pool; return pool; }

void DuelWeaponPool::load(const std::string& path) {
    mPath = path;
    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(path, ec);
    if (!ec) mLastWrite = writeTime;
    std::ifstream file(path);
    if (!file.is_open()) {
        Debug::warn(Debug::Category::Duel, "[DUEL WEAPONS] Missing %s; keeping previous config.\n", path.c_str());
        return;
    }
    try {
        json root; file >> root;
        std::unordered_set<std::string> nextAllowed;
        std::unordered_map<int, int> nextDuelToNative;
        std::unordered_map<int, int> nextNativeToDuel;
        if (!root.contains("weapons") || !root["weapons"].is_array()) throw std::runtime_error("missing weapons array");
        for (const auto& item : root["weapons"]) {
            if (!item.is_object() || !item.contains("id") || !item["id"].is_string()) continue;
            const std::string id = item.value("id", "");
            const bool allowed = item.value("equippable", false);
            const int duelSlot = item.value("duelSlot", 0);
            if (allowed) nextAllowed.insert(id);
            if (allowed && duelSlot > 0) {
                nextDuelToNative[duelSlot] = duelSlot; // replaced below by registry slot
                nextNativeToDuel[duelSlot] = duelSlot;
            }
        }
        // Native slots are assigned by weapon registration; resolve the ids here.
        for (const auto& item : root["weapons"]) {
            if (!item.is_object() || !item.value("equippable", false)) continue;
            const auto* def = WeaponRegistry::instance().get(item.value("id", ""));
            const int duelSlot = item.value("duelSlot", 0);
            if (def && duelSlot > 0) {
                nextDuelToNative[duelSlot] = def->slot;
                nextNativeToDuel[def->slot] = duelSlot;
            }
        }
        if (nextAllowed.empty() || nextDuelToNative.empty()) throw std::runtime_error("no valid duel weapons");
        mEquippable = std::move(nextAllowed); mDuelToNative = std::move(nextDuelToNative); mNativeToDuel = std::move(nextNativeToDuel);
        Debug::log(Debug::Category::Duel, "[DUEL WEAPONS] loaded %zu allowed weapons from %s\n", mEquippable.size(), path.c_str());
    } catch (const std::exception& e) {
        Debug::warn(Debug::Category::Duel, "[DUEL WEAPONS] Parse error in %s: %s; keeping previous config.\n", path.c_str(), e.what());
    }
}

void DuelWeaponPool::pollReload() {
    if (mPath.empty()) return;
    std::error_code ec; const auto t = std::filesystem::last_write_time(mPath, ec);
    if (!ec && t != mLastWrite) load(mPath);
}
bool DuelWeaponPool::isEquippable(const std::string& id) const { return mEquippable.count(id) != 0; }
int DuelWeaponPool::nativeSlotForDuelSlot(int slot) const { auto it = mDuelToNative.find(slot); return it == mDuelToNative.end() ? 0 : it->second; }
int DuelWeaponPool::duelSlotForNativeSlot(int slot) const { auto it = mNativeToDuel.find(slot); return it == mNativeToDuel.end() ? 0 : it->second; }
