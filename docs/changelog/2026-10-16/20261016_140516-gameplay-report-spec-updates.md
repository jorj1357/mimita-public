# 2026-10-16 gameplay report: specification updates

- Canonical time: 2026-10-16T14:05:16Z
- Display time: 2026-10-16 10:05:16 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD at start/end: `3a34520009d91d10a571dfd08ca3ee90c9c56639`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and scope

Routed the user's spoken gameplay report and screenshots into existing behavior
specifications and the existing open-gameplay-issues index. The screenshots
were treated as evidence, not instructions. No game code, configuration,
process, build, or test was changed or run.

## Existing workspace state

The repository had extensive modified and untracked user work before this
session, including `devscripts/dev-loop.py`, networking and gameplay specs,
configuration, source, and the untracked `docs/specs/open-gameplay-issues-2026-10-16.md`
and `docs/specs/npc-behavior/npc-behavior.md`. Those files were inspected and
preserved; additions below were made in place. `docs/doc-review-09-03-2026.md`,
required by the router's documentation route, is absent in this checkout.

## Changes made

- `docs/specs/dev-loop/dev-loop.md:1465` — added status-output behavior and
  launch-stage timing requirements. Before: command reference started at
  `## 22. Command reference` and had no contract for in-place refresh or
  bounded plain-log output. After: `### Status display and launch-time
  feedback` requires in-place updates where supported, no repeated unchanged
  status blocks, distinct lifecycle/journal identity, and stage-attributed
  launch timing. The screenshot's stale uptime/empty process state is recorded
  as a report, not a diagnosis.
- `docs/specs/gamemodes/gamemodes.md:111` — recorded the selected journal's
  mismatched server/client map loads, missing map retries, and out-of-bounds
  actor/corpse presentation. Before: the spawn audit noted initial join and
  `npc_spawn` used separate GLB/local-world paths without this run's evidence.
  After: the report and a same-session initial-join/respawn/map-change/NPC-spawn
  acceptance scenario are explicit; the spawn writer remains unproven.
- `docs/specs/gamemodes/supersmashbrosmelee.md:67` — resolved NPC participation
  in first playable scope. Before: `Is the first playable scope human-versus-
  human only, or do NPCs participate?` After: NPCs use the same move,
  animation, contact, hit confirmation, and result path as players; profiles
  select decisions.
- `docs/specs/networking/networking.md:443` — added per-life equipped weapon
  agreement and journal evidence. Before: the death/respawn notes said a
  grenade works on first life and breaks on second, without specifying the
  weapon-slot agreement after respawn. After: client/server selected weapon
  state must agree before attack; journal records 26 not-equipped rejections,
  with five Rocket Launcher rejects showing logical/native slot disagreement.
  Actor generation is absent from those events, so the causal link remains
  unproven.
- `docs/specs/performance/performance.md:57` — added low-power device support,
  the frame-time interpretation of 240/999 FPS, and the need for per-thread
  profiling before multi-core redesign. Before: performance rules said
  “Measure before optimizing.” After: goals preserve fixed 60 Hz gameplay and
  describe 999 FPS as an aspiration; low total CPU utilization is explicitly
  insufficient evidence that only one core is used.
- `docs/specs/replay/replay.md:56` — added shared headless/interactive export
  and progress reporting at least once per 60 source simulation ticks, plus
  separate ragdoll/sound fidelity requirements. Before: replay notes reported
  ragdoll/sound issues but did not specify headless export or progress. After:
  desired behavior and unverified status are explicit.
- `docs/specs/visuals/blood-bulletholes-viscera.md:277` — added the requested
  complete-image decal appearance, aspect preservation, and open alive/dead
  variant policy. Before: the effect representation section listed decals and
  other representations without a stamp appearance contract. After: distortion
  is documented as the reported symptom; rendering cause remains unproven.
- `docs/specs/npc-behavior/npc-behavior.md:157` — added a profile-level
  high-challenge target and avatar-specific collision requirement. Before:
  authored combat/movement behavior focused on legal actions but did not
  specify the requested challenge benchmark or selected-avatar collision
  expectation. After: around 20 NPC kills versus five player hits is an
  uncalibrated aspiration, not a balance promise; collision detail remains
  under existing avatar/collision ownership.
- `docs/specs/open-gameplay-issues-2026-10-16.md:25` — added prioritized triage
  with rationale and owners, categorized as “Do now,” “Plan next,” and “Needs
  planning.” Before: the index was a flat issue table with work-focus notes.
  After: join/spawn and weapon rejection blockers precede dev-loop and leave/
  rejoin reproduction; gameplay, visual, performance, network, and architecture
  work have explicit dependencies and evidence limits. Also notes the later
  screenshot showing `Room: ZZCV2WK`, and the 82,126 repeated NPC-avatar
  snapshot events without blaming them for FPS.

## Existing documentation found sufficient

The one-command `ss`/leave/rejoin contract and runtime caveats already exist in
`docs/specs/networking/networking.md` section 67.1 and
`docs/specs/dev-loop/dev-loop.md`. Live mode switching is already in the
Gamemodes spec. JSON hot reload is routed to the existing JSON-configuration
and live-development specs. Reusable generic weapon requests and attack types
already exist in `docs/specs/weapons/weapons.md`. Blender-to-runtime keyframes,
root-relative posing, and 60 Hz runtime animation are already specified in
`docs/specs/animations/blender-animation-authoring.md` and
`docs/specs/animations/animations.md`; the former still labels exporter details
as a proposal. No duplicate documents were created.

## Evidence and validation

Inspected `logs/2026-10-16/20261016_134002/events.jsonl`: 111,695 parsed
records; server and client loaded different maps during the observed startup,
then both later reported `funworld3.glb`; repeated attempts to load a missing
non-v4 map were present; local actor/corpse presentation appeared around
Z=-501, below logged map bounds. The journal contained 26 server
`weapon_not_equipped` rejections and 82,126
`client.npc-avatar.snapshot-reassembled` events. The screenshot with the room
code is a later captured state and does not prove the join/rejoin sequence.

Read `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`,
`docs/skills/documentation-checker-v1.md`,
`docs/workflows/runtime-scenario-validation.md`, and
`docs/operations/task-completion/task-completion.md`. The doc review file
routed by the router is missing. Manual link/owner review was performed for
new references. No build, tests, live play, or human visual acceptance was
performed because this session changed specifications only. The user's claim
that multi-core performance is absent remains unverified; current journal data
is not per-thread profiling. Combined 500 ms network latency and low-FPS
behavior remain to be tested under a controlled scenario.

## Human review still needed

Confirm the open balance choices for melee, movement mobility, avatar collision
shape, decal alive/dead variants, combined adverse-network matrix, and the
scope of the performance targets before implementation. Reproduce the spawn,
weapon-slot, leave/rejoin, dev-loop status, decal, NPC performance, and replay
reports through the real dev-loop executable and inspect its exact journal.
