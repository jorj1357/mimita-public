// 07 21 2026, 18 32
/* purpose
* Declares the bounded raw UDP echo server used by process-level transport tests.
* Provides a graphics-free launch path that proves bind, receive, and sendto behavior.
* Keeps echo diagnostics separate from gameplay packet handling.
* Does NOT own gameplay server simulation, packet schemas, ICE, or coordinator calls.
* Does NOT change player movement, weapons, projectiles, or authoritative state.
* Does NOT run without a caller-supplied or default timeout.
*/

#pragma once

#include "network/net_mode.h"

namespace MimitaNet {

int runUdpEchoServer(const LaunchOptions& options);

} // namespace MimitaNet
