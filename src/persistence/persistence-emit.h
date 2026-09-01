// 09 01 2026, 00 00
/* purpose
* Provide server-side helpers that emit persistence events to the async queue.
* Called after authoritative kills and match results are confirmed.
* Does NOT block the simulation tick or wait for backend confirmation.
* Does NOT implement reward logic or database schema.
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace MimitaNet {

struct ServerPlayer;

void emitPvPKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t victimPlayerId,
    uint32_t weaponId,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& victimPos);

void emitPvPKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t victimPlayerId,
    const std::string& weaponName,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& victimPos);

void emitNpcKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t npcEntityId,
    uint32_t weaponId,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& npcPos);

void emitNpcKillPersistenceEvent(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t attackerPlayerId,
    uint32_t npcEntityId,
    const std::string& weaponName,
    uint32_t serverTick,
    const glm::vec3& attackerPos,
    const glm::vec3& npcPos);

} // namespace MimitaNet
