// 2026-09-30
/* purpose
* Implement server-side replication of destructible PhysicalEntity objects.
* Edges against the authoritative entity list become reliable gameplay events
  * (spawn / cut / despawn); transforms go out as a small unreliable per-tick
* state broadcast. No triangle data is ever sent: clients rebuild the same
* canonical base (box half extents or a GLB path) and apply the same ordered
* cut events, so destruction stays deterministic and cheap.
* Does NOT own the authoritative simulation; PhysicalEntitySystem still does.
*/

#include "network/server.h"

#include <cstdio>
#include <cstring>

#include <glm/glm.hpp>

#include "network/packets.h"
#include "physics/physical-entity.h"

namespace MimitaNet {
namespace {

bool entityIsReplicable(const PhysicalEntity& e)
{
    return !e.serverDriven && e.destructible.enabled && e.networkId != 0;
}

void copyPath(char* out, size_t capacity, const std::string& value)
{
    if (capacity == 0)
        return;
    std::memset(out, 0, capacity);
    std::strncpy(out, value.c_str(), capacity - 1);
}

void fillSpawnPacket(const PhysicalEntity& e,
                     PhysicalEntitySpawnEventPacket& pkt, uint32_t tick)
{
    pkt.header.type = PACKET_PHYSICAL_ENTITY_SPAWN;
    pkt.header.tick = tick;
    pkt.networkId = e.networkId;
    pkt.motion = (uint8_t)e.motion;
    pkt.materialId = e.destructible.materialId != 0 ? e.destructible.materialId
                                                    : e.materialId;
    pkt.halfExtents[0] = e.halfExtents.x;
    pkt.halfExtents[1] = e.halfExtents.y;
    pkt.halfExtents[2] = e.halfExtents.z;

    const glm::vec3 position(e.transform[3]);
    pkt.position[0] = position.x;
    pkt.position[1] = position.y;
    pkt.position[2] = position.z;
    pkt.orientation[0] = e.orientation.w;
    pkt.orientation[1] = e.orientation.x;
    pkt.orientation[2] = e.orientation.y;
    pkt.orientation[3] = e.orientation.z;
    pkt.velocity[0] = e.velocity.x;
    pkt.velocity[1] = e.velocity.y;
    pkt.velocity[2] = e.velocity.z;
    pkt.angularVelocity[0] = e.angularVelocity.x;
    pkt.angularVelocity[1] = e.angularVelocity.y;
    pkt.angularVelocity[2] = e.angularVelocity.z;
    pkt.density = e.density;

    // "generated:" marks a procedural base; anything else is an authored GLB.
    const bool isGlb = !e.modelPath.empty() &&
                       e.modelPath.rfind("generated:", 0) != 0;
    pkt.sourceKind = isGlb ? PHYSICAL_ENTITY_SOURCE_GLB
                           : PHYSICAL_ENTITY_SOURCE_BOX;
    if (isGlb)
        copyPath(pkt.modelPath, sizeof(pkt.modelPath), e.modelPath);
    copyPath(pkt.texturePath, sizeof(pkt.texturePath), e.texturePath);
}

void fillCutPacket(const PhysicalEntity& e, const MimitaImpact::DestructionCut& cut,
                   PhysicalEntityCutEventPacket& pkt, uint32_t tick)
{
    pkt.header.type = PACKET_ENTITY_CUT_EVENT;
    pkt.header.tick = tick;
    pkt.networkId = e.networkId;
    pkt.cutId = (uint32_t)cut.cutId;
    pkt.sourceEntityId = cut.sourceEntityId;
    pkt.predictionKey = cut.predictionKey;
    pkt.cutterType = (uint8_t)cut.cutter.type;
    pkt.materialId = cut.materialId;
    pkt.localCenter[0] = cut.cutter.localCenter.x;
    pkt.localCenter[1] = cut.cutter.localCenter.y;
    pkt.localCenter[2] = cut.cutter.localCenter.z;
    pkt.localDirection[0] = cut.cutter.localDirection.x;
    pkt.localDirection[1] = cut.cutter.localDirection.y;
    pkt.localDirection[2] = cut.cutter.localDirection.z;
    pkt.radius = cut.cutter.radius;
    pkt.length = cut.cutter.length;
    pkt.damage = cut.damage;
    pkt.energy = cut.energy;
}

} // namespace

void serverReplicatePhysicalEntities(SOCKET sock,
                                     std::unordered_map<uint32_t, ServerPlayer>& players,
                                     uint32_t tick, uint64_t& totalPacketsOut,
                                     PhysicalEntityReplicationState& state)
{
    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();

    uint32_t spawnCount = 0;
    uint32_t cutCount = 0;

    for (const PhysicalEntity& e : system.entities())
    {
        if (!entityIsReplicable(e))
            continue;

        auto known = state.broadcastCuts.find(e.networkId);
        if (known == state.broadcastCuts.end())
        {
            PhysicalEntitySpawnEventPacket spawn{};
            fillSpawnPacket(e, spawn, tick);
            const uint32_t eventId = nextReliableGameplayEventId();
            const uint32_t session = serverReliableEventSessionId();
            queueReliableGameplayEventToAll(sock, players, &spawn, sizeof(spawn),
                                            eventId, session, totalPacketsOut);
            known = state.broadcastCuts.emplace(e.networkId, 0u).first;
            ++spawnCount;
        }

        uint32_t& alreadySent = known->second;
        while (alreadySent < e.destructible.cuts.size())
        {
            PhysicalEntityCutEventPacket cut{};
            fillCutPacket(e, e.destructible.cuts[alreadySent], cut, tick);
            const uint32_t eventId = nextReliableGameplayEventId();
            const uint32_t session = serverReliableEventSessionId();
            queueReliableGameplayEventToAll(sock, players, &cut, sizeof(cut),
                                            eventId, session, totalPacketsOut);
            ++alreadySent;
            ++cutCount;
        }
    }

    for (auto it = state.broadcastCuts.begin(); it != state.broadcastCuts.end(); )
    {
        // Server entities use their runtime id as the network id.
        PhysicalEntity* e = system.find(it->first);
        const bool stillHere = e && entityIsReplicable(*e);
        if (stillHere)
        {
            ++it;
            continue;
        }
        PhysicalEntityDespawnEventPacket despawn{};
        despawn.header.type = PACKET_PHYSICAL_ENTITY_DESPAWN;
        despawn.header.tick = tick;
        despawn.networkId = it->first;
        const uint32_t eventId = nextReliableGameplayEventId();
        const uint32_t session = serverReliableEventSessionId();
        queueReliableGameplayEventToAll(sock, players, &despawn, sizeof(despawn),
                                        eventId, session, totalPacketsOut);
        it = state.broadcastCuts.erase(it);
    }

    // Transform broadcast at 30 Hz (every other 60 Hz tick). The reliable
    // events above carry membership; this only keeps visible motion smooth.
    if ((tick & 1u) != 0u)
        return;

    PhysicalEntityStatePacket statePkt{};
    statePkt.header.type = PACKET_PHYSICAL_ENTITY_STATE;
    statePkt.header.tick = tick;
    uint16_t count = 0;
    for (const PhysicalEntity& e : system.entities())
    {
        if (!entityIsReplicable(e))
            continue;
        if (count >= MAX_PHYSICAL_ENTITY_STATE_ENTRIES)
            break;
        PhysicalEntityStateEntry& entry = statePkt.entities[count++];
        entry.networkId = e.networkId;
        const glm::vec3 position(e.transform[3]);
        entry.position[0] = position.x;
        entry.position[1] = position.y;
        entry.position[2] = position.z;
        entry.orientation[0] = e.orientation.w;
        entry.orientation[1] = e.orientation.x;
        entry.orientation[2] = e.orientation.y;
        entry.orientation[3] = e.orientation.z;
        entry.velocity[0] = e.velocity.x;
        entry.velocity[1] = e.velocity.y;
        entry.velocity[2] = e.velocity.z;
        entry.angularVelocity[0] = e.angularVelocity.x;
        entry.angularVelocity[1] = e.angularVelocity.y;
        entry.angularVelocity[2] = e.angularVelocity.z;
    }
    if (count == 0)
        return;
    statePkt.entityCount = count;

    for (auto& kv : players)
    {
        if (!kv.second.spawned)
            continue;
        if (serverSendToPlayer(sock, kv.second, &statePkt, sizeof(statePkt)))
            ++totalPacketsOut;
    }

    if ((spawnCount > 0 || cutCount > 0) && (tick % 60u) == 0u)
    {
        printf("[PHYS REPL] tick=%u spawned=%u cuts=%u tracked=%zu\n",
               tick, spawnCount, cutCount, state.broadcastCuts.size());
    }
}

} // namespace MimitaNet