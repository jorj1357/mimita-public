#pragma once

class Player;
struct World;

// Resolve body part (limb) collisions against world geometry.
// Called after capsule root collision has resolved.
// Does NOT modify the player's root position (pos/vel).
// Instead, adjusts each body part's worldTransform translation
// so limbs visually stop at walls/floors/ceilings.
void resolveBodyPartCollisions(Player& p, const World& world, float dt);
