// 10 04 2026
/* purpose
* Focused, world-independent tests for the NPC target-selection policy:
* team hostility (opposite-team and legacy player modes), inclusion flags, and
* the nearest-hostile selection matrix across players and NPCs.
* Runs as a plain check() + exit-code harness (no gtest) and links only
* src/npc/npc-targeting.cpp.
*/

#include "npc/npc-targeting.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int gChecks = 0;

void check(bool condition, const char* message)
{
    ++gChecks;
    if (!condition) {
        std::printf("[npc-targeting-test] FAIL %s\n", message);
        std::exit(1);
    }
}

void testOppositeTeamMatrix()
{
    NpcTargetingPolicy p;
    p.configured = true;
    p.mode = NpcTargetingMode::OppositeTeam;

    check(npcTargetingIsHostile(p, 0, 1), "CT hostile to T");
    check(npcTargetingIsHostile(p, 1, 0), "T hostile to CT");
    check(!npcTargetingIsHostile(p, 0, 0), "CT not hostile to CT");
    check(!npcTargetingIsHostile(p, 1, 1), "T not hostile to T");
    check(!npcTargetingIsHostile(p, -1, 1), "unknown self team is not hostile");
    check(!npcTargetingIsHostile(p, 0, -1), "unknown candidate team is not hostile");

    // CT NPC (team 0) versus every combination.
    std::vector<NpcTargetCandidate> cands = {
        {1, 0, false, false, {1, 0, 0}},   // CT human
        {2, 0, true, false, {2, 0, 0}},    // CT NPC
        {3, 1, false, false, {5, 0, 0}},   // T human
        {4, 1, true, false, {3, 0, 0}},    // T NPC
    };
    check(selectNpcTargetId(p, 0, cands) == 4, "CT NPC selects nearest T NPC");
    check(selectNpcTargetId(p, 1, cands) == 1, "T actor selects nearest CT human");

    // Teammate-only: no hostile target.
    std::vector<NpcTargetCandidate> mates = {
        {1, 0, false, false, {1, 0, 0}},
        {2, 0, true, false, {2, 0, 0}},
    };
    check(selectNpcTargetId(p, 0, mates) == 0, "CT ignores CT teammate");

    // T teammate-only.
    std::vector<NpcTargetCandidate> tMates = {
        {3, 1, false, false, {1, 0, 0}},
        {4, 1, true, false, {2, 0, 0}},
    };
    check(selectNpcTargetId(p, 1, tMates) == 0, "T ignores T teammate");

    // Dead hostile skipped.
    std::vector<NpcTargetCandidate> dead = {
        {3, 1, true, true, {1, 0, 0}},
        {4, 1, true, false, {4, 0, 0}},
    };
    check(selectNpcTargetId(p, 0, dead) == 4, "dead hostile is skipped");

    // Inclusion flags.
    NpcTargetingPolicy npcOnly = p;
    npcOnly.includePlayers = false;
    check(selectNpcTargetId(npcOnly, 0, cands) == 4, "include_players=false picks T NPC");
    NpcTargetingPolicy playerOnly = p;
    playerOnly.includeNpcs = false;
    check(selectNpcTargetId(playerOnly, 0, cands) == 3, "include_npcs=false picks T human");
}

void testLegacyPlayerMode()
{
    NpcTargetingPolicy p;
    p.mode = NpcTargetingMode::Player;
    check(npcTargetingIsHostile(p, -1, 0), "legacy: no-team actor is hostile");
    check(npcTargetingIsHostile(p, 0, 1), "legacy: different teams hostile");
    check(!npcTargetingIsHostile(p, 0, 0), "legacy: same team not hostile");

    std::vector<NpcTargetCandidate> mixed = {
        {3, 1, false, false, {9, 0, 0}},   // far hostile human
        {4, 1, true, false, {1, 0, 0}},    // near hostile NPC
    };
    check(selectNpcTargetId(p, 0, mixed) == 3, "player mode prefers hostile human");
}

void testModeParsing()
{
    bool ok = false;
    check(npcTargetingModeFromString("player", ok) == NpcTargetingMode::Player && ok,
          "parse player");
    check(npcTargetingModeFromString("closest", ok) == NpcTargetingMode::Closest && ok,
          "parse closest");
    check(npcTargetingModeFromString("opposite_team", ok) == NpcTargetingMode::OppositeTeam && ok,
          "parse opposite_team");
    npcTargetingModeFromString("bogus", ok);
    check(!ok, "unknown mode fails to parse");
    check(std::string(npcTargetingModeName(NpcTargetingMode::OppositeTeam)) == "opposite_team",
          "mode name");
}

} // namespace

int main()
{
    std::string report;
    check(npcTargetingSelfTest(report), "npcTargetingSelfTest");
    testOppositeTeamMatrix();
    testLegacyPlayerMode();
    testModeParsing();
    std::printf("[npc-targeting-test] PASS (%d checks)\n", gChecks);
    return 0;
}
