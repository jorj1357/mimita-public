// 09 23 2026
/* purpose
* Declare the versioned schema ids for the core gameplay packets so the cold
* dispatcher and hot codecs agree on (schemaId, schemaVersion) without either
* side depending on the other's C++ layout. This is the start of packets.h
* becoming a schema declaration layer: the wire struct stays the source of the
* layout, but the schema identity is a stable hash that survives a header edit
* as a NEW version rather than an unsafe in-place cast.
* Does NOT define layouts, codecs, sockets, or transport.
*/
#pragma once

#include "hot-reload/game-api.h"

namespace MimitaNet::Schemas {

// Schema ids are stable hashes. Bump the version for an incompatible layout
// change; keep the old version registered during a transition.
static constexpr std::uint64_t kPing = gameHash("packet.ping");
static constexpr std::uint32_t kPingVersion = 1;

static constexpr std::uint64_t kHotPing = gameHash("packet.hot.ping");
static constexpr std::uint32_t kHotPingVersion = 1;

} // namespace MimitaNet::Schemas
