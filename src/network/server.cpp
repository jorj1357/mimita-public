// 07 19 2026, 11 05
/* purpose
* Owns authoritative server startup, fixed-step scheduling, and tick orchestration.
* Routes packets into server subsystems and reports server tick health diagnostics.
* Keeps gameplay simulation at the shared fixed 60 Hz server delta.
* Does NOT render, implement client prediction, or create local-only gameplay rules.
* Does NOT own weapon definitions, projectile physics internals, or packet schemas.
* Does NOT change simulation delta to compensate for load.
*/

#include "network/server.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-system.h"
#include "network/net_mode.h"
#include "network/server-gamemode.h"
#include "network/server-infinite-collision.h"
#include "procedural/procedural-world.h"
#include "gamemode/arena-ball.h"
#include "gamemode/gamemode.h"
#include "gamemode/map-config.h"
#include "gamemode/match-roles.h"
#include "gamemode/monster-pools.h"
#include "gamemode/mode-pack-registry.h"
#include "npc/npc-behavior.h"
#include "gamemode/gamemode-map-pool.h"
#include "network/community-server-config.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "network/network-weapons.h"
#include "network/ice-transport.h"
#include "void-death/void-death.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "combat/actor-preset-weapons.h"
#include "npc/npc.h"
#include "npc/npc-difficulty-config.h"
#include "entities/player.h"
#include "world/world.h"
#include "map/map-catalog.h"
#include "map/map-authored-geometry.h"
#include "config/networking-config.h"
#include "config/movement-config.h"
#include "config/collision-config.h"
#include "config/spawn-velocity-config.h"
#include "physics/runtime-objects.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "debug/crash-handler.h"
#include "hot-reload/module-host.h"
#include "host/host-runtime.h"
#include "host/host-server-entity-adapter.h"
#include "host/host-server-transport-adapter.h"
#include "host/host-server-world-adapter.h"
#include "host/host-server-session-events.h"
#include "host/host-session-identity.h"
#include "host/abi/game-server-abi.h"
#include "host/abi/transport-capability.h"
#include "host/generated/packet-registry-v1.h"
#include "devtools/scenario-server-playback.h"
#include "devtools/scenario-server-recorder.h"
#include "audio/audio.h"
#include "persistence/persistence-queue.h"
#include "auth/auth-system.h"
#include "debug/structured-log.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <random>
#include <system_error>
#include <thread>
#include <windows.h>

// Global-scope accessor into the client GUI's listen-server state (gui-main.cpp).
namespace MimitaNet { struct ListenServerState; }
MimitaNet::ListenServerState* getListenServerState();

namespace MimitaNet {

namespace {

const char* residentServerTimestamp() {
    static thread_local char buffer[32];
    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::snprintf(buffer, sizeof(buffer), "[%02u:%02u:%02u.%03u]",
                  time.wHour, time.wMinute, time.wSecond,
                  time.wMilliseconds);
    return buffer;
}

TransportConnectionId residentMakeUdpConnectionId(const sockaddr_in& endpoint) {
    mimita::host::ServerConnectionIdV1 id{};
    if (mimita::host::HostRuntime::instance().serverPolicyMakeUdpConnectionId(
            endpoint.sin_addr.s_addr, endpoint.sin_port, &id)) {
        return {static_cast<TransportKind>(id.kind), id.value};
    }
    return {TransportKind::Udp, 0};
}

void setSpawnOverrideThunk(void*, std::int32_t enabled, float x, float y,
                           float z) {
    mimita::host::HostServerWorldAdapter::instance().setSpawnOverride(
        enabled != 0, x, y, z);
}

struct HotnessState {
    std::atomic<std::uint64_t> dllCount{0};
    std::atomic<std::uint64_t> residentCount{0};
    std::atomic<std::uint64_t> violationCount{0};
    std::atomic<std::uint64_t> serverCalls{0};
    std::atomic<std::uint64_t> serverFailures{0};
    std::atomic<std::uint64_t> lastSummaryMs{0};
    std::array<std::atomic<std::uint64_t>, 256> lastTypeLogMs{};

    HotnessState() {
        for (auto& value : lastTypeLogMs) {
            value.store(0, std::memory_order_relaxed);
        }
    }
};

HotnessState& hotnessState() {
    static HotnessState state;
    return state;
}

bool isMigratedPacketType(std::uint8_t type) {
    return mimita::host::packet_registry::clientPath(type) ==
           mimita::host::packet_registry::Path::Dll;
}

void noteHotnessPath(bool dll, std::uint8_t type, std::uint32_t player,
                     std::uint32_t tick) {
    HotnessState& state = hotnessState();
    (dll ? state.dllCount : state.residentCount)
        .fetch_add(1, std::memory_order_relaxed);
    const bool migrated = isMigratedPacketType(type);
    const bool violation =
        !dll && migrated &&
        mimita::host::HostRuntime::instance().probeEnabled();
    if (violation) {
        state.violationCount.fetch_add(1, std::memory_order_relaxed);
    }
    const std::uint64_t now = nowMs();
    auto& last = state.lastTypeLogMs[type];
    if (now - last.load(std::memory_order_relaxed) >= 1000) {
        last.store(now, std::memory_order_relaxed);
        ::StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "game.transport.path", std::to_string(player),
            dll ? "packet handled by the game generation"
                : "packet handled by the resident fallback",
            tick,
            {{"type", type},
             {"player", player},
             {"path", dll ? "dll" : "resident"},
             {"migrated", migrated},
             {"generation",
              mimita::host::HostRuntime::instance().activeGeneration()}},
            __FILE__, __LINE__, __func__);
    }
    if (violation) {
        ::StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "game.hotness.violation", std::to_string(player),
            "migrated packet fell back to the resident handler",
            tick,
            {{"kind", "resident-fallback"},
             {"type", type},
             {"player", player},
             {"generation",
              mimita::host::HostRuntime::instance().activeGeneration()}},
            __FILE__, __LINE__, __func__);
    }
}

void noteServerCall(bool ok) {
    HotnessState& state = hotnessState();
    (ok ? state.serverCalls : state.serverFailures)
        .fetch_add(1, std::memory_order_relaxed);
}

void emitHotnessSummary(std::uint32_t tick, std::size_t playerCount,
                        std::size_t npcCount) {
    HotnessState& state = hotnessState();
    const std::uint64_t now = nowMs();
    const std::uint64_t last =
        state.lastSummaryMs.load(std::memory_order_relaxed);
    if (now - last < 1000) {
        return;
    }
    state.lastSummaryMs.store(now, std::memory_order_relaxed);
    ::StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "game.hotness.summary", "", "DLL vs resident packet handling", tick,
        {{"dll", state.dllCount.load(std::memory_order_relaxed)},
         {"resident", state.residentCount.load(std::memory_order_relaxed)},
         {"violations", state.violationCount.load(std::memory_order_relaxed)},
         {"probe", mimita::host::HostRuntime::instance().probeEnabled()},
         {"server_path",
          mimita::host::HostRuntime::instance().serverApiAvailable()
              ? "dll"
              : "resident"},
         {"server_calls", state.serverCalls.load(std::memory_order_relaxed)},
         {"server_failures",
          state.serverFailures.load(std::memory_order_relaxed)},
         {"players", playerCount},
         {"npcs", npcCount},
         {"runtime_revision",
          mimita::host::HostRuntime::instance().activeServerApi()
              ? mimita::host::HostRuntime::instance().activeServerApi()
                    ->runtimeRevision
              : 0},
         {"generation",
          mimita::host::HostRuntime::instance().activeGeneration()}},
        __FILE__, __LINE__, __func__);
}

static bool deliverInboundToProbe(
    const TransportReceiveEvent& event,
    uint32_t serverTick,
    mimita::host::TransportDispatchV1* dispatch) {
    auto& runtime = mimita::host::HostRuntime::instance();
    if (!dispatch || !runtime.probeEnabled() || !event.payload ||
        event.payloadBytes <= 0) {
        return false;
    }

    *dispatch = mimita::host::TransportDispatchV1{};
    dispatch->structSize =
        static_cast<std::uint32_t>(sizeof(mimita::host::TransportDispatchV1));
    dispatch->version = mimita::host::kTransportCapabilityVersion;
    dispatch->serverTick = serverTick;
    dispatch->message.structSize =
        static_cast<std::uint32_t>(sizeof(mimita::host::TransportMessageV1));
    dispatch->message.version = mimita::host::kTransportCapabilityVersion;
    dispatch->message.session = 0;
    dispatch->message.peer = event.connectionId.value;
    dispatch->message.typeId = 0;
    dispatch->message.payload = event.payload;
    dispatch->message.payloadSize = static_cast<std::uint32_t>(event.payloadBytes);
    dispatch->message.receivedAtMs = event.receivedAtMs;
    dispatch->message.serverTick = serverTick;

    if (dispatch->message.payloadSize >= sizeof(PacketHeader)) {
        const auto* header =
            reinterpret_cast<const PacketHeader*>(dispatch->message.payload);
        dispatch->message.typeId = static_cast<std::uint32_t>(header->type);
    }
    return runtime.dispatchInbound(dispatch);
}

// Mirrors the resident liveness rule for a packet the game generation handled:
// only a source-owned, non-HELLO player is refreshed.
static void noteHandledInbound(
    const TransportReceiveEvent& event,
    std::unordered_map<uint32_t, ServerPlayer>& players) {
    if (event.payloadBytes < static_cast<int>(sizeof(PacketHeader))) {
        return;
    }
    const auto* header =
        reinterpret_cast<const PacketHeader*>(event.payload);
    if (header->type == PACKET_HELLO) {
        return;
    }
    auto it = players.find(header->playerId);
    if (it == players.end()) {
        return;
    }
    const bool ownsSource = it->second.hasConnectionId
        ? it->second.connectionId == event.connectionId
        : (it->second.addr.sin_addr.s_addr ==
               event.remoteEndpoint.sin_addr.s_addr &&
           it->second.addr.sin_port == event.remoteEndpoint.sin_port);
    if (ownsSource) {
        it->second.lastHeardMs = nowMs();
    }
    it->second.addr = event.remoteEndpoint;
}

static bool deliverInboundToGame(const TransportReceiveEvent& event,
                                 uint32_t serverTick) {
    auto& runtime = mimita::host::HostRuntime::instance();
    if (!runtime.probeEnabled() || !event.payload || event.payloadBytes <= 0) {
        return false;
    }
    mimita::host::TransportMessageV1 message{};
    message.structSize =
        static_cast<std::uint32_t>(sizeof(mimita::host::TransportMessageV1));
    message.version = mimita::host::kTransportCapabilityVersion;
    message.session = 0;
    message.peer = event.connectionId.value;
    message.typeId = 0;
    message.payload = event.payload;
    message.payloadSize = static_cast<std::uint32_t>(event.payloadBytes);
    message.receivedAtMs = event.receivedAtMs;
    message.serverTick = serverTick;
    if (message.payloadSize >= sizeof(PacketHeader)) {
        const auto* header =
            reinterpret_cast<const PacketHeader*>(message.payload);
        message.typeId = static_cast<std::uint32_t>(header->type);
    }
    return runtime.deliverInbound(message);
}

static void sendProbeResponse(
    SOCKET sock, const sockaddr_in& from,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    const mimita::host::TransportDispatchV1& dispatch) {
    if (dispatch.response.payloadSize == 0) {
        return;
    }
    bool usedTransport = false;
    bool sent = false;
    uint32_t playerId = 0;
    if (dispatch.message.payloadSize >= sizeof(PacketHeader)) {
        const auto* header =
            reinterpret_cast<const PacketHeader*>(dispatch.message.payload);
        playerId = header->playerId;
        auto it = players.find(playerId);
        if (it != players.end() && it->second.transport) {
            usedTransport = true;
            sent = it->second.transport->send(
                dispatch.response.payload, dispatch.response.payloadSize);
        }
    }
    if (!sent) {
        const int result = sendto(
            sock, reinterpret_cast<const char*>(dispatch.response.payload),
            static_cast<int>(dispatch.response.payloadSize), 0,
            reinterpret_cast<const sockaddr*>(&from), sizeof(from));
        sent = result == static_cast<int>(dispatch.response.payloadSize);
    }
    uint32_t headerMagic = 0;
    uint32_t headerVersion = 0;
    uint32_t headerType = 0;
    uint32_t headerTick = 0;
    uint32_t headerPlayer = 0;
    if (dispatch.response.payloadSize >= sizeof(PacketHeader)) {
        const auto* header =
            reinterpret_cast<const PacketHeader*>(dispatch.response.payload);
        headerMagic = header->magic;
        headerVersion = header->version;
        headerType = header->type;
        headerTick = header->tick;
        headerPlayer = header->playerId;
    }
    if (dispatch.response.typeId == PACKET_PING) {
        static uint64_t lastPingResponseLogMs = 0;
        const uint64_t now = nowMs();
        if (now - lastPingResponseLogMs < 5000) {
            return;
        }
        lastPingResponseLogMs = now;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "probe.transport.response-sent", "probe-response",
        "resident server sent the game-owned response payload",
        dispatch.serverTick,
        {{"bytes", dispatch.response.payloadSize},
         {"type", dispatch.response.typeId},
         {"sent", sent},
         {"transport", usedTransport},
         {"player_id", playerId},
         {"magic", headerMagic},
         {"version", headerVersion},
         {"header_type", headerType},
         {"header_tick", headerTick},
         {"header_player", headerPlayer}},
        __FILE__, __LINE__, __func__);
}

