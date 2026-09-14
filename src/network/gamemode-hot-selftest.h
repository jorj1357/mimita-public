// 09 14 2026
/* purpose
* Headless self-test for runtime-registered hot gamemodes: mode discovery,
* data-driven active-domain routing, domain-scoped events, hot authoritative
* scoring + win through generic capabilities, the packet score bridge, and a
* mode schema migration.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#pragma once

#include <string>

bool runGamemodeHotSelfTest(std::string& report);
