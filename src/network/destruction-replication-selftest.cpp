// 2026-09-30
/* purpose
* Deterministic self-test for destructible physical-entity replication. Proves
* that a client mirror built from spawn + ordered cut events matches the
* server's authoritative cut history, that a re-sent cut is ignored, and that
* despawn removes the mirror. Runs in-process on fixed inputs: it drives the
* real apply functions, not a parallel implementation.
* Does NOT open sockets, start a listen server, or render.
*/

#include "network/destruction-replication-selftest.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "config/material-config.h"
#include "impact/destructible-geometry.h"
#include "impact/impact-system.h"
#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "physics/physical-entity.h"

namespace MimitaNet {
namespace {

uint64_t checksumTriangles(const std::vector<CollisionTriangle>& triangles)
{
    uint64_t sum = 1469598103934665603ull;
    auto mix = [&](const glm::vec3& p) {
        const uint64_t x = (uint64_t)(int64_t)std::lround(p.x * 4096.0f);
        const uint64_t y = (uint64_t)(int64_t)std::lround(p.y * 4096.0f);
        const uint64_t z = (uint64_t)(int64_t)std::lround(p.z * 4096.0f);
        sum = (sum ^ x) * 1099511628211ull;
        sum = (sum ^ y) * 1099511628211ull;
        sum = (sum ^ z) * 1099511628211ull;
    };
    for (const CollisionTriangle& t : triangles)
    {
        mix(t.a);
        mix(t.b);
        mix(t.c);
    }
    return sum;
}

// Builds the exact wire spawn packet the server would send for a box entity.
void fillBoxSpawn(uint32_t networkId, const PhysicalEntity& server,
                  PhysicalEntitySpawnEventPacket& pkt, uint32_t eventId)
{
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.header.type = PACKET_PHYSICAL_ENTITY_SPAWN;
    pkt.eventId = eventId;
    pkt.eventSessionId = 77;
    pkt.networkId = networkId;
    pkt.motion = (uint8_t)server.motion;
    pkt.sourceKind = PHYSICAL_ENTITY_SOURCE_BOX;
    pkt.materialId = server.destructible.materialId;
    pkt.halfExtents[0] = server.halfExtents.x;
    pkt.halfExtents[1] = server.halfExtents.y;
    pkt.halfExtents[2] = server.halfExtents.z;
    const glm::vec3 pos(server.transform[3]);
    pkt.position[0] = pos.x;
    pkt.position[1] = pos.y;
    pkt.position[2] = pos.z;
    pkt.orientation[0] = server.orientation.w;
    pkt.orientation[1] = server.orientation.x;
    pkt.orientation[2] = server.orientation.y;
    pkt.orientation[3] = server.orientation.z;
    pkt.density = server.density;
}

void fillCut(uint32_t networkId, const MimitaImpact::DestructionCut& cut,
             PhysicalEntityCutEventPacket& pkt, uint32_t eventId)
{
    std::memset(&pkt, 0, sizeof(pkt));
    pkt.header.type = PACKET_ENTITY_CUT_EVENT;
    pkt.eventId = eventId;
    pkt.eventSessionId = 77;
    pkt.networkId = networkId;
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

bool destructionReplicationSelfTest(std::string* outSummary)
{
    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    if (MimitaImpact::MaterialConfig::instance().revision() == 0)
        MimitaImpact::MaterialConfig::instance().load();

    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    system.clear();

    constexpr uint32_t kNetworkId = 4242;
    constexpr float kHalf = 2.5f;

    // ── Server: authoritative entity plus two real cuts ──────────────
    std::vector<CollisionTriangle> box;
    buildBoxCollisionTriangles(box, glm::vec3(0.0f), glm::vec3(kHalf));
    const uint32_t serverId = system.add(
        box, glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 1.0f)),
        PhysicalEntityMotion::Dynamic, MimitaImpact::materialIdForName("wood"));
    PhysicalEntity* server = system.find(serverId);
    server->networkId = kNetworkId;
    server->halfExtents = glm::vec3(kHalf);
    server->density = 1.0f;
    MimitaImpact::ImpactSystem::instance().initializeEntity(
        *server, MimitaImpact::materialIdForName("wood"), glm::vec3(kHalf));

    MimitaImpact::DestructionCut cutA;
    cutA.cutter.type = MimitaImpact::BooleanCutterType::Sphere;
    cutA.cutter.localCenter = glm::vec3(0.0f, 0.0f, kHalf);
    cutA.cutter.radius = 0.8f;
    cutA.sourceEntityId = 9;
    cutA.predictionKey = 1001;
    MimitaImpact::DestructibleGeometrySystem::instance().addCut(
        server->destructible, cutA);
    server->localTriangles = server->destructible.collisionTriangles;

