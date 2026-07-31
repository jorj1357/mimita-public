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
    case MimitaNet::PACKET_DISAGREEMENT:
        return true;
    default:
        return false;
    }
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
bool processPacketDirection(Sim& sim, std::vector<uint8_t> bytes, bool isOutgoing)
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

    if (badConnDirectionApplies(sim.preset.loss.direction, isOutgoing) &&
        badConnRollLoss(sim.preset.loss, sim.rng))
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
                           holdMs, holdMs, sim.sessionGeneration, sim.rng, now);
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

const char* configPath()
{
    return "config/badconnconfig.json";
}

bool processOutgoing(const void* data, size_t bytes)
{
    Sim& sim = simInstance();
    if (!sim.active || !data || bytes == 0)
        return false;
    std::vector<uint8_t> copy(
        static_cast<const uint8_t*>(data),
        static_cast<const uint8_t*>(data) + bytes);
    return processPacketDirection(sim, std::move(copy), true);
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
        if (processPacketDirection(sim, std::move(bytes), false))
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
    clearQueues(sim);
    sim.active = false;
    sim.activePresetId.clear();
    sim.preset = BadConnPreset{};
    sim.blackoutState = BadConnBlackoutState{};
    sim.metrics = BadConnMetrics{};
    sim.presets = std::move(loadedPresets);
}

} // namespace badconn
