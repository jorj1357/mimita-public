// 10 02 2026
// Adapter from NavigationRequest to the existing NpcGoal plus segment
// classification. No pathfinding here; the navigator remains the owner.
#include "npc/npc-nav-request.h"

#include <cmath>

NpcGoal navigationRequestToGoal(const NavigationRequest& request)
{
    NpcGoal goal;
    goal.kind = request.goal;
    goal.targetActorId = request.targetActorId;
    goal.targetPos = request.destination;
    goal.desiredDistance = request.desiredDistance;
    goal.tolerance = request.tolerance;
    return goal;
}

NavSegmentType classifySegment(float heightDelta,
                               bool hasGap,
                               const MovementCapabilities& caps,
                               float maxJumpHeight)
{
    if (hasGap) {
        // A gap is crossable only with a jump (or dash, folded into canJump in
        // this slice); otherwise it is unreachable.
        return caps.canJump ? NavSegmentType::Gap : NavSegmentType::Unreachable;
    }
    if (heightDelta > 0.05f) {
        if (!caps.canJump || heightDelta > maxJumpHeight)
            return NavSegmentType::Unreachable;
        return NavSegmentType::Jump;
    }
    if (heightDelta < -0.05f) {
        if (!caps.canDrop)
            return NavSegmentType::Unreachable;
        return NavSegmentType::Drop;
    }
    if (!caps.canWalk)
        return NavSegmentType::Unreachable;
    return NavSegmentType::Walk;
}

bool npcNavRequestSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Adapter preserves fields.
    {
        NavigationRequest req;
        req.goal = NpcGoalKind::ReachPosition;
        req.destination = glm::vec3(3, 4, 5);
        req.tolerance = 2.0f;
        const NpcGoal g = navigationRequestToGoal(req);
        if (g.kind != NpcGoalKind::ReachPosition) fail("goal kind not preserved");
        if (glm::length(g.targetPos - req.destination) > 0.001f) fail("destination not preserved");
        if (g.tolerance != 2.0f) fail("tolerance not preserved");
    }

    MovementCapabilities caps;  // walk/jump/drop on

    // Flat ground -> walk.
    if (classifySegment(0.0f, false, caps, 1.5f) != NavSegmentType::Walk)
        fail("flat segment should be Walk");
    // Small step up -> jump within cap.
    if (classifySegment(0.8f, false, caps, 1.5f) != NavSegmentType::Jump)
        fail("step up within jump cap should be Jump");
    // Too high -> unreachable.
    if (classifySegment(3.0f, false, caps, 1.5f) != NavSegmentType::Unreachable)
        fail("step up beyond jump cap should be Unreachable");
    // Drop down -> drop.
    if (classifySegment(-2.0f, false, caps, 1.5f) != NavSegmentType::Drop)
        fail("downward segment should be Drop");
    // Gap with jump capability -> gap.
    if (classifySegment(0.0f, true, caps, 1.5f) != NavSegmentType::Gap)
        fail("gap should be Gap when jump allowed");

    // Capability gating.
    MovementCapabilities noJump;
    noJump.canJump = false;
    if (classifySegment(0.8f, false, noJump, 1.5f) != NavSegmentType::Unreachable)
        fail("step up should be Unreachable without jump");
    if (classifySegment(0.0f, true, noJump, 1.5f) != NavSegmentType::Unreachable)
        fail("gap should be Unreachable without jump");

    MovementCapabilities noDrop;
    noDrop.canDrop = false;
    if (classifySegment(-2.0f, false, noDrop, 1.5f) != NavSegmentType::Unreachable)
        fail("drop should be Unreachable without canDrop");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
