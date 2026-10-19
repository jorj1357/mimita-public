# Zombie Tower first playable slice spec review

- Status: `REVIEW_COMPLETE_NEEDS_SPEC_DECISIONS`.
- Timestamp: 2026-10-19T22:13:00Z (2026-10-19 18:13:00 EDT, America/New_York).
- Scope: Read-only assessment of the requested first playable Zombie Tower slice; no source, config, or spec edits.
- Skills: Followed `docs/skills/spec-behavior-review-v1.md`.
- Router note: `docs/doc-review-09-03-2026.md`, listed by `docs/ROUTER.md`, is absent in this checkout.

## Findings

1. The new opening note gives the broad phase sequence and 30-second intermission, but leaves material behavior unresolved: what finishes active play, whether 80% means each floor or whole run, what checkpoints are in scope, and what the results screen contains before resetting. It explicitly lists incompatible alternatives at `docs/specs/gamemodes/zombie-tower/zombie-tower.md:38-46`.
2. The existing detailed v1 still requires a complete authored tower of at least 50 floors, checkpoints, limited attempts, revive, persistence, multiple bosses, final boss, and replay at lines 4959-4989 and a full completion acceptance at 5041-5073. This is much larger than the “for now” slice in lines 7-28. A clearly labeled first-playable-slice scope/acceptance list is needed so implementation does not silently choose between them.
3. The desired human roster is not specified as an executable rule. Section 6 calls four actors the default target and allows variable groups (lines 460-477), but does not say this first slice has a four-actor Humans team, how humans displace NPCs as they join, whether party NPCs are created only after the roster is finalized, or how the monster/spectator teams fit the mode.
4. Current mode evidence is only configuration/source, not play acceptance: `config/gamemodes/zombie_tower.json` sets `intermission_seconds: 5`, `max_count: 1`, `policy: persistent`, and `win_condition: persistent_npc_spawns`; it has Humans/NPCs teams with capacity 0 and no explicit spectator team. `config/onlinemodes.json` exposes it as selectable and `config/server-presets.json` maps `zombie_tower_1` to `zombietower4`. Server code includes persistent NPC spawning, map entity processing, checkpoints, and party-wipe/run-attempt handling, but that does not demonstrate the requested finite wave flow or a complete playable run.
5. For the one-human case, specify the invariant directly: exactly four friendly actors at start, one human plus three allied NPCs; joining humans occupy those slots and reduce/remove NPC slots so the team does not exceed four. Say whether NPCs fill only after intermission closes or immediately, and what happens if more than four humans join.
6. “Walk near it and it kills the monsters” needs an encounter contract: volume trigger/activation rule, finite monster set/count, spawn spacing/timing, whether it may retrigger, how an encounter becomes cleared, and whether all monsters stay hostile to the Humans team. Pick a small playable route/goal and a finite terminal condition for this first slice.
7. Countdown behavior needs the authoritative scope: which actors are frozen and where, whether NPCs freeze too, exact on-screen presentation for 3/2/1/GO, and the tick at which movement/combat becomes enabled. Define active-phase outcomes for player/team death, no surviving humans, disconnects, and reaching the route end.

## Evidence limits

No files were edited; no tests, build, or runtime validation were performed. Existing config/source findings are not claims that the new phase flow, four-actor roster, monster encounters, UI, or completion flow work in a live session. The existing unrelated working-tree changes were preserved.
