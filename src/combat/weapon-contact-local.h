#pragma once
#include <cstdint>
class Player;
class NpcSystem;
// Offline adapter: real Player/NPC bodies use the same authoritative contact,
// action, team, and health owners as an online match, without sending packets.
void tickLocalPhysicalContactWeapons(Player& player, NpcSystem& npcs, uint32_t tick);
