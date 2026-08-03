#pragma once

#include <string>
#include <vector>
#include <unordered_map>

extern bool gWeaponCollisionVisualsJson;
extern bool gWeaponCollisionVisualsProbes;

#include "weapon-types.h"
#include "weapon-viewmodel.h"
#include "weapon-godball.h"
#include "weapon-swordsword.h"
#include "weapon-hafs.h"
#include "weapon-rocket-launcher.h"
#include "weapon-grenade-launcher.h"

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

    void update(Camera& camera, Player& player, NpcSystem& npcs, const World& world, float dt);
    void render(const Camera& camera, const Player& player) const;
    RevolverShotResult fire(
        Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const World& world,
        const std::unordered_map<uint32_t, Player>* remotePlayers = nullptr,
        std::unordered_map<uint32_t, Player>* remoteNpcs = nullptr);

    RevolverShotResult fireAlt(
        Camera& camera,
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
    uint8_t networkVisualState(const Player& player) const;
    bool isShooting() const { return mShootingTimer > 0.0f; }

    const std::vector<std::string>& killfeed() const { return mKillfeed; }

    // Render the equipped weapon on a remote player's hand.
    // Uses the same viewmodel mesh and attachment logic as the local player,
    // but does not modify the remote player's arm poses.
    void renderRemoteWeapon(uint32_t entityId, const Player& player, const Camera& camera, float dt);

    const GodballPhysics& godballPhysics() const { return mGodballPhys; }
    GodballPhysics& godballPhysics() { return mGodballPhys; }
    void tagLatestLocalRocket(uint32_t fireSerial);
    bool attachAuthoritativeRocket(uint32_t fireSerial, uint32_t projectileId);
    bool removeAuthoritativeRocket(uint32_t projectileId);
    bool removeLocalRocketByFireSerial(uint32_t fireSerial);

    const WeaponDefinition* getCurrentDef(const Player& player) const;
    const WeaponDefinition* getDefForSlot(int slot) const;

private:
    static constexpr int MAX_SLOTS = 64;
    WeaponViewModel mViewModels[MAX_SLOTS];
    std::unordered_map<std::string, WeaponViewModel> mRemoteViewModels;
    GodballPhysics mGodballPhys;
    SwordswordState mSwordswordState;
    HafsState mHafsState;
    RocketLauncherState mRocketState;
    float mShotCooldown = 0.0f;
    float mShootingTimer = 0.0f;
    float mRecoilValue = 0.0f;
    float mDisturbance = 0.0f;

    std::string mCurrentWeaponId;
    int mCurrentSlot = 0;

    std::vector<std::string> mKillfeed;
    std::unordered_map<uint32_t, float> mRemoteGodballCooldowns;
    void addKillLine(const std::string& line);

    // World-space aim trail ring buffer
    struct TrailPoint { glm::vec3 pos; glm::vec3 normal; int spawnTick; };
    static constexpr int MAX_TRAIL_POINTS = 256;
    TrailPoint mTrailPoints[MAX_TRAIL_POINTS]{};
    int mTrailHead = 0;
    int mTrailCount = 0;
    int mTrailTick = 0;

    RevolverShotResult fireHitscan(
        const Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const World& world,
        const std::unordered_map<uint32_t, Player>* remotePlayers,
        std::unordered_map<uint32_t, Player>* remoteNpcs = nullptr);
    void fireGodball(Camera& camera, Player& player, NpcSystem& npcs, const World& world);
    RevolverShotResult fireSwordsword(
        Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const std::unordered_map<uint32_t, Player>* remotePlayers);
    RevolverShotResult fireRocketLauncher(
        Camera& camera,
        Player& player,
        NpcSystem& npcs,
        const World& world,
        const std::unordered_map<uint32_t, Player>* remotePlayers);

    WeaponRuntime* getCurrentRuntime(Player& player);

    int slotIndex(int slot) const { return std::max(0, std::min(slot, MAX_SLOTS - 1)); }
};
