// 09 13 2026
/* purpose
* Implements the server-authoritative generic constraint lifecycle.
* Does NOT simulate ragdoll physics.
*/
#include "network/server-constraints.h"

#include <algorithm>
#include <cstring>

#include "debug/debug-log.h"
#include "network/constraint-codec.h"
#include "network/packets.h"
#include "physics/constraints/constraint-store.h"

namespace MimitaNet {

namespace {

constexpr std::int32_t kMaxLimbIndex = 16;

bool ownerOf(const ConstraintCreateRequestPacket& pkt, const ServerPlayer* auth,
             const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    if (pkt.ownerActorId == 0 || pkt.ownerActorId != pkt.header.playerId)
        return false;
    if (auth && auth->id != pkt.ownerActorId)
        return false;
    return players.find(pkt.ownerActorId) != players.end();
}

void broadcastCreate(SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
                     const Physics::ConstraintComponent& component, uint64_t& totalPacketsOut)
{
    ConstraintCreatePacket pkt{};
    pkt.header.type = PACKET_CONSTRAINT_CREATE;
    encodeConstraint(component, pkt.constraint);
    queueReliableGameplayEventToAll(sock, players, &pkt, sizeof(pkt),
                                    nextReliableGameplayEventId(), 0, totalPacketsOut);
}

void broadcastRelease(SOCKET sock, std::unordered_map<uint32_t, ServerPlayer>& players,
                      uint32_t serial, uint32_t releaseTick, uint8_t reason,
                      uint64_t& totalPacketsOut)
{
    ConstraintReleasePacket pkt{};
    pkt.header.type = PACKET_CONSTRAINT_RELEASE;
    pkt.constraintSerial = serial;
    pkt.releaseTick = releaseTick;
    pkt.reason = reason;
    queueReliableGameplayEventToAll(sock, players, &pkt, sizeof(pkt),
                                    nextReliableGameplayEventId(), 0, totalPacketsOut);
}

} // namespace

void handleConstraintCreateRequest(SOCKET sock, const char* buffer, int bytes,
                                   std::unordered_map<uint32_t, ServerPlayer>& players,
                                   uint32_t tick, const ServerPlayer* authenticatedPlayer,
                                   uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ConstraintCreateRequestPacket))
        return;
    const ConstraintCreateRequestPacket* req =
        reinterpret_cast<const ConstraintCreateRequestPacket*>(buffer);
    if (!ownerOf(*req, authenticatedPlayer, players))
        return;
    if (req->type > (uint8_t)Physics::ConstraintType::Grab)
        return;
    if (req->limbA < 0 || req->limbA > kMaxLimbIndex)
        return;
    if (req->bodyA == 0)
        return;
    if (req->worldTarget == 0 && req->bodyB == 0)
        return;

    std::uint32_t serial = req->requestSerial;
    if (serial == 0 || Physics::ConstraintStore::instance().active(serial) ||
        Physics::ConstraintStore::instance().tombstoned(serial))
        serial = Physics::ConstraintStore::instance().allocateSerial(req->ownerActorId);

    Physics::ConstraintComponent component;
    component.constraint.type = (Physics::ConstraintType)req->type;
    component.constraint.active = true;
    component.constraint.bodyA = req->bodyA;
    component.constraint.limbA = req->limbA;
    component.constraint.bodyB = req->worldTarget ? Physics::kWorldBody : req->bodyB;
    component.constraint.limbB = req->limbB;
    component.constraint.anchorA = glm::vec3(req->anchorA[0], req->anchorA[1], req->anchorA[2]);
    component.constraint.anchorB = glm::vec3(req->anchorB[0], req->anchorB[1], req->anchorB[2]);
    component.constraint.worldPoint = glm::vec3(req->worldPoint[0], req->worldPoint[1],
                                                req->worldPoint[2]);
    component.constraint.strength = req->strength;
    component.constraint.damping = req->damping;
    component.constraint.minDistance = req->minDistance;
    component.constraint.maxDistance = req->maxDistance;
    component.constraintSerial = serial;
    component.ownerActor = req->ownerActorId;
    component.createdTick = req->createdTick ? req->createdTick : tick;

    Physics::ConstraintStore::instance().create(EntityRealm::Server, component);
    broadcastCreate(sock, players, component, totalPacketsOut);

    Debug::log(Debug::Category::Networking,
        "[CONSTRAINT] server create serial=%u owner=%u type=%u limbA=%d bodyB=%u\n",
        serial, component.ownerActor, (unsigned)req->type, req->limbA, req->bodyB);
}

void handleConstraintReleaseRequest(SOCKET sock, const char* buffer, int bytes,
                                    std::unordered_map<uint32_t, ServerPlayer>& players,
                                    uint32_t tick, const ServerPlayer* authenticatedPlayer,
                                    uint64_t& totalPacketsOut)
{
    (void)tick;
    if (bytes < (int)sizeof(ConstraintReleasePacket))
        return;
    const ConstraintReleasePacket* req =
        reinterpret_cast<const ConstraintReleasePacket*>(buffer);
    if (req->constraintSerial == 0)
        return;
    if (authenticatedPlayer && authenticatedPlayer->id != req->header.playerId)
        return;
    const Physics::ConstraintComponent* component =
        Physics::ConstraintStore::instance().component(req->constraintSerial);
    if (!component)
        return;
    if (component->ownerActor != req->header.playerId)
        return;

    if (Physics::ConstraintStore::instance().release(req->constraintSerial, tick, req->reason))
        broadcastRelease(sock, players, req->constraintSerial, tick, req->reason,
                         totalPacketsOut);
}

void sendActiveConstraintSnapshot(SOCKET sock, ServerPlayer& player, uint32_t tick,
                                  uint64_t& totalPacketsOut)
{
    std::vector<std::uint32_t> serials = Physics::ConstraintStore::instance().activeSerials();
    if (serials.empty())
        return;
    ConstraintSnapshotPacket pkt{};
    pkt.header.type = PACKET_CONSTRAINT_SNAPSHOT;
    pkt.serverTick = tick;
    std::uint16_t count = 0;
    for (std::uint32_t serial : serials) {
        if (count >= (std::uint16_t)MAX_ACTIVE_CONSTRAINTS)
            break;
        const Physics::ConstraintComponent* c =
            Physics::ConstraintStore::instance().component(serial);
        if (!c)
            continue;
        encodeConstraint(*c, pkt.constraints[count]);
        ++count;
    }
    pkt.constraintCount = count;
    if (count == 0)
        return;
    queueReliableGameplayEventToPlayer(sock, player, &pkt, sizeof(pkt),
                                       nextReliableGameplayEventId(),
                                       reliableGameplayEventSessionForPlayer(player),
                                       totalPacketsOut);
}

} // namespace MimitaNet
