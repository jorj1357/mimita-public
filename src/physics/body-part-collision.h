#pragma once

#include <glm/glm.hpp>

#include "physics/physics-types.h"

class Player;
struct World;

// Resolve body part (limb) collisions against world geometry.
// Called after capsule root collision has resolved.
// Body/weapon-style collider corrections push the authoritative player root.
void resolveBodyPartCollisions(Player& p, const World& world, float dt);

// Reuse the body-part capsule collision/root-response path for equipped weapons.
glm::vec3 resolveWeaponCollisionCapsule(
    Player& p,
    const World& world,
    const Capsule& weaponCap,
    const char* weaponName);
