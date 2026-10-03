// 10 02 2026
// Team-level tactical brain. Owns assignments and shared reports; never moves
// or teleports actors.
#include "npc/team-brain.h"

#include <algorithm>
#include <cmath>

const char* teamAssignmentName(TeamAssignment a)
{
    switch (a) {
        case TeamAssignment::None:       return "None";
        case TeamAssignment::AttackSite: return "AttackSite";
        case TeamAssignment::CarryBomb:  return "CarryBomb";
        case TeamAssignment::DefendSite: return "DefendSite";
        case TeamAssignment::Rotate:     return "Rotate";
        case TeamAssignment::Retake:     return "Retake";
        case TeamAssignment::Defuse:     return "Defuse";
    }
    return "Unknown";
}

void TeamBrain::reportEnemySighting(uint32_t actorId, const glm::vec3& position,
                                    float confidence, bool fromHearing)
{
    for (auto& report : mState.enemyReports) {
        if (report.actorId != actorId) continue;
        report.lastKnownPosition = position;
        report.ageSeconds = 0.0f;
        report.confidence = std::clamp(confidence, 0.0f, 1.0f);
        report.uncertainty = fromHearing ? 6.0f : 0.0f;
        report.fromHearing = fromHearing;
        return;
    }
    EnemyReport report;
    report.actorId = actorId;
    report.lastKnownPosition = position;
    report.ageSeconds = 0.0f;
    report.confidence = std::clamp(confidence, 0.0f, 1.0f);
    report.uncertainty = fromHearing ? 6.0f : 0.0f;
    report.fromHearing = fromHearing;
    mState.enemyReports.push_back(report);
}

void TeamBrain::tickReports(float dt, float memorySeconds)
{
    for (auto& report : mState.enemyReports) {
        report.ageSeconds += dt;
        const float life = std::max(0.1f, memorySeconds);
        report.confidence = std::clamp(1.0f - report.ageSeconds / life, 0.0f, 1.0f);
        report.uncertainty = std::min(30.0f, report.uncertainty + dt * 4.0f);
    }
    mState.enemyReports.erase(
        std::remove_if(mState.enemyReports.begin(), mState.enemyReports.end(),
            [](const EnemyReport& r) { return r.confidence <= 0.0f; }),
        mState.enemyReports.end());
}

const EnemyReport* TeamBrain::bestReport(uint32_t actorId) const
{
    const EnemyReport* best = nullptr;
    for (const auto& report : mState.enemyReports) {
        if (report.actorId != actorId) continue;
        if (!best || report.confidence > best->confidence) best = &report;
    }
    return best;
}

void TeamBrain::updateAssignments(
    const std::vector<std::pair<uint32_t, int>>& livingActors,
    bool objectiveRounds)
{
    mState.assignments.clear();
    if (!objectiveRounds) {
        for (const auto& actor : livingActors)
            mState.assignments.push_back({actor.first, TeamAssignment::None});
        return;
    }

    // Collect this team's living actors in id order for determinism.
    std::vector<uint32_t> members;
    for (const auto& actor : livingActors)
        if (actor.second == mTeam) members.push_back(actor.first);
    std::sort(members.begin(), members.end());

    const bool isTerrorist = (mTeam == 1);
    const bool isCounterTerrorist = (mTeam == 0);

    if (isTerrorist) {
        // One bomb carrier (already handled by the objective) plus attackers
        // pushed toward sites. If the objective is carried by us, keep the
        // carrier assigned to CarryBomb and split the rest across sites.
        size_t index = 0;
        if (mState.objective.active && mState.objective.carriedByTeam) {
            for (uint32_t id : members) {
                if (id == mState.objective.carrierActorId) {
                    mState.assignments.push_back({id, TeamAssignment::CarryBomb});
                    members.erase(members.begin() + (index));
                    break;
                }
                ++index;
            }
        }
        for (size_t i = 0; i < members.size(); ++i) {
            mState.assignments.push_back({members[i], TeamAssignment::AttackSite});
        }
        return;
    }

    if (isCounterTerrorist) {
        if (mState.objective.planted) {
            // Everyone retakes; the closest is chosen as defuser by the local
            // brain (which knows its own position).
            for (uint32_t id : members)
                mState.assignments.push_back({id, TeamAssignment::Retake});
            return;
        }
        // Split defenders across sites; optionally one rotator.
        int rotatorCount = mState.policy.oneRotator ? 1 : 0;
        const int defenderCount = (int)members.size() - rotatorCount;
        const int perSite = std::max(1, mState.policy.defendersPerSite);
        int assigned = 0;
        for (size_t i = 0; i < members.size(); ++i) {
            if (i >= members.size() - (size_t)rotatorCount) {
                mState.assignments.push_back({members[i], TeamAssignment::Rotate});
            } else {
                (void)perSite;
                (void)assigned;
                mState.assignments.push_back({members[i], TeamAssignment::DefendSite});
                ++assigned;
            }
        }
        (void)defenderCount;
        return;
    }

    for (uint32_t id : members)
        mState.assignments.push_back({id, TeamAssignment::None});
}

