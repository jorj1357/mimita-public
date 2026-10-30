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

bool TeamBrain::bestTeamReport(glm::vec3& outPosition, float& outConfidence) const
{
    const EnemyReport* best = nullptr;
    for (const auto& report : mState.enemyReports) {
        if (!best || report.confidence > best->confidence) best = &report;
    }
    if (!best || best->confidence <= 0.0f) return false;
    outPosition = best->lastKnownPosition;
    outConfidence = best->confidence;
    return true;
}

bool TeamBrain::selectFocus(const glm::vec3& origin, glm::vec3& outPosition,
                            uint32_t& outActorId, float& outConfidence) const
{
    const EnemyReport* best = nullptr;
    float bestScore = -1.0f;
    for (const auto& report : mState.enemyReports) {
        if (report.confidence <= 0.0f) continue;
        const float dx = report.lastKnownPosition.x - origin.x;
        const float dy = report.lastKnownPosition.y - origin.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        // Confidence dominates; proximity breaks ties. The actor id keeps the
        // choice deterministic when two enemies are otherwise equal.
        const float score = report.confidence / (1.0f + dist * 0.02f)
                          - (float)report.actorId * 1e-9f;
        if (!best || score > bestScore) {
            best = &report;
            bestScore = score;
        }
    }
    if (!best) return false;
    outPosition = best->lastKnownPosition;
    outActorId = best->actorId;
    outConfidence = best->confidence;
    return true;
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

void TeamBrain::updateSquad(const std::vector<SquadMember>& living,
                            const glm::vec3& focusPos, const glm::vec3& focusForward,
                            bool focusActive, const SquadTuning& tuning,
                            uint32_t focusActorId, uint32_t tick)
{
    mState.squadSlots.clear();
    SquadState& sq = mState.squad;
    sq.memberCount = (int)living.size();
    sq.focusPos = focusPos;
    sq.focusActorId = focusActive ? focusActorId : 0;

    if (living.empty()) {
        sq.active = false;
        sq.mode = 0;
        sq.anchor = glm::vec3(0.0f);
        return;
    }

    // Centroid of the living members is the starting anchor.
    glm::vec3 centroid(0.0f);
    for (const auto& m : living) centroid += m.pos;
    centroid /= static_cast<float>(living.size());
    sq.anchor = centroid;

    // Deterministic order by actor id.
    std::vector<SquadMember> ordered = living;
    std::sort(ordered.begin(), ordered.end(),
              [](const SquadMember& a, const SquadMember& b) { return a.id < b.id; });

    sq.active = tuning.enabled;
    if (!tuning.enabled) {
        sq.mode = 0;
        for (const auto& m : ordered)
            mState.squadSlots.push_back({m.id, sq.anchor});
        return;
    }

    const bool hasFocus = focusActive;
    // Advance the anchor toward the focus so the whole squad moves as a unit
    // instead of only the committed attackers leaving spawn.
    const float standoff = tuning.spreadRadiusMeters * 2.0f;
    float anchorToFocus = 0.0f;
    if (hasFocus) {
        const glm::vec2 c(centroid.x, centroid.y);
        const glm::vec2 f(focusPos.x, focusPos.y);
        const glm::vec2 to = f - c;
        anchorToFocus = glm::length(to);
        if (anchorToFocus > standoff) {
            const glm::vec2 dir = to / anchorToFocus;
            const glm::vec2 a = f - dir * standoff;
            sq.anchor = glm::vec3(a.x, a.y, focusPos.z);
        }
    }
    sq.mode = hasFocus
        ? (anchorToFocus <= standoff * 1.25f ? 2 : 1)
        : 1;

    // ── Target-relative approach sectors ───────────────────────────────
    // Opt-in: only group-focus profiles that explicitly request sectors use
    // this. TeamBrain owns assignment; movement only consumes the direction.
    const int sectorCount = std::clamp(tuning.approachDirections, 0, 8);
    const bool sectorMode = hasFocus && tuning.swarm && sectorCount >= 2 &&
        tuning.approachStyle == "sectors";
    mState.squadApproachCount = sectorMode ? (uint8_t)sectorCount : 0;
    if (!sectorMode) {
        mState.squadApproachSectors.clear();
        mState.squadApproachFocusActor = 0;
    } else {
        std::vector<SquadMember> npcMembers;
        for (const SquadMember& m : ordered)
            if (m.isNpc) npcMembers.push_back(m);

        auto hash32 = [](uint32_t x) {
            x ^= x >> 16; x *= 0x7feb352du;
            x ^= x >> 15; x *= 0x846ca68bu;
            x ^= x >> 16; return x;
        };
        auto previousSector = [&](uint32_t id) -> int {
            for (const auto& e : mState.squadApproachSectors)
                if (e.first == id) return (int)e.second;
            return -1;
        };
        auto hasSector = [&](uint32_t id) {
            return previousSector(id) >= 0;
        };
        bool membershipChanged = false;
        for (const SquadMember& m : npcMembers)
            if (!hasSector(m.id)) { membershipChanged = true; break; }
        if (!membershipChanged &&
            mState.squadApproachSectors.size() != npcMembers.size())
            membershipChanged = true;

        const uint32_t interval = (uint32_t)std::max(1, tuning.approachReassignTicks);
        const bool intervalElapsed =
            tick >= mState.squadApproachAssignedTick + interval;
        const bool configChanged =
            mState.squadApproachCount != (uint8_t)sectorCount;
        const bool focusChanged = mState.squadApproachFocusActor != focusActorId;
        const bool redraw = mState.squadApproachSectors.empty() ||
            membershipChanged || intervalElapsed || configChanged || focusChanged;

        if (redraw) {
            std::vector<std::pair<uint32_t, uint8_t>> next;
            next.reserve(npcMembers.size());
            int counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            for (const SquadMember& m : npcMembers) {
                const uint32_t h = hash32(m.id ^
                    (mState.squadApproachEpoch * 2654435761u));
                const int previous = previousSector(m.id);
                const float keepRoll =
                    (float)(h & 0xffffu) / 65535.0f;
                int sector = -1;
                if (previous >= 0 && previous < sectorCount &&
                    keepRoll < tuning.slotStickiness) {
                    // Hit-by-hit stickiness keeps a member on its side.
                    sector = previous;
                } else {
                    const int start = (int)(h % (uint32_t)sectorCount);
                    int best = (start + 0) % sectorCount;
                    for (int k = 0; k < sectorCount; ++k) {
                        const int s = (start + k) % sectorCount;
                        if (counts[s] < counts[best]) best = s;
                    }
                    sector = best;
                }
                next.push_back({m.id, (uint8_t)sector});
                ++counts[sector];
            }
            mState.squadApproachSectors = std::move(next);
            ++mState.squadApproachEpoch;
            mState.squadApproachAssignedTick = tick;
            mState.squadApproachFocusActor = focusActorId;
        }
    }

    const int count = (int)ordered.size();
    const int attackers = hasFocus
        ? std::min(count, std::max(1, tuning.maxAttackersPerTarget)) : 0;
    // High cohesion pulls a member back toward the squad anchor sooner. This
    // is the behavior that stops one NPC from running off alone across the map.
    const float rallyLimit = std::max(
        tuning.spreadRadiusMeters * 1.25f,
        tuning.rallyDistanceMeters * (1.0f + tuning.cohesion));
    for (int i = 0; i < count; ++i) {
        const glm::vec2 memberXy(ordered[i].pos.x, ordered[i].pos.y);
        const glm::vec2 anchorXy(sq.anchor.x, sq.anchor.y);
        const float anchorDist = glm::length(memberXy - anchorXy);
        const bool needsRally = anchorDist > rallyLimit;

        glm::vec3 slot;
        if (needsRally) {
            // Regroup first: head to the squad anchor, maintaining a small
            // offset so members do not occupy the exact same point.
            const float step = 6.2831853f / static_cast<float>(std::max(1, count));
            const float angle = step * static_cast<float>(i);
            slot = sq.anchor + glm::vec3(std::cos(angle), std::sin(angle), 0.0f) *
                                   (tuning.spreadRadiusMeters * 0.45f);
        } else if (hasFocus && i < attackers && tuning.swarm) {
            // Ring around the focus; approach from the squad's own side by
            // default so members do not cross the target to the far side.
            const float baseAngle = (tuning.approachStyle == "flank") ? 0.0f
                                                                      : 3.14159265f;
            const float step = 6.2831853f / static_cast<float>(std::max(1, attackers));
            const float angle = baseAngle + step * static_cast<float>(i);
            slot = focusPos + glm::vec3(std::cos(angle), std::sin(angle), 0.0f) *
                                  tuning.spreadRadiusMeters;
        } else {
            const float step = 6.2831853f / static_cast<float>(std::max(1, count));
            const float angle = step * static_cast<float>(i);
            slot = sq.anchor + glm::vec3(std::cos(angle), std::sin(angle), 0.0f) *
                                   (tuning.spreadRadiusMeters * 0.5f);
        }
        mState.squadSlots.push_back({ordered[i].id, slot});
    }
}

bool TeamBrain::squadSlotFor(uint32_t actorId, glm::vec3& outSlot,
                             glm::vec3& outAnchor) const
{
    for (const auto& entry : mState.squadSlots) {
        if (entry.first == actorId) {
            outSlot = entry.second;
            outAnchor = mState.squad.anchor;
            return true;
        }
    }
    return false;
}

int TeamBrain::squadApproachSectorFor(uint32_t actorId) const
{
    for (const auto& entry : mState.squadApproachSectors)
        if (entry.first == actorId) return (int)entry.second;
    return -1;
}

glm::vec3 TeamBrain::approachDirection(const glm::vec3& focusForward,
                                       int sector, int sectorCount)
{
    if (sectorCount < 2) return glm::vec3(0.0f);
    glm::vec3 fwd(focusForward.x, focusForward.y, 0.0f);
    if (glm::length(fwd) < 0.001f) fwd = glm::vec3(1.0f, 0.0f, 0.0f);
    fwd = glm::normalize(fwd);
    const glm::vec3 right(fwd.y, -fwd.x, 0.0f);
    const float theta = 6.2831853f * (float)(sector % sectorCount) /
        (float)sectorCount;
    return fwd * std::cos(theta) + right * std::sin(theta);
}

bool TeamBrain::nearestSitePosition(const glm::vec3& from, glm::vec3& out) const
{
    const TeamSiteInfo* best = nullptr;
    float bestDist = 0.0f;
    for (const auto& site : mState.sites) {
        if (!site.hasPosition) continue;
        const float d = glm::length(site.position - from);
        if (!best || d < bestDist) {
            best = &site;
            bestDist = d;
        }
    }
    if (!best) return false;
    out = best->position;
    return true;
}

bool TeamBrain::objectiveTargetPosition(uint32_t actorId, const glm::vec3& actorPos,
                                        glm::vec3& out) const
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
        // The carrier must deliver the bomb: send it to the nearest site so it
        // walks there and plants. Supporters head to the bomb (the carrier).
        if (actorId != 0 && actorId == mState.objective.carrierActorId &&
            nearestSitePosition(actorPos, out)) {
            return true;
        }
        out = mState.objective.bombPosition;
        return true;
    }
    // Default: nearest available site.
    if (nearestSitePosition(actorPos, out)) return true;
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
        if (!brain.objectiveTargetPosition(0, glm::vec3(0.0f), out))
            fail("should resolve a site target");
        brain.state().objective.planted = true;
        brain.state().objective.plantedSiteId = "A";
        if (!brain.objectiveTargetPosition(0, glm::vec3(0.0f), out) ||
            glm::length(out - glm::vec3(10, 0, 0)) > 0.001f)
            fail("planted site should be the objective target");
        report += "objective_target=ok\n";
    }

    // Focus selection prefers the enemy closest to the squad centroid, not the
    // first report inserted, and records the chosen actor id.
    {
        TeamBrain brain(0);
        brain.reportEnemySighting(90, glm::vec3(80, 0, 0), 1.0f, false);
        brain.reportEnemySighting(91, glm::vec3(10, 0, 0), 1.0f, false);
        glm::vec3 focus;
        uint32_t focusId = 0;
        float confidence = 0.0f;
        if (!brain.selectFocus(glm::vec3(0.0f), focus, focusId, confidence))
            fail("focus should resolve with reports present");
        if (focusId != 91) fail("focus should choose the nearest reported enemy");
        SquadTuning tuning;
        tuning.enabled = true;
        tuning.swarm = true;
        std::vector<SquadMember> members = {
            {1, glm::vec3(0, 0, 0)}, {2, glm::vec3(2, 0, 0)}};
        brain.updateSquad(members, focus, glm::vec3(1, 0, 0), true, tuning,
                          focusId, 0);
        if (brain.squad().focusActorId != focusId)
            fail("squad focus actor id should be recorded");
        glm::vec3 slot, anchor;
        if (!brain.squadSlotFor(1, slot, anchor))
            fail("squad members should receive slots");
        report += "focus_select=ok\n";
    }

    // Target-relative sectors: balanced assignment and correct direction math.
    {
        TeamBrain brain(0);
        SquadTuning tuning;
        tuning.enabled = true;
        tuning.swarm = true;
        tuning.approachStyle = "sectors";
        tuning.approachDirections = 4;
        tuning.approachReassignTicks = 60;
        tuning.slotStickiness = 0.0f;
        std::vector<SquadMember> members;
        for (uint32_t i = 1; i <= 8; ++i)
            members.push_back({i, glm::vec3((float)i, 0.0f, 0.0f), true});
        const glm::vec3 focusFwd(1.0f, 0.0f, 0.0f);
        brain.updateSquad(members, glm::vec3(0.0f), focusFwd, true,
                          tuning, 99, 0);
        int counts[4] = {0, 0, 0, 0};
        for (uint32_t i = 1; i <= 8; ++i) {
            const int s = brain.squadApproachSectorFor(i);
            if (s < 0 || s >= 4) fail("sector should be assigned");
            else ++counts[s];
        }
        for (int s = 0; s < 4; ++s)
            if (counts[s] < 1) fail("sectors should be balanced");
        const glm::vec3 front =
            TeamBrain::approachDirection(focusFwd, 0, 4);
        const glm::vec3 right =
            TeamBrain::approachDirection(focusFwd, 1, 4);
        if (front.x < 0.99f) fail("sector 0 should be the target front");
        if (right.y > -0.99f) fail("sector 1 should be the target right");
        report += "approach_sectors=ok\n";
    }

    // A bomb-carrying Terrorist is sent to a site, not to its own position.
    {
        TeamBrain brain(1);
        brain.state().objective.active = true;
        brain.state().objective.carriedByTeam = true;
        brain.state().objective.carrierActorId = 7;
        brain.state().objective.bombPosition = glm::vec3(0, 0, 0);
        TeamSiteInfo a;
        a.id = "A";
        a.position = glm::vec3(50, 0, 0);
        a.hasPosition = true;
        brain.state().sites.push_back(a);
        glm::vec3 out;
        if (!brain.objectiveTargetPosition(7, glm::vec3(1, 0, 0), out) ||
            glm::length(out - glm::vec3(50, 0, 0)) > 0.001f)
            fail("carrier should be sent to the nearest site");
        // A supporter still follows the bomb/carrier.
        if (!brain.objectiveTargetPosition(8, glm::vec3(1, 0, 0), out) ||
            glm::length(out - glm::vec3(0, 0, 0)) > 0.001f)
            fail("supporter should follow the carried bomb");
        report += "carrier_target=ok\n";
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
