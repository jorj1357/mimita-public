#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_network_policy {

constexpr std::uint32_t ABI_VERSION = 1;

struct SnapshotPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t tick;
    std::uint32_t chunkCount;
    std::uint32_t entityCount;
    std::uint32_t playerCount;
    std::uint64_t queueBytes;
    std::uint32_t queueChunks;
    std::int32_t redundancyCopies;
    std::int32_t resendPrevious;
    std::int32_t partialKeepalive;
    std::int32_t maxBufferedTicks;
    std::int32_t maxAvatarQueueChunks;
    std::int32_t result;
};

struct ReconnectPolicyV1
{
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint64_t lastHeardAgeMs;
    std::uint64_t snapshotAgeMs;
    std::uint32_t receivedChunks;
    std::uint32_t expectedChunks;
    std::int32_t reconnectAttempts;
    std::int32_t action;
    std::int32_t backoffMs;
    std::int32_t retryIntervalMs;
    std::int32_t graceMs;
    std::int32_t result;
};

} // namespace mimita_network_policy
