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
                       uint64_t sessionGeneration, BadConnRng& rng, uint64_t now)
{
    if (queue.size() >= capacity)
        queue.pop_front();

    const int delayMs = rng.nextInt(minMs, maxMs);
    BadConnQueuedPacket queued;
    queued.bytes = bytes;
    queued.sessionGeneration = sessionGeneration;
    queued.deliverAtMs = now + static_cast<uint64_t>(std::max(0, delayMs));
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
