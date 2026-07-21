// 07 21 2026, 18 32
/* purpose
* Implements shared packet-header validation for MiMITA networking code.
* Keeps protocol magic, version, and packet type checks centralized.
* Provides a tiny helper reused by tests and runtime packet handlers.
* Does NOT own socket transport, packet dispatch, gameplay authority, or ICE.
* Does NOT mutate packet layouts, movement rules, or weapon behavior.
* Does NOT allocate, send, receive, or buffer network datagrams.
*/

#include "network/packets.h"

namespace MimitaNet {

bool validHeader(const PacketHeader& header, uint8_t expectedType)
{
    return header.magic == PROTOCOL_MAGIC &&
           header.version == PROTOCOL_VERSION &&
           header.type == expectedType;
}

} // namespace MimitaNet
