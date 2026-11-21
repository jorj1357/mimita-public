# Counter-Strike NPC targeting and movement regression

## Observed behavior

During a human Counter-Strike playtest on `dust2cyberiav4`, the team-spawn
problem appeared fixed, but some NPCs did not attack the nearby human. Terrorist
NPCs escaped spawn farther than Counter-Terrorist NPCs, while other NPCs held a
wall, doorway, or crate corner and repeated the same movement attempt.

The supplied screenshot shows several NPCs clustered at a narrow doorway and is
human-observation evidence, not automated runtime proof.

## Cause found

Counter-Strike selected the configured `opposite_team` policy, but the active
behavior-profile branch applied weighted Rage2 target scoring instead of the
nearest-hostile selection used by Sandbox. In addition, the hostile-team gate
read `ServerPlayer.matchTeam` without falling back to the authoritative
`ServerGamemodeState.matchTeams` roster. A stale mirror could therefore make a
valid human target appear teamless.

The policy stuck-recovery branch requested a repath and jump but did not first
choose a locally open direction, allowing repeated pressure into the same
obstacle.

## Correction

- Resolve human target teams from the authoritative roster map first.
- Use nearest hostile selection for Counter-Strike `opposite_team`, including
  both humans and NPCs; keep Rage2 for combat tuning.
- Choose the most open local unstuck direction before jumping and repathing.
- The earlier spawn correction is recorded in the Counter-Strike feature
  document and was reported fixed by the human playtest.

## Evidence

- `python build_agent.py`: BUILD SUCCESS; 2 translation units compiled and the
  executable was relinked.
- `--counterstrike-acceptance-selftest`: PASS.
- `--npc-targeting-selftest`: PASS.
- `--spawn-tag-selftest`: PASS.
- Live post-fix acceptance remains pending.

## Follow-up — unified movement executor (2026-10-04)

The correction above is now enforced architecturally. Counter-Strike and Sandbox
both reach the one shared `NpcSystem::updateOneNpc` through a typed
`NpcMovementContext`, selected by the generic actor-preset `movement_executor`
(`sandbox_shared`). Mode code supplies only target/objective/goal; the shared
executor owns steering, wall avoidance, pathing, stuck recovery, and physics.
Rage2 remains combat tuning. See
`docs/changelog/2026-10-04/20261004_233026-unified-npc-movement-executor.md`
and Counter-Strike feature Attempt 17.

## Follow-up — wall-escape diagnostic verification (2026-10-05)

The deferred items in `docs/specs/20261005-counterstrike-wall-escape-handoff.md`
are closed:

- The `--npc-movement-policy-selftest` failure was a **stale test fixture**, not
  the Option-A movement change: after Phase 1 the actor was already past the
  hard-coded Phase 2 wall at `x=4`, so no wall was encountered. The wall is now
  placed relative to the actor (`npc->body.pos.x + 3.0f`); the assertion is
  unchanged and passes.
- The `npc.wall-escape` diagnostic is now proven to reach the canonical
  `logs/<date>/<run>/events.jsonl` via `--npc-wall-escape-event-selftest`, which
  reads the file back and asserts a `counter_strike` / `team:0` record.
- The backtrack event is rate-limited to once per episode (it had fired once per
  tick while an actor was pinned).

See `docs/changelog/2026-10-05/20261005_011500-wall-escape-event-verify.md` and
Counter-Strike feature Attempt 18. Live CT-spawn escape acceptance remains
pending.

## Reported recurrence — Fun Counter Strike CTs repeatedly dash into spawn (2026-11-20)

A user reported that Fun Counter Strike NPC behavior had previously worked and
then regressed. The reported prior-good behavior is: Terrorist NPCs choose one
bomb site and rush it; CT NPCs split or cover either site, then share a valid
last-spotted Terrorist position after a CT sees one and continue fighting around
that information. The current reported failure is CT NPCs staying at a spawn
corner and repeatedly dashing into it instead of leaving spawn.

This is a **reported recurrence**, not yet independently reproduced or confirmed
as a code regression. The current desired behavior is already stated in
`docs/specs/gamemodes/funcounterstrike.md`, section 12 (Objective rush and team
commitment): Terrorists make a stable team site choice; CTs defend usable sites
without learning the hidden choice; perception-backed reports cause CTs to rush
the reported enemy area. This specification supports the behavior target but
does not prove when or where the old behavior worked.

Tracked as `issue-be1adb51b6ac` in
`docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md`.
Reproduction must measure NPC net displacement alongside dash attempts/results,
route and goal state, stuck recovery, spawn geometry, and CT report creation and
receipt. The root cause is unknown; do not raise global movement speed or dash
probability until a pinned live Fun Counter Strike run identifies the first
failing stage. The active dev-loop run checked at intake was Sandbox, so it is
not evidence for this FCS report. Historical FCS proof of the reported prior-
good behavior remains to be located.
