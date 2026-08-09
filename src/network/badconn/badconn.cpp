// 07 31 2026, 15 30
/* purpose
* Implements the per-client bad-connection simulator singleton and packet hooks.
* Routes outgoing and incoming packets through the active preset's impairments.
* Owns latency/reorder queues, blackout state, metrics, and session-generation guards.
* Does NOT own config file parsing or the impairment roll helpers.
* Does NOT touch the server simulation, sockets, or coordinator traffic.
* Does NOT persist any preset between game launches.
*/

#include "network/badconn/badconn.h"
#include "network/badconn/badconn-internal.h"
#include "network/packets.h"
#include "debug/debug-log.h"
#include "utils/path_utils.h"

#include <deque>

namespace badconn {

namespace {

constexpr size_t kLatencyQueueCapacity = 4096;
constexpr size_t kReorderQueueCapacity = 32;

struct Sim
{
    bool active = false;
    BadConnPreset preset;
    std::string activePresetId;
    std::vector<BadConnPreset> presets;
    BadConnRng rng;
    uint64_t sessionGeneration = 1;
    std::deque<BadConnQueuedPacket> outLatency;
    std::deque<BadConnQueuedPacket> inLatency;
    std::deque<BadConnQueuedPacket> outReorder;
    std::deque<BadConnQueuedPacket> inReorder;
    BadConnBlackoutState blackoutState;
    BadConnJitterState outJitter;
    BadConnJitterState inJitter;
    BadConnBurstState outLossBurst;
    BadConnBurstState inLossBurst;
    BadConnMetrics metrics;
};

Sim& simInstance()
{
    static Sim s;
    return s;
}

uint8_t packetType(const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < sizeof(MimitaNet::PacketHeader))
        return 0;
    const MimitaNet::PacketHeader* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(bytes.data());
    if (header->magic != MimitaNet::PROTOCOL_MAGIC ||
        header->version != MimitaNet::PROTOCOL_VERSION)
        return 0;
    return header->type;
}

bool isExemptType(uint8_t type)
{
    switch (type)
    {
    case MimitaNet::PACKET_HELLO:
    case MimitaNet::PACKET_WELCOME:
    case MimitaNet::PACKET_JOIN_REQUEST:
    case MimitaNet::PACKET_JOIN_ACCEPT:
    case MimitaNet::PACKET_JOIN_REJECT:
    case MimitaNet::PACKET_RECONNECT_REQUEST:
    case MimitaNet::PACKET_RECONNECT_ACCEPT:
    case MimitaNet::PACKET_CLIENT_MAP_READY:
    case MimitaNet::PACKET_DISCONNECT:
    case MimitaNet::PACKET_SPAWN_ACK:
    case MimitaNet::PACKET_SPAWN_ACTIVATED:
    case MimitaNet::PACKET_RELIABLE_EVENT_ACK:
    case MimitaNet::PACKET_PLAYER_LIST:
    case MimitaNet::PACKET_VIP_STYLE_EVENT:
    case MimitaNet::PACKET_DISAGREEMENT:
    case MimitaNet::PACKET_PLAYER_CONNECTION_STATE:
        return true;
    default:
        return false;
    }
}

// Returns true when bytes is an attack request that is already in flight in
// the outbound queues (same requestId, playerId, and spawn generation). The
// client retries unacknowledged attacks every 100 ms; without this collapse
// each retry would re-roll a fresh random latency and let a retry overtake the
// movement packets it depends on, letting attacks bypass the configured delay.
bool isDuplicateInFlightAttack(const Sim& sim, const std::vector<uint8_t>& bytes)
{
    if (bytes.size() < sizeof(MimitaNet::AttackRequestPacket))
        return false;
    const MimitaNet::PacketHeader* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(bytes.data());
    if (header->magic != MimitaNet::PROTOCOL_MAGIC ||
        header->version != MimitaNet::PROTOCOL_VERSION ||
        header->type != MimitaNet::PACKET_ATTACK_REQUEST)
        return false;
    const MimitaNet::AttackRequestPacket* req =
        reinterpret_cast<const MimitaNet::AttackRequestPacket*>(bytes.data());

    const auto matches = [req](const BadConnQueuedPacket& queued)
    {
        if (queued.bytes.size() < sizeof(MimitaNet::AttackRequestPacket))
            return false;
        const MimitaNet::PacketHeader* h =
            reinterpret_cast<const MimitaNet::PacketHeader*>(queued.bytes.data());
        if (h->magic != MimitaNet::PROTOCOL_MAGIC ||
            h->version != MimitaNet::PROTOCOL_VERSION ||
            h->type != MimitaNet::PACKET_ATTACK_REQUEST)
            return false;
        const MimitaNet::AttackRequestPacket* q =
            reinterpret_cast<const MimitaNet::AttackRequestPacket*>(queued.bytes.data());
        return q->requestId == req->requestId &&
               q->header.playerId == req->header.playerId &&
               q->spawnGeneration == req->spawnGeneration;
    };

    for (const BadConnQueuedPacket& queued : sim.outLatency)
        if (matches(queued))
            return true;
    for (const BadConnQueuedPacket& queued : sim.outReorder)
        if (matches(queued))
            return true;
    return false;
}

void clearQueues(Sim& sim)
{
    sim.outLatency.clear();
    sim.inLatency.clear();
    sim.outReorder.clear();
    sim.inReorder.clear();
}

const char* directionName(Direction direction)
{
    switch (direction)
    {
    case Direction::In: return "in";
    case Direction::Out: return "out";
    default: return "both";
    }
}

// Returns true when the packet was consumed (delayed, reordered, or dropped).
// Takes a const reference: classification never owns the bytes, and exempt
// (pass-through) packets must reach the caller intact. Consuming paths copy
// into their queues via badConnDelayPacket.
bool processPacketDirection(Sim& sim, const std::vector<uint8_t>& bytes, bool isOutgoing)
{
    const uint64_t now = nowMs();
    const uint8_t type = packetType(bytes);
    if (isExemptType(type))
        return false;

    if (sim.blackoutState.active &&
        badConnDirectionApplies(sim.preset.blackout.direction, isOutgoing))
    {
        ++sim.metrics.packetsDropped;
        return true;
    }

    // Collapse attack retries that duplicate an in-flight request so they
    // cannot re-roll a cheaper latency and overtake their own movement.
    if (isOutgoing && isDuplicateInFlightAttack(sim, bytes))
    {
        ++sim.metrics.attackRetriesCollapsed;
        return true;
    }

    if (badConnDirectionApplies(sim.preset.loss.direction, isOutgoing) &&
        badConnRollLoss(sim.preset.loss, sim.rng,
                        isOutgoing ? sim.outLossBurst : sim.inLossBurst))
    {
        ++sim.metrics.packetsDropped;
        return true;
    }

    if (badConnDirectionApplies(sim.preset.reorder.direction, isOutgoing) &&
        badConnRollReorder(sim.preset.reorder, sim.rng))
    {
        std::deque<BadConnQueuedPacket>& buffer =
            isOutgoing ? sim.outReorder : sim.inReorder;
        const int holdMs = badConnReorderDelayMs(sim.preset.reorder);
        badConnDelayPacket(buffer, kReorderQueueCapacity, bytes,
                           holdMs, holdMs, 0, 0,
                           isOutgoing ? sim.outJitter : sim.inJitter,
                           sim.sessionGeneration, sim.rng, now);
        ++sim.metrics.packetsReordered;
        return true;
    }

    if (sim.preset.latency.enabled &&
        badConnDirectionApplies(sim.preset.latency.direction, isOutgoing))
    {
        std::deque<BadConnQueuedPacket>& queue =
            isOutgoing ? sim.outLatency : sim.inLatency;
        const int delayMs = badConnDelayPacket(
            queue, kLatencyQueueCapacity, bytes,
            sim.preset.latency.minMs, sim.preset.latency.maxMs,
            sim.preset.latency.baseMs, sim.preset.latency.jitterMs,
            isOutgoing ? sim.outJitter : sim.inJitter,
            sim.sessionGeneration, sim.rng, now);
        sim.metrics.lastLatencyMs = delayMs;
        ++sim.metrics.packetsDelayed;
        return true;
    }

    return false;
}

void sendAll(IGameTransport* transport, const std::vector<std::vector<uint8_t>>& packets)
{
    if (!transport)
        return;
    for (const std::vector<uint8_t>& packet : packets)
        transport->send(packet.data(), packet.size());
}

} // namespace

