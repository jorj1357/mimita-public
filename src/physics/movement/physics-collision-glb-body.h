#pragma once

class Player;
class World;

// TODO-DELETE: legacy body+weapon collision phase. Superseded by the
// actor-triangle solver (actor-triangle-solver.h). See the TODO-DELETE comment
// at the definition in physics-collision-glb-body.cpp for removal conditions.
void doBodyWeaponCollisionPhase(Player& p, const World& world, bool& groundedThisFrame);
