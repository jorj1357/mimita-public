// 09 14 2026
/* purpose
* Headless self-test for generic match phase/score/respawn ownership by a hot
* mode: runtime mode registration, hot-owned phase transition, team scoring from
* generic actor.killed facts, score-limit finish, and respawn policy.
* Does NOT own rendering/presentation or the network transport.
*/
#pragma once

#include <string>

bool runMatchPolicySelfTest(std::string& report);
