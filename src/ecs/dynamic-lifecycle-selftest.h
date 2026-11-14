// 09 14 2026
/* purpose
* Headless self-test for the generic dynamic entity/component lifecycle:
* create/destroy, attach/read/write/remove/enumerate/inspect, relationships,
* activation-time schema migration, and safe failure of a bad migration.
* Does NOT own gameplay systems or the live-code pipeline.
*/
#pragma once

#include <string>

bool runDynamicLifecycleSelfTest(std::string& report);
