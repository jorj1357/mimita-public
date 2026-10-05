// 10 04 2026
// Pure helpers for the shared NPC movement context / executor selector.
#include "npc/npc-movement-context.h"

const char* npcMovementExecutorName(NpcMovementExecutor executor)
{
    switch (executor) {
        case NpcMovementExecutor::SandboxShared:     return "sandbox_shared";
        case NpcMovementExecutor::SurfaceNavigation: return "surface_navigation";
        case NpcMovementExecutor::Direct:            return "direct";
    }
    return "sandbox_shared";
}

bool npcMovementExecutorFromString(const std::string& value, NpcMovementExecutor& out)
{
    if (value == "sandbox_shared")     { out = NpcMovementExecutor::SandboxShared; return true; }
    if (value == "surface_navigation") { out = NpcMovementExecutor::SurfaceNavigation; return true; }
    if (value == "direct")             { out = NpcMovementExecutor::Direct; return true; }
    return false;
}

bool npcMovementExecutorSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    if (std::string(npcMovementExecutorName(NpcMovementExecutor::SandboxShared)) != "sandbox_shared")
        fail("sandbox_shared name");
    if (std::string(npcMovementExecutorName(NpcMovementExecutor::SurfaceNavigation)) != "surface_navigation")
        fail("surface_navigation name");
    if (std::string(npcMovementExecutorName(NpcMovementExecutor::Direct)) != "direct")
        fail("direct name");

    NpcMovementExecutor e = NpcMovementExecutor::Direct;
    if (!npcMovementExecutorFromString("sandbox_shared", e) || e != NpcMovementExecutor::SandboxShared)
        fail("parse sandbox_shared");
    if (!npcMovementExecutorFromString("surface_navigation", e) || e != NpcMovementExecutor::SurfaceNavigation)
        fail("parse surface_navigation");
    if (!npcMovementExecutorFromString("direct", e) || e != NpcMovementExecutor::Direct)
        fail("parse direct");
    if (npcMovementExecutorFromString("bogus", e))
        fail("unknown executor must be rejected");

    NpcMovementContext ctx;
    if (ctx.valid()) fail("default context has no target and must be invalid");
    if (!ctx.objective.objectivePos.x && ctx.hasGoalOverride)
        fail("default context has no goal override");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
