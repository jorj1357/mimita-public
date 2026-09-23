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
    // Hot-reload safety hook: invalidate any callback generation and drop queued
    // callback events before a module swap. Default no-op for transports whose
    // callbacks cannot outlive a generation; ICE overrides it.
    virtual void quiesceForReload() {}
};

// Hot-reload safety registry for live transports. Transports with OS/thread
// callbacks (ICE) register here; the reload barrier calls quiesceAllTransports()
// before swapping a module generation so no callback can outlive its generation.
// The registry holds non-owning pointers only; a closed transport must unregister.
namespace MimitaTransport {

void registerTransport(IGameTransport* transport);
void unregisterTransport(IGameTransport* transport);
// Invalidate callback generations and drop queued callback events on every live
// transport. Safe to call with none registered.
void quiesceAllTransports();

} // namespace MimitaTransport
