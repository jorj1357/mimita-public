// 07 31 2026, 15 30
/* purpose
* Implements the bounded latency queues and shared time/direction helpers.
* Queues packet copies with a randomized delivery time per packet.
* Flushes due packets while discarding packets from an old connection session.
* Does NOT own the simulator state, preset list, or packet classification.
*/

#include "network/badconn/badconn-internal.h"

#include <algorithm>
#include <chrono>
#include <deque>

namespace badconn {

uint64_t nowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool badConnDirectionApplies(Direction blockDirection, bool isOutgoing)
{
    if (blockDirection == Direction::Both)
        return true;
    if (blockDirection == Direction::In)
        return !isOutgoing;
    return isOutgoing;
}

int badConnDelayPacket(std::deque<BadConnQueuedPacket>& queue, size_t capacity,
                       const std::vector<uint8_t>& bytes, int minMs, int maxMs,
                       int baseMs, int jitterMs, BadConnJitterState& jitter,
                       uint64_t sessionGeneration, BadConnRng& rng, uint64_t now)
{
    if (queue.size() >= capacity)
        queue.pop_front();

    int delayMs;
    if (baseMs > 0)
    {
        // Realistic model: fixed base latency plus a correlated random walk.
        // Real delay drifts (queueing/interference) instead of jumping
        // independently per packet, so jitter evolves in bounded small steps.
        delayMs = baseMs + jitter.currentMs;
        const int step = std::max(1, jitterMs / 5);
        jitter.currentMs += rng.nextInt(-step, step);
        if (jitter.currentMs > jitterMs)
            jitter.currentMs = jitterMs;
        if (jitter.currentMs < -jitterMs)
            jitter.currentMs = -jitterMs;
    }
    else
    {
        delayMs = rng.nextInt(minMs, maxMs);
    }

    BadConnQueuedPacket queued;
    queued.bytes = bytes;
    queued.sessionGeneration = sessionGeneration;
    // FIFO link model: a packet enqueued later must never release before a
    // packet already in the queue, otherwise the simulator invents reordering
    // (up to max-min ms) that a real network link would not produce. Per-packet
    // jitter stays within the configured range; it only ever delays a packet.
    uint64_t deliverAtMs = now + static_cast<uint64_t>(std::max(0, delayMs));
    if (!queue.empty() && deliverAtMs < queue.back().deliverAtMs + 1)
        deliverAtMs = queue.back().deliverAtMs + 1;
    queued.deliverAtMs = deliverAtMs;
    queue.push_back(std::move(queued));
    return delayMs;
}

void badConnFlushDue(std::deque<BadConnQueuedPacket>& queue, uint64_t now,
                     uint64_t currentGeneration, std::vector<std::vector<uint8_t>>& due,
                     uint64_t& staleDiscarded)
{
    while (!queue.empty())
    {
        BadConnQueuedPacket& front = queue.front();
        if (front.deliverAtMs > now)
            break;
        if (front.sessionGeneration != currentGeneration)
        {
            ++staleDiscarded;
            queue.pop_front();
            continue;
        }
        due.push_back(std::move(front.bytes));
        queue.pop_front();
    }
}

} // namespace badconn
