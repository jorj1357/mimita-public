# Duel Outline Team Identity

Time created: 2026-10-26T00:32:25Z
Time last updated: 2026-10-26T00:32:25Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/graphics/graphics-presentation.md`

Related changelog:
`docs/changelog/2026-10-26/20261026_003225-duel-outline-team-identity.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-26T00:31:52Z`.

The Duel profile loaded with `player_outlines=true`, red enemy color, and
`enemy_outline_alpha=1.0`, but renderer event `player_visuals.outline` for
actor 1000 reported `drawn=false` and `skipped:team_identity_unknown`.

### Expected Behavior

When Duel's authoritative participant roster identifies an actor as opposing
the local player's team, the renderer applies the mode's enemy outline policy.

### Actual Behavior

The render caller gated replicated team lookup on `CommunityMatchClient::active()`.
That method excludes Duel, so Duel's replicated participant team IDs were not
passed to the renderer.

### Why This Is Bad

The configured outline was enabled but could not be applied to an enemy whose
team ID was missing at the render boundary.

### Specification

`docs/specs/graphics/graphics-presentation.md` says:

> In a community match, `player_outlines` must be explicitly true before the
> renderer may draw any actor outline; outlines are applied from the active mode
> profile, and unknown-team actors remain unoutlined.

The Duel presentation must use the server-replicated known team identity; it
must not guess an unknown actor's team.

### Wrong Code

File: `src/engine/engine-tick-render.cpp`, former team resolution around
`engineTickRender`'s network-entity render loop.

```cpp
if (MimitaNet::CommunityMatchClient::instance().active()) {
    const uint8_t replicatedTeam =
        MimitaNet::CommunityMatchClient::instance().teamForActor(kv.first);
    remoteTeam = replicatedTeam == 0xFF
        ? -1 : static_cast<int>(replicatedTeam);
}
```

### Confirmed Cause

The `active()` predicate is false for Duel even when `CommunityMatchClient`
contains Duel's replicated actor identities. The render loop therefore passed
`-1` for the local and remote teams.

Evidence: before the fix, `logs/2026-10-26/20261026_001810/events.jsonl`
recorded `local_team=-1`, `actor_team=-1`, and
`reason=skipped:team_identity_unknown` for actor 200000; the Duel profile event
in the same run contained `player_outlines=true`, red enemy color, and alpha
1.0.

### Attempted Fix 1

Time: `2026-10-26T00:30:12Z`.

Change: use `localTeam()` and `teamForActor()` whenever those replicated IDs
exist, independently of the generic community-match `active()` predicate. Keep
the actor's existing team as fallback when no replicated identity is present.

Result: build 3356 succeeded. A fresh Duel NPC Elite run logged actor 200000
with `actor_team=0`, local team 1, `mode_forced=true`, and `drawn=true` at
`2026-10-26T00:32:07Z`. Human visual acceptance remains pending.

### Corrected Code

File: `src/engine/engine-tick-render.cpp`, `engineTickRender` network-player
and network-NPC render loops.

```cpp
const uint8_t replicatedTeam = communityMatch.teamForActor(kv.first);
if (replicatedTeam != 0xFF)
    remoteTeam = static_cast<int>(replicatedTeam);
```

The local team uses the same “query if present, otherwise keep the existing
actor team” rule.

### Fix

Do not use the generic community-match activity flag as a proxy for whether
authoritative team identities are available. Query the replicated identity
owner directly for all network actors. Unknown actors remain unknown and are
still suppressed by the renderer.

### Proof

Automated/runtime evidence: build 3356 succeeded and the dev-loop launched
Duel NPC Elite on `xxity2`. Journal:
`logs/2026-10-26/20261026_003112/events.jsonl`.
The `player_visuals.outline` event at sequence 1417 reports actor 200000 as
team 0, local team 1, `mode_forced=true`, and `drawn=true`.

Human review: not yet confirmed; the structured event proves the draw path ran,
not that the result looked correct on screen.

