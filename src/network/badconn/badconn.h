// 07 31 2026, 15 30
/* purpose
* Declares the per-client bad-connection simulator types and public API.
* Provides presets (latency, loss, reorder, blackout), packet hooks, and metrics.
* Keeps badconn state process-local so each game client simulates independently.
* Does NOT own sockets, packet schemas, or the authoritative server simulation.
* Does NOT touch coordinator, ICE, STUN, or TURN traffic.
* Does NOT persist any preset between game launches.
*/

#pragma once

#include "network/game-transport.h"

#include <cstdint>
#include <string>
#include <vector>

namespace badconn {

enum class Direction : uint8_t
{
    Both,
    In,
    Out
};

struct BadConnLatency
{
    bool enabled = false;
    Direction direction = Direction::Both;
    // Uniform per-packet delay range (legacy model).
    int minMs = 0;
    int maxMs = 0;
    // Realistic model: fixed base latency + correlated random-walk jitter.
    // When baseMs > 0 this model is used instead of minMs/maxMs.
    int baseMs = 0;
    int jitterMs = 0;
};

struct BadConnLoss
{
    bool enabled = false;
    Direction direction = Direction::Both;
    float minPercent = 0.0f;
    float maxPercent = 0.0f;
    // Burst model: occasionally enter a short high-loss burst (bad wifi).
    float burstPercent = 0.0f;
    float burstProbability = 0.0f;
};

struct BadConnReorder
{
    bool enabled = false;
    Direction direction = Direction::Both;
    float minPercent = 0.0f;
    float maxPercent = 0.0f;
    int window = 8;
};

struct BadConnBlackout
{
    bool enabled = false;
    Direction direction = Direction::Both;
    double startProbabilityPerSecond = 0.0;
    int minMs = 0;
    int maxMs = 0;
    int cooldownMs = 0;
    bool releaseQueued = false;
};

struct BadConnPreset
{
    std::string id;
    std::string name;
    uint32_t seed = 0;
    BadConnLatency latency;
    BadConnLoss loss;
    BadConnReorder reorder;
    BadConnBlackout blackout;
};

struct BadConnMetrics
{
    uint64_t packetsDelayed = 0;
    uint64_t packetsDropped = 0;
    uint64_t packetsReordered = 0;
    uint64_t attackRetriesCollapsed = 0;
    uint64_t staleDiscarded = 0;
    uint64_t blackoutsStarted = 0;
    bool blackoutActive = false;
    uint64_t blackoutRemainingMs = 0;
    int lastLatencyMs = 0;
    size_t outQueueSize = 0;
    size_t inQueueSize = 0;
};

const std::string& configPath();
bool loadConfig(const std::string& path);
const std::vector<BadConnPreset>& presets();
bool activatePreset(const std::string& id);
void disable();
bool active();
const std::string& activePresetId();
const std::string& activePresetName();
const BadConnMetrics& metrics();

// Connection lifecycle hooks (called from multiplayer send/receive code).
void noteConnectionEstablished();
void noteConnectionTeardown();

// Packet hooks. processOutgoing returns true when the packet was consumed
// (delayed, reordered, or dropped) and the caller must NOT send it.
bool processOutgoing(const void* data, size_t bytes);
void processIncoming(std::vector<ReceivedPacket>& raw);
void tick(class IGameTransport* transport);

} // namespace badconn
