// 10 04 2026
// Pure NPC target-selection policy: team hostility and nearest-hostile pick.
#include "npc/npc-targeting.h"

#include <limits>

NpcTargetingMode npcTargetingModeFromString(const std::string& value, bool& ok)
{
    ok = true;
    if (value == "player") return NpcTargetingMode::Player;
    if (value == "closest") return NpcTargetingMode::Closest;
    if (value == "opposite_team") return NpcTargetingMode::OppositeTeam;
    ok = false;
    return NpcTargetingMode::Closest;
}

const char* npcTargetingModeName(NpcTargetingMode mode)
{
    switch (mode) {
        case NpcTargetingMode::Player:       return "player";
        case NpcTargetingMode::Closest:      return "closest";
        case NpcTargetingMode::OppositeTeam: return "opposite_team";
    }
    return "closest";
}

bool npcTargetingIsHostile(const NpcTargetingPolicy& policy,
                           int myTeam, int candidateTeam)
{
    if (policy.mode == NpcTargetingMode::Player) {
        // Legacy free-for-all: an actor with no team is hostile to everyone.
        if (myTeam < 0 || candidateTeam < 0) return true;
        return myTeam != candidateTeam;
    }
    // Closest / OppositeTeam: only real, differing teams are hostile.
    return myTeam >= 0 && candidateTeam >= 0 && myTeam != candidateTeam;
}

bool npcTargetingIncludesPlayers(const NpcTargetingPolicy& policy)
{
    return policy.includePlayers;
}

bool npcTargetingIncludesNpcs(const NpcTargetingPolicy& policy)
{
    return policy.includeNpcs;
}

namespace {

// Nearest candidate satisfying the inclusion + hostility + alive filters.
// `wantNpc` selects the NPC/human half; returns 0 when none.
uint32_t nearestCandidate(const NpcTargetingPolicy& policy, int myTeam,
                          const std::vector<NpcTargetCandidate>& candidates,
                          bool wantNpc)
{
    uint32_t best = 0;
    float bestD2 = std::numeric_limits<float>::max();
    for (const NpcTargetCandidate& c : candidates) {
        if (c.id == 0 || c.dead) continue;
        if (c.isNpc != wantNpc) continue;
        if (c.isNpc && !policy.includeNpcs) continue;
        if (!c.isNpc && !policy.includePlayers) continue;
        if (!npcTargetingIsHostile(policy, myTeam, c.team)) continue;
        const float d2 = c.pos.x * c.pos.x + c.pos.y * c.pos.y + c.pos.z * c.pos.z;
        if (d2 < bestD2) { bestD2 = d2; best = c.id; }
    }
    return best;
}

} // namespace

uint32_t selectNpcTargetId(const NpcTargetingPolicy& policy, int myTeam,
                           const std::vector<NpcTargetCandidate>& candidates,
                           uint32_t currentTargetId)
{
    (void)currentTargetId;  // stickiness/scoring is applied by the caller.
    if (policy.mode == NpcTargetingMode::Player) {
        const uint32_t human = nearestCandidate(policy, myTeam, candidates, false);
        if (human != 0) return human;
        return nearestCandidate(policy, myTeam, candidates, true);
    }
    const uint32_t human = nearestCandidate(policy, myTeam, candidates, false);
    const uint32_t npc = nearestCandidate(policy, myTeam, candidates, true);
    // Closest across both halves: compare the winning distances again.
    float bestD2 = std::numeric_limits<float>::max();
    uint32_t best = 0;
    auto consider = [&](uint32_t id) {
        if (id == 0) return;
        for (const NpcTargetCandidate& c : candidates) {
            if (c.id != id) continue;
            const float d2 = c.pos.x * c.pos.x + c.pos.y * c.pos.y + c.pos.z * c.pos.z;
            if (d2 < bestD2) { bestD2 = d2; best = id; }
        }
    };
    consider(human);
    consider(npc);
    return best;
}

