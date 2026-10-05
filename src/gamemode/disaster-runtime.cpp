// Generic JSON-defined gamemode runtime - disaster runtime rules.
//
// Pure deterministic logic. Every function is safe to call from a test with no
// world, no network, and no registry loaded.

#include "gamemode/disaster-runtime.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

#include "gamemode/deterministic-rng.h"

namespace MimitaGamemode {

const DisasterDefinition* disasterSelect(const ModePack& pack, uint32_t seed)
{
    if (pack.disasters.empty())
        return nullptr;

    // Order-independent selection: score each disaster from (seed, pack id, its
    // own id) and take the highest, with the id as a stable tie-breaker. Editing
    // the manifest order can never change the same (manifest, seed) outcome.
    const DisasterDefinition* best = nullptr;
    uint32_t bestScore = 0;
    for (const DisasterDefinition& disaster : pack.disasters) {
        const uint32_t score = mixSeed(
            seed, ((uint64_t)hashString(pack.id) * 1099511628211ull) ^
                  ((uint64_t)hashString(disaster.id) * 2654435761ull));
        if (!best || score > bestScore ||
            (score == bestScore && disaster.id < best->id)) {
            best = &disaster;
            bestScore = score;
        }
    }
    return best;
}

void disasterConfigure(DisasterState& state, const ModePack& pack, uint32_t seed,
                       const std::vector<std::string>& fallbackWeaponPool)
{
    const DisasterDefinition* selected = disasterSelect(pack, seed);
    if (!selected) {
        state = DisasterState{};
        return;
    }

    state = DisasterState{};
    state.configured = true;
    state.packId = pack.id;
    state.disasterId = selected->id;
    state.name = selected->name.empty() ? selected->id : selected->name;
    state.description = selected->description;
    state.seed = seed;
    state.durationTicks = (uint32_t)(selected->durationSeconds * 60.0f + 0.5f);
    state.winPolicy = selected->winPolicy.empty() ? "last_actor_alive" : selected->winPolicy;
    state.weaponPool = selected->weaponPool.empty() ? fallbackWeaponPool : selected->weaponPool;
}

void disasterBegin(DisasterState& state, const std::vector<uint32_t>& actors,
                   uint32_t startTick)
{
    state.weaponByActor.clear();
    state.winnerActor = 0;
    state.resolved = false;
    state.resolveSource = 0;
    state.startTick = startTick;
    state.active = state.configured && !state.weaponPool.empty();

    for (uint32_t actorId : actors) {
        // Salt by both the round seed and the actor id so two actors in the
        // same round cannot always draw the same weapon, and so the same actor
        // draws the same weapon on every server and client.
        const uint64_t salt = ((uint64_t)state.seed << 32) ^ (uint64_t)actorId;
        if (const std::string* picked = pickStable(state.weaponPool, state.seed, salt))
            state.weaponByActor[actorId] = *picked;
    }
}

const std::string* disasterWeaponForActor(const DisasterState& state, uint32_t actorId)
{
    auto it = state.weaponByActor.find(actorId);
    return it == state.weaponByActor.end() ? nullptr : &it->second;
}

bool disasterDurationElapsed(const DisasterState& state, uint32_t tick)
{
    if (!state.active || state.durationTicks == 0)
        return false;
    return tick >= state.startTick + state.durationTicks;
}

uint32_t disasterPickTimeoutWinner(uint32_t seed, const std::vector<uint32_t>& survivors)
{
    if (survivors.empty())
        return 0;
    // Score each survivor from (seed, its own id) and take the highest, with the
    // id as a stable tie-breaker. The result therefore does not depend on the
    // order the survivors were collected in.
    uint32_t best = 0;
    uint32_t bestScore = 0;
    for (uint32_t id : survivors) {
        const uint32_t score = mixSeed(seed, (uint64_t)id * 0x9e3779b97f4a7c15ull);
        if (best == 0 || score > bestScore || (score == bestScore && id < best)) {
            best = id;
            bestScore = score;
        }
    }
    return best;
}

// ── World-independent selftest ──────────────────────────────────────────
namespace {

int nextCheck = 0;
bool check(bool condition, const char* what, std::string& report)
{
    ++nextCheck;
    if (!condition)
        report += std::string("  FAIL: ") + what + "\n";
    return condition;
}

} // namespace

bool disasterRuntimeSelfTest(std::string& report)
{
    nextCheck = 0;
    report.clear();
    std::ostringstream out;

    ModePack pack;
    pack.id = "survive_disasters";
    DisasterDefinition d;
    d.id = "random_weapon_last_alive";
    d.name = "Random Weapon Last Alive";
    d.durationSeconds = 90.0f;
    d.weaponPool = {"revolver", "rocket", "knife", "shotgun"};
    d.winPolicy = "last_actor_alive";
    pack.disasters.push_back(d);

    // Same (pack, seed) selects the same disaster.
    const DisasterDefinition* a = disasterSelect(pack, 1234);
    const DisasterDefinition* b = disasterSelect(pack, 1234);
    bool ok = true;
    ok &= check(a && b && a->id == b->id, "same seed selects same disaster", report);

    // Order-independent selection: reorder the disasters, same seed -> same id.
    ModePack reordered = pack;
    std::reverse(reordered.disasters.begin(), reordered.disasters.end());
    const DisasterDefinition* c = disasterSelect(reordered, 1234);
    ok &= check(c && a && c->id == a->id, "reordered manifest keeps selection", report);

    DisasterState state;
    disasterConfigure(state, pack, 777);
    ok &= check(state.configured && state.disasterId == d.id,
                "configure stores selected disaster", report);
    ok &= check(state.durationTicks == 5400, "duration converts seconds to ticks", report);

    const std::vector<uint32_t> actors = {11, 22, 33, 44};
    disasterBegin(state, actors, 100);
    ok &= check(state.active && state.weaponByActor.size() == actors.size(),
                "begin assigns a weapon per actor", report);

    // Same seed + same actor set -> identical assignment.
    DisasterState again;
    disasterConfigure(again, pack, 777);
    disasterBegin(again, actors, 100);
    bool same = true;
    for (uint32_t id : actors)
        same &= disasterWeaponForActor(state, id) &&
                *disasterWeaponForActor(state, id) == *disasterWeaponForActor(again, id);
    ok &= check(same, "same seed gives same per-actor weapon", report);

    // Reordered actor list still gives each actor the same weapon.
    std::vector<uint32_t> reorderedActors = {44, 11, 33, 22};
    DisasterState reorderedState;
    disasterConfigure(reorderedState, pack, 777);
    disasterBegin(reorderedState, reorderedActors, 100);
    bool stable = true;
    for (uint32_t id : actors)
        stable &= *disasterWeaponForActor(state, id) == *disasterWeaponForActor(reorderedState, id);
    ok &= check(stable, "actor order does not change assignment", report);

    // Assigned weapons all come from the declared pool.
    bool inPool = true;
    for (const auto& kv : state.weaponByActor)
        inPool &= std::find(d.weaponPool.begin(), d.weaponPool.end(), kv.second) != d.weaponPool.end();
    ok &= check(inPool, "assigned weapons come from the pool", report);

    ok &= check(!disasterDurationElapsed(state, 100 + 5399) &&
                disasterDurationElapsed(state, 100 + 5400),
                "duration boundary is exact", report);

    const std::vector<uint32_t> survivorsA = {11, 22, 33};
    const std::vector<uint32_t> survivorsB = {33, 11, 22};
    const uint32_t w1 = disasterPickTimeoutWinner(777, survivorsA);
    const uint32_t w2 = disasterPickTimeoutWinner(777, survivorsB);
    ok &= check(w1 == w2 && w1 != 0, "timeout winner is order-independent and deterministic", report);
    ok &= check(disasterPickTimeoutWinner(777, {}) == 0, "empty survivors resolve to none", report);

    out << "  checks=" << nextCheck << "\n";
    report = out.str() + report;
    return ok;
}

} // namespace MimitaGamemode
