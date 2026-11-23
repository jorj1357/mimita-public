# mode3-latency-goal: late-join review and build resource-coupling datapoint

2026-11-23 17:23 UTC. Agent `mode3-latency-goal`, wake `wake-9724b287e9d8`,
context `ctx-5c4916ed87b7`, issue `issue-5d82fc58a875` (owned by
`latejoin-team-identity`). No source files were edited; no build or test-run was
started in this session.

## What was done

- Acknowledged `ctx-5c4916ed87b7`; confirmed ownership of the late-join fix is
  `latejoin-team-identity` and did not duplicate it.
- Independently read the pinned gen565 run `20261123_165703` before its journal
  rotated and confirmed the core behavior: server
  `match.actor.late-join-assigned` actor_id=2 team=1 role=elite_fighter
  reason=live_late_join; client-02 `client.gamemode.local-actor-identity`
  found=true team=1; `player_visuals.outline` `team_identity_known=true` drawn
  for a teammate and an enemy; `npc.target-changed` team0 NPCs
  `target_actor=2`; `server.damage.applied` attacker_type=npc
  target_actor_id=2 target_name=LateJoin target_team=1 (7 hits).
- Reported to `latejoin-team-identity`: acceptance names
  `gamemode.spawn-new-actor`, which the journal never emitted (observed
  `spawn.server-selected` instead); and the live journal was reset/rotated, so
  proof must pin/copy the exact journal or re-run on generation 566.
- Reported build-latency evidence to `build-coordinator`
  (`issue-136d4553ff92`) and resource evidence to `resource-efficiency-goal`
  (`issue-ed3ac775283d`).

## Build resource-coupling datapoint

- Generation 566 (`game-dll-20261123T171100081510Z-c4f29b`), only 2 compiled TUs
  (641 skipped): total 242.157s; compile 135.969s (`server-gamemode.cpp`
  98.422s, generated `module-identity-provider.cpp` 37.364s); lld link 32.315s
  for 642 objects / 484,685,544 bytes; `jobs=10`.
- Generation 565 (`game-dll-20261123T165431661826Z-6c221c`), 29 compiled TUs:
  compile 76.122s (`server-gamemode.cpp` 48.345s), link 6.27s, total 89.011s.
- The same TU was about 2x slower and the link about 5x slower while one server
  plus two clients from run `20261123_165703` were live. Edit-to-published
  latency is dominated by the single `server-gamemode.cpp` TU and the monolithic
  link, not type-check count.
- A prior gen17 attempt failed pre-worker in 4.06s with `codegen check failed:
  packet_registry_codegen.py` after `server-gamemode.cpp` changed; regeneration
  must precede the batch when that file changes.

## Wake/goal-mode question

`docs/specs/dev-loop/agent-wake.md` defines `agent-wake.py` as a durable,
condition-driven queue that starts a bounded agent turn when there is
actionable, unowned or owned-unblocked work (one active wake per issue/agent,
cooldowns, halt/freeze aware). It is not a perpetual "pursue goal" loop, but a
standing goal issue plus `wake-check`/`next --claim`, scheduled review wakes, and
proof gates is the MiMITA equivalent: agents are re-woken until the goal's proof
exists. The "wake" sessions in the board are those bounded activations.