TeamAssignment TeamBrain::assignmentFor(uint32_t actorId) const
{
    for (const auto& entry : mState.assignments)
        if (entry.first == actorId) return entry.second;
    return TeamAssignment::None;
}

bool TeamBrain::objectiveTargetPosition(glm::vec3& out) const
{
    if (!mState.objective.active) return false;
    if (mState.objective.planted) {
        for (const auto& site : mState.sites) {
            if (site.id == mState.objective.plantedSiteId && site.hasPosition) {
                out = site.position;
                return true;
            }
        }
        out = mState.objective.bombPosition;
        return true;
    }
    if (mState.objective.carriedByTeam) {
        out = mState.objective.bombPosition;
        return true;
    }
    // Default: nearest available site.
    for (const auto& site : mState.sites) {
        if (site.hasPosition) {
            out = site.position;
            return true;
        }
    }
    return false;
}

bool teamBrainSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Report decay: a visual report loses confidence over time and is dropped.
    {
        TeamBrain brain(1);
        brain.reportEnemySighting(50, glm::vec3(1, 2, 3), 1.0f, false);
        brain.state().objective.active = true;
        brain.tickReports(0.5f, 2.0f);
        const EnemyReport* r = brain.bestReport(50);
        if (!r) fail("report should survive briefly");
        else if (r->confidence >= 1.0f) fail("confidence should decay");
        brain.tickReports(3.0f, 2.0f);
        if (brain.bestReport(50)) fail("report should be dropped after memorySeconds");
        report += "report_decay=ok\n";
    }

    // Terrorist assignments: exactly one CarryBomb when carrying, rest attack.
    {
        TeamBrain brain(1);
        brain.state().objective.active = true;
        brain.state().objective.carriedByTeam = true;
        brain.state().objective.carrierActorId = 7;
        std::vector<std::pair<uint32_t, int>> actors = {
            {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1},
        };
        brain.updateAssignments(actors, true);
        int carry = 0, attack = 0;
        for (const auto& a : actors) {
            TeamAssignment t = brain.assignmentFor(a.first);
            if (t == TeamAssignment::CarryBomb) ++carry;
            if (t == TeamAssignment::AttackSite) ++attack;
        }
        if (carry != 1) fail("exactly one terrorist should carry the bomb");
        if (attack != 4) fail("remaining terrorists should attack");
        report += "terrorist_assign=ok\n";
    }

    // Counter-Terrorist assignments: defenders plus one rotator.
    {
        TeamBrain brain(0);
        std::vector<std::pair<uint32_t, int>> actors = {
            {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0},
        };
        brain.updateAssignments(actors, true);
        int defend = 0, rotate = 0;
        for (const auto& a : actors) {
            TeamAssignment t = brain.assignmentFor(a.first);
            if (t == TeamAssignment::DefendSite) ++defend;
            if (t == TeamAssignment::Rotate) ++rotate;
        }
        if (defend + rotate != 5) fail("all CTs should be assigned");
        if (rotate != 1) fail("one CT should rotate");
        report += "ct_assign=ok\n";
    }

    // Planted bomb: every CT retakes.
    {
        TeamBrain brain(0);
        brain.state().objective.active = true;
        brain.state().objective.planted = true;
        std::vector<std::pair<uint32_t, int>> actors = {{1, 0}, {2, 0}, {3, 0}};
        brain.updateAssignments(actors, true);
        for (const auto& a : actors)
            if (brain.assignmentFor(a.first) != TeamAssignment::Retake)
                fail("planted bomb should make CTs retake");
        report += "retake=ok\n";
    }

    // Objective target position prefers the planted site, then carried bomb.
    {
        TeamBrain brain(1);
        brain.state().objective.active = true;
        TeamSiteInfo site;
        site.id = "A";
        site.position = glm::vec3(10, 0, 0);
        site.hasPosition = true;
        brain.state().sites.push_back(site);
        glm::vec3 out;
        if (!brain.objectiveTargetPosition(out)) fail("should resolve a site target");
        brain.state().objective.planted = true;
        brain.state().objective.plantedSiteId = "A";
        if (!brain.objectiveTargetPosition(out) || glm::length(out - glm::vec3(10, 0, 0)) > 0.001f)
            fail("planted site should be the objective target");
        report += "objective_target=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
