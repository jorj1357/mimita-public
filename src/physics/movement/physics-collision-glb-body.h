#pragma once

class Player;
class World;

void doBodyWeaponCollisionPhase(Player& p, const World& world, bool& groundedThisFrame);
