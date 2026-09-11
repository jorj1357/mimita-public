#pragma once

#include <glm/glm.hpp>
#include "npc/npc.h"

struct World;
struct WeaponDefinition;

float clamp01(float v);
float difficulty01(float difficulty);
float random01(unsigned int& state);
glm::vec3 randomPlanarDirection(unsigned int& state);

// Situational jump: returns true if NPC should jump (obstacle, stuck)
bool shouldJump(Npc& npc, float d01, const World& world);

// Situational dash: returns true if NPC should dash (engage, escape, dodge)
bool shouldDash(Npc& npc, float d01, float distance, const WeaponDefinition* def, bool targetCanSeeMe);

// Check if the target has line of sight back to the NPC
float targetCanSeeNpc(const Npc& npc, const World& world);

// Get effective range of the NPC's equipped weapon (from WeaponDefinition or default 150)
float weaponEffectiveRange(const Npc& npc);

// Effective range of a weapon definition (same formula as above).
float weaponEffectiveRangeOf(const WeaponDefinition& def);

// Practical selection range for weapon scoring: explicit effectiveRange, else
// projectile travel, else hitscan falloff distance, else melee/near.
float weaponSelectionRangeOf(const WeaponDefinition& def);

// Initialize all weapons in the NPC's loadout (from config) and equip the starting weapon.
// Each loadout weapon gets a WeaponRuntime with full ammo.
void npcInitLoadout(Npc& npc);

// Apply an explicit role-resolved loadout: replaces the NPC's weapon runtimes
// with exactly these weapons, stores the list as the AI switching override, and
// equips startingWeapon (or the first weapon). An empty list clears the
// override and leaves the current loadout untouched.
void npcApplyLoadout(Npc& npc, const std::vector<std::string>& weaponIds,
                     const std::string& startingWeapon);

// Switch the NPC's equipped weapon. Starts background reload on the old weapon.
// Returns true if the switch succeeded.
bool npcSwitchWeapon(Npc& npc, const std::string& weaponId);
