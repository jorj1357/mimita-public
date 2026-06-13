#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "weapon-types.h"
#include "weapon-viewmodel.h"
#include "weapon-godball.h"
#include "weapon-swordsword.h"

class Camera;
class Player;
class NpcSystem;
struct World;

enum class WeaponCrosshairState {
    Ready,
    Delay,
    Reloading
};

class WeaponSystem {
public:
    WeaponSystem();

    void update(const Camera& camera, Player& player, NpcSystem& npcs, float dt);
    void render(const Camera& camera, const Player& player) const;
    RevolverShotResult fire(
        const Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const World& world,
        const std::unordered_map<uint32_t, Player>* remotePlayers = nullptr);

    RevolverShotResult fireAlt(
        const Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const World& world);
    std::vector<RevolverShotResult> collectRemoteGodballHits(
        Player& player,
        const std::unordered_map<uint32_t, Player>& remotePlayers,
        float dt);
    bool reload(Player& player);
    void equip(Player& player, int slot);
    void unequip(Player& player);
    void inspect() const;

    WeaponCrosshairState crosshairState(const Player& player) const;
    bool isReloading(const Player& player) const;
    bool isShooting() const { return mShootingTimer > 0.0f; }

    const std::vector<std::string>& killfeed() const { return mKillfeed; }

    const GodballPhysics& godballPhysics() const { return mGodballPhys; }
    GodballPhysics& godballPhysics() { return mGodballPhys; }

    const WeaponDefinition* getCurrentDef(const Player& player) const;
    const WeaponDefinition* getDefForSlot(int slot) const;

private:
    static constexpr int MAX_SLOTS = 11;
    WeaponViewModel mViewModels[MAX_SLOTS];
    GodballPhysics mGodballPhys;
    SwordswordState mSwordswordState;
    float mShotCooldown = 0.0f;
    float mShootingTimer = 0.0f;
    float mRecoilValue = 0.0f;
    float mDisturbance = 0.0f;

    std::string mCurrentWeaponId;
    int mCurrentSlot = 0;

    std::vector<std::string> mKillfeed;
    std::unordered_map<uint32_t, float> mRemoteGodballCooldowns;
    void addKillLine(const std::string& line);

    RevolverShotResult fireHitscan(
        const Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const World& world,
        const std::unordered_map<uint32_t, Player>* remotePlayers);
    void fireGodball(const Camera& camera, Player& player, NpcSystem& npcs, const World& world);
    void fireSwordsword(const Camera& camera, Player& player, NpcSystem& npcs);

    WeaponRuntime* getCurrentRuntime(Player& player);

    int slotIndex(int slot) const { return std::max(0, std::min(slot, MAX_SLOTS - 1)); }
};
