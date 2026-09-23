// 09 22 2026
/* purpose
* Generic gameplay-event transport primitive. Hot code builds any packet itself
* and asks the kernel only to assign the reliable ticket and queue/send the
* bytes. No per-event kernel slot; packet contents stay hot-editable.
* Does NOT own any specific event's meaning.
*/
#pragma once

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Assigns the next reliable event id/session ticket.
void serverEventNextId(GameReliableEventTicketV1* out);

// Queues (reliable) or sends (unreliable) a caller-built packet.
void serverEventBroadcast(const GameEventBroadcastV1& request);

// Sends caller-built bytes back to ONE connection (the originating player),
// over its own transport when present. Used by the generic hot-packet handler
// to answer a received packet through the `net.packet-reply` capability. The
// bytes may themselves be a hot-coded datagram.
void serverPacketReply(std::uint32_t connectionId, const void* bytes,
                       std::uint32_t size);

} // namespace MimitaNet
