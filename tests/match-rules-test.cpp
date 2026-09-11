// 09 10 2026
/* purpose
* Unit-test the pure authoritative match lifecycle rules in network/actor-match.h.
* Verifies the Alive/Dead/Respawning/Spectating transitions and kill-heal policy
* without the server, networking, or a graphics context.
* Does NOT test the server loop, packets, or gameplay simulation.
*/

#include <cstdio>

#include "network/actor-match.h"

using namespace MimitaNet;

static int gFailures = 0;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

static const char* name(ActorState s)
{
    switch (s)
    {
        case ActorState::Alive: return "alive";
        case ActorState::Dead: return "dead";
        case ActorState::Respawning: return "respawning";
        case ActorState::Spectating: return "spectating";
    }
    return "?";
}

static void expect(ActorState in, bool dead, bool respawns, ActorState want, const char* msg)
{
    const ActorState got = nextActorState(in, dead, respawns);
    if (got != want)
    {
        ++gFailures;
        std::printf("FAIL: %s (from %s dead=%d respawns=%d got %s want %s)\n",
                    msg, name(in), (int)dead, (int)respawns, name(got), name(want));
    }
}

int main()
{
    // ── Kill-heal policy ────────────────────────────────────────────
    check(matchKillHeals(true, true) == true, "kill_heals=true heals");
    check(matchKillHeals(true, false) == false, "kill_heals=false does not heal");
    check(matchKillHeals(false, false) == true, "gamemode disabled keeps legacy heal");

    // ── Respawning mode: Alive -> Dead -> Respawning -> Alive ───────
    const bool R = true;
    expect(ActorState::Alive, false, R, ActorState::Alive, "alive stays alive");
    expect(ActorState::Alive, true, R, ActorState::Dead, "lethal -> dead");
    expect(ActorState::Dead, true, R, ActorState::Respawning, "dead -> respawning");
    expect(ActorState::Respawning, true, R, ActorState::Respawning, "respawning holds");
    expect(ActorState::Respawning, false, R, ActorState::Alive, "respawn -> alive");

    // ── One-life mode: Alive -> Dead -> Spectating (terminal) ───────
    const bool N = false;
    expect(ActorState::Alive, true, N, ActorState::Dead, "one-life lethal -> dead");
    expect(ActorState::Dead, true, N, ActorState::Spectating, "one-life dead -> spectating");
    expect(ActorState::Spectating, true, N, ActorState::Spectating, "spectating is terminal");
    expect(ActorState::Spectating, false, N, ActorState::Alive, "new round resets to alive");

    if (gFailures == 0)
        std::printf("PASS: actor match rules (%d cases)\n", 12);
    else
        std::printf("FAILURES: %d\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