// Reused by the authoritative tick loops; the NPC system keeps its own copy
// with retained capacity, so rebuilding the projectile hazard view allocates
// only when the live projectile peak grows.
thread_local std::vector<AreaEffect> projectileHazardScratch;

struct ServerLoopPerf
{
    std::array<double, 240> loopMs{};
    uint32_t sampleCount = 0;
    uint32_t sampleWrite = 0;
    uint64_t totalLoopUs = 0;
    double maxLoopMs = 0.0;
    uint64_t overrunCount = 0;
    uint64_t cappedCatchupCount = 0;
};

struct ServerTransportStats
{
    uint64_t recvAttempts = 0;
    uint64_t recvWouldBlock = 0;
    uint64_t recvErrors = 0;
    uint64_t malformedPackets = 0;
    uint64_t protocolMismatches = 0;
    uint64_t unknownPacketTypes = 0;
    uint64_t helloPackets = 0;
    uint64_t joinPackets = 0;
    uint64_t reconnectPackets = 0;
    uint64_t inputPackets = 0;
};

struct ServerTickSample
{
    uint32_t tick = 0;
    uint64_t totalUs = 0;
    uint64_t npcUs = 0;
    uint64_t snapshotUs = 0;
    uint64_t gamemodeUs = 0;
    uint64_t projectileUs = 0;
    uint64_t contactWeaponUs = 0;
};

struct ServerTickTiming
{
    uint64_t tickCount = 0;
    uint64_t totalUs = 0;
    uint64_t maxTotalUs = 0;
    uint64_t npcUs = 0;
    uint64_t maxNpcUs = 0;
    uint64_t snapshotUs = 0;
    uint64_t maxSnapshotUs = 0;
    uint64_t gamemodeUs = 0;
    uint64_t maxGamemodeUs = 0;
    uint64_t projectileUs = 0;
    uint64_t maxProjectileUs = 0;
    uint64_t contactWeaponUs = 0;
    uint64_t maxContactWeaponUs = 0;
    std::array<uint64_t, 600> totalSamplesUs{};
    std::array<ServerTickSample, 600> recentSamples{};
    uint32_t sampleCount = 0;
    uint32_t sampleWrite = 0;
    uint64_t over100MsCount = 0;
};

uint64_t elapsedMicroseconds(const std::chrono::steady_clock::time_point& start)
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count());
}

void reportServerTickTiming(ServerTickTiming& timing, uint32_t tick,
                            size_t players, size_t npcs,
                            const std::string& map, const std::string& mode,
                            uint64_t packetsIn, uint64_t packetsOut)
{
    if (timing.tickCount == 0)
        return;
    const auto avgMs = [](uint64_t us, uint64_t count) {
        return count == 0 ? 0.0 : static_cast<double>(us) / count / 1000.0;
    };
    const uint32_t sampleCount = std::min<uint32_t>(
        timing.sampleCount,
        static_cast<uint32_t>(timing.totalSamplesUs.size()));
    std::array<uint64_t, 600> sorted = timing.totalSamplesUs;
    std::sort(sorted.begin(), sorted.begin() + sampleCount);
    const auto percentileMs = [&](double percentile) {
        if (sampleCount == 0)
            return 0.0;
        const size_t index = static_cast<size_t>(std::ceil(
            percentile * static_cast<double>(sampleCount - 1)));
        return static_cast<double>(sorted[index]) / 1000.0;
    };
    std::array<ServerTickSample, 600> slowestTicks = timing.recentSamples;
    std::sort(slowestTicks.begin(), slowestTicks.begin() + sampleCount,
        [](const ServerTickSample& a, const ServerTickSample& b) {
            return a.totalUs > b.totalUs;
        });
    nlohmann::json slowTickRows = nlohmann::json::array();
    const uint32_t slowTickCount = std::min<uint32_t>(12, sampleCount);
    for (uint32_t i = 0; i < slowTickCount; ++i) {
        const auto& sample = slowestTicks[i];
        slowTickRows.push_back({
            {"tick", sample.tick},
            {"total_ms", static_cast<double>(sample.totalUs) / 1000.0},
            {"npc_ms", static_cast<double>(sample.npcUs) / 1000.0},
            {"snapshot_ms", static_cast<double>(sample.snapshotUs) / 1000.0},
            {"gamemode_ms", static_cast<double>(sample.gamemodeUs) / 1000.0},
            {"projectile_ms", static_cast<double>(sample.projectileUs) / 1000.0},
            {"contact_weapon_ms", static_cast<double>(sample.contactWeaponUs) / 1000.0}});
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Performance, StructuredLevel::Important,
        "performance.server-tick-window", "server", "fixed-tick-stage-timing",
        tick,
        nlohmann::json{
            {"tick_count", timing.tickCount},
            {"sample_count", sampleCount},
            {"players", players},
            {"npcs", npcs},
            {"map", map}, {"mode", mode},
            {"packets_in", packetsIn}, {"packets_out", packetsOut},
            {"slowest_ticks", slowTickRows},
            {"total_avg_ms", avgMs(timing.totalUs, timing.tickCount)},
            {"total_max_ms", static_cast<double>(timing.maxTotalUs) / 1000.0},
            {"total_p50_ms", percentileMs(0.50)},
            {"total_p95_ms", percentileMs(0.95)},
            {"total_p99_ms", percentileMs(0.99)},
            {"over_100ms_count", timing.over100MsCount},
            {"over_100ms_percent", timing.tickCount == 0 ? 0.0 :
                static_cast<double>(timing.over100MsCount) * 100.0 /
                    static_cast<double>(timing.tickCount)},
            {"npc_avg_ms", avgMs(timing.npcUs, timing.tickCount)},
            {"npc_max_ms", static_cast<double>(timing.maxNpcUs) / 1000.0},
            {"snapshot_avg_ms", avgMs(timing.snapshotUs, timing.tickCount)},
            {"snapshot_max_ms", static_cast<double>(timing.maxSnapshotUs) / 1000.0},
            {"gamemode_avg_ms", avgMs(timing.gamemodeUs, timing.tickCount)},
             {"gamemode_max_ms", static_cast<double>(timing.maxGamemodeUs) / 1000.0},
             {"projectile_avg_ms", avgMs(timing.projectileUs, timing.tickCount)},
             {"projectile_max_ms", static_cast<double>(timing.maxProjectileUs) / 1000.0},
             {"contact_weapon_avg_ms", avgMs(timing.contactWeaponUs, timing.tickCount)},
             {"contact_weapon_max_ms", static_cast<double>(timing.maxContactWeaponUs) / 1000.0}},
        __FILE__, __LINE__, __FUNCTION__);
    timing = ServerTickTiming{};
}

std::string processPath()
{
    char path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, path, (DWORD)sizeof(path));
    if (length == 0 || length >= sizeof(path))
        return "(unknown)";
    return std::string(path, path + length);
}

std::string currentDirectory()
{
    std::error_code error;
    std::filesystem::path path = std::filesystem::current_path(error);
    if (error)
        return "(unknown)";
    return path.string();
}

sockaddr_in actualSocketAddress(SOCKET sock, const sockaddr_in& fallback)
{
    sockaddr_in actual = fallback;
    int actualLen = sizeof(actual);
    if (getsockname(sock, (sockaddr*)&actual, &actualLen) != 0)
        actual = fallback;
    return actual;
}

bool resolveServerBindAddress(const LaunchOptions& options,
                              sockaddr_in& bindAddr,
                              std::string& requested)
{
    if (options.bindExplicit)
        requested = options.bind;
    else if (options.connectExplicit)
        requested = options.connect;
    else
        requested = "0.0.0.0:" + std::to_string(DEFAULT_PORT);
    return parseAddress(requested, bindAddr, true);
}

void countPacketType(ServerTransportStats& stats, uint8_t type)
{
    if (type == PACKET_HELLO)
        ++stats.helloPackets;
    else if (type == PACKET_JOIN_REQUEST)
        ++stats.joinPackets;
    else if (type == PACKET_RECONNECT_REQUEST)
        ++stats.reconnectPackets;
    else if (type == PACKET_INPUT)
        ++stats.inputPackets;
}

void recordServerLoopPerf(ServerLoopPerf& perf, uint64_t loopUs, bool cappedCatchup)
{
    const double loopMs = (double)loopUs / 1000.0;
    perf.loopMs[perf.sampleWrite % perf.loopMs.size()] = loopMs;
    ++perf.sampleWrite;
    if (perf.sampleCount < perf.loopMs.size())
        ++perf.sampleCount;
    perf.totalLoopUs += loopUs;
    perf.maxLoopMs = std::max(perf.maxLoopMs, loopMs);
    if (loopUs > 16667)
        ++perf.overrunCount;
    if (cappedCatchup)
        ++perf.cappedCatchupCount;
}

double serverLoopP95Ms(const ServerLoopPerf& perf)
{
    if (perf.sampleCount == 0)
        return 0.0;
    std::array<double, 240> copy = perf.loopMs;
    std::sort(copy.begin(), copy.begin() + perf.sampleCount);
    size_t idx = (size_t)std::ceil((double)perf.sampleCount * 0.95) - 1;
    if (idx >= perf.sampleCount)
        idx = perf.sampleCount - 1;
    return copy[idx];
}

void reportServerPerf(const char* label,
                      ServerLoopPerf& perf,
                      uint32_t tick,
                      uint32_t previousTick,
                      uint64_t elapsedMs)
{
    mimita::host::ServerProjectilePerfV1 projectile{};
    mimita::host::HostRuntime::instance().serverPolicyProjectilePerf(&projectile);
    const double elapsedSec = std::max(0.001, (double)elapsedMs / 1000.0);
    const double hz = (double)(tick - previousTick) / elapsedSec;
    const double avgLoopMs = perf.sampleCount > 0
        ? (double)perf.totalLoopUs / (double)perf.sampleCount / 1000.0
        : 0.0;
    const double p95LoopMs = serverLoopP95Ms(perf);
    const double projectileSimMs = (double)projectile.projectileSimUs / 1000.0;

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Performance, StructuredLevel::Important,
        "performance.server-loop-window", "server", "outer-loop timing and projectile workload",
        tick,
        nlohmann::json{
            {"tick_count", tick - previousTick},
            {"elapsed_ms", elapsedMs},
            {"actual_hz", hz},
            {"loop_avg_ms", avgLoopMs},
            {"loop_p95_ms", p95LoopMs},
            {"loop_max_ms", perf.maxLoopMs},
            {"overruns", perf.overrunCount},
            {"capped_catchup", perf.cappedCatchupCount},
            {"active_projectiles", projectile.activeProjectiles},
            {"moving_projectiles", projectile.movingProjectiles},
            {"sleeping_projectiles", projectile.sleepingProjectiles},
            {"projectile_sim_ms", projectileSimMs},
            {"triangle_queries", projectile.triangleQueryCount},
            {"triangle_candidates", projectile.triangleCandidateTotal},
            {"triangle_candidate_max", projectile.triangleCandidateMax},
            {"player_capsule_candidates", projectile.playerCapsuleCandidateTotal},
            {"player_capsule_candidate_max", projectile.playerCapsuleCandidateMax},
            {"projectile_corrections", projectile.correctionPackets},
            {"correction_bytes", projectile.correctionBytes}},
        __FILE__, __LINE__, __FUNCTION__);

    if (perf.maxLoopMs >= 50.0 || perf.cappedCatchupCount > 0)
        recordCrashBreadcrumb("server-loop",
            "tick=%u hz=%.1f loopAvg=%.1f loopP95=%.1f loopMax=%.1f overruns=%llu capped=%llu projectileMs=%.1f activeProjectiles=%u",
            tick, hz, avgLoopMs, p95LoopMs, perf.maxLoopMs,
            (unsigned long long)perf.overrunCount,
            (unsigned long long)perf.cappedCatchupCount,
            projectileSimMs, projectile.activeProjectiles);

    printf("%s [SERVER PERF] hz=%.1f loopAvg=%.3fms loopP95=%.3fms loopMax=%.3fms "
           "overruns=%llu cappedCatchup=%llu activeProjectiles=%u moving=%u sleeping=%u "
           "projectileSim=%.3fms triQueries=%llu triCandidates=%llu triMax=%u "
           "playerCapsuleCandidates=%llu playerCapsuleMax=%u projectileCorrections=%llu correctionBytes=%llu\n",
           label, hz, avgLoopMs, p95LoopMs, perf.maxLoopMs,
           (unsigned long long)perf.overrunCount,
           (unsigned long long)perf.cappedCatchupCount,
           projectile.activeProjectiles, projectile.movingProjectiles,
           projectile.sleepingProjectiles, projectileSimMs,
           (unsigned long long)projectile.triangleQueryCount,
           (unsigned long long)projectile.triangleCandidateTotal,
           projectile.triangleCandidateMax,
           (unsigned long long)projectile.playerCapsuleCandidateTotal,
           projectile.playerCapsuleCandidateMax,
           (unsigned long long)projectile.correctionPackets,
           (unsigned long long)projectile.correctionBytes);

    perf = ServerLoopPerf{};
}

} // namespace

