#include "network/packets.h"

namespace MimitaNet {

bool validHeader(const PacketHeader& header, uint8_t expectedType)
{
    return header.magic == PROTOCOL_MAGIC &&
           header.version == PROTOCOL_VERSION &&
           header.type == expectedType;
}

} // namespace MimitaNet