const std::string& configPath()
{
    // Badconn presets live inside the shared networking config file so the
    // networking settings have a single source of truth that hot-reloads.
    static const std::string path = resolveAssetPath("config/networkingconfig.json");
    return path;
}

bool processOutgoing(const void* data, size_t bytes)
{
    Sim& sim = simInstance();
    if (!sim.active || !data || bytes == 0)
        return false;
    std::vector<uint8_t> copy(
        static_cast<const uint8_t*>(data),
        static_cast<const uint8_t*>(data) + bytes);
    return processPacketDirection(sim, copy, true);
}

void processIncoming(std::vector<ReceivedPacket>& raw)
{
    Sim& sim = simInstance();
    if (!sim.active)
        return;

    const uint64_t now = nowMs();
    const uint64_t generation = sim.sessionGeneration;

    std::vector<ReceivedPacket> deliver;
    deliver.reserve(raw.size() + 8);
    for (ReceivedPacket& received : raw)
    {
        std::vector<uint8_t> bytes = std::move(received.bytes);
        if (processPacketDirection(sim, bytes, false))
            continue;
        ReceivedPacket passThrough;
        passThrough.bytes = std::move(bytes);
        passThrough.receivedAtMs = now;
        deliver.push_back(std::move(passThrough));
    }

    std::vector<std::vector<uint8_t>> due;
    badConnFlushDue(sim.inReorder, now, generation, due, sim.metrics.staleDiscarded);
    for (std::vector<uint8_t>& packet : due)
    {
        ReceivedPacket delivered;
        delivered.bytes = std::move(packet);
        delivered.receivedAtMs = now;
        deliver.push_back(std::move(delivered));
    }
    due.clear();
    badConnFlushDue(sim.inLatency, now, generation, due, sim.metrics.staleDiscarded);
    for (std::vector<uint8_t>& packet : due)
    {
        ReceivedPacket delivered;
        delivered.bytes = std::move(packet);
        delivered.receivedAtMs = now;
        deliver.push_back(std::move(delivered));
    }

    raw.clear();
    for (ReceivedPacket& packet : deliver)
        raw.push_back(std::move(packet));
}

