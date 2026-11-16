// 2026-11-16
// Runtime packet-route evidence for the hotness census.
//
// One event per packet id/direction/generation. The static census scans the
// actual inbound dispatch branches; this runtime event proves the path that
// really handled the packet.

#pragma once

#include <cstdint>

namespace MimitaNet {
namespace PacketRoute {

enum class Direction : std::uint8_t {
    Client = 1,
    Server = 2,
};

void record(Direction direction, std::uint8_t packetId, const char* path,
            const char* source);

}  // namespace PacketRoute
}  // namespace MimitaNet
