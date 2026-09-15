// 09 14 2026
/* purpose
* Headless self-test for generic NPC/actor team/role/profile/target state:
* authoritative dynamic components + relationship, generic replication, stale
* safety, migration, and a hot AI consumer reading the generic state.
* Does NOT own simulation, gameplay policy, or the renderer.
*/
#pragma once

#include <string>

bool runNpcActorStateSelfTest(std::string& report);