// Forward declaration for background listen server thread
static void listenServerThreadFunc(ListenServerState& state);

namespace {

ServerGameOverrides gServerOverrides;

} // namespace

std::string gServerHostPlayerName;
uint32_t gServerHostPlayerId = 0;

uint32_t serverHostPlayerId()
{
    return gServerHostPlayerId;
}

void setServerHostPlayerId(uint32_t playerId)
{
    gServerHostPlayerId = playerId;
}

ServerGameOverrides& serverGameOverrides()
{
    return gServerOverrides;
}

bool isServerHost()
{
    // Dedicated server process.
    {
        const char* cmd = GetCommandLineA();
        if (cmd && (strstr(cmd, "--server") || strstr(cmd, "-server")))
            return true;
    }
    // Listen server active in this process (hosting via the in-game menu).
    return hostedRoomSession().active;
}

int runServer(const LaunchOptions& options)
{
    // A new server process/session gets exactly one host owner: its first
    // accepted player connection. Reconnects keep that player ID.
    gServerHostPlayerId = 0;
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Disable audio on dedicated server — it only wastes resources
    // Audio disable is generation-owned server policy startup.

    ::StructuredLogger::instance().init();
    mimita::host::HostRuntime::instance().startFromEnvironment();
    if (!mimita::host::HostRuntime::instance().serverApiAvailable()) {
        ::StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "game.server.hot-required", "server",
            "server refused to start without the hot server generation", 0,
            {{"reason", "no-generation"}},
            __FILE__, __LINE__, __func__);
        return 1;
    }
    if (!mimita::host::HostRuntime::instance().serverPolicyAvailable()) {
        ::StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "game.server.policy-required", "server",
            "server refused to start without the hot server policy export", 0,
            {{"reason", "missing-server-policy"}},
            __FILE__, __LINE__, __func__);
        return 1;
    }
    auto logStartupStage = [&](const char* stage, const char* result,
                               const char* reason, double elapsedMs = 0.0,
                               nlohmann::json extra = nlohmann::json::object()) {
        extra["stage"] = stage;
        extra["result"] = result;
        extra["elapsed_ms"] = elapsedMs;
        extra["pid"] = static_cast<unsigned long long>(GetCurrentProcessId());
        extra["map"] = options.mapName;
        extra["mode"] = options.gameMode;
        extra["events_jsonl"] = ::StructuredLogger::instance().eventsPath();
        ::StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "server.startup.stage", "server-startup", reason, 0, extra,
            __FILE__, __LINE__, __func__);
    };
    logStartupStage("logger_init", "complete", "structured logger initialized");
    auto startupStageStart = std::chrono::steady_clock::now();
    logStartupStage("server_modules", "started", "loading server modules");
    ModuleHost::instance().startupServerModules();
    logStartupStage("server_modules", "complete", "server modules loaded",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startupStageStart).count());
    auto logServerExit = [&](int exitCode, const char* reason,
                             const std::string& roomCode, uint32_t serverTick) {
        ::StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "server.process.exited", roomCode,
            reason ? reason : "unspecified server exit", serverTick,
            nlohmann::json{{"pid", static_cast<unsigned long long>(GetCurrentProcessId())},
                           {"exit_code", exitCode},
                           {"exit_reason", reason ? reason : "unspecified"},
                           {"room_code", roomCode},
                           {"mode", options.gameMode}, {"map", options.mapName},
                           {"events_jsonl", ::StructuredLogger::instance().eventsPath()},
                           {"run_id", ::StructuredLogger::instance().runId()}},
            __FILE__, __LINE__, __func__);
    };
    ::StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.process.started", "server",
        "dedicated server entered runServer", 0,
        nlohmann::json{{"pid", static_cast<unsigned long long>(GetCurrentProcessId())},
                       {"mode", options.gameMode}, {"map", options.mapName},
                       {"timeout_seconds", options.timeoutSecs},
                       {"events_jsonl", ::StructuredLogger::instance().eventsPath()},
                       {"run_id", ::StructuredLogger::instance().runId()}},
        __FILE__, __LINE__, __func__);
    recordCrashBreadcrumb("server-start",
        "pid=%lu mode=%s map=%s run=%s events=%s",
        (unsigned long)GetCurrentProcessId(), options.gameMode.c_str(),
        options.mapName.c_str(), ::StructuredLogger::instance().runId().c_str(),
        ::StructuredLogger::instance().eventsPath().c_str());

    mimita::host::ServerStartupV1 startupRequest{};
    startupRequest.mapName = options.mapName.c_str();
    startupRequest.gameMode = options.gameMode.c_str();
    startupRequest.gamemodeId = options.gamemodeId.c_str();
    startupRequest.serverName = options.name.c_str();
    startupRequest.hostPlayerName = options.hostPlayerName.c_str();
    startupRequest.password = options.password.c_str();
    startupRequest.sessionToken = options.sessionToken.c_str();
    startupRequest.proceduralMode = options.proceduralMode.c_str();
    startupRequest.proceduralSeed = options.proceduralSeed;
    startupRequest.npcCount = options.npcCount;
    startupRequest.npcsEnabled = options.npcsEnabled ? 1u : 0u;
    startupRequest.weaponSetId = options.weaponSetId;
    startupRequest.weaponSetExplicit = options.weaponSetExplicit ? 1u : 0u;
    startupRequest.autoMapRotation = options.autoMapRotation ? 1u : 0u;
    startupRequest.mapRotationMinutes = options.mapRotationMinutes;
    startupRequest.discordNotification = options.discordNotification ? 1u : 0u;
    startupRequest.passwordProtected = options.passwordProtected ? 1u : 0u;
    startupRequest.maxPlayers = options.maxPlayers;
    startupRequest.duel = options.duel ? 1u : 0u;
    startupRequest.spawnOverrideEnabled = gServerOverrides.spawnOverrideEnabled ? 1u : 0u;
    startupRequest.spawnOverride[0] = gServerOverrides.spawnOverridePosition.x;
    startupRequest.spawnOverride[1] = gServerOverrides.spawnOverridePosition.y;
    startupRequest.spawnOverride[2] = gServerOverrides.spawnOverridePosition.z;

    startupStageStart = std::chrono::steady_clock::now();
    logStartupStage("configuration_load", "started",
                    "loading server mode and actor configuration");
    mimita::host::ServerStartupResultV1 startupResult{};
    const bool startupOk =
        mimita::host::HostRuntime::instance().serverPolicyStartup(
            &startupRequest, &startupResult);
    if (!startupOk || startupResult.ok == 0) {
        logStartupStage("configuration_load", "failed",
                        startupResult.error[0] ? startupResult.error
                                               : "server policy startup failed");
        ::StructuredLogger::instance().shutdown();
        return 1;
    }
    std::string mapName = startupResult.resolvedMapName;
    std::string mapPath = startupResult.resolvedMapPath;
    const std::uint32_t resolvedMaxPlayers = startupResult.resolvedMaxPlayers;
    mimita::host::setSessionMapId(mapName);
    logStartupStage("configuration_load", "complete",
        "server mode and actor configuration loaded",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startupStageStart).count(),
        nlohmann::json{{"requested_map", options.mapName},
                       {"resolved_map", mapName},
                       {"requested_mode", options.gameMode}});
    printf("%s [SERVER MAP] mapId=%s path=%s\n", residentServerTimestamp(),
           mapName.c_str(), mapPath.c_str());

    HeadlessWorld world;
    World npcWorld;
    startupStageStart = std::chrono::steady_clock::now();
    logStartupStage("world_load", "started", "loading map collision world", 0.0,
                    nlohmann::json{{"resolved_map_path", mapPath}});
    mimita::host::ServerWorldLoadV1 worldRequest{};
    worldRequest.mapName = mapName.c_str();
    worldRequest.mapPath = mapPath.c_str();
    worldRequest.gameMode = options.gameMode.c_str();
    worldRequest.headlessWorld = &world;
    worldRequest.npcWorld = &npcWorld;
    mimita::host::ServerWorldLoadResultV1 worldResult{};
    const bool worldOk =
        mimita::host::HostRuntime::instance().serverPolicyWorldLoad(
            &worldRequest, &worldResult);
    if (!worldOk || worldResult.ok == 0) {
        logStartupStage("world_load", "failed",
                        worldResult.error[0] ? worldResult.error
                                             : "server policy world load failed");
        ::StructuredLogger::instance().shutdown();
        return 1;
    }
    logStartupStage("world_load", worldResult.mapLoaded ? "complete" : "fallback",
        worldResult.mapLoaded ? "map collision world loaded"
                              : "map load used floor fallback",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startupStageStart).count(),
        nlohmann::json{{"resolved_map_path", mapPath},
                       {"triangle_count", worldResult.triangleCount},
                       {"spawn_count", worldResult.spawnCount},
                       {"npc_collision_triangles",
                        worldResult.npcCollisionTriangles}});

    struct PolicyPlayerGuard {
        void* pointer = nullptr;
        ~PolicyPlayerGuard() {
            if (pointer) {
                mimita::host::HostRuntime::instance()
                    .serverPolicyDestroyPlayer(pointer);
            }
        }
    } mirrorPlayerGuard{
        mimita::host::HostRuntime::instance().serverPolicyCreatePlayer()};
    void* mirrorPlayer = mirrorPlayerGuard.pointer;
    if (!mirrorPlayer) {
        logStartupStage("player_mirror", "failed",
                        "server policy could not create the NPC mirror player");
        ::StructuredLogger::instance().shutdown();
        return 1;
    }
    NpcSystem npcSystem;
    std::unordered_set<uint32_t> npcIdsAlive;

    if (!netStartup())
    {
        printf("%s [SERVER] FATAL: WSAStartup failed\n", residentServerTimestamp());
        logServerExit(1, "winsock_startup_failed", "", 0);
        ::StructuredLogger::instance().shutdown();
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("%s [SERVER] FATAL: socket() failed error=%d\n", residentServerTimestamp(), WSAGetLastError());
        logServerExit(1, "socket_creation_failed", "", 0);
        netShutdown();
        ::StructuredLogger::instance().shutdown();
        return 1;
    }
    disableUdpConnReset(sock);

    BOOL exclusiveAddressUse = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (const char*)&exclusiveAddressUse,
                   sizeof(exclusiveAddressUse)) == SOCKET_ERROR)
    {
        printf("%s [SERVER] FATAL: setsockopt SO_EXCLUSIVEADDRUSE failed error=%d\n",
               residentServerTimestamp(), WSAGetLastError());
        logServerExit(1, "exclusive_bind_policy_failed", "", 0);
        closesocket(sock);
        netShutdown();
        ::StructuredLogger::instance().shutdown();
        return 1;
    }

    const bool nonBlockingOk = setNonBlocking(sock);

    sockaddr_in bindAddr{};
    std::string requestedBind;
    if (!resolveServerBindAddress(options, bindAddr, requestedBind))
    {
        printf("%s [SERVER] FATAL: invalid bind address requested=%s family=AF_INET\n",
               residentServerTimestamp(), requestedBind.c_str());
        logServerExit(1, "invalid_bind_address", "", 0);
        closesocket(sock);
        netShutdown();
        ::StructuredLogger::instance().shutdown();
        return 1;
    }
    startupStageStart = std::chrono::steady_clock::now();
    logStartupStage("network_bind", "started", "binding UDP socket",
                    0.0, nlohmann::json{{"requested_bind", requestedBind}});
    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        printf("%s [SERVER] FATAL: bind() failed error=%d\n", residentServerTimestamp(), err);
        if (err == WSAEADDRINUSE)
            printf("%s [SERVER] HINT: Address %s is already in use. Is another server already running?\n",
                   residentServerTimestamp(), addressToString(bindAddr).c_str());
        logServerExit(1, "socket_bind_failed", "", 0);
        closesocket(sock);
        netShutdown();
        ::StructuredLogger::instance().shutdown();
        return 1;
    }

    sockaddr_in actualBindAddr = actualSocketAddress(sock, bindAddr);
    const uint16_t actualPort = ntohs(actualBindAddr.sin_port);
    logStartupStage("network_bind", "complete", "UDP socket bound",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startupStageStart).count(),
        nlohmann::json{{"actual_bind", addressToString(actualBindAddr)},
                       {"port", actualPort}});
    printf("%s [SERVER TRANSPORT READY] protocol=%u role=dedicated-udp pid=%lu exe=\"%s\" cwd=\"%s\" "
           "requested=%s actual=%s family=AF_INET reuse=1 nonblocking=%d\n",
           residentServerTimestamp(), PROTOCOL_VERSION, (unsigned long)GetCurrentProcessId(),
           processPath().c_str(), currentDirectory().c_str(),
           requestedBind.c_str(), addressToString(actualBindAddr).c_str(),
           (int)nonBlockingOk);
    printf("%s [SERVER] bound to %s\n", residentServerTimestamp(), addressToString(actualBindAddr).c_str());
    printf("%s [SERVER] waiting for connections...\n", residentServerTimestamp());

    std::unordered_map<uint32_t, ServerPlayer> players;
    std::unordered_map<uint32_t, ServerNpc> npcs;
    std::unordered_map<uint32_t, ServerProjectile> projectiles;
    if (mimita::host::HostRuntime::instance().probeEnabled()) {
        auto& entityAdapter = mimita::host::HostServerEntityAdapter::instance();
        entityAdapter.bindPlayers(&players);
        entityAdapter.bindNpcs(&npcs);
        mimita::host::HostRuntime::instance().setEntityAdapter(
            entityAdapter.adapter(), entityAdapter.owner());

        auto& worldAdapter = mimita::host::HostServerWorldAdapter::instance();
        worldAdapter.bind(&world);
        mimita::host::HostRuntime::instance().setWorldAdapter(
            worldAdapter.adapter(), worldAdapter.owner());

        auto& transportAdapter =
            mimita::host::HostServerTransportAdapter::instance();
        transportAdapter.bindPlayers(&players);
        transportAdapter.setSender(
            [sock, &players](std::uint32_t playerId, const void* data,
                             std::size_t size) {
                auto it = players.find(playerId);
                if (it == players.end() || !data || size == 0) {
                    return false;
                }
                if (it->second.addr.sin_port != 0) {
                    return sendto(sock, static_cast<const char*>(data),
                                  static_cast<int>(size), 0,
                                  reinterpret_cast<const sockaddr*>(&it->second.addr),
                                  sizeof(it->second.addr)) == static_cast<int>(size);
                }
                if (it->second.transport) {
                    return it->second.transport->send(data, size);
                }
                return false;
            });
        transportAdapter.setEndpointSender(
            [sock](std::uint32_t addressNetworkOrder,
                   std::uint16_t portNetworkOrder, const void* data,
                   std::size_t size) {
                sockaddr_in target{};
                target.sin_family = AF_INET;
                target.sin_addr.s_addr = addressNetworkOrder;
                target.sin_port = portNetworkOrder;
                return sendto(sock, static_cast<const char*>(data),
                              static_cast<int>(size), 0,
                              reinterpret_cast<const sockaddr*>(&target),
                              sizeof(target)) == static_cast<int>(size);
            });
        mimita::host::HostRuntime::instance().setTransportAdapter(
            transportAdapter.adapter(), transportAdapter.owner());
        mimita::host::HostServerSessionEvents::instance().bindPlayers(&players);
    }
    uint32_t nextPlayerId = 1;
    uint32_t nextEntityId = 1000;
    uint32_t nextProjectileId = 1;
    if (mimita::host::HostRuntime::instance().probeEnabled()) {
        mimita::host::HostServerEntityAdapter::instance().bindNextIds(
            &nextPlayerId, &nextEntityId);
    }
    uint32_t tick = 0;
    uint64_t lastLog = nowMs();
    uint64_t totalPacketsIn = 0;
    uint64_t totalPacketsOut = 0;
    ServerPacketStats transportStats;
    DisagreementRetransmitState disagreementRetransmit;
    PhysicalEntityReplicationState physicalEntityReplication;
    ServerVehicleState dedicatedVehicleState;

    // A dev-loop/CLI procedural mode becomes active for the first real client
    // instead of starting before a player exists. This lets the existing
    // procedural command owner perform the normal authoritative teleport.
    std::int32_t automaticProceduralStartQueued = 0;

    // ── Dedicated server ICE support ──
    ListenServerState dedicatedIceState;
    dedicatedIceState.serverName = options.name.empty() ? "MiMITA Server" : options.name;
    dedicatedIceState.mapName = mapName;
    dedicatedIceState.gameMode = options.gameMode;
    dedicatedIceState.maxPlayers = resolvedMaxPlayers;
    dedicatedIceState.weaponSetId = options.weaponSetId;
    dedicatedIceState.autoMapRotation = options.autoMapRotation;
    dedicatedIceState.mapRotationMinutes = options.mapRotationMinutes;
    dedicatedIceState.discordNotification = options.discordNotification;
    dedicatedIceState.passwordProtected = options.passwordProtected;
    dedicatedIceState.password = options.password;
    dedicatedIceState.hostPlayerName = options.hostPlayerName;
    // The host is identified by name so the person who launched the server can
    // run host commands (healthall / setspawn / npc_delete_all), no account.
    gServerHostPlayerName = options.hostPlayerName;
    dedicatedIceState.port = actualPort;
    std::vector<PendingServerTransport> pendingIceTransports;

    startupStageStart = std::chrono::steady_clock::now();
    logStartupStage("map_config_load", "started",
                    "configuring map objective and mode policy");
    mimita::host::ServerConfigureV1 configureRequest{};
    configureRequest.mapName = mapName.c_str();
    configureRequest.gameMode = options.gameMode.c_str();
    configureRequest.gamemodeId = options.gamemodeId.c_str();
    configureRequest.npcCount = options.npcCount;
    configureRequest.npcsEnabled = options.npcsEnabled ? 1u : 0u;
    configureRequest.weaponSetId = options.weaponSetId;
    configureRequest.weaponSetExplicit = options.weaponSetExplicit ? 1u : 0u;
    configureRequest.autoMapRotation = options.autoMapRotation ? 1u : 0u;
    configureRequest.mapRotationMinutes = options.mapRotationMinutes;
    configureRequest.duel = options.duel ? 1u : 0u;
    configureRequest.spawnOverrideEnabled = gServerOverrides.spawnOverrideEnabled ? 1u : 0u;
    configureRequest.spawnOverride[0] = gServerOverrides.spawnOverridePosition.x;
    configureRequest.spawnOverride[1] = gServerOverrides.spawnOverridePosition.y;
    configureRequest.spawnOverride[2] = gServerOverrides.spawnOverridePosition.z;
    configureRequest.headlessWorld = &world;
    configureRequest.npcs = &npcs;
    configureRequest.nextEntityId = &nextEntityId;
    mimita::host::ServerConfigureResultV1 configureResult{};
    const bool configureOk =
        mimita::host::HostRuntime::instance().serverPolicyConfigure(
            &configureRequest, &configureResult);
    if (!configureOk || configureResult.ok == 0) {
        logStartupStage("map_config_load", "failed",
                        configureResult.error[0] ? configureResult.error
                                                 : "server policy configure failed");
        ::StructuredLogger::instance().shutdown();
        return 1;
    }
    logStartupStage("map_config_load", "complete", "map configuration applied",
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - startupStageStart).count(),
        nlohmann::json{{"map", mapName},
                       {"mode", options.gameMode},
                       {"duel", options.duel},
                       {"arena_ball_spawned", configureResult.arenaBallSpawned != 0},
                       {"startup_npcs", configureResult.startupNpcCount}});

    // ── Register server with coordinator in the background ──
    std::string serverCode;
    std::mutex registrationMutex;
    bool registrationFinished = false;
    bool registrationSucceeded = false;
    ListenServerState registrationResult;
    std::thread registrationThread;
    const auto roomRegistrationStart = std::chrono::steady_clock::now();
    logStartupStage("room_register", "started",
                    "registering room with coordinator");
    {
        if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=headless-server-runServer\n",
                   hostedRoomSession().roomCode.c_str(), serverCode.c_str());
        }

        registrationThread = std::thread([&]() {
            registrationResult.serverName = dedicatedIceState.serverName;
            registrationResult.hostPlayerName = dedicatedIceState.hostPlayerName;
            registrationResult.mapName = dedicatedIceState.mapName;
            registrationResult.gameMode = dedicatedIceState.gameMode;
            registrationResult.maxPlayers = dedicatedIceState.maxPlayers;
            registrationResult.discordNotification = dedicatedIceState.discordNotification;
            registrationResult.passwordProtected = dedicatedIceState.passwordProtected;
            registrationResult.password = dedicatedIceState.password;
            registrationResult.port = dedicatedIceState.port;
            const bool ok = initServerIceListener(registrationResult);
            {
                std::lock_guard<std::mutex> lock(registrationMutex);
                registrationSucceeded = ok;
                registrationFinished = true;
            }
            if (!ok)
                printf("%s [SERVER ICE] registration failed; local server remains available\n",
                       residentServerTimestamp());
        });
    }
    ::StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "network.server.local-gameplay-ready", "server",
        "authoritative UDP loop can start before coordinator registration", 0,
        nlohmann::json{{"pid", static_cast<unsigned long long>(GetCurrentProcessId())},
                       {"port", actualPort}, {"map", mapName},
                       {"mode", options.gameMode},
                       {"coordinator_registration", "pending"}},
        __FILE__, __LINE__, __func__);

    uint64_t lastCoordinatorHb = 0;
    uint64_t iceCoordinatorPollCount = 0;

    uint64_t serverStartMs = nowMs();

    // main() installs crash diagnostics before dispatching dedicated-server
    // startup. Keep that process-wide handler in place for the server lifetime.

    // Accumulator-based fixed-step timing
    auto previousTime = std::chrono::steady_clock::now();
    double accumulator = 0.0;
    constexpr int MAX_STEPS = 5;
    ServerLoopPerf loopPerf;
    ServerTickTiming tickTiming;
    uint32_t lastPerfTick = 0;
    uint64_t lastPerfMs = nowMs();
    uint64_t lastTickTimingReportMs = nowMs();
    uint64_t lastLifecycleHeartbeatMs = serverStartMs;
    const char* serverExitReason = "server_loop_completed";

    {
        mimita::host::ServerAuthIdentityV1 authIdentity{};
        if (mimita::host::HostRuntime::instance().serverPolicyAuthIdentity(
                &authIdentity)) {
            PersistenceQueue::instance().beginSession(
                serverCode, authIdentity.token, authIdentity.userId);
        }
    }
    while (true)
    {
        ModuleHost::instance().poll();
        mimita::host::HostRuntime::instance().poll();
        auto loopStart = std::chrono::steady_clock::now();
        bool registrationReady = false;
        bool registrationOk = false;
        {
            std::lock_guard<std::mutex> lock(registrationMutex);
            registrationReady = registrationFinished;
            registrationOk = registrationSucceeded;
            if (registrationReady)
                registrationFinished = false;
        }
        if (registrationReady && registrationOk)
        {
            registrationThread.join();
            dedicatedIceState.iceListenerAgent =
                std::move(registrationResult.iceListenerAgent);
            dedicatedIceState.iceSessionId =
                std::move(registrationResult.iceSessionId);
            dedicatedIceState.serverCode =
                std::move(registrationResult.serverCode);
            dedicatedIceState.joinToken =
                std::move(registrationResult.joinToken);
            dedicatedIceState.lastIceCoordinatorPollMs =
                registrationResult.lastIceCoordinatorPollMs;
            serverCode = dedicatedIceState.serverCode;
            publishServerIceListener(dedicatedIceState);
            logStartupStage("room_register", "complete",
                "room registration completed",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - roomRegistrationStart).count(),
                nlohmann::json{{"room_code", serverCode}, {"port", actualPort}});
            printf("%s [SERVER ICE] registered: code=%s\n",
                   residentServerTimestamp(), serverCode.c_str());
            std::vector<uint32_t> startupNpcIds;
            startupNpcIds.reserve(npcs.size());
            for (const auto& entry : npcs)
                startupNpcIds.push_back(entry.first);
            std::sort(startupNpcIds.begin(), startupNpcIds.end());
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "network.room.registered", serverCode,
                "dedicated server room registered with coordinator", tick,
                nlohmann::json{{"room_code", serverCode},
                               {"server_name", options.name},
                               {"mode", options.gameMode},
                               {"map", options.mapName},
                               {"max_players", options.maxPlayers},
                               {"startup_npc_count", startupNpcIds.size()},
                               {"startup_npc_ids", startupNpcIds},
                               {"local_gameplay_started_first", true}},
                __FILE__, __LINE__, __func__);
            if (!options.roomFilePath.empty())
            {
                FILE* rf = fopen(options.roomFilePath.c_str(), "w");
                if (rf)
                {
                    fprintf(rf, "%s\n", serverCode.c_str());
                    fclose(rf);
                    printf("[SERVER] wrote room code %s to %s\n",
                           serverCode.c_str(), options.roomFilePath.c_str());
                }
            }
        }
        else if (registrationReady)
        {
            registrationThread.join();
            logStartupStage("room_register", "failed",
                "coordinator room registration failed; local server remains available",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - roomRegistrationStart).count(),
                nlohmann::json{{"port", actualPort}});
        }
        // Auto-exit when --timeout is set (for CI/agent testing)
        if (options.timeoutSecs > 0 && nowMs() - serverStartMs > (uint64_t)options.timeoutSecs * 1000)
        {
            printf("%s [SERVER] timeout=%us reached, exiting\n", residentServerTimestamp(), options.timeoutSecs);
            serverExitReason = "timeout_elapsed";
            break;
        }

        const uint64_t heartbeatNowMs = nowMs();
        if (heartbeatNowMs - lastLifecycleHeartbeatMs >= 5000)
        {
            lastLifecycleHeartbeatMs = heartbeatNowMs;
            ::StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.process.heartbeat", serverCode,
                "dedicated server loop is alive", tick,
                nlohmann::json{{"pid", static_cast<unsigned long long>(GetCurrentProcessId())},
                               {"tick", tick}, {"room_code", serverCode},
                               {"mode", options.gameMode}, {"map", options.mapName},
                               {"players", players.size()}, {"npcs", npcs.size()},
                               {"projectiles", projectiles.size()},
                               {"uptime_ms", heartbeatNowMs - serverStartMs}},
                __FILE__, __LINE__, __func__);
            recordCrashBreadcrumb("server-loop",
                "pid=%lu tick=%u room=%s players=%zu npcs=%zu projectiles=%zu uptimeMs=%llu",
                (unsigned long)GetCurrentProcessId(), tick, serverCode.c_str(),
                players.size(), npcs.size(), projectiles.size(),
                (unsigned long long)(heartbeatNowMs - serverStartMs));
        }

        // Game-owned config reload policy runs in the active generation.
        mimita::host::HostRuntime::instance().serverPolicyPollReload(
            &npcSystem, tick);

        // Measure wall-clock elapsed time
        auto currentTime = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;
        if (elapsed > 0.1) elapsed = 0.1;
        accumulator += elapsed;

        char buffer[2048];
        sockaddr_in from{};

        // Drain all pending packets
        for (;;)
        {
            int fromLen = sizeof(from);
            ++transportStats.recvAttempts;
            int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
            {
                int wsaErr = WSAGetLastError();
                if (wsaErr != WSAEWOULDBLOCK)
                {
                    sockaddr_in localEp{};
                    int localEpLen = sizeof(localEp);
                    std::string localStr = "(unknown)";
                    if (getsockname(sock, (sockaddr*)&localEp, &localEpLen) == 0)
                        localStr = addressToString(localEp);
                    printf("%s [NET RX ERROR] sock=%d error=%d local=%s\n",
                           residentServerTimestamp(), (int)sock, wsaErr, localStr.c_str());
                    ++transportStats.recvErrors;
                }
                else
                    ++transportStats.recvWouldBlock;
                break;
            }
            TransportReceiveEvent event{};
            event.connectionId = residentMakeUdpConnectionId(from);
            event.remoteEndpoint = from;
            event.payload = reinterpret_cast<const uint8_t*>(buffer);
            event.payloadBytes = bytes;
            event.receivedAtMs = nowMs();
            event.transportKind = TransportKind::Udp;
            std::uint8_t eventType = 0;
            std::uint32_t eventPlayer = 0;
            if (event.payloadBytes >= static_cast<int>(sizeof(PacketHeader))) {
                const auto* header =
                    reinterpret_cast<const PacketHeader*>(event.payload);
                eventType = header->type;
                eventPlayer = header->playerId;
            }
            const bool probeInbound =
                mimita::host::HostRuntime::instance().probeEnabled();
            auto& transportAdapter =
                mimita::host::HostServerTransportAdapter::instance();
            if (probeInbound) {
                transportAdapter.noteInbound(event.connectionId.value,
                                             from.sin_addr.s_addr, from.sin_port);
            }
            mimita::host::TransportDispatchV1 probeDispatch{};
            bool handledInbound = false;
            if (deliverInboundToProbe(event, tick, &probeDispatch)) {
                noteHandledInbound(event, players);
                noteHotnessPath(true, eventType, eventPlayer, tick);
                sendProbeResponse(sock, from, players, probeDispatch);
                handledInbound = true;
            } else if (deliverInboundToGame(event, tick)) {
                noteHandledInbound(event, players);
                noteHotnessPath(true, eventType, eventPlayer, tick);
                handledInbound = true;
            }
            if (probeInbound) {
                transportAdapter.clearInbound();
            }
            if (handledInbound) {
                ++totalPacketsIn;
                continue;
            }
            ServerPacketProcessResult residentResult{};
            bool handledByDll = false;
            if (mimita::host::HostRuntime::instance().serverApiAvailable()) {
                mimita::host::ServerFrameV1 packetFrame{};
                packetFrame.tick = tick;
                packetFrame.players = &players;
                packetFrame.npcs = &npcs;
                packetFrame.projectiles = &projectiles;
                packetFrame.npcSystem = &npcSystem;
                packetFrame.world = &world;
                packetFrame.npcWorld = &npcWorld;
                packetFrame.vehicleState = &dedicatedVehicleState;
                packetFrame.physicalEntityReplication =
                    &physicalEntityReplication;
                packetFrame.transportStats = &transportStats;
                packetFrame.disagreementRetransmit = &disagreementRetransmit;
                packetFrame.npcIdsAlive = &npcIdsAlive;
                packetFrame.mirrorPlayer = mirrorPlayer;
                packetFrame.nextPlayerId = &nextPlayerId;
                packetFrame.nextEntityId = &nextEntityId;
                packetFrame.nextProjectileId = &nextProjectileId;
                packetFrame.totalPacketsIn = &totalPacketsIn;
                packetFrame.totalPacketsOut = &totalPacketsOut;
                packetFrame.socket = static_cast<std::uintptr_t>(sock);
                mimita::host::ServerPacketResultV1 dllResult{};
                handledByDll =
                    mimita::host::HostRuntime::instance().serverProcessPacket(
                        &packetFrame, &event, &dllResult);
                residentResult.handled = dllResult.handled != 0;
                residentResult.playerId = dllResult.playerId;
            }
            // Hot authority only. MimitaNet::processServerPacket owns every
            // server packet type inside the DLL. Do not fall back to a resident
            // duplicate when the generation rejects a packet.
            if (residentResult.handled) {
                noteHotnessPath(handledByDll, eventType,
                                residentResult.playerId ? residentResult.playerId
                                                        : eventPlayer,
                                tick);
            }
        }

        emitHotnessSummary(tick, players.size(), npcs.size());
        mimita::host::HostRuntime::instance().poll();
        ::StructuredLogger::instance().pollConfig();
        ::StructuredLogger::instance().tick();

        // Accumulator-based timing: run simulation ticks for accumulated debt
        int steps = 0;
        ModuleHost::instance().beginFixedTick();
        while (accumulator >= (double)SERVER_DT && steps < MAX_STEPS)
        {
            const auto tickStart = std::chrono::steady_clock::now();
            const bool hotServer =
                mimita::host::HostRuntime::instance().serverApiAvailable();
            mimita::host::ServerFrameV1 serverFrame{};
            serverFrame.tick = tick;
            serverFrame.players = &players;
            serverFrame.npcs = &npcs;
            serverFrame.projectiles = &projectiles;
            serverFrame.npcSystem = &npcSystem;
            serverFrame.world = &world;
            serverFrame.npcWorld = &npcWorld;
            serverFrame.vehicleState = &dedicatedVehicleState;
            serverFrame.physicalEntityReplication = &physicalEntityReplication;
            serverFrame.transportStats = &transportStats;
            serverFrame.disagreementRetransmit = &disagreementRetransmit;
            serverFrame.npcIdsAlive = &npcIdsAlive;
            serverFrame.mirrorPlayer = mirrorPlayer;
            serverFrame.nextPlayerId = &nextPlayerId;
            serverFrame.nextEntityId = &nextEntityId;
            serverFrame.nextProjectileId = &nextProjectileId;
            serverFrame.totalPacketsIn = &totalPacketsIn;
            serverFrame.totalPacketsOut = &totalPacketsOut;
            serverFrame.socket = static_cast<std::uintptr_t>(sock);
            serverFrame.procedural_mode = options.proceduralMode.c_str();
            serverFrame.procedural_seed = options.proceduralSeed;
            serverFrame.procedural_start_queued = &automaticProceduralStartQueued;
            serverFrame.set_spawn_override = &setSpawnOverrideThunk;
            std::uint64_t npcUs = 0;
            std::uint64_t projectileUs = 0;
            std::uint64_t meleeActionsUs = 0;
            std::uint64_t physicalContactUs = 0;
            std::uint64_t worldImpactFlushUs = 0;
            std::uint64_t contactWeaponUs = 0;
            std::uint64_t snapshotUs = 0;
            std::uint64_t gamemodeUs = 0;
            const bool hotPlayers = hotServer &&
                mimita::host::HostRuntime::instance().serverSimulatePlayers(
                    &serverFrame);
            noteServerCall(hotPlayers);
            
            // Simulate all NPCs once per tick. simulateSharedNpcs already walks
            // the entire NpcSystem; looping over npcs here would advance every
            // NPC once per NPC and make them move N× faster.
            const bool hotWorld = hotServer &&
                mimita::host::HostRuntime::instance().serverSimulateWorld(
                    &serverFrame);
            noteServerCall(hotWorld);
            if (hotWorld) {
                npcUs = serverFrame.simulate_world_us;
            } 
            tickIcePeers(serverCode, dedicatedIceState.iceSessionId,
                         pendingIceTransports);
            tickServerIceTransports(sock, players, npcs, projectiles,
                                    nextEntityId, nextProjectileId,
                                    nextPlayerId, pendingIceTransports, world,
                                    tick, totalPacketsIn, totalPacketsOut,
                                    &transportStats, &disagreementRetransmit,
                                    &npcSystem);

            const bool hotPublish = hotServer &&
                mimita::host::HostRuntime::instance().serverPublish(
                    &serverFrame);
            noteServerCall(hotPublish);
            if (hotPublish) {
                snapshotUs = serverFrame.publish_us;
            }             const bool hotGamemode = hotServer &&
                mimita::host::HostRuntime::instance().serverGamemode(
                    &serverFrame);
            noteServerCall(hotGamemode);
            if (hotGamemode) {
                gamemodeUs = serverFrame.gamemode_us;
            } 
            const uint64_t totalTickUs = elapsedMicroseconds(tickStart);
            ++tickTiming.tickCount;
            tickTiming.totalUs += totalTickUs;
            tickTiming.maxTotalUs = std::max(tickTiming.maxTotalUs, totalTickUs);
            tickTiming.npcUs += npcUs;
            tickTiming.maxNpcUs = std::max(tickTiming.maxNpcUs, npcUs);
            tickTiming.snapshotUs += snapshotUs;
            tickTiming.maxSnapshotUs = std::max(tickTiming.maxSnapshotUs, snapshotUs);
            tickTiming.gamemodeUs += gamemodeUs;
            tickTiming.maxGamemodeUs = std::max(tickTiming.maxGamemodeUs, gamemodeUs);
            tickTiming.projectileUs += projectileUs;
            tickTiming.maxProjectileUs = std::max(tickTiming.maxProjectileUs, projectileUs);
            tickTiming.contactWeaponUs += contactWeaponUs;
            tickTiming.maxContactWeaponUs = std::max(tickTiming.maxContactWeaponUs, contactWeaponUs);
            tickTiming.totalSamplesUs[tickTiming.sampleWrite] = totalTickUs;
            tickTiming.recentSamples[tickTiming.sampleWrite] = {
                tick, totalTickUs, npcUs, snapshotUs, gamemodeUs,
                projectileUs, contactWeaponUs};
            tickTiming.sampleWrite = static_cast<uint32_t>(
                (tickTiming.sampleWrite + 1) %
                tickTiming.totalSamplesUs.size());
            if (tickTiming.sampleCount < static_cast<uint32_t>(
                    tickTiming.totalSamplesUs.size()))
                ++tickTiming.sampleCount;
            if (totalTickUs > 100000)
            {
                ++tickTiming.over100MsCount;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Performance, StructuredLevel::Important,
                    "performance.server-tick-stall", "server",
                    "fixed tick exceeded 100 ms", tick,
                    nlohmann::json{
                        {"tick", tick},
                        {"total_ms", static_cast<double>(totalTickUs) / 1000.0},
                        {"npc_ms", static_cast<double>(npcUs) / 1000.0},
                        {"snapshot_ms", static_cast<double>(snapshotUs) / 1000.0},
                        {"gamemode_ms", static_cast<double>(gamemodeUs) / 1000.0},
                        {"projectile_ms", static_cast<double>(projectileUs) / 1000.0},
                        {"contact_weapon_ms", static_cast<double>(contactWeaponUs) / 1000.0},
                        {"melee_actions_ms", static_cast<double>(meleeActionsUs) / 1000.0},
                        {"physical_contact_ms", static_cast<double>(physicalContactUs) / 1000.0},
                        {"world_impact_flush_ms", static_cast<double>(worldImpactFlushUs) / 1000.0},
                        {"players", players.size()},
                        {"npcs", npcs.size()},
                        {"steps", steps}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            const double projectileMs = static_cast<double>(projectileUs) / 1000.0;
            const double contactMs = static_cast<double>(physicalContactUs) / 1000.0;
            const double npcPhaseMs = static_cast<double>(npcUs) / 1000.0;
            mimita_network_policy::ServerPhaseBudgetPolicyV1 budget{};
            budget.version = mimita_network_policy::ABI_VERSION;
            budget.structSize = sizeof(budget);
            budget.tick = tick;
            budget.npcCount = static_cast<std::uint32_t>(npcs.size());
            budget.playerCount = static_cast<std::uint32_t>(players.size());
            budget.totalMs = static_cast<double>(totalTickUs) / 1000.0;
            budget.npcMs = npcPhaseMs;
            budget.projectileMs = projectileMs;
            budget.contactMs = contactMs;
            const bool budgetPolicyApplied =
                ModuleHost::instance().networkServerPhaseBudgetPolicy(&budget);
            const double projectileBudgetMs = budgetPolicyApplied
                ? (double)budget.outProjectileBudgetMs : 4.0;
            const double contactBudgetMs = budgetPolicyApplied
                ? (double)budget.outContactBudgetMs : 4.0;
            const double npcBudgetMs = budgetPolicyApplied
                ? (double)budget.outNpcBudgetMs : 10.0;
            const bool phaseOverBudget = budgetPolicyApplied
                ? budget.outOverBudget != 0
                : (projectileMs > 4.0 || contactMs > 4.0 ||
                   npcPhaseMs > 10.0);
            if (phaseOverBudget)
            {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Performance, StructuredLevel::Important,
                    "performance.server-phase-budget", "server",
                    "server phase exceeded its 60 Hz share", tick,
                    nlohmann::json{
                        {"tick", tick},
                        {"projectile_ms", projectileMs},
                        {"projectile_budget_ms", projectileBudgetMs},
                        {"physical_contact_ms", contactMs},
                        {"physical_contact_budget_ms", contactBudgetMs},
                        {"npc_ms", npcPhaseMs},
                        {"npc_budget_ms", npcBudgetMs},
                        {"projectile_over", projectileMs > projectileBudgetMs},
                        {"contact_over", contactMs > contactBudgetMs},
                        {"npc_over", npcPhaseMs > npcBudgetMs},
                        {"defer_npc", budgetPolicyApplied
                            ? (budget.outDeferNpc != 0) : false},
                        {"defer_contacts", budgetPolicyApplied
                            ? (budget.outDeferContacts != 0) : false},
                        {"policy_generation", budgetPolicyApplied
                            ? budget.outGeneration : 0u},
                        {"npcs", npcs.size()},
                        {"players", players.size()}},
                    __FILE__, __LINE__, __FUNCTION__);
            }

            accumulator -= (double)SERVER_DT;
            ++tick;
            ++steps;
        }
        ModuleHost::instance().endFixedTick();
        const bool cappedCatchup = steps >= MAX_STEPS && accumulator >= (double)SERVER_DT;

        // Poll coordinator for incoming ICE requests every outer loop
        // (rate-limited to 500ms inside tickIceCoordinator).
        tickIceCoordinator(dedicatedIceState, players.size());
        fflush(stdout);

        // ICE rooms stay alive via coordinatorIceHostPoll in tickIceCoordinator

        // Timing measurement every 600 ticks (~10 seconds)
        static uint32_t s_lastTimingTick = 0;
        static uint64_t s_timingStartMs = nowMs();
        if (tick - s_lastTimingTick >= 600)
        {
            uint64_t nowMsVal = nowMs();
            double elapsedSec = (double)(nowMsVal - s_timingStartMs) / 1000.0;
            double actualHz = (double)(tick - s_lastTimingTick) / elapsedSec;
            printf("%s [SERVER TIMING] ticks=%u elapsed=%.1fs actualHz=%.1f players=%zu projectiles=%zu\n",
                   residentServerTimestamp(), tick - s_lastTimingTick, elapsedSec, actualHz,
                   players.size(), projectiles.size());
            s_lastTimingTick = tick;
            s_timingStartMs = nowMsVal;
        }

        // Heartbeat every ~300 ticks (~5 seconds) to prove server is alive
        {
            static uint32_t s_lastHeartbeatTick = 0;
            if (tick - s_lastHeartbeatTick >= 300)
            {
                printf("%s [SERVER HEARTBEAT] tick=%u players=%zu — alive\n",
                       residentServerTimestamp(), tick, players.size());
                fflush(stdout);
                s_lastHeartbeatTick = tick;
            }
        }

        // Status log every second
        if (nowMs() - lastLog >= 1000)
        {
            printf("%s [SERVER STATUS] tick=%u players=%zu packetsIn=%llu packetsOut=%llu "
                   "recvAttempts=%llu recvWouldBlock=%llu recvErrors=%llu malformed=%llu "
                   "protocolMismatch=%llu unknown=%llu hello=%llu join=%llu reconnect=%llu input=%llu\n",
                   residentServerTimestamp(), tick, players.size(),
                   (unsigned long long)totalPacketsIn, (unsigned long long)totalPacketsOut,
                   (unsigned long long)transportStats.recvAttempts,
                   (unsigned long long)transportStats.recvWouldBlock,
                   (unsigned long long)transportStats.recvErrors,
                   (unsigned long long)transportStats.malformedPackets,
                   (unsigned long long)transportStats.protocolMismatches,
                   (unsigned long long)transportStats.unknownPacketTypes,
                   (unsigned long long)transportStats.helloPackets,
                   (unsigned long long)transportStats.joinPackets,
                   (unsigned long long)transportStats.reconnectPackets,
                   (unsigned long long)transportStats.inputPackets);
            lastLog = nowMs();
        }

        // Per-second structured divergence log: the server's authoritative view
        // of every player (position/velocity/HP/ground state). Write to the
        // Network log file so a test can compare this against each client's
        // own per-second report. Gated by debuglogger.json "network" level.
        {
            static uint64_t s_lastServerDivergenceLog = 0;
            const uint64_t nowDiv = nowMs();
            if (nowDiv - s_lastServerDivergenceLog >= 1000 &&
                ::StructuredLogger::instance().shouldLog(
                    ::StructuredCategory::Network, ::StructuredLevel::Verbose))
            {
                s_lastServerDivergenceLog = nowDiv;
                std::string msg;
                char buf[256];
                snprintf(buf, sizeof(buf), "tick=%u players=%zu npcs=%zu",
                         tick, players.size(), npcs.size());
                msg += buf;
                for (const auto& kv : players)
                {
                    const ServerPlayer& p = kv.second;
                    snprintf(buf, sizeof(buf),
                        " | p%u pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) "
                        "hp=%d onGround=%d yaw=%.1f stale=%d",
                        p.id, p.pos.x, p.pos.y, p.pos.z,
                        p.vel.x, p.vel.y, p.vel.z,
                        p.health, (int)p.onGround, p.yaw,
                        (int)p.connectionStale);
                    msg += buf;
                }
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::Network;
                e.level = ::StructuredLevel::Verbose;
                e.eventId = "server-divergence";
                e.reason = "server authoritative player state";
                e.sourceFile = __FILE__;
                e.sourceLine = __LINE__;
                e.functionName = __FUNCTION__;
                if (msg.size() > 3000) msg.resize(3000);
                e.message = msg;
                ::StructuredLogger::instance().write(e);
            }
        }

        // NOTE: no extra `accumulator += postElapsed` here. The loop-top
        // `elapsed` measurement already covers the full previous iteration
        // (body work + sleep). Adding the current iteration's body time a
        // second time made the accumulator grow faster than real time and the
        // server run at ~80Hz instead of the locked 60Hz (SERVER_DT). That
        // clock mismatch is what made clients think snapshots were missing and
        // extrapolate remote bodies ahead of their real position.
        const uint64_t loopUs = (uint64_t)std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - loopStart).count();
        recordServerLoopPerf(loopPerf, loopUs, cappedCatchup);
        const uint64_t perfNowMs = nowMs();
        if (perfNowMs - lastPerfMs >= 1000)
        {
            reportServerPerf(residentServerTimestamp(), loopPerf, tick, lastPerfTick,
                             perfNowMs - lastPerfMs);
            lastPerfTick = tick;
            lastPerfMs = perfNowMs;
        }
        if (perfNowMs - lastTickTimingReportMs >= 1000)
        {
            reportServerTickTiming(tickTiming, tick, players.size(), npcs.size(),
                                   mapName, options.gameMode,
                                   totalPacketsIn, totalPacketsOut);
            lastTickTimingReportMs = perfNowMs;
        }

        // Sleep only when no simulation debt remains. Use microseconds for precision.
        if (accumulator < (double)SERVER_DT)
        {
            double sleepSec = (double)SERVER_DT - accumulator;
            uint64_t sleepUs = (uint64_t)(sleepSec * 1000000.0);
            if (sleepUs > 1000)
                std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
        }
    }

    if (registrationThread.joinable())
        registrationThread.join();
    PersistenceQueue::instance().flushBlocking();
    shutdownIceCoordinatorPoller();
    logServerExit(0, serverExitReason, serverCode, tick);
    mimita::host::HostRuntime::instance().shutdown();
    ::StructuredLogger::instance().shutdown();
    if (!serverCode.empty())
    {
        printf("%s [SERVER] deregistering room %s\n", residentServerTimestamp(), serverCode.c_str());
        mimita::host::HostRuntime::instance().serverPolicyCoordinatorIceDone(
            serverCode.c_str());
    }
    mimita::host::HostRuntime::instance().clearEntityAdapter();
    mimita::host::HostRuntime::instance().clearWorldAdapter();
    mimita::host::HostRuntime::instance().clearTransportAdapter();
    mimita::host::HostServerSessionEvents::instance().clear();
    closesocket(sock);
    netShutdown();
    printf("%s [SERVER] shutdown complete\n", residentServerTimestamp());
    return 0;
}

// ─── Listen Server ─────────────────────────────────────────────────────────

ListenServerState::~ListenServerState()
{
    // Unique_ptr members (NpcSystem/World/Player) need complete types; this
    // TU includes them, so the state can safely outlive other translation units.
}

bool startListenServer(ListenServerState& state, uint16_t port,
    const std::string& publicIp, const std::string& hostSessionId,
    const ServerLaunchSettings* settings)
{
    if (state.active)
        return false;

    if (!netStartup())
    {
        printf("[LISTEN SERVER] FATAL: WSAStartup failed\n");
        return false;
    }

    state.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (state.sock == INVALID_SOCKET)
    {
        printf("[LISTEN SERVER] FATAL: socket() failed error=%d\n", WSAGetLastError());
        netShutdown();
        return false;
    }
    disableUdpConnReset(state.sock);

    BOOL exclusiveAddressUse = TRUE;
    if (setsockopt(state.sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (const char*)&exclusiveAddressUse,
                   sizeof(exclusiveAddressUse)) == SOCKET_ERROR)
    {
        printf("[LISTEN SERVER] FATAL: SO_EXCLUSIVEADDRUSE failed error=%d\n",
               WSAGetLastError());
        closesocket(state.sock);
        state.sock = INVALID_SOCKET;
        netShutdown();
        return false;
    }
    setNonBlocking(state.sock);

    const bool localOnly = settings && settings->startLocalServer;

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = localOnly ? inet_addr("127.0.0.1") : htonl(INADDR_ANY);
    bindAddr.sin_port = htons(port);

    if (bind(state.sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        printf("[LISTEN SERVER] FATAL: bind() port=%u failed error=%d\n", port, err);
        if (err == WSAEADDRINUSE)
            printf("[LISTEN SERVER] HINT: Port %u already in use\n", port);
        closesocket(state.sock);
        netShutdown();
        return false;
    }

    if (localOnly)
        printf("[LISTEN SERVER] bound to port %u (localhost only)\n", port);
    else
        printf("[LISTEN SERVER] bound to port %u (all interfaces)\n", port);

    const std::string settingsMapName =
        settings ? settings->mapName : "funworld3";
    const std::string settingsMapPath =
        settings ? settings->resolvedMapPath : "";
    const std::string settingsGameMode =
        settings ? settings->gameMode : "sandbox";
    const std::string settingsGamemodeId =
        settings ? settings->gamemodeId : "duel";

    mimita::host::ServerStartupV1 startupRequest{};
    startupRequest.mapName = settingsMapName.c_str();
    startupRequest.mapPath = settingsMapPath.c_str();
    startupRequest.gameMode = settingsGameMode.c_str();
    startupRequest.gamemodeId = settingsGamemodeId.c_str();
    startupRequest.serverName = settings ? settings->serverName.c_str() : "";
    startupRequest.hostPlayerName =
        settings ? settings->hostPlayerName.c_str() : "";
    startupRequest.password = settings ? settings->password.c_str() : "";
    startupRequest.npcCount = settings ? settings->startupNpcCount : 0;
    startupRequest.npcsEnabled =
        settings && settings->startupNpcsEnabled ? 1u : 0u;
    startupRequest.weaponSetId = settings ? settings->weaponSetId : 1;
    startupRequest.weaponSetExplicit =
        settings && settings->weaponSetExplicit ? 1u : 0u;
    startupRequest.autoMapRotation =
        settings && settings->autoMapRotation ? 1u : 0u;
    startupRequest.mapRotationMinutes =
        settings ? settings->mapRotationMinutes : 0;
    startupRequest.discordNotification =
        settings && settings->discordNotification ? 1u : 0u;
    startupRequest.passwordProtected =
        settings && settings->passwordProtected ? 1u : 0u;
    startupRequest.maxPlayers = settings ? settings->maxPlayers : 999;
    startupRequest.duel = settings && settings->duelMode ? 1u : 0u;
    startupRequest.spawnOverrideEnabled =
        gServerOverrides.spawnOverrideEnabled ? 1u : 0u;
    startupRequest.spawnOverride[0] = gServerOverrides.spawnOverridePosition.x;
    startupRequest.spawnOverride[1] = gServerOverrides.spawnOverridePosition.y;
    startupRequest.spawnOverride[2] = gServerOverrides.spawnOverridePosition.z;
    mimita::host::ServerStartupResultV1 startupResult{};
    if (!mimita::host::HostRuntime::instance().serverPolicyStartup(
            &startupRequest, &startupResult) ||
        startupResult.ok == 0) {
        printf("[LISTEN SERVER] FATAL: server policy startup failed: %s\n",
               startupResult.error);
        return false;
    }
    const std::string mapName = startupResult.resolvedMapName;
    const std::string mapPath = startupResult.resolvedMapPath;
    mimita::host::setSessionMapId(mapName);
    printf("[LISTEN SERVER MAP] mapId=%s path=%s\n", mapName.c_str(), mapPath.c_str());

    state.npcWorld = std::make_unique<World>();
    state.npcSystem = std::make_unique<NpcSystem>();
    mimita::host::ServerWorldLoadV1 worldRequest{};
    worldRequest.mapName = mapName.c_str();
    worldRequest.mapPath = mapPath.c_str();
    worldRequest.gameMode = settingsGameMode.c_str();
    worldRequest.headlessWorld = &state.world;
    worldRequest.npcWorld = state.npcWorld.get();
    mimita::host::ServerWorldLoadResultV1 worldResult{};
    if (!mimita::host::HostRuntime::instance().serverPolicyWorldLoad(
            &worldRequest, &worldResult) ||
        worldResult.ok == 0) {
        printf("[LISTEN SERVER] FATAL: server policy world load failed: %s\n",
               worldResult.error);
        return false;
    }
    state.mirrorPlayer =
        mimita::host::HostRuntime::instance().serverPolicyCreatePlayer();
    if (!state.mirrorPlayer) {
        printf("[LISTEN SERVER] FATAL: server policy could not create player\n");
        return false;
    }
    state.npcIdsAlive.clear();

    state.publicIp = publicIp;
    state.hostSessionId = hostSessionId;
    state.active = true;
    mimita::host::HostRuntime::instance().serverPolicyClearRuntimeObjects(
        "listen-server-start");
    state.port = port;
    state.players.clear();
    state.npcs.clear();
    state.projectiles.clear();
    state.nextPlayerId = 1;
    state.nextEntityId = 1000;
    state.nextProjectileId = 1;
    state.tick = 0;
    state.vehicleState = ServerVehicleState{};
    state.lastLog = 0;
    state.totalPacketsIn = 0;
    state.totalPacketsOut = 0;
    state.startTimeMs = nowMs();
    state.accumulator = 0.0f;
    if (mimita::host::HostRuntime::instance().probeEnabled()) {
        auto& entityAdapter = mimita::host::HostServerEntityAdapter::instance();
        entityAdapter.bindPlayers(&state.players);
        entityAdapter.bindNpcs(&state.npcs);
        entityAdapter.bindNextIds(&state.nextPlayerId, &state.nextEntityId);
        mimita::host::HostRuntime::instance().setEntityAdapter(
            entityAdapter.adapter(), entityAdapter.owner());

        auto& worldAdapter = mimita::host::HostServerWorldAdapter::instance();
        worldAdapter.bind(&state.world);
        mimita::host::HostRuntime::instance().setWorldAdapter(
            worldAdapter.adapter(), worldAdapter.owner());

        auto& transportAdapter =
            mimita::host::HostServerTransportAdapter::instance();
        transportAdapter.bindPlayers(&state.players);
        transportAdapter.setSender(
            [&state](std::uint32_t playerId, const void* data, std::size_t size) {
                auto it = state.players.find(playerId);
                if (it == state.players.end() || !data || size == 0) {
                    return false;
                }
                if (it->second.addr.sin_port != 0) {
                    return sendto(state.sock, static_cast<const char*>(data),
                                  static_cast<int>(size), 0,
                                  reinterpret_cast<const sockaddr*>(&it->second.addr),
                                  sizeof(it->second.addr)) == static_cast<int>(size);
                }
                if (it->second.transport) {
                    return it->second.transport->send(data, size);
                }
                return false;
            });
        transportAdapter.setEndpointSender(
            [&state](std::uint32_t addressNetworkOrder,
                     std::uint16_t portNetworkOrder, const void* data,
                     std::size_t size) {
                sockaddr_in target{};
                target.sin_family = AF_INET;
                target.sin_addr.s_addr = addressNetworkOrder;
                target.sin_port = portNetworkOrder;
                return sendto(state.sock, static_cast<const char*>(data),
                              static_cast<int>(size), 0,
                              reinterpret_cast<const sockaddr*>(&target),
                              sizeof(target)) == static_cast<int>(size);
            });
        mimita::host::HostRuntime::instance().setTransportAdapter(
            transportAdapter.adapter(), transportAdapter.owner());
        mimita::host::HostServerSessionEvents::instance().bindPlayers(
            &state.players);
    }
    if (settings)
    {
        state.serverName = settings->serverName;
        state.mapName = settings->mapName;
        state.gameMode = settings->gameMode;
        state.maxPlayers = settings->maxPlayers;
        state.weaponSetId = settings->weaponSetId;
        state.weaponSetExplicit = settings->weaponSetExplicit;
        state.autoMapRotation = settings->autoMapRotation;
        state.mapRotationMinutes = settings->mapRotationMinutes;
        state.discordNotification = settings->discordNotification;
        state.passwordProtected = settings->passwordProtected;
        state.password = settings->password;
        state.hostPlayerName = settings->hostPlayerName;
    }
    state.mapName = mapName;
    state.maxPlayers = startupResult.resolvedMaxPlayers;
    mimita::host::ServerConfigureV1 configureRequest{};
    configureRequest.mapName = mapName.c_str();
    configureRequest.gameMode = settingsGameMode.c_str();
    configureRequest.gamemodeId = settingsGamemodeId.c_str();
    configureRequest.npcCount = settings ? settings->startupNpcCount : 0;
    configureRequest.npcsEnabled =
        (!settings || settings->startupNpcsEnabled) ? 1u : 0u;
    configureRequest.weaponSetId = settings ? settings->weaponSetId : 1;
    configureRequest.weaponSetExplicit =
        settings && settings->weaponSetExplicit ? 1u : 0u;
    configureRequest.autoMapRotation =
        settings && settings->autoMapRotation ? 1u : 0u;
    configureRequest.mapRotationMinutes =
        settings ? settings->mapRotationMinutes : 0;
    configureRequest.duel = settings && settings->duelMode ? 1u : 0u;
    configureRequest.spawnOverrideEnabled =
        gServerOverrides.spawnOverrideEnabled ? 1u : 0u;
    configureRequest.spawnOverride[0] = gServerOverrides.spawnOverridePosition.x;
    configureRequest.spawnOverride[1] = gServerOverrides.spawnOverridePosition.y;
    configureRequest.spawnOverride[2] = gServerOverrides.spawnOverridePosition.z;
    configureRequest.headlessWorld = &state.world;
    configureRequest.npcs = &state.npcs;
    configureRequest.nextEntityId = &state.nextEntityId;
    mimita::host::ServerConfigureResultV1 configureResult{};
    if (!mimita::host::HostRuntime::instance().serverPolicyConfigure(
            &configureRequest, &configureResult) ||
        configureResult.ok == 0) {
        printf("[LISTEN SERVER] FATAL: server policy configure failed: %s\n",
               configureResult.error);
        return false;
    }
    gServerHostPlayerName = settings ? settings->hostPlayerName : "";
    printf("[LISTEN SERVER NPC STARTUP] enabled=%d requested=%u spawned=%u\n",
           (int)(configureRequest.npcsEnabled != 0),
           configureRequest.npcCount, configureResult.startupNpcCount);

    // Register with coordinator (skip if external server process handles it, or local-only)
    if (localOnly)
    {
        // Generate a local-only room code (LOCAL-XXXX)
        const char* chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        std::string suffix;
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 31);
            for (int i = 0; i < 4; ++i)
                suffix += chars[dist(gen)];
        }
        state.serverCode = "LOCAL-" + suffix;
        mimita::host::setSessionCoordinatorCode("LOCAL");
        mimita::host::setSessionCoordinatorJoinToken("");
        printf("[LISTEN SERVER] local-only mode: code=%s\n", state.serverCode.c_str());
    }
    else if (!settings || !settings->externalProcessLaunched)
    {
        if (hostedRoomSession().active)
        {
            printf("[ROOM DUPLICATE ERROR] existingCode=%s attemptedCode=%s caller=startListenServer\n",
                   hostedRoomSession().roomCode.c_str(), state.serverCode.c_str());
        }

        if (!initServerIceListener(state))
        {
            printf("[LISTEN SERVER] ICE init failed — aborting. Check STUN/TURN connectivity.\n");
            closesocket(state.sock);
            netShutdown();
            return false;
        }
        publishServerIceListener(state);
        printf("[LISTEN SERVER] ICE registered: code=%s\n", state.serverCode.c_str());
        hostedRoomSession().coordinatorRoomType = "ice";
    }

    printf("[LISTEN SERVER] started port=%u code=%s\n", port, state.serverCode.c_str());

    // Spawn background thread for genuine 60 Hz independent server timing
    {
        mimita::host::ServerAuthIdentityV1 authIdentity{};
        if (mimita::host::HostRuntime::instance().serverPolicyAuthIdentity(
                &authIdentity)) {
            PersistenceQueue::instance().beginSession(
                state.serverCode, authIdentity.token, authIdentity.userId);
        }
    }
    state.serverRunning = true;
    state.serverThread = std::thread(listenServerThreadFunc, std::ref(state));

    return true;
}

void stopListenServer(ListenServerState& state)
{
    if (!state.active)
        return;

    printf("[LISTEN SERVER] stopping code=%s players=%zu uptime=%llus tick=%u\n",
           state.serverCode.c_str(), state.players.size(),
           (unsigned long long)((nowMs() - state.startTimeMs) / 1000),
           state.tick);

    // Signal background thread to stop
    state.serverRunning = false;

    if (state.serverThread.joinable())
        state.serverThread.join();

    // Runtime-only physical objects are process state, not map data. Stopping
    // the server must not leave vehicles, constraints, grenades, or corpses
    // behind for the next session.
    mimita::host::HostRuntime::instance().serverPolicyClearRuntimeObjects(
        "listen-server-stop");
    if (state.mirrorPlayer) {
        mimita::host::HostRuntime::instance().serverPolicyDestroyPlayer(
            state.mirrorPlayer);
        state.mirrorPlayer = nullptr;
    }

    shutdownIceCoordinatorPoller();

    // Flush any remaining persistence events before shutdown
    {
        PersistenceQueue::instance().flushBlocking();
        const size_t depth = PersistenceQueue::instance().queueDepth();
        if (depth > 0)
            printf("[PERSISTENCE] WARNING: %zu events could not be flushed on shutdown\n", depth);
    }

    // Deregister with coordinator (skip for local-only servers)
    if (!state.serverCode.empty() && state.serverCode.find("LOCAL-") != 0)
        mimita::host::HostRuntime::instance().serverPolicyCoordinatorIceDone(
            state.serverCode.c_str());

    closesocket(state.sock);
    state.sock = INVALID_SOCKET;
    state.active = false;
    mimita::host::HostRuntime::instance().clearEntityAdapter();
    mimita::host::HostRuntime::instance().clearWorldAdapter();
    mimita::host::HostRuntime::instance().clearTransportAdapter();
    mimita::host::HostServerSessionEvents::instance().clear();
    netShutdown();
}

// ── One tick of the authoritative server simulation ────────────────────
// This is the shared body used by both the background listen server thread
// and extracted from the old tickListenServer accumulator loop.
static void simulateOneServerTick(ListenServerState& state)
{
    // Game-owned config reload policy runs in the active generation.
    mimita::host::HostRuntime::instance().serverPolicyPollReload(
        state.npcSystem.get(), state.tick);

    {
        char buffer[2048];
        sockaddr_in from{};
        for (;;)
        {
            int fromLen = sizeof(from);
            int bytes = recvfrom(state.sock, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from, &fromLen);
            if (bytes <= 0)
            {
                int wsaErr = WSAGetLastError();
                if (wsaErr != WSAEWOULDBLOCK)
                {
                    sockaddr_in localEp{};
                    int localEpLen = sizeof(localEp);
                    std::string localStr = "(unknown)";
                    if (getsockname(state.sock, (sockaddr*)&localEp, &localEpLen) == 0)
                        localStr = addressToString(localEp);
                    printf("[LISTEN SERVER RX ERROR] sock=%d error=%d local=%s\n",
                           (int)state.sock, wsaErr, localStr.c_str());
                }
                break;
            }
            TransportReceiveEvent event{};
            event.connectionId = residentMakeUdpConnectionId(from);
            event.remoteEndpoint = from;
            event.payload = reinterpret_cast<const uint8_t*>(buffer);
            event.payloadBytes = bytes;
            event.receivedAtMs = nowMs();
            event.transportKind = TransportKind::Udp;
            std::uint8_t eventType = 0;
            std::uint32_t eventPlayer = 0;
            if (event.payloadBytes >= static_cast<int>(sizeof(PacketHeader))) {
                const auto* header =
                    reinterpret_cast<const PacketHeader*>(event.payload);
                eventType = header->type;
                eventPlayer = header->playerId;
            }
            const bool probeInbound =
                mimita::host::HostRuntime::instance().probeEnabled();
            auto& transportAdapter =
                mimita::host::HostServerTransportAdapter::instance();
            if (probeInbound) {
                transportAdapter.noteInbound(event.connectionId.value,
                                             from.sin_addr.s_addr, from.sin_port);
            }
            mimita::host::TransportDispatchV1 probeDispatch{};
            bool handledInbound = false;
            if (deliverInboundToProbe(event, state.tick, &probeDispatch)) {
                noteHandledInbound(event, state.players);
                noteHotnessPath(true, eventType, eventPlayer, state.tick);
                sendProbeResponse(state.sock, from, state.players, probeDispatch);
                handledInbound = true;
            } else if (deliverInboundToGame(event, state.tick)) {
                noteHandledInbound(event, state.players);
                noteHotnessPath(true, eventType, eventPlayer, state.tick);
                handledInbound = true;
            }
            if (probeInbound) {
                transportAdapter.clearInbound();
            }
            if (handledInbound) {
                ++state.totalPacketsIn;
                continue;
            }
            ServerPacketProcessResult residentResult{};
            bool handledByDll = false;
            if (mimita::host::HostRuntime::instance().serverApiAvailable()) {
                mimita::host::ServerFrameV1 packetFrame{};
                packetFrame.tick = state.tick;
                packetFrame.players = &state.players;
                packetFrame.npcs = &state.npcs;
                packetFrame.projectiles = &state.projectiles;
                packetFrame.npcSystem = state.npcSystem.get();
                packetFrame.world = &state.world;
                packetFrame.npcWorld = state.npcWorld.get();
                packetFrame.vehicleState = &state.vehicleState;
                packetFrame.physicalEntityReplication =
                    &state.physicalEntityReplication;
                packetFrame.transportStats = nullptr;
                packetFrame.disagreementRetransmit =
                    &state.disagreementRetransmit;
                packetFrame.npcIdsAlive = &state.npcIdsAlive;
                packetFrame.mirrorPlayer = state.mirrorPlayer;
                packetFrame.nextPlayerId = &state.nextPlayerId;
                packetFrame.nextEntityId = &state.nextEntityId;
                packetFrame.nextProjectileId = &state.nextProjectileId;
                packetFrame.totalPacketsIn = &state.totalPacketsIn;
                packetFrame.totalPacketsOut = &state.totalPacketsOut;
                packetFrame.socket =
                    static_cast<std::uintptr_t>(state.sock);
                mimita::host::ServerPacketResultV1 dllResult{};
                handledByDll =
                    mimita::host::HostRuntime::instance().serverProcessPacket(
                        &packetFrame, &event, &dllResult);
                residentResult.handled = dllResult.handled != 0;
                residentResult.playerId = dllResult.playerId;
            }
            // Hot authority only; see the dedicated loop above.
            if (residentResult.handled) {
                noteHotnessPath(handledByDll, eventType,
                                residentResult.playerId ? residentResult.playerId
                                                        : eventPlayer,
                                state.tick);
            }
        }

        emitHotnessSummary(state.tick, state.players.size(), state.npcs.size());
        mimita::host::HostRuntime::instance().poll();
        const bool hotServer =
            mimita::host::HostRuntime::instance().serverApiAvailable();
        mimita::host::ServerFrameV1 serverFrame{};
        serverFrame.tick = state.tick;
        serverFrame.flags = mimita::host::kServerFrameHazardScratch;
        serverFrame.players = &state.players;
        serverFrame.npcs = &state.npcs;
        serverFrame.projectiles = &state.projectiles;
        serverFrame.npcSystem = state.npcSystem.get();
        serverFrame.world = &state.world;
        serverFrame.npcWorld = state.npcWorld.get();
        serverFrame.vehicleState = &state.vehicleState;
        serverFrame.physicalEntityReplication =
            &state.physicalEntityReplication;
        serverFrame.transportStats = nullptr;
        serverFrame.disagreementRetransmit = &state.disagreementRetransmit;
        serverFrame.npcIdsAlive = &state.npcIdsAlive;
        serverFrame.mirrorPlayer = state.mirrorPlayer;
        serverFrame.nextPlayerId = &state.nextPlayerId;
        serverFrame.nextEntityId = &state.nextEntityId;
        serverFrame.nextProjectileId = &state.nextProjectileId;
        serverFrame.totalPacketsIn = &state.totalPacketsIn;
        serverFrame.totalPacketsOut = &state.totalPacketsOut;
        serverFrame.socket = static_cast<std::uintptr_t>(state.sock);
        serverFrame.set_spawn_override = &setSpawnOverrideThunk;
        const bool hotPlayers = hotServer &&
            mimita::host::HostRuntime::instance().serverSimulatePlayers(
                &serverFrame);
        noteServerCall(hotPlayers);
                const bool hotWorld = hotServer &&
            mimita::host::HostRuntime::instance().serverSimulateWorld(
                &serverFrame);
        noteServerCall(hotWorld);
        
        // Skip ICE coordinator for local-only servers
        if (state.serverCode.find("LOCAL-") != 0)
        {
            tickIceCoordinator(state, state.players.size());
            tickIcePeers(state.serverCode, state.iceSessionId, state.pendingIceTransports);
        }
        tickServerIceTransports(state.sock, state.players, state.npcs,
                                state.projectiles, state.nextEntityId,
                                state.nextProjectileId, state.nextPlayerId,
                                state.pendingIceTransports, state.world,
                                state.tick, state.totalPacketsIn,
                                state.totalPacketsOut, nullptr,
                                &state.disagreementRetransmit,
                                state.npcSystem.get());

        const bool hotPublish = hotServer &&
            mimita::host::HostRuntime::instance().serverPublish(&serverFrame);
        noteServerCall(hotPublish);
                const bool hotGamemode = hotServer &&
            mimita::host::HostRuntime::instance().serverGamemode(&serverFrame);
        noteServerCall(hotGamemode);
        
        uint64_t now = nowMs();
        if (now - state.lastLog >= 1000)
        {
            printf("[LISTEN SERVER] tick=%u players=%zu packetsIn=%llu packetsOut=%llu code=%s\n",
                   state.tick, state.players.size(),
                   (unsigned long long)state.totalPacketsIn,
                   (unsigned long long)state.totalPacketsOut,
                   state.serverCode.c_str());
            if (state.players.size() <= 10)
            {
                for (const auto& kv : state.players)
                    printf("[LISTEN SERVER] player id=%u name=\"%s\" pos=(%.1f,%.1f,%.1f)\n",
                           kv.second.id, kv.second.name.c_str(),
                           kv.second.pos.x, kv.second.pos.y, kv.second.pos.z);
            }
            state.lastLog = now;
        }

        // ICE rooms stay alive via coordinatorIceHostPoll in tickIceCoordinator

        ++state.tick;
    }
}