    MimitaImpact::DestructionCut cutB;
    cutB.cutter.type = MimitaImpact::BooleanCutterType::Sphere;
    cutB.cutter.localCenter = glm::vec3(1.2f, 0.4f, kHalf);
    cutB.cutter.radius = 0.6f;
    cutB.sourceEntityId = 9;
    cutB.predictionKey = 1002;
    MimitaImpact::DestructibleGeometrySystem::instance().addCut(
        server->destructible, cutB);
    server->localTriangles = server->destructible.collisionTriangles;
    refreshEntityMassProperties(*server);

    const uint64_t serverChecksum = checksumTriangles(server->localTriangles);
    const float serverVolume = server->destructible.remainingVolume;
    const float serverMass = server->mass;

    // ── Client: a fresh context applies the same spawn + cut events ──
    MultiplayerContext ctx;
    ctx.reliableEventSessionId = 77;

    PhysicalEntitySpawnEventPacket spawnPkt{};
    fillBoxSpawn(kNetworkId, *server, spawnPkt, 1);
    mpProcessPhysicalEntitySpawnEventPacket(ctx, &spawnPkt);

    PhysicalEntity* mirror = system.findByNetworkId(kNetworkId);
    check(mirror != nullptr, "client builds a mirror from the spawn event");
    check(mirror && mirror->serverDriven,
          "mirror is marked serverDriven so the tick never simulates it");

    PhysicalEntityCutEventPacket cutPktA{};
    fillCut(kNetworkId, server->destructible.cuts[0], cutPktA, 2);
    mpProcessEntityCutEventPacket(ctx, &cutPktA);
    PhysicalEntityCutEventPacket cutPktB{};
    fillCut(kNetworkId, server->destructible.cuts[1], cutPktB, 3);
    mpProcessEntityCutEventPacket(ctx, &cutPktB);

    mirror = system.findByNetworkId(kNetworkId);
    const bool mirrorMatches =
        mirror && mirror->destructible.cuts.size() == 2 &&
        checksumTriangles(mirror->localTriangles) == serverChecksum;
    check(mirrorMatches, "mirror geometry matches the server cut history");
    check(mirror && std::fabs(mirror->destructible.remainingVolume - serverVolume) < 1e-4f,
          "mirror remaining volume matches the server");
    check(mirror && std::fabs(mirror->mass - serverMass) < 1e-3f,
          "mirror mass matches the server");

    // ── Re-sent reliable cut must be ignored ─────────────────────────
    const size_t cutsBefore = mirror ? mirror->destructible.cuts.size() : 0;
    const uint64_t checksumBefore = mirror ? checksumTriangles(mirror->localTriangles) : 0;
    mpProcessEntityCutEventPacket(ctx, &cutPktA);
    mirror = system.findByNetworkId(kNetworkId);
    check(mirror && mirror->destructible.cuts.size() == cutsBefore &&
              checksumTriangles(mirror->localTriangles) == checksumBefore,
          "a re-sent cut event is ignored (no double apply)");

    // ── State packet drives the visible transform ────────────────────
    PhysicalEntityStatePacket statePkt{};
    statePkt.header.type = PACKET_PHYSICAL_ENTITY_STATE;
    statePkt.entityCount = 1;
    statePkt.entities[0].networkId = kNetworkId;
    statePkt.entities[0].position[0] = 7.0f;
    statePkt.entities[0].position[1] = 0.0f;
    statePkt.entities[0].position[2] = 2.0f;
    statePkt.entities[0].orientation[0] = 1.0f;
    mpProcessPhysicalEntityStatePacket(ctx, &statePkt);
    mirror = system.findByNetworkId(kNetworkId);
    check(mirror && std::fabs(mirror->transform[3].x - 7.0f) < 1e-4f,
          "state packet moves the mirror");

    // ── Despawn removes only the mirror ──────────────────────────────
    PhysicalEntityDespawnEventPacket despawnPkt{};
    despawnPkt.header.type = PACKET_PHYSICAL_ENTITY_DESPAWN;
    despawnPkt.eventSessionId = 77;
    despawnPkt.networkId = kNetworkId;
    mpProcessPhysicalEntityDespawnEventPacket(ctx, &despawnPkt);
    check(system.findByNetworkId(kNetworkId) == nullptr,
          "despawn removes the mirror");
    check(system.find(serverId) != nullptr,
          "despawn leaves the authoritative entity untouched");

    system.clear();

    if (outSummary)
        *outSummary = report;
    return ok;
}

} // namespace MimitaNet