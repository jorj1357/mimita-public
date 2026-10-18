# Live animation JSON reload

Timestamp: 2026-10-18 16:16:13 UTC  
Branch: `20261011zt`  
HEAD before this work: `702973f3506712aa6e37741b523b17a1dbbad8c8`

## Result

Authored animation JSON now follows the JSON configuration contract: the
registry polls avatar JSON, the selected animation file, included files, and
included directories at 100 ms intervals. At the start of a fixed simulation
tick, it parses changed data into a temporary set and installs a complete
valid set in place. Existing actor pointers remain valid. Invalid saves emit
`anim.set.reload_rejected` and keep the last valid set. Successful reloads emit
`anim.set.reloaded` with the revision, included paths, and activation tick.

Animation specification now states the live-edit delay, failure behavior,
active-action behavior, and distinction between clip edits and rig/model
re-imports. JSON configuration architecture routes authored animation to that
contract. The stale source-review statements about quaternion support and
Melee exclusive pose composition were corrected. The Melee spec links the live
edit contract.

## Files and exact changes

- `docs/architecture/json-configuration/json-configuration.md:39`: added the
  policy that live presentation JSON is polled while active, validated as a
  whole, and applied at a safe boundary; added authored animation to the JSON
  inventory and linked its verification contract.
- `docs/specs/animations/animations.md:364`: corrected implementation evidence
  for quaternion parsing/SLERP and exclusive Melee poses. Lines 567-621 define
  100 ms polling, next-tick activation, atomic set replacement, active-action
  continuity, bad-save retention, rig/model boundaries, and diagnostics.
  The live-edit acceptance section requires revision/timing evidence, an edit
  during an active move, invalid-save retention, and visual review.
- `docs/specs/gamemodes/supersmashbrosmelee.md:700`: replaced the stale pose
  audit with a source review and linked the shared live-edit rule. This file
  already had user edits; those were preserved.
- `src/entities/player-avatar-animations.cpp:82`: included JSON dependencies
  are tracked and invalid includes reject the candidate. Lines 183-273 parse
  candidates separately, retain a previous valid set on failure, keep cache
  nodes stable, and log load/reload revisions. Lines 296-319 poll the selected
  avatar file and all dependencies every 100 ms and install changes before
  actor updates.
- `src/entities/player-avatar-animations.h:32`: added activation-tick metadata
  and dependency timestamp storage; changed polling to accept the activation
  tick.
- `src/entities/player-animation.cpp:326`: actors refresh their set through
  the stable registry entry and record the current simulation tick when a new
  set is loaded. This file already had user edits; those were preserved.
- `src/sim/simulate-tick.cpp:94`: added the registry poll before player and NPC
  animation updates at the start of each eligible simulation tick. This file
  already had user edits; those were preserved.

## Exact prior and new text

- `src/entities/player-avatar-animations.cpp`: before, the registry watched
  only `avatar.json` and a hardcoded `animations.json`, and erased entries on
  invalidation. After: it watches the selected animation path and included
  dependencies, reloads candidates in place, and retains previous valid data
  on failure.
- `src/entities/player-avatar-animations.h`: before: `void pollReload();` and
  one `includeWrite` timestamp. After: `void pollReload(std::uint32_t
  activationTick = 0);` and per-path `dependencyWrites`.
- `src/entities/player-animation.cpp`: before: the animation set was resolved
  only when the avatar name changed. After: `getAnimationSet` is also called
  for the already-bound avatar, with its current simulation tick.
- `src/sim/simulate-tick.cpp`: before: no animation-registry poll ran at the
  fixed-tick boundary. After: `AvatarAnimationRegistry::instance().pollReload(
  (std::uint32_t)sim.player->movementSimulationTick);` runs before actor pose
  updates.
- `docs/specs/animations/animations.md`: before: “`animations.json` and the
  shared library are hot-reloaded by file mtime.” After: a detailed contract
  specifying a 100 ms poll, next fixed-tick application, atomic replacement,
  existing actor updates, and safe rejection of incomplete or invalid saves;
  a live-edit acceptance scenario checks active moves and invalid saves.
- `docs/architecture/json-configuration/json-configuration.md`: there was no
  authored-animation family or live-presentation reload rule. Added the family
  and shared reload policy.
- `docs/specs/gamemodes/supersmashbrosmelee.md`: before, the source audit said
  Melee still layered idle, aimbody, weapon, dash, and freeze poses after
  authored clips. After, the audit describes the authored-only path and notes
  that runtime visual verification remains open; the live edit behavior links
  to the shared animation spec.

## Pre-existing edits

The working tree had extensive unrelated user changes, including edits to the
Melee specification, `player-animation.cpp`, and `simulate-tick.cpp`, plus
deleted asset files. No unrelated edits were reverted or included in this
change. The Melee spec, player animation, and simulation files were edited in
place while preserving their pre-existing hunks.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/animations/animations.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Read `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/documentation-checker-v1.md`. Result: PASS_WITH_HUMAN_REVIEW;
  source and spec agree on live polling, safe replacement, and tick boundary;
  current-game visual acceptance is still required.

## Validation and remaining evidence

- Dev-loop status reported published build 2578, success, source generation
  23 matching the latest successful generation. The changed C++ files had
  modification times before the build executable was linked.
- Ran `C:\mimita-v9\.dev\builds\2578\mimita.exe --versioninfo`; it reported
  build 2578 and
  `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_121347/events.jsonl`.
- Focused `git diff --check` found no whitespace problems in the new code or
  JSON architecture text. It reported trailing spaces in user-provided raw
  notes in the already-edited Melee spec; those notes were left unchanged.
- Did not run a gameplay save-and-see check. The open dev-loop client is build
  2575 (PID 25252), and its dedicated server is dead. The local dev-loop
  daemon is running an older script revision and rejected a subsequent build
  request as `unknown AI action: build`. Launching build 2578 through the loop
  would replace the open client, so the session was preserved.
- The new code compiled in published build 2578. Live file-save detection,
  visible update timing, and human acceptance remain unverified until build
  2578 is run in a fresh game session and an animation value is saved.
