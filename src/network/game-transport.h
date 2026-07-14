#pragma once

#include <vector>
#include <cstdint>
#include <memory>

struct ReceivedPacket
{
    std::vector<uint8_t> bytes;
    uint64_t receivedAtMs = 0;
};

class IGameTransport
{
public:
    virtual ~IGameTransport() = default;
    virtual bool send(const void* data, size_t size) = 0;
    virtual void poll(std::vector<ReceivedPacket>& out) = 0;
    virtual bool connected() const = 0;
    virtual void close() = 0;
};
