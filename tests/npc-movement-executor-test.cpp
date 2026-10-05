// 10 04 2026
/* purpose
* Focused, world-independent tests for the generic NPC movement-executor
* selector: the pure context helper and the actor-preset movement_executor key
* on the movement-policy owner.
* Runs as a plain check() + exit-code harness (no gtest) and links
* src/npc/npc-movement-context.cpp + src/npc/npc-movement-policy.cpp.
* The live shared-executor parity is covered by the in-binary
* --npc-movement-executor-selftest.
*/

#include "npc/npc-movement-context.h"
#include "npc/npc-movement-policy.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int gChecks = 0;

void check(bool condition, const char* message)
{
    ++gChecks;
    if (!condition) {
        std::printf("[npc-movement-executor-test] FAIL %s\n", message);
        std::exit(1);
    }
}

void testExecutorHelpers()
{
    check(std::string(npcMovementExecutorName(NpcMovementExecutor::SandboxShared)) == "sandbox_shared",
          "sandbox_shared name");
    check(std::string(npcMovementExecutorName(NpcMovementExecutor::SurfaceNavigation)) == "surface_navigation",
          "surface_navigation name");
    check(std::string(npcMovementExecutorName(NpcMovementExecutor::Direct)) == "direct",
          "direct name");

    NpcMovementExecutor e = NpcMovementExecutor::Direct;
    check(npcMovementExecutorFromString("sandbox_shared", e) && e == NpcMovementExecutor::SandboxShared,
          "parse sandbox_shared");
    check(npcMovementExecutorFromString("surface_navigation", e) && e == NpcMovementExecutor::SurfaceNavigation,
          "parse surface_navigation");
    check(npcMovementExecutorFromString("direct", e) && e == NpcMovementExecutor::Direct,
          "parse direct");
    check(!npcMovementExecutorFromString("bogus", e), "unknown executor rejected");
}

void testPolicySetting()
{
    // Default is the shared Sandbox executor.
    NpcMovementPolicy def;
    check(def.movementExecutor == "sandbox_shared", "policy default is sandbox_shared");

    // Parse inside the npc_behavior block.
    {
        NpcMovementPolicy p;
        std::string err;
        check(parseNpcMovementPolicy(
                  nlohmann::json::parse(R"({"movement_executor":"surface_navigation"})"), p, err),
              "movement_executor parses");
        check(p.movementExecutor == "surface_navigation", "movement_executor value kept");
    }
    // Unknown value is rejected (caller keeps the last valid preset).
    {
        NpcMovementPolicy p;
        std::string err;
        check(!parseNpcMovementPolicy(
                  nlohmann::json::parse(R"({"movement_executor":"teleport"})"), p, err),
              "unknown movement_executor rejected");
    }
}

void testContextDefaults()
{
    NpcMovementContext ctx;
    check(!ctx.valid(), "context with no target is invalid");
    check(!ctx.hasObjective, "objective not provided by default");
    check(!ctx.hasGoalOverride, "goal override not provided by default");
}

} // namespace

int main()
{
    std::string report;
    check(npcMovementExecutorSelfTest(report), "npcMovementExecutorSelfTest");
    testExecutorHelpers();
    testPolicySetting();
    testContextDefaults();
    std::printf("[npc-movement-executor-test] PASS (%d checks)\n", gChecks);
    return 0;
}
