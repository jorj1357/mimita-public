// 2026-11-14
// Whole-game DLL server behavior entry points.
//
// The resident server loop keeps sockets, ICE, and scheduling. Every gameplay
// step below runs inside the replaceable generation, so editing player, NPC,
// combat, publish, or gamemode behavior no longer requires an EXE rebuild.

#include "game/server-runtime.h"

#include "game/entity-domain-bridge-module.h"

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
#include "procedural/procedural-world.h"

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

namespace {

void validatePlayerDomains(Players& players, Npcs& npcs, std::uint32_t tick) {
    if ((tick % 300) != 0) {
        return;
    }
    std::size_t checked = 0;
    std::size_t mismatches = 0;
    for (const auto& entry : players) {
        const MimitaNet::ServerPlayer& player = entry.second;
        const auto handle = mimita::host::makeEntityHandle(
            mimita::host::EntityKind::Player, player.id);
        mimita::host::state::player_core::Record core{};
        mimita::host::state::player_session::Record session{};
        mimita::host::state::player_chat::Record chat{};
        mimita::host::state::player_vip::Record vip{};
        mimita::host::state::player_combat::Record combat{};
        mimita::host::state::player_movement::Record movement{};
        if (!mimita::game::readDomain(mimita::host::DomainId::PlayerCore, handle, &core) ||
            !mimita::game::readDomain(mimita::host::DomainId::PlayerSession, handle, &session) ||
            !mimita::game::readDomain(mimita::host::DomainId::PlayerChat, handle, &chat) ||
            !mimita::game::readDomain(mimita::host::DomainId::PlayerVip, handle, &vip) ||
            !mimita::game::readDomain(mimita::host::DomainId::PlayerCombat, handle, &combat) ||
            !mimita::game::readDomain(mimita::host::DomainId::PlayerMovement, handle, &movement)) {
            ++mismatches;
            continue;
        }
        ++checked;
        const bool coreOk =
            core.id == player.id &&
            core.health == player.health &&
            core.max_health == player.maxHealth &&
            core.position[0] == player.pos.x &&
            core.position[1] == player.pos.y &&
            core.position[2] == player.pos.z;
        const bool sessionOk =
            session.spawn_generation == player.spawnGeneration &&
            session.last_heard_ms == player.lastHeardMs;
        const bool chatOk =
            chat.chat_rate_tokens == player.chatRateTokens &&
            chat.last_request_id == player.lastChatRequestId;
        const bool vipOk =
            vip.account_id == player.accountId &&
            vip.vip_account_id == player.vipAccountId;
        const bool combatOk =
            combat.equipped_slot == player.equippedSlot &&
            combat.last_shot_serial == player.lastShotSerial;
        const bool movementOk =
            movement.on_ground == player.onGround &&
            movement.dash_available == player.dashAvailable;
        if (!coreOk || !sessionOk || !chatOk || !vipOk || !combatOk ||
            !movementOk) {
            ++mismatches;
        }
    }
    for (const auto& entry : npcs) {
        const MimitaNet::ServerNpc& npc = entry.second;
        const auto handle = mimita::host::makeEntityHandle(
            mimita::host::EntityKind::Npc, npc.entityId);
        mimita::host::state::npc_core::Record core{};
        mimita::host::state::npc_combat::Record combat{};
        if (!mimita::game::readDomain(mimita::host::DomainId::NpcCore, handle, &core) ||
            !mimita::game::readDomain(mimita::host::DomainId::NpcCombat, handle, &combat)) {
            ++mismatches;
            continue;
        }
        ++checked;
        const bool coreOk =
            core.id == npc.entityId &&
            core.health == npc.health &&
            core.transform_epoch == npc.transformEpoch;
        const bool combatOk =
            combat.attack_timer == npc.lastAttackTime &&
            combat.ai_state == static_cast<std::uint8_t>(npc.aiState);
        if (!coreOk || !combatOk) {
            ++mismatches;
        }
    }
    ::StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        "game.state.domain-parity", "server",
        "all resident state domains parity check", tick,
        {{"players", checked}, {"mismatches", mismatches}},
        __FILE__, __LINE__, __func__);
}

}  // namespace

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
    validatePlayerDomains(players, npcs, frame.tick);
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
    if (frame.procedural_mode && *frame.procedural_mode &&
        frame.procedural_start_queued && !*frame.procedural_start_queued &&
        !MimitaNet::serverGamemodeState().procedural.enabled &&
        !players.empty()) {
        if (MimitaProcedural::proceduralWorldConfig().modes.empty()) {
            MimitaProcedural::loadProceduralWorldConfig();
        }
        const auto* mode = MimitaProcedural::proceduralModeById(
            frame.procedural_mode);
        auto& gamemode = MimitaNet::serverGamemodeState();
        if (mode) {
            const auto firstPlayer = players.begin();
            gamemode.pendingProcedural.start = true;
            gamemode.pendingProcedural.modeId = frame.procedural_mode;
            gamemode.pendingProcedural.seed =
                frame.procedural_seed != 0
                    ? frame.procedural_seed
                    : MimitaProcedural::proceduralWorldConfig().defaultSeed;
            gamemode.pendingProcedural.requesterId = firstPlayer->first;
            std::printf("[PROCEDURAL AUTO START] mode=%s seed=%u requesterId=%u\n",
                        frame.procedural_mode,
                        gamemode.pendingProcedural.seed,
                        firstPlayer->first);
        } else {
            std::printf("[PROCEDURAL AUTO START] rejected unknown mode=%s\n",
                        frame.procedural_mode);
        }
        *frame.procedural_start_queued = 1;
    }
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
    if (frame.set_spawn_override) {
        const auto& overrides = MimitaNet::serverGameOverrides();
        frame.set_spawn_override(
            frame.spawn_override_context,
            overrides.spawnOverrideEnabled ? 1 : 0,
            overrides.spawnOverridePosition.x,
            overrides.spawnOverridePosition.y,
            overrides.spawnOverridePosition.z);
    }
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
