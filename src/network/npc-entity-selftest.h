// 09 14 2026
/* purpose
* Headless self-test for generic NPC/monster-like entity lifecycle + health:
* generic entity creation, authoritative ActorHealthState component, generic
* damage.apply, generic component replication, generic destroy, stale rejection,
* and schema migration.
* Does NOT own simulation, gameplay policy, or the renderer.
*/
#pragma once

#include <string>

bool runNpcEntitySelfTest(std::string& report);
