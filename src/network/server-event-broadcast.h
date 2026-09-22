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

} // namespace MimitaNet
