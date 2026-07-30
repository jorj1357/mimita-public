// 07 30 2026, 13 26
/* purpose
* Implements token-bucket chat rate limiting.
* Each player has a token balance up to bucketSize.
* Tokens refill at refillRate per tick up to maxRefillTicks.
* A send consumes 1 token. If tokens < 1, the send is rejected.
* Does NOT own network state, packet delivery, or player identity.
*/
#include "chat-rate-limiter.h"
#include <algorithm>
#include <cmath>

bool ChatRateLimiter::canSend(uint32_t playerId, uint64_t currentTick, uint64_t* outRemainingTicks)
{
    PlayerState& state = mStates[playerId];

    uint64_t elapsedTicks = currentTick - state.lastUpdateTick;
    if (elapsedTicks > mConfig.maxRefillTicks)
        elapsedTicks = mConfig.maxRefillTicks;

    state.tokens += static_cast<double>(elapsedTicks) * mConfig.refillRate;
    if (state.tokens > mConfig.bucketSize)
        state.tokens = mConfig.bucketSize;
    state.lastUpdateTick = currentTick;

    if (state.tokens >= 1.0)
        return true;

    if (outRemainingTicks)
    {
        double needed = 1.0 - state.tokens;
        *outRemainingTicks = static_cast<uint64_t>(std::ceil(needed / mConfig.refillRate));
    }
    return false;
}

void ChatRateLimiter::recordSend(uint32_t playerId, uint64_t currentTick)
{
    PlayerState& state = mStates[playerId];
    state.tokens -= 1.0;
    if (state.tokens < 0.0)
        state.tokens = 0.0;
}

void ChatRateLimiter::removePlayer(uint32_t playerId)
{
    mStates.erase(playerId);
}

void ChatRateLimiter::reset()
{
    mStates.clear();
}
