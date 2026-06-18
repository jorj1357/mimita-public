#pragma once

class Player;
struct World;

// Resolve body part (limb) collisions against world geometry.
//
// Called after capsule root collision has resolved.
// Does NOT modify the player's root position.
// Instead, adjusts each body part's worldTransform translation
// so limbs visually collide with walls/floors/ceilings.
//
// The movement capsule remains the authoritative physical collider.
// Body part correction is purely visual.
void resolveBodyPartCollisions(Player& p, const World& world, float dt);
