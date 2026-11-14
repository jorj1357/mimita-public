// 2026-11-14
// Whole-game DLL server behavior entry points.
//
// The resident server loop keeps sockets, ICE, and scheduling. Every gameplay
// step below runs inside the replaceable generation, so editing player, NPC,
// combat, publish, or gamemode behavior no longer requires an EXE rebuild.

#include "game/server-runtime.h"

#include <chrono>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "combat/area-effect.h"
#include "devtools/scenario-server-playback.h"
#include "devtools/scenario-server-recorder.h"
#include "network/network-weapons.h"
#include "network/server-gamemode.h"
#include "network/server-infinite-collision.h"
#include "network/server.h"
#include "debug/structured-log.h"
#include "npc/npc.h"

namespace mimita::game {
namespace {

using Players = std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>;
using Npcs = std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>;
using Projectiles = std::unordered_map<std::uint32_t, MimitaNet::ServerProjectile>;
using AliveIds = std::unordered_set<std::uint32_t>;

using Clock = std::chrono::steady_clock;

std::uint64_t microsecondsSince(Clock::time_point start) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - start).count());
}

Players& playersOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<Players*>(frame.players);
}

Npcs& npcsOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<Npcs*>(frame.npcs);
}

Projectiles& projectilesOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<Projectiles*>(frame.projectiles);
}

NpcSystem& npcSystemOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<NpcSystem*>(frame.npcSystem);
}

MimitaNet::HeadlessWorld& worldOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<MimitaNet::HeadlessWorld*>(frame.world);
}

World& npcWorldOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<World*>(frame.npcWorld);
}

Player& mirrorOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<Player*>(frame.mirrorPlayer);
}

AliveIds& aliveIdsOf(mimita::host::ServerFrameV1& frame) {
    return *static_cast<AliveIds*>(frame.npcIdsAlive);
}

SOCKET socketOf(mimita::host::ServerFrameV1& frame) {
    return static_cast<SOCKET>(frame.socket);
}

std::uint64_t& packetsOutOf(mimita::host::ServerFrameV1& frame) {
    return *frame.totalPacketsOut;
}

}  // namespace

