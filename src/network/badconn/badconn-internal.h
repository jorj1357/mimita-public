// 07 31 2026, 15 30
/* purpose
* Declares shared internal state and impairment helpers used by the badconn modules.
* Provides the bounded queued-packet type, per-client RNG, and blackout state.
* Keeps impairment implementations in focused per-feature files.
* Does NOT expose internals through the public badconn API.
* Does NOT own the preset list, config loading, or packet routing.
*/

#pragma once

#include "network/badconn/badconn.h"

#include <cstdint>
#include <deque>
#include <random>
#include <vector>
namespace badconn {

struct BadConnQueuedPacket
{
    std::vector<uint8_t> bytes;
    uint64_t deliverAtMs = 0;
    uint64_t sessionGeneration = 0;
};

class BadConnRng
{
public:
    void seed(uint32_t seed);
    int nextInt(int lowInclusive, int highInclusive);
    float nextPercent();
    double nextUnit();

private:
    std::mt19937 mGen;
};

struct BadConnBlackoutState
{
    bool active = false;
    uint64_t endMs = 0;
    uint64_t lastEndMs = 0;
};

uint64_t nowMs();

bool badConnDirectionApplies(Direction blockDirection, bool isOutgoing);

// latency (also used for bounded reorder holds)
int badConnDelayPacket(std::deque<BadConnQueuedPacket>& queue, size_t capacity,
                       const std::vector<uint8_t>& bytes, int minMs, int maxMs,
                       uint64_t sessionGeneration, BadConnRng& rng, uint64_t now);
void badConnFlushDue(std::deque<BadConnQueuedPacket>& queue, uint64_t now,
                     uint64_t currentGeneration, std::vector<std::vector<uint8_t>>& due,
                     uint64_t& staleDiscarded);

// loss
bool badConnRollLoss(const BadConnLoss& block, BadConnRng& rng);

// reorder
bool badConnRollReorder(const BadConnReorder& block, BadConnRng& rng);
int badConnReorderDelayMs(const BadConnReorder& block);

// blackout
void badConnTickBlackout(BadConnBlackoutState& state, const BadConnBlackout& block,
                         BadConnRng& rng, uint64_t now, bool& started, bool& ended);

// Store parsed presets into the simulator (clears the active preset first).
void applyLoadedPresets(std::vector<BadConnPreset> presets);

} // namespace badconn

#include "network/badconn/badconn-rng.h"
