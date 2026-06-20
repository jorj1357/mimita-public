#pragma once

class Player;
struct World;

// Resolve body part (limb) collisions against world geometry.
// Called after capsule root collision has resolved.
// Body/weapon-style collider corrections push the authoritative player root.
void resolveBodyPartCollisions(Player& p, const World& world, float dt);
