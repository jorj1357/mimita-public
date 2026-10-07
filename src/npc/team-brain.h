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

// ── General group/focus behavior (see npc-group-behavior.md) ─────────────
// One living squad member for coordination.
struct SquadMember
{
    uint32_t id = 0;
    glm::vec3 pos{0.0f};
};

// Per-team group tuning resolved from the members' behavior profiles.
struct SquadTuning
{
    bool enabled = false;
    bool swarm = false;
    float cohesion = 0.0f;
    float spreadRadiusMeters = 6.0f;
    int maxAttackersPerTarget = 6;
    std::string approachStyle = "arc";
};

// Current shared squad state. Focus is a travel target, never an aim target.
struct SquadState
{
    bool active = false;
    glm::vec3 anchor{0.0f};
    glm::vec3 focusPos{0.0f};
    uint32_t focusActorId = 0;
    int memberCount = 0;
    // 0 none, 1 advance, 2 engage (near focus).
    int mode = 0;
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
    // General squad state and per-actor slots (group behavior).
    SquadState squad;
    std::vector<std::pair<uint32_t, glm::vec3>> squadSlots;
    // Change-edge reporting memory (set by the caller; avoids per-tick spam).
    int reportedSquadMode = -1;
    bool hasReportedFocus = false;
    glm::vec3 reportedFocusPos{0.0f};
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
    // Highest-confidence enemy report anywhere on the team (the team's best
    // estimate of where enemies are). Returns false when there are none.
    bool bestTeamReport(glm::vec3& outPosition, float& outConfidence) const;

    // Recompute assignments for the given living actors (ids + teams). Roles are
    // apportioned from the policy. Deterministic for a given state.
    void updateAssignments(const std::vector<std::pair<uint32_t, int>>& livingActors,
                           bool objectiveRounds);
    // Assignment for an actor, or None.
    TeamAssignment assignmentFor(uint32_t actorId) const;

    // Recompute the shared squad anchor and per-actor slots. `living` is this
    // team's living members; `focusActive`/`focusPos` come from the team's best
    // enemy report. Deterministic for a given input. Produces suggestions only.
    void updateSquad(const std::vector<SquadMember>& living,
                     const glm::vec3& focusPos, bool focusActive,
                     const SquadTuning& tuning);
    // Slot + anchor for an actor. False when the actor has no squad slot.
    bool squadSlotFor(uint32_t actorId, glm::vec3& outSlot,
                      glm::vec3& outAnchor) const;
    const SquadState& squad() const { return mState.squad; }

    // Objective position this actor should move to right now. `actorId` and
    // `actorPos` let a bomb-carrying Terrorist be sent to a site to plant
    // instead of being pointed at its own position. Planted site wins, then the
    // carried bomb (for supporters), then the nearest site.
    bool objectiveTargetPosition(uint32_t actorId, const glm::vec3& actorPos,
                                 glm::vec3& out) const;

private:
    // Nearest site with a position to `from`. False when no site is usable.
    bool nearestSitePosition(const glm::vec3& from, glm::vec3& out) const;

    int mTeam = -1;
    TeamBrainState mState;
};

// World-independent selftest for report decay, assignment apportionment, and
// objective targeting.
bool teamBrainSelfTest(std::string& report);
