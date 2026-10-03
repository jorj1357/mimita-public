#pragma once

#include <glm/glm.hpp>
#include "npc/npc.h"

struct World;
struct WeaponDefinition;
struct NpcMovementPolicy;

float clamp01(float v);
float difficulty01(float difficulty);
float random01(unsigned int& state);
glm::vec3 randomPlanarDirection(unsigned int& state);
// Randomized first-fire delay for a new NPC life, expressed as fixed 60 Hz
// server ticks in JSON and converted to the shared cooldown's seconds here.
float npcSpawnFireDelaySeconds(Npc& npc);

// The actor-preset movement policy for this NPC, resolved live from
// MatchRoleRegistry by npc.actorPresetId. Returns nullptr when the NPC has no
// preset policy (legacy brain) or the configured flag is unset.
const NpcMovementPolicy* activeMovementPolicy(const Npc& npc);

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
