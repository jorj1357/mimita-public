// 2026-09-30
// Exposes the deterministic destruction-replication self-test for game-cli.cpp.
#pragma once

#include <string>

namespace MimitaNet {
bool destructionReplicationSelfTest(std::string* outSummary);
}