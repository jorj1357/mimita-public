// 08 05 2026, 00 00
/* purpose
* Declares terminal commands for inspecting and previewing VIP appearance state.
* Keeps account-owned VIP diagnostics discoverable through the in-game console.
* Exposes a small registration hook used by main subsystem startup.
* DOES NOT verify payments, mint entitlements, or contact the website.
* DOES NOT change server-authoritative multiplayer VIP state.
* DOES NOT render HUD names directly.
*/

#pragma once

void registerVipCommands();
