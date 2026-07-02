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