// ── Background thread: runs the listen server at a genuine 60 Hz ─────
// Uses accumulator-based timing (not sleep_until deadline) so that late
// wakes are caught up on the next iteration rather than drifting forever.
static void listenServerThreadFunc(ListenServerState& state)
{
    using namespace std::chrono;
    constexpr double kFixedDt = 1.0 / 60.0;
    constexpr auto kFixedDtNs = nanoseconds(1'000'000'000 / 60);
    constexpr int kMaxCatchup = 5;

    auto previousTime = steady_clock::now();
    double accumulator = 0.0;

    printf("[LISTEN SERVER THREAD] started with accumulator timing\n");

    uint64_t lastLogTick = 0;
    uint64_t lastHzLog = 0;
    ServerLoopPerf loopPerf;
    uint32_t lastPerfTick = 0;
    uint64_t lastPerfMs = nowMs();

    while (state.serverRunning)
    {
        auto loopStart = steady_clock::now();
        auto currentTime = steady_clock::now();
        double elapsed = duration_cast<duration<double>>(currentTime - previousTime).count();
        previousTime = currentTime;

        // Clamp excessive elapsed time (pauses, debugger stops) to prevent spiral
        if (elapsed > 0.1) elapsed = 0.1;

        accumulator += elapsed;

        int steps = 0;
        ModuleHost::instance().beginFixedTick();
        while (accumulator >= kFixedDt && steps < kMaxCatchup)
        {
            simulateOneServerTick(state);
            accumulator -= kFixedDt;
            ++steps;
        }
        ModuleHost::instance().endFixedTick();
        const bool cappedCatchup = steps >= kMaxCatchup && accumulator >= kFixedDt;

        // Log Hz every ~10 seconds
        if (state.tick - lastHzLog >= 600)
        {
            printf("[LISTEN SERVER THREAD] tick=%u accumulator=%.4f catchupSteps=%d\n",
                   state.tick, accumulator, steps);
            lastHzLog = state.tick;
        }

        // Flush structured logger periodically
        if (state.tick - lastLogTick >= 60)
        {
            ::StructuredLogger::instance().pollConfig();
            ::StructuredLogger::instance().tick();
            lastLogTick = state.tick;
        }

        const uint64_t loopUs = (uint64_t)duration_cast<duration<double, std::micro>>(
            steady_clock::now() - loopStart).count();
        recordServerLoopPerf(loopPerf, loopUs, cappedCatchup);
        const uint64_t perfNowMs = nowMs();
        if (perfNowMs - lastPerfMs >= 1000)
        {
            reportServerPerf("[LISTEN SERVER]", loopPerf, state.tick,
                             lastPerfTick, perfNowMs - lastPerfMs);
            lastPerfTick = state.tick;
            lastPerfMs = perfNowMs;
        }

        // Sleep only for the remaining time until the next tick is due.
        if (accumulator < kFixedDt)
        {
            double sleepSec = kFixedDt - accumulator;
            if (sleepSec > 0.001) // only sleep if more than 1ms
            {
                auto sleepUs = duration_cast<microseconds>(duration<double>(sleepSec));
                std::this_thread::sleep_for(sleepUs);
            }
        }
    }

    printf("[LISTEN SERVER THREAD] exiting\n");
}

void tickListenServer(ListenServerState& state, float /*dt*/)
{
    mimita::host::ServerAuthIdentityV1 authIdentity{};
    if (mimita::host::HostRuntime::instance().serverPolicyAuthIdentity(
            &authIdentity)) {
        PersistenceQueue::instance().setHostToken(authIdentity.token,
                                                  authIdentity.userId);
    }
    // No-op: the background thread handles all authoritative server ticks.
    // This function exists only as compatibility for the render-loop caller.
    if (!state.serverRunning && state.active)
    {
        static uint64_t lastWarn = 0;
        uint64_t now = nowMs();
        if (now - lastWarn > 5000)
        {
            printf("[LISTEN SERVER] thread died unexpectedly at tick=%u\n", state.tick);
            lastWarn = now;
        }
    }
}

// ─── Run server with explicit settings (from GUI process launch) ──────────

int runServerWithSettings(const ServerLaunchSettings& settings)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    printf("============================================================\n");
    printf("                    MiMITA SERVER\n");
    printf("          This window is the server you started.\n");
    printf("       Keep it open while people are playing.\n");
    printf("     Closing this window will stop your server.\n");
    printf("============================================================\n");
    printf("Version: %u\n", PROTOCOL_VERSION);
    printf("PID: %lu\n", (unsigned long)GetCurrentProcessId());
    printf("Mode: Headless authoritative server\n");
    printf("Map: %s\n", settings.mapName.c_str());
    printf("Players: 0 / %u\n", settings.maxPlayers);
    printf("Started by: mimita.exe Start Server button\n");
    printf("Executable path: ...\n");
    printf("Working directory: ...\n");
    printf("============================================================\n");
    printf("[SERVER CONFIG] map=%s gamemode=%s maxPlayers=%u npcs=%d count=%u\n",
           settings.mapName.c_str(), settings.gameMode.c_str(),
           settings.maxPlayers, (int)settings.startupNpcsEnabled, settings.startupNpcCount);

    // Use server launch settings by creating a LaunchOptions equivalent
    LaunchOptions opts;
    opts.server = true;
    opts.mapName = settings.mapName;
    opts.name = settings.serverName;
    opts.hostPlayerName = settings.hostPlayerName;
    opts.maxPlayers = settings.maxPlayers;
    opts.passwordProtected = settings.passwordProtected;
    opts.password = settings.password;
    opts.npcsEnabled = settings.startupNpcsEnabled;
    opts.npcCount = settings.startupNpcCount;
    opts.bind = "0.0.0.0:" + std::to_string(settings.port);
    opts.bindExplicit = true;
    opts.duel = settings.duelMode;
    opts.gamemodeId = settings.gamemodeId;
    opts.gameMode = settings.gameMode;
    opts.weaponSetId = settings.weaponSetId;
    opts.weaponSetExplicit = settings.weaponSetExplicit;
    opts.autoMapRotation = settings.autoMapRotation;
    opts.mapRotationMinutes = settings.mapRotationMinutes;
    opts.discordNotification = settings.discordNotification;

    // Delegate to existing runServer
    return runServer(opts);
}

} // namespace MimitaNet