void tick(IGameTransport* transport)
{
    Sim& sim = simInstance();
    if (!sim.active)
        return;

    const uint64_t now = nowMs();
    bool started = false;
    bool ended = false;
    badConnTickBlackout(sim.blackoutState, sim.preset.blackout, sim.rng,
                        now, started, ended);
    if (started)
    {
        ++sim.metrics.blackoutsStarted;
        if (!sim.preset.blackout.releaseQueued)
            clearQueues(sim);
        Debug::warn(Debug::Category::Networking,
                    "[BADCONN] blackout STARTED duration=%llu ms direction=%s\n",
                    (unsigned long long)(sim.blackoutState.endMs - now),
                    directionName(sim.preset.blackout.direction));
    }
    if (ended)
        Debug::warn(Debug::Category::Networking, "[BADCONN] blackout ENDED\n");

    sim.metrics.blackoutActive = sim.blackoutState.active;
    sim.metrics.blackoutRemainingMs = sim.blackoutState.active
        ? (sim.blackoutState.endMs > now ? sim.blackoutState.endMs - now : 0)
        : 0;

    std::vector<std::vector<uint8_t>> due;
    badConnFlushDue(sim.outReorder, now, sim.sessionGeneration, due,
                    sim.metrics.staleDiscarded);
    sendAll(transport, due);
    due.clear();
    badConnFlushDue(sim.outLatency, now, sim.sessionGeneration, due,
                    sim.metrics.staleDiscarded);
    sendAll(transport, due);

    sim.metrics.outQueueSize = sim.outLatency.size() + sim.outReorder.size();
    sim.metrics.inQueueSize = sim.inLatency.size() + sim.inReorder.size();

    // Aggregated impairment counters (once per second, never per packet).
    Debug::logThrottled(Debug::Category::Networking, "badconn-aggregate", 1.0,
                        "[BADCONN] agg delayed=%llu dropped=%llu reordered=%llu attackCollapsed=%llu stale=%llu "
                        "blackouts=%llu queues(out=%zu/in=%zu)\n",
                        (unsigned long long)sim.metrics.packetsDelayed,
                        (unsigned long long)sim.metrics.packetsDropped,
                        (unsigned long long)sim.metrics.packetsReordered,
                        (unsigned long long)sim.metrics.attackRetriesCollapsed,
                        (unsigned long long)sim.metrics.staleDiscarded,
                        (unsigned long long)sim.metrics.blackoutsStarted,
                        sim.metrics.outQueueSize, sim.metrics.inQueueSize);
}