int serverRuntimeProcessPacket(void* rawFrame, const void* rawEvent,
                               mimita::host::ServerPacketResultV1* out) {
    if (!rawFrame || !rawEvent || !out) {
        return 0;
    }
    auto& frame = *static_cast<mimita::host::ServerFrameV1*>(rawFrame);
    const auto& event = *static_cast<const MimitaNet::TransportReceiveEvent*>(rawEvent);
    const auto result = MimitaNet::processServerPacket(
        socketOf(frame), event, playersOf(frame), npcsOf(frame),
        projectilesOf(frame), *frame.nextPlayerId, *frame.nextEntityId,
        *frame.nextProjectileId, worldOf(frame), frame.tick,
        *frame.totalPacketsIn, packetsOutOf(frame),
        static_cast<MimitaNet::ServerPacketStats*>(frame.transportStats),
        static_cast<MimitaNet::DisagreementRetransmitState*>(
            frame.disagreementRetransmit),
        static_cast<MimitaNet::ServerPlayer*>(frame.authenticatedPlayer),
        static_cast<std::unique_ptr<IGameTransport>*>(frame.claimedTransport),
        static_cast<std::vector<std::uint32_t>*>(frame.pendingRemovals),
        frame.npcSystem ? &npcSystemOf(frame) : nullptr);
    out->handled = result.handled ? 1u : 0u;
    out->transportConsumed = result.transportConsumed ? 1u : 0u;
    out->playerId = result.playerId;
    // Per-handler hotness proof (verbose, off by default). The resident loop
    // owns the aggregate `game.transport.path` record.
    if (event.payload && event.payloadBytes >=
                             static_cast<int>(sizeof(MimitaNet::PacketHeader)) &&
        StructuredLogger::instance().shouldLog(StructuredCategory::Network,
                                               StructuredLevel::Verbose)) {
        const auto* header = reinterpret_cast<const MimitaNet::PacketHeader*>(
            event.payload);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Verbose,
            "game.server.handler", "server",
            "inbound packet handler ran in the generation", frame.tick,
            nlohmann::json{
                {"type", static_cast<std::uint32_t>(header->type)},
                {"handled", result.handled},
                {"player", result.playerId},
                {"path", "dll"}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    return out->handled ? 1 : 0;
}

int serverRuntimeSimulatePlayers(void* rawFrame) {
    if (!rawFrame) {
        return 0;
    }
    auto& frame = *static_cast<mimita::host::ServerFrameV1*>(rawFrame);
    const auto start = Clock::now();
    auto& players = playersOf(frame);
    auto& npcs = npcsOf(frame);
    const SOCKET sock = socketOf(frame);

    MimitaNet::handleClientTimeout(players, sock, frame.tick,
                                   packetsOutOf(frame));
    for (auto& entry : players) {
        entry.second.shotsThisTick = 0;
        entry.second.attackPktsThisTick = 0;
    }
    for (auto& entry : players) {
        if (entry.second.connectionStale) {
            continue;
        }
        MimitaNet::simulatePlayer(entry.second, worldOf(frame));
        MimitaNet::updateServerBroadcastInterp(entry.second, frame.tick);
        if (entry.second.justRespawned) {
            entry.second.justRespawned = false;
            MimitaNet::completeAuthoritativeSpawn(sock, entry.second, false);
        }
    }
    MimitaNet::tickWeaponRuntimes(players, frame.tick);
    MimitaNet::checkVoidDeath(players, npcs);
    frame.simulate_players_us = microsecondsSince(start);
    return 1;
}

int serverRuntimeSimulateWorld(void* rawFrame) {
    if (!rawFrame) {
        return 0;
    }
    auto& frame = *static_cast<mimita::host::ServerFrameV1*>(rawFrame);
    const auto start = Clock::now();
    auto& players = playersOf(frame);
    auto& npcs = npcsOf(frame);
    auto& projectiles = projectilesOf(frame);
    auto& npcSystem = npcSystemOf(frame);
    auto& npcWorld = npcWorldOf(frame);
    const SOCKET sock = socketOf(frame);
    std::uint64_t& packetsOut = packetsOutOf(frame);

    ScenarioServerPlayback::instance().tick(
        frame.tick, players, npcSystem);
    {
        std::vector<glm::vec3> anchors;
        anchors.reserve(players.size());
        for (const auto& entry : players) {
            if (!entry.second.connectionStale && !entry.second.dead) {
                anchors.push_back(entry.second.pos);
            }
        }
        for (const Npc& npc : npcSystem.all()) {
            if (!npc.body.dead) {
                anchors.push_back(npc.body.pos);
            }
        }
        serverInfiniteCollisionTick(
            anchors, npcWorld,
            GamemodeRegistry::instance()
                .get(MimitaNet::serverGamemodeState().matchMode)
                .worldStreaming.enabled);
    }
    MimitaNet::simulateSharedNpcs(
        sock, players, npcs, npcSystem, worldOf(frame), npcWorld,
        mirrorOf(frame), aliveIdsOf(frame), projectiles,
        *frame.nextProjectileId, frame.tick, packetsOut);
    for (auto& entry : players) {
        if (!entry.second.connectionStale) {
            MimitaNet::pushPositionHistory(entry.second, frame.tick);
        }
    }
    ScenarioServerRecorder::instance().captureTick(
        players, npcs, npcSystem, projectiles, worldOf(frame), frame.tick);
    MimitaNet::tickServerProjectiles(sock, players, npcs, projectiles,
                                     worldOf(frame), MimitaNet::SERVER_DT,
                                     frame.tick, packetsOut);
    if ((frame.flags & mimita::host::kServerFrameHazardScratch) != 0) {
        std::vector<AreaEffect> hazards;
        for (const auto& entry : projectiles) {
            const MimitaNet::ServerProjectile& projectile = entry.second;
            if (projectile.exploded) {
                continue;
            }
            const std::string* id =
                MimitaNet::weaponIdForDefNetworkId(projectile.weaponDefNetworkId);
            if (!id || *id != "frag") {
                continue;
            }
            AreaEffect hazard;
            hazard.kind = AreaEffectKind::Danger;
            hazard.position = projectile.position;
            hazard.radius = projectile.splashRadius;
            hazard.height = 0.0f;
            hazard.durationTicks = 30;
            hazard.alive = true;
            hazards.push_back(hazard);
        }
        npcSystem.setProjectileHazards(hazards);
    }
    MimitaNet::tickServerMeleeActions(sock, players, npcs, frame.tick,
                                      packetsOut);
    MimitaNet::tickServerPhysicalContactWeapons(
        sock, players, npcs, worldOf(frame), MimitaNet::SERVER_DT, frame.tick,
        packetsOut);
    MimitaNet::flushServerWorldImpacts(
        worldOf(frame), npcWorld, MimitaNet::serverGamemodeState().mapId,
        frame.tick);
    frame.simulate_world_us = microsecondsSince(start);
    return 1;
}

int serverRuntimePublish(void* rawFrame) {
    if (!rawFrame) {
        return 0;
    }
    auto& frame = *static_cast<mimita::host::ServerFrameV1*>(rawFrame);
    const auto start = Clock::now();
    auto& players = playersOf(frame);
    auto& npcs = npcsOf(frame);
    auto& npcWorld = npcWorldOf(frame);
    const SOCKET sock = socketOf(frame);
    std::uint64_t& packetsOut = packetsOutOf(frame);
    auto& vehicleState = *static_cast<MimitaNet::ServerVehicleState*>(
        frame.vehicleState);

    MimitaNet::serverTickVehicles(vehicleState, players, &npcWorld,
                                  *frame.nextEntityId, frame.tick);
    MimitaNet::buildAndSendSnapshot(sock, players, npcs, frame.tick,
                                    packetsOut);
    MimitaNet::serverReplicatePhysicalEntities(
        sock, players, frame.tick, packetsOut,
        *static_cast<MimitaNet::PhysicalEntityReplicationState*>(
            frame.physicalEntityReplication));
    MimitaNet::serverReplicateVehicles(sock, vehicleState, players, frame.tick,
                                       packetsOut);
    MimitaNet::tickDisagreementRetransmit(
        sock, players,
        *static_cast<MimitaNet::DisagreementRetransmitState*>(
            frame.disagreementRetransmit),
        packetsOut);
    MimitaNet::tickReliableGameplayEvents(sock, players, packetsOut);
    frame.publish_us = microsecondsSince(start);
    return 1;
}

int serverRuntimeGamemode(void* rawFrame) {
    if (!rawFrame) {
        return 0;
    }
    auto& frame = *static_cast<mimita::host::ServerFrameV1*>(rawFrame);
    const auto start = Clock::now();
    auto& players = playersOf(frame);
    auto& npcs = npcsOf(frame);
    auto& npcWorld = npcWorldOf(frame);
    const SOCKET sock = socketOf(frame);
    std::uint64_t& packetsOut = packetsOutOf(frame);

    MimitaNet::serverGamemodeTick(
        sock, players, worldOf(frame), npcWorld, npcs, npcSystemOf(frame),
        aliveIdsOf(frame), frame.tick, packetsOut);
    const std::string& destructionMode =
        MimitaNet::serverGamemodeState().matchMode;
    const Gamemode& destructionGamemode =
        GamemodeRegistry::instance().get(destructionMode);
    const std::string& resetPolicy =
        destructionGamemode.features.destructibleWorldReset;
    const bool resetsWorldDestruction =
        resetPolicy == "match" || resetPolicy == "round" ||
        (resetPolicy.empty() &&
         (destructionMode == "tdm" || destructionMode == "juggernaut" ||
          destructionMode == "jugger_horde"));
    if (resetsWorldDestruction &&
        worldOf(frame).lastDestructionMatchId !=
            MimitaNet::serverGamemodeState().duelId) {
        MimitaNet::resetServerWorldDestruction(worldOf(frame), npcWorld,
                                               frame.tick);
        worldOf(frame).lastDestructionMatchId =
            MimitaNet::serverGamemodeState().duelId;
    }
    MimitaNet::tickServerProgression(sock, players, true, packetsOut);
    frame.gamemode_us = microsecondsSince(start);
    return 1;
}

}  // namespace mimita::game
