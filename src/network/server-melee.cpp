#include "network/server.h"
#include "network/network-weapons.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace MimitaNet {

void handleMeleeHitRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(MeleeHitRequestPacket))
        return;
    const MeleeHitRequestPacket* request =
        reinterpret_cast<const MeleeHitRequestPacket*>(buffer);

    auto attackerIt = players.find(request->header.playerId);
    const bool ownsAttacker =
        attackerIt != players.end() &&
        sameAddress(attackerIt->second.addr, from);
    if (!ownsAttacker)
    {
        printf("%s [SWORD SERVER VALIDATE] attackerId=%u targetId=%u "
               "attackSerial=%u accepted=0 reason=sender-address-mismatch\n",
               serverTimestamp(), request->header.playerId,
               request->targetPlayerId, request->attackSerial);
        return;
    }

    ServerPlayer& attacker = attackerIt->second;
    auto targetIt = players.find(request->targetPlayerId);

    const uint8_t equippedWeapon = networkWeaponTypeForSlot(attacker.equippedSlot);
    const bool stateValid =
        !attacker.dead &&
        request->weapon == NETWORK_WEAPON_SWORDSWORD &&
        equippedWeapon == NETWORK_WEAPON_SWORDSWORD;
    const bool serialValid =
        request->attackSerial != 0 &&
        (attacker.lastMeleeAttackSerial == 0 ||
         (int32_t)(request->attackSerial - attacker.lastMeleeAttackSerial) > 0);
    const bool targetValid =
        targetIt != players.end() &&
        !targetIt->second.dead &&
        targetIt->first != attacker.id;
    const glm::vec3 hit(request->hitX, request->hitY, request->hitZ);
    const glm::vec3 normal(request->normalX, request->normalY, request->normalZ);
    const glm::vec3 knock(request->knockX, request->knockY, request->knockZ);
    const bool finite =
        std::isfinite(hit.x) && std::isfinite(hit.y) && std::isfinite(hit.z) &&
        std::isfinite(normal.x) && std::isfinite(normal.y) && std::isfinite(normal.z) &&
        std::isfinite(knock.x) && std::isfinite(knock.y) && std::isfinite(knock.z) &&
        std::isfinite(request->weaponSpeed);
    const float distance = targetValid
        ? glm::length(targetIt->second.pos - attacker.pos)
        : 99999.0f;
    const bool distanceValid = distance <= 6.5f;
    const bool damageValid = request->damage > 0 && request->damage <= 200;
    const bool accepted =
        stateValid && serialValid && targetValid &&
        finite && distanceValid && damageValid;

    printf("%s [SWORD SERVER VALIDATE] attackerId=%u targetId=%u "
           "attackSerial=%u attackerPos=(%.2f,%.2f,%.2f) "
           "targetPos=(%.2f,%.2f,%.2f) distance=%.2f stateValid=%d "
           "cooldownValid=%d accepted=%d reason=%s\n",
           serverTimestamp(), attacker.id, request->targetPlayerId,
           request->attackSerial,
           attacker.pos.x, attacker.pos.y, attacker.pos.z,
           targetValid ? targetIt->second.pos.x : 0.0f,
           targetValid ? targetIt->second.pos.y : 0.0f,
           targetValid ? targetIt->second.pos.z : 0.0f,
           distance, (int)stateValid, (int)serialValid, (int)accepted,
           accepted ? "accepted" :
           !stateValid ? "invalid-state-or-weapon" :
           !serialValid ? "duplicate-or-stale-serial" :
           !targetValid ? "invalid-target" :
           !finite ? "non-finite" :
           !distanceValid ? "distance" : "damage");

    if (!accepted)
        return;

    attacker.lastMeleeAttackSerial = request->attackSerial;
    ServerPlayer& target = targetIt->second;
    ServerDamageResult damage = applyServerDamage(
        players, target, attacker.id, request->damage, knock,
        ServerDamageSource::Melee);

    MeleeHitEventPacket event{};
    event.header.type = PACKET_MELEE_HIT_EVENT;
    event.header.tick = tick;
    event.header.playerId = attacker.id;
    event.attackSerial = request->attackSerial;
    event.attackerPlayerId = attacker.id;
    event.targetPlayerId = target.id;
    event.damage = request->damage;
    event.targetHealth = damage.healthAfter;
    event.weapon = request->weapon;
    event.attackType = request->attackType;
    event.killed = damage.killed ? 1 : 0;
    event.damageConfirmed = damage.applied ? 1 : 0;
    event.hitX = hit.x;
    event.hitY = hit.y;
    event.hitZ = hit.z;
    event.normalX = normal.x;
    event.normalY = normal.y;
    event.normalZ = normal.z;
    event.knockX = knock.x;
    event.knockY = knock.y;
    event.knockZ = knock.z;

    for (const auto& playerEntry : players)
    {
        sendto(sock, (const char*)&event, sizeof(event), 0,
               (sockaddr*)&playerEntry.second.addr,
               sizeof(playerEntry.second.addr));
        ++totalPacketsOut;
    }
}

} // namespace MimitaNet