void noteConnectionEstablished()
{
    Sim& sim = simInstance();
    ++sim.sessionGeneration;
    if (sim.sessionGeneration == 0)
        sim.sessionGeneration = 1;
}

void noteConnectionTeardown()
{
    Sim& sim = simInstance();
    clearQueues(sim);
    sim.blackoutState = BadConnBlackoutState{};
    ++sim.sessionGeneration;
    if (sim.sessionGeneration == 0)
        sim.sessionGeneration = 1;
}

bool activatePreset(const std::string& id)
{
    Sim& sim = simInstance();
    for (const BadConnPreset& preset : sim.presets)
    {
        if (preset.id != id)
            continue;
        clearQueues(sim);
        sim.active = true;
        sim.preset = preset;
        sim.activePresetId = preset.id;
        sim.rng.seed(preset.seed);
        sim.blackoutState = BadConnBlackoutState{};
        sim.metrics = BadConnMetrics{};
        Debug::warn(Debug::Category::Networking,
                    "[BADCONN] preset %s '%s' activated (client-local)\n",
                    preset.id.c_str(), preset.name.c_str());
        return true;
    }
    return false;
}

void disable()
{
    Sim& sim = simInstance();
    clearQueues(sim);
    sim.active = false;
    sim.preset = BadConnPreset{};
    sim.activePresetId.clear();
    sim.metrics = BadConnMetrics{};
    Debug::warn(Debug::Category::Networking,
                "[BADCONN] all impairments disabled\n");
}

bool active()
{
    return simInstance().active;
}

const std::string& activePresetId()
{
    return simInstance().activePresetId;
}

const std::string& activePresetName()
{
    return simInstance().preset.name;
}

const BadConnMetrics& metrics()
{
    return simInstance().metrics;
}

const std::vector<BadConnPreset>& presets()
{
    return simInstance().presets;
}

void applyLoadedPresets(std::vector<BadConnPreset> loadedPresets)
{
    Sim& sim = simInstance();
    const std::string previousActiveId = sim.active ? sim.activePresetId : std::string();
    clearQueues(sim);
    sim.active = false;
    sim.activePresetId.clear();
    sim.preset = BadConnPreset{};
    sim.blackoutState = BadConnBlackoutState{};
    sim.metrics = BadConnMetrics{};
    sim.presets = std::move(loadedPresets);

    // Hot-reload: keep the currently active preset active (with new values).
    if (!previousActiveId.empty())
    {
        for (const BadConnPreset& preset : sim.presets)
        {
            if (preset.id != previousActiveId)
                continue;
            sim.active = true;
            sim.preset = preset;
            sim.activePresetId = preset.id;
            sim.rng.seed(preset.seed);
            sim.blackoutState = BadConnBlackoutState{};
            sim.metrics = BadConnMetrics{};
            Debug::warn(Debug::Category::Networking,
                        "[BADCONN] preset %s '%s' re-activated after config reload\n",
                        preset.id.c_str(), preset.name.c_str());
            break;
        }
    }
}

} // namespace badconn
