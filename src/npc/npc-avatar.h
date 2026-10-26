// 08 22 2026, 12 35
/* purpose
* Declares JSON-driven avatar assignment for NPC lives.
* Keeps an NPC's chosen avatar stable until it respawns.
* Supports one forced avatar or deterministic avatar selection per life.
* Does NOT own player account avatars or replay serialization.
* Does NOT create or modify avatar files.
* Does NOT select network player cosmetics.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Npc;
struct GamemodeNpcAvatarPolicy;

void pollNpcAvatarConfig();
std::string npcAvatarNameForLife(std::uint32_t npcId, std::uint16_t transformEpoch);
bool assignNpcAvatar(Npc& npc);
bool assignNpcAvatarForMode(Npc& npc, const GamemodeNpcAvatarPolicy& policy,
                            const std::string& modeId, const std::string& preferredName,
                            const std::vector<std::string>& roleAllowed,
                            const std::vector<std::string>& roleDenied);
