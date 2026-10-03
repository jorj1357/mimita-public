// 10 02 2026
/* purpose
* Team-level tactical brain shared by all NPCs on a team. Owns team membership,
* shared last-known enemy reports, bomb ownership/location and planted state,
* site availability, and optional JSON-controlled basic assignments.
* Produces per-actor objective intent (positions/roles) that the local ActorBrain
* consumes; it NEVER teleports actors or overrides physics.
* Does NOT path, move, fire, or apply damage.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// Which objective job a team has assigned to an actor this round.
enum class TeamAssignment : uint8_t
{
    None = 0,
    AttackSite,    // Terrorist pushing a site
    CarryBomb,     // Terrorist carrying/delivering the bomb
    DefendSite,    // Counter-Terrorist holding a site
    Rotate,        // Counter-Terrorist reinforcing/rotating
    Retake,        // Counter-Terrorist retaking a planted site
    Defuse         // Counter-Terrorist defusing the planted bomb
};

const char* teamAssignmentName(TeamAssignment a);

// One site the brain knows about. Availability is owned by the objective/mode.
struct TeamSiteInfo
{
    std::string id;
    glm::vec3 position{0.0f};
    float radius = 4.0f;
    bool hasPosition = false;
    bool hasBomb = false;    // bomb is planted here
};

// A shared, decaying report of where an enemy was last seen. Hearing reports are
// uncertain (higher uncertainty, lower confidence).
struct EnemyReport
{
    uint32_t actorId = 0;
    glm::vec3 lastKnownPosition{0.0f};
    float ageSeconds = 0.0f;
    float confidence = 0.0f;
    float uncertainty = 0.0f;
    bool fromHearing = false;
};

// Objective state the brain consumes (owned by the mode/server).
struct TeamObjectiveContext
{
    bool active = false;
    bool carriedByTeam = false;
    bool planted = false;
    uint32_t carrierActorId = 0;
    std::string plantedSiteId;
    glm::vec3 bombPosition{0.0f};
};

// Optional per-team assignment policy from JSON. When a team has no policy the
// brain falls back to simple defaults.
struct TeamAssignmentPolicy
{
    int attackersPerSite = 2;
    int defendersPerSite = 2;
    bool oneRotator = true;
};

struct TeamBrainState
{
    int team = -1;
    std::vector<EnemyReport> enemyReports;
    TeamObjectiveContext objective;
    std::vector<TeamSiteInfo> sites;
    TeamAssignmentPolicy policy;
    // actor id -> assignment for the current round.
    std::vector<std::pair<uint32_t, TeamAssignment>> assignments;
    glm::vec3 assignedObjectivePos{0.0f};
};

// The generic TeamBrain. One instance per team; used by both sides.
class TeamBrain
{
public:
    explicit TeamBrain(int team) : mTeam(team) {}

    int team() const { return mTeam; }
    TeamBrainState& state() { return mState; }
    const TeamBrainState& state() const { return mState; }

    // Replace the shared enemy report list (visual + hearing). Visual reports
    // are exact; hearing reports are uncertain.
    void reportEnemySighting(uint32_t actorId, const glm::vec3& position,
                             float confidence, bool fromHearing);
    // Age and drop stale reports each tick.
    void tickReports(float dt, float memorySeconds);
    // Best current report for an actor, or nullptr.
    const EnemyReport* bestReport(uint32_t actorId) const;

    // Recompute assignments for the given living actors (ids + teams). Roles are
    // apportioned from the policy. Deterministic for a given state.
    void updateAssignments(const std::vector<std::pair<uint32_t, int>>& livingActors,
                           bool objectiveRounds);
    // Assignment for an actor, or None.
    TeamAssignment assignmentFor(uint32_t actorId) const;

    // Objective position the team should care about right now (bomb if carried
    // by us, planted site if planted, else the team's assigned site).
    bool objectiveTargetPosition(glm::vec3& out) const;

private:
    int mTeam = -1;
    TeamBrainState mState;
};

// World-independent selftest for report decay, assignment apportionment, and
// objective targeting.
bool teamBrainSelfTest(std::string& report);
