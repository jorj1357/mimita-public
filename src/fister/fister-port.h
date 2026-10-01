#pragma once

struct InputState;
struct Player;
struct World;

namespace FisterPort
{
    // F8 toggles the v1 John Fister player mode. The mode is intentionally
    // local-only until its player/NPC authority adapter is connected.
    bool tick(Player& player, const World& world, const InputState& input, float dt);
    bool enabled();
    void reset();
}