bool npcTargetingSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) { ok = false; report += "FAIL: " + why + "\n"; };

    // Strict opposite-team hostility.
    NpcTargetingPolicy opposite;
    opposite.configured = true;
    opposite.mode = NpcTargetingMode::OppositeTeam;
    opposite.includePlayers = true;
    opposite.includeNpcs = true;
    if (!npcTargetingIsHostile(opposite, 0, 1)) fail("CT should be hostile to T");
    if (!npcTargetingIsHostile(opposite, 1, 0)) fail("T should be hostile to CT");
    if (npcTargetingIsHostile(opposite, 0, 0)) fail("CT must not be hostile to CT");
    if (npcTargetingIsHostile(opposite, 1, 1)) fail("T must not be hostile to T");
    if (npcTargetingIsHostile(opposite, -1, 0)) fail("unknown team must not be hostile");
    if (npcTargetingIsHostile(opposite, 0, -1)) fail("unknown candidate must not be hostile");

    // Legacy player mode keeps free-for-all hostility.
    NpcTargetingPolicy legacy;
    legacy.mode = NpcTargetingMode::Player;
    if (!npcTargetingIsHostile(legacy, -1, 0)) fail("legacy: no-team actor is hostile");
    if (!npcTargetingIsHostile(legacy, 0, 1)) fail("legacy: different teams hostile");
    if (npcTargetingIsHostile(legacy, 0, 0)) fail("legacy: same team not hostile");

    // Candidate matrix: CT NPC (team 0).
    std::vector<NpcTargetCandidate> cands = {
        {1, 0, false, false, {1, 0, 0}},   // CT human teammate
        {2, 0, true, false, {2, 0, 0}},    // CT NPC teammate
        {3, 1, false, false, {5, 0, 0}},   // T human
        {4, 1, true, false, {3, 0, 0}},    // T NPC
    };
    if (selectNpcTargetId(opposite, 0, cands) != 4)
        fail("CT NPC should pick the nearest T (NPC)");
    if (selectNpcTargetId(opposite, 1, cands) != 1)
        fail("T actor should pick the nearest CT (human)");
    // Teammate-only: no hostile target.
    std::vector<NpcTargetCandidate> teammates = {
        {1, 0, false, false, {1, 0, 0}},
        {2, 0, true, false, {2, 0, 0}},
    };
    if (selectNpcTargetId(opposite, 0, teammates) != 0)
        fail("CT must not target a CT teammate");
    // Dead hostile is skipped.
    std::vector<NpcTargetCandidate> deadEnemy = {
        {3, 1, true, true, {1, 0, 0}},
        {4, 1, true, false, {4, 0, 0}},
    };
    if (selectNpcTargetId(opposite, 0, deadEnemy) != 4)
        fail("dead hostile must be skipped");

    // Inclusion flags.
    NpcTargetingPolicy npcOnly = opposite;
    npcOnly.includePlayers = false;
    if (selectNpcTargetId(npcOnly, 0, cands) != 4)
        fail("include_players=false should pick the T NPC");
    NpcTargetingPolicy playerOnly = opposite;
    playerOnly.includeNpcs = false;
    if (selectNpcTargetId(playerOnly, 0, cands) != 3)
        fail("include_npcs=false should pick the T human");

    // Player mode prefers a human over a nearer NPC.
    std::vector<NpcTargetCandidate> mixed = {
        {3, 1, false, false, {9, 0, 0}},   // T human far
        {4, 1, true, false, {1, 0, 0}},    // T NPC near
    };
    if (selectNpcTargetId(legacy, 0, mixed) != 3)
        fail("player mode should prefer the hostile human");

    if (std::string(npcTargetingModeName(NpcTargetingMode::OppositeTeam)) != "opposite_team")
        fail("mode name mismatch");
    bool parsed = false;
    if (npcTargetingModeFromString("bogus", parsed) != NpcTargetingMode::Closest || parsed)
        fail("unknown mode should fail to parse");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}
