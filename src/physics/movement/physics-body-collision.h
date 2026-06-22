#pragma once

class Player;
class World;

// Check body parts (arms, legs, head, torso) and weapon capsule against world geometry.
// If any part penetrates the world, push Player::pos outward to resolve.
// Call once per frame AFTER animation update (so body part world transforms are current).
void doBodyCollision(
    Player& p,
    const World& world,
    float dt
);
