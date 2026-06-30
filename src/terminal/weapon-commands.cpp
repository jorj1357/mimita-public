#include <cstdio>
#include <string>
#include <vector>
#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "combat/weapon-system.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-data.h"
#include "combat/weapon-config.h"
#include "combat/weapon-registry.h"
#include "network/net_mode.h"
#include "config/player-settings.h"
#include "debug/debug-log.h"
#include "entities/player-animation-config.h"

void registerWeaponCommands()
{
    Terminal::instance().registerCommand({
        "shoot", "Fire weapon", "shoot",
        [](const std::vector<std::string>&) {
            Camera& camera = THE_CAMERA;
            Player& player = THE_PLAYER;
            NpcSystem& npcSystem = THE_NPC_SYSTEM;
            World& world = THE_WORLD;
            WeaponSystem& weapons = THE_WEAPONS;
            MimitaNet::MultiplayerContext& mpContext = MP_CONTEXT;

            const auto* remotePlayers = mpContext.active
                ? &mpContext.remotePlayers
                : nullptr;
            RevolverShotResult shot = weapons.fire(
                camera, player, npcSystem, world, remotePlayers);
            if (!shot.fired) {
                Terminal::instance().addLog("[WEAPON] dry fire or no active weapon");
                return;
            }

            {
                float pitchKick = GetPlayerSettings().weaponRecoilCameraPitch;
                camera.addPunch(pitchKick, 0.0f);
                if (DebugConfig::DEBUG_RECOIL)
                    Debug::log(Debug::Category::General, "[RECOIL] camera punch=%.2f pitch=%.2f\n",
                               pitchKick, camera.punchPitch);
            }

            if (mpContext.active) {
                const glm::vec3 direction = glm::length(shot.end - shot.start) > 0.001f
                    ? glm::normalize(shot.end - shot.start)
                    : camera.front;
                uint16_t effectFlags =
                    MimitaNet::SHOT_EFFECT_MUZZLE |
                    MimitaNet::SHOT_EFFECT_TRACER |
                    MimitaNet::SHOT_EFFECT_SHOOT_SOUND |
                    MimitaNet::SHOT_EFFECT_WEAPON_TRIGGER;
                uint8_t impactType = MimitaNet::SHOT_IMPACT_NONE;
                uint32_t targetId = 0;
                int damage = 0;
                if (shot.targetIsRemotePlayer) {
                    impactType = MimitaNet::SHOT_IMPACT_ENTITY;
                    targetId = shot.targetId;
                    damage = (int)shot.damage;
                    effectFlags |=
                        MimitaNet::SHOT_EFFECT_ENTITY_IMPACT |
                        MimitaNet::SHOT_EFFECT_BLOOD |
                        MimitaNet::SHOT_EFFECT_HIT_SOUND;
                } else if (shot.hitWorld) {
                    impactType = MimitaNet::SHOT_IMPACT_WORLD;
                    effectFlags |=
                        MimitaNet::SHOT_EFFECT_WORLD_IMPACT |
                        MimitaNet::SHOT_EFFECT_DEBRIS |
                        MimitaNet::SHOT_EFFECT_HIT_SOUND;
                }
                uint8_t netWeapon = MimitaNet::NETWORK_WEAPON_REVOLVER;
                const WeaponDefinition* wdef = weapons.getCurrentDef(player);
                if (wdef) {
                    if (wdef->id == "shotgun")
                        netWeapon = MimitaNet::NETWORK_WEAPON_SHOTGUN;
                    else if (wdef->id == "godball")
                        netWeapon = MimitaNet::NETWORK_WEAPON_GODBALL;
                    else if (wdef->id == "swordsword")
                        netWeapon = MimitaNet::NETWORK_WEAPON_SWORDSWORD;
                    else if (wdef->id == "rocket_launcher")
                        netWeapon = MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER;
                }
                MimitaNet::mpSendShotEvent(
                    mpContext, targetId, damage, shot.damage,
                    effectFlags,
                    netWeapon,
                    impactType,
                    shot.start, shot.end, direction, shot.hitNormal,
                    shot.knockbackImpulse);
            }
            Terminal::instance().addLog("[WEAPON] fired");
        }
    });

    Terminal::instance().registerCommand({
        "reload", "Reload weapon", "reload",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            WeaponSystem& weapons = THE_WEAPONS;
            bool loaded = weapons.reload(player);
            if (DebugConfig::DEBUG_INPUT)
                Debug::log(Debug::Category::General, "[INPUT] action=reload command=reload weapon=%s\n",
                           loaded ? "executed" : "ignored");
            Terminal::instance().addLog(loaded ? "[WEAPON] reload complete" : "[WEAPON] reload unavailable");
        }
    });

    Terminal::instance().registerCommand({
        "openinventory", "Toggle inventory", "openinventory",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            player.inventoryOpen = !player.inventoryOpen;
            Terminal::instance().addLog(player.inventoryOpen ? "[INVENTORY] opened" : "[INVENTORY] closed");
        }
    });

    for (int keySlot = 0; keySlot <= 9; ++keySlot) {
        int slot = keySlot == 0 ? 10 : keySlot;
        std::string name = "equipslot" + std::to_string(keySlot);
        Terminal::instance().registerCommand({
            name, "Equip inventory slot " + std::to_string(slot), name,
            [slot](const std::vector<std::string>&) {
                Player& player = THE_PLAYER;
                WeaponSystem& weapons = THE_WEAPONS;
                if (player.equippedSlot == slot && player.hasValidWeapon) {
                    weapons.unequip(player);
                    GetPlayerSettings().equippedSlot = 0;
                    SavePlayerSettings();
                    Terminal::instance().addLog("[INVENTORY] unequipped slot " + std::to_string(slot));
                } else {
                    weapons.equip(player, slot);
                    GetPlayerSettings().equippedSlot = slot;
                    SavePlayerSettings();
                    Terminal::instance().addLog("[INVENTORY] equipped slot " + std::to_string(slot));
                }
            }
        });
    }

    Terminal::instance().registerCommand({
        "weapon_inspect", "Print active weapon module state", "weapon_inspect",
        [](const std::vector<std::string>&) {
            Player& player = THE_PLAYER;
            WeaponSystem& weapons = THE_WEAPONS;
            weapons.inspect();
        }
    });

    Terminal::instance().registerCommand({
        "weapon_config_reload",
        "Reload config/weapons.json",
        "weapon_config_reload",
        [](const std::vector<std::string>&) {
            WeaponConfig::instance().reloadNow();
            WeaponData::registerBuiltinWeapons();
            Terminal::instance().addLog("[WEAPON CONFIG] Reloaded config/weapons.json");
        },
        std::string(),
        CommandCategory::Weapon
    });

    Terminal::instance().registerCommand({
        "weapon_config_inspect",
        "List registered weapon config data",
        "weapon_config_inspect",
        [](const std::vector<std::string>&) {
            const auto& all = WeaponRegistry::instance().all();
            Terminal::instance().addLog("[WEAPON CONFIG] Registered weapons: " + std::to_string(all.size()));
            for (const auto& pair : all) {
                const WeaponDefinition& def = pair.second;
                Terminal::instance().addLog(
                    "  " + def.id + " slot=" + std::to_string(def.slot) +
                    " model=" + def.modelPath +
                    " damage=" + std::to_string((int)def.damage));
            }
        },
        std::string(),
        CommandCategory::Weapon
    });

    Terminal::instance().registerCommand({
        "animation_config_reload",
        "Reload config/animations.json",
        "animation_config_reload",
        [](const std::vector<std::string>&) {
            if (reloadPlayerProceduralConfig()) {
                Terminal::instance().addLog("[ANIMATION CONFIG] Reloaded config/animations.json");
            } else {
                Terminal::instance().addLog("[ANIMATION CONFIG] Reload failed");
            }
        },
        std::string(),
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "animation_config_inspect",
        "List loaded animation config counts",
        "animation_config_inspect",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog(
                "[ANIMATION CONFIG] clips=" + std::to_string(gPlayerProcedural.layers.animations.size()) +
                " weaponPoses=" + std::to_string(gPlayerProcedural.weaponPoses.size()));
            for (const auto& pair : gPlayerProcedural.layers.animations)
                Terminal::instance().addLog("  clip " + pair.first);
        },
        std::string(),
        CommandCategory::Player
    });

}

void registerWeaponDebugCommand()
{
    Terminal::instance().registerCommand({
        "weapon_debug",
        "Toggle weapon debug logging (0=off, 1=on). Prints shotgun stats and pellet directions on each shot.",
        "weapon_debug <0|1>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                WeaponFire::setWeaponDebug(!WeaponFire::weaponDebugEnabled());
            } else {
                WeaponFire::setWeaponDebug(args[0] != "0");
            }
            Terminal::instance().addLog(
                WeaponFire::weaponDebugEnabled()
                ? "[OK] weapon debug enabled"
                : "[OK] weapon debug disabled");
        },
        "2026-06-30",
        CommandCategory::Weapon
    });
}
