// 07 30 2026, 13 26
/* purpose
* Implements a server-authoritative token-bucket rate limiter for chat messages.
* Allows burst of up to bucketSize messages, then refills at refillRate per tick.
* Configurable via chatconfig.json fields: bucketSize, refillRate, maxRefillTicks.
* Does NOT own player state, network delivery, or message validation.
* Does NOT track per-player identity or store message history.
*/
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct ChatRateLimitConfig
{
    double bucketSize = 5.0;
    double refillRate = 1.0 / 30.0; // one token per 30 ticks
    uint64_t maxRefillTicks = 300;
};

class ChatRateLimiter
{
public:
    void setConfig(const ChatRateLimitConfig& config) { mConfig = config; }
    const ChatRateLimitConfig& getConfig() const { return mConfig; }

    // Returns true if the player can send a message at the given tick.
    // If true, consumes one token.
    // If false, sets outRemainingTicks to the number of ticks the player must wait.
    bool canSend(uint32_t playerId, uint64_t currentTick, uint64_t* outRemainingTicks = nullptr);

    // Record a successful send (consumes token, resets state if needed).
    void recordSend(uint32_t playerId, uint64_t currentTick);

    // Remove player state (on disconnect).
    void removePlayer(uint32_t playerId);

    void reset();

private:
    struct PlayerState
    {
        double tokens = 0.0;
        uint64_t lastUpdateTick = 0;
    };

    ChatRateLimitConfig mConfig;
    std::unordered_map<uint32_t, PlayerState> mStates;
};
