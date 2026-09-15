// 09 14 2026
/* purpose
* Headless self-test for the authoritative gameplay.60 boundary: one domain run
* per server tick, a hot NPC/monster combat system performing a real
* authoritative decision + mutation through generic capabilities, generic
* actor state driving behavior, and stale/destroyed entity safety.
* Does NOT own simulation, gameplay policy, or the renderer.
*/
#pragma once

#include <string>

bool runGameplayBoundarySelfTest(std::string& report);
