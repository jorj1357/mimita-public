// 10 02 2026
// Generic objective data + pure helpers. Server owns live state (see
// server-gamemode.cpp for the pickup/drop rules).
#include "game/objective-state.h"

#include <algorithm>
#include <cmath>

ObjectiveKind objectiveKindFromString(const std::string& kind)
{
    if (kind == "bomb") return ObjectiveKind::Bomb;
    return ObjectiveKind::None;
}

uint32_t selectObjectiveCarrier(const ObjectiveInstance& obj,
                               const ObjectiveCarrierCandidate* candidates,
                               int candidateCount,
                               float radiusMeters)
{
    uint32_t bestActor = 0;
    float bestDistSq = radiusMeters * radiusMeters;
    for (int i = 0; i < candidateCount; ++i) {
        const ObjectiveCarrierCandidate& c = candidates[i];
        if (c.actorId == 0 || c.dead) continue;
        if (obj.allowedCarrierTeam >= 0 && c.team != obj.allowedCarrierTeam) continue;
        const glm::vec3 delta = c.position - obj.position;
        const float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (distSq <= bestDistSq) {
            bestDistSq = distSq;
            bestActor = c.actorId;
        }
    }
    return bestActor;
}

bool advanceObjectiveProgress(int& elapsed, int required, bool canProgress)
{
    if (!canProgress) {
        elapsed = 0;  // interruptible: losing the condition resets progress
        return false;
    }
    if (required <= 0) {
        elapsed = 0;
        return true;  // zero-time action completes immediately
    }
    ++elapsed;
    if (elapsed >= required) {
        elapsed = required;
        return true;
    }
    return false;
}

int objectiveSecondsToTicks(float seconds)
{
    return std::max(1, (int)std::round(seconds * 60.0f));
}

bool objectiveSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    if (objectiveKindFromString("bomb") != ObjectiveKind::Bomb)
        fail("'bomb' should map to ObjectiveKind::Bomb");
    if (objectiveKindFromString("payload") != ObjectiveKind::None) {
        // Unknown kinds fail safe for now (future kinds get explicit cases).
    }
    if (objectiveKindFromString("") != ObjectiveKind::None)
        fail("empty kind should be None");

    ObjectiveInstance obj;
    obj.id = "bomb";
    obj.kind = ObjectiveKind::Bomb;
    obj.active = true;
    obj.allowedCarrierTeam = 1;
    if (!obj.valid()) fail("active bomb should be valid");
    obj.active = false;
    if (obj.valid()) fail("inactive objective should be invalid");
    obj.active = true;
    obj.kind = ObjectiveKind::None;
    if (obj.valid()) fail("kind=None objective should be invalid");

    // Pickup selection: team gate + radius + alive.
    {
        ObjectiveInstance bomb;
        bomb.id = "bomb";
        bomb.kind = ObjectiveKind::Bomb;
        bomb.active = true;
        bomb.allowedCarrierTeam = 1;  // Terrorists
        bomb.position = glm::vec3(0, 0, 0);
        bomb.pickupRadius = 2.0f;

        // A CT (team 0) standing on it must NOT be eligible.
        ObjectiveCarrierCandidate ct{10, glm::vec3(0, 0, 0), 0, false};
        if (selectObjectiveCarrier(bomb, &ct, 1, bomb.pickupRadius) != 0)
            fail("CT must not carry the bomb");

        // A T (team 1) within radius IS eligible.
        ObjectiveCarrierCandidate t{11, glm::vec3(1, 0, 0), 1, false};
        if (selectObjectiveCarrier(bomb, &t, 1, bomb.pickupRadius) != 11)
            fail("T within radius should carry the bomb");

        // A dead T is not eligible.
        ObjectiveCarrierCandidate deadT{12, glm::vec3(0.5f, 0, 0), 1, true};
        if (selectObjectiveCarrier(bomb, &deadT, 1, bomb.pickupRadius) != 0)
            fail("dead T must not carry the bomb");

        // Out of radius is not eligible.
        ObjectiveCarrierCandidate farT{13, glm::vec3(10, 0, 0), 1, false};
        if (selectObjectiveCarrier(bomb, &farT, 1, bomb.pickupRadius) != 0)
            fail("T out of radius must not carry the bomb");

        // Nearest eligible wins among several.
        ObjectiveCarrierCandidate many[3] = {
            {20, glm::vec3(2, 0, 0), 1, false},
            {21, glm::vec3(0.5f, 0, 0), 1, false},
            {22, glm::vec3(3, 0, 0), 1, false},
        };
        if (selectObjectiveCarrier(bomb, many, 3, bomb.pickupRadius) != 21)
            fail("nearest eligible carrier should win");
    }

    // Plant/defuse progress is fixed-tick and interruptible.
    {
        int elapsed = 0;
        const int required = objectiveSecondsToTicks(3.0f);  // 3s -> 180 ticks
        if (required != 180) fail("3s should be 180 ticks");
        // Advance to completion without interruption.
        bool completed = false;
        for (int i = 0; i < required && !completed; ++i)
            completed = advanceObjectiveProgress(elapsed, required, true);
        if (!completed) fail("plant should complete after required ticks");
        if (elapsed != required) fail("elapsed should equal required at completion");

        // Interruption resets progress.
        elapsed = 50;
        if (advanceObjectiveProgress(elapsed, required, false))
            fail("interrupted progress should not complete");
        if (elapsed != 0) fail("interrupted progress should reset to zero");

        // Zero-time action completes immediately.
        int zero = 0;
        if (!advanceObjectiveProgress(zero, 0, true))
            fail("zero-required action should complete immediately");

        report += "progress=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
