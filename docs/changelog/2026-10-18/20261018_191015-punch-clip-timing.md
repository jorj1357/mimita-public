# Use the punch clip as the Melee timeline

- Timestamp: 2026-10-18T19:10:15Z (2026-10-18 15:10 EDT)
- Branch: `20261011zt`
- HEAD before work: `b3bd13ae8b8b4d46db0bd339577f60a3a873cb18`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Summary

The grounded Captain Falcon primary punch now references `punch_a`. Its
duration and phase lengths are resolved from that clip's `durationTicks` and
`phase.windup`, `phase.active`, `phase.recovery`, and `action.end` markers.
Server gameplay and the local client read those resolved lengths; the client
samples the one animation at its exact action elapsed tick. The 60-tick clip
currently yields windup 20, active 20, recovery 20. The hitbox remains active
for active-phase-local ticks 11 through 14, preserving the previous four-tick
window after clamping.

Actions without `animation_clip_id` still use the old `phases` format as an
explicit migration path so the unmigrated move roster continues to work. An
action that does provide a clip ID is rejected if its clip ID is missing,
ambiguous, or has invalid markers; it cannot fall back to weapon-config timing.

## Changed files and exact edits

### `config/weapons.json:1694-1701`

- Before:
  ```json
  "phases": {
    "windup":   {"ticks": 5, "clip_id": "cf_punch_windup"},
    "active":   {"ticks": 15, "clip_id": "cf_punch_active"},
    "recovery": {"ticks": 15, "clip_id": "cf_punch_recovery"}
  },
  ...
  "active_start_tick": 11, "active_end_tick": 22
  ```
- After:
  ```json
  "animation_clip_id": "punch_a",
  ...
  "active_start_tick": 11, "active_end_tick": 14
  ```

### `src/combat/weapon-melee-config.cpp:30-79,285-309`

- Before: `if (j.contains("phases") && j["phases"].is_object()) {` then each
  phase was passed to `parsePhase(...)`.
- After: `bool loadClipTimeline(const std::string& clipId, int& duration, int& windupEnd, int& activeEnd)` scans `config/animations/library`, validates
  unique clip ID and required ordered markers, and derives phase lengths from
  `durationTicks`. `parseAction` rejects invalid referenced timelines and
  retains the explicit old `phases` path when no clip ID is specified.

### `src/combat/weapon-melee-config.h:121`

- Before: `MeleePhase recovery;` was followed by the hitbox vector.
- After: `std::string animationClipId;` records the one-clip action reference.

### `src/combat/melee-client.cpp:416-448`

- Before: `const int total = windup + active + recovery;` and
  `player.playOneShotPhase(clip, phaseTick, phaseTicks);`
- After: `const int total = action->totalTicks();` and
  `player.playOneShotAtTick(action->animationClipId, elapsed);`
  format uses one `animationClipId` and requests the exact elapsed clip tick.

### `src/entities/player.h:644,655,662` and `src/entities/player-animation.cpp:169-178,399-400,641-642`

- Before: there was no `lockedClipSampleTick` field or `playOneShotAtTick` API.
- After: `int lockedClipSampleTick = -1;` and
  `void playOneShotAtTick(const std::string& clip, int clipTick);` pin a clip to the action's exact clip-local tick;
  the action owner clears it at the exclusive end. Existing phase playback is
  unchanged for migration actions.

### `src/network/server-melee-actions.cpp:567,609-622,689,867`

- Before: `const int total = windup + active + recovery;`
- After: `const int total = action->totalTicks();` and the start event emits
  `"weapon_config_timing_used", false`; all completion checks use the parsed action timeline, and player
  action start emits `melee.action.timeline` with clip ID, duration, phase
  lengths, and `weapon_config_timing_used=false`.

### `docs/specs/animations/animations.md:359-369`

- Before: `legacy phase timing must never override the referenced clip.`
- After: `Existing actions with only the old phases object remain an explicit migration compatibility path until each is given a single clip and markers.` Clip timing is explicitly the runtime path whenever
  `animation_clip_id` is present; old `phases` is explicitly marked as a
  temporary compatibility path for not-yet-migrated actions.

### This changelog

- Before: no code-change record existed for this session.
- After: this file records the result, evidence, and review still required.

## Specification and code path

The routed requirements are in `docs/specs/animations/animations.md`,
`docs/specs/weapons/weapons.md`, and
`docs/specs/gamemodes/supersmashbrosmelee.md`. Relevant workflow and review
documents: `docs/workflows/runtime-scenario-validation.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/skills/spec-behavior-review-v1.md`, and
`docs/skills/asset-checker-v1.md`.

The client obtains the pose clip through Captain Falcon's existing
`assets/avatars/captainfalcon/animations.json` include of
`config/animations/library/melee/captainfalcon/punch_a.json`. The weapon
parser independently reads that shared clip's timeline markers so the server
does not depend on an avatar-render profile to decide authoritative hit
eligibility.

## Pre-existing workspace edits preserved

Before implementation, the worktree already contained user work in
`config/animations/library/melee/captainfalcon/punch_a.json`,
`docs/specs/animations/animations.md`, and
`docs/specs/animations/blender-animation-authoring.md`, plus the untracked
changelogs `20261018_150000-punch-timeline-markers.md` and
`20261018_150200-melee-clip-timing-editable-spec.md`. This change preserves
those edits; only the animation spec received the compatibility clarification
above. No regression was confirmed.

## Validation evidence

- `config/weapons.json` and `punch_a.json` both parse as JSON.
- `git diff --check` passed.
- Canonical dev-loop build request
  `2026-10-18T19:09:18.787Z-6c107db0` succeeded and published build 2601 for
  source generation 12. Build output reported `Compiled: 2`, `Skipped: 553`.
- The exact executable
  `C:\mimita-v9\.dev\builds\2601\mimita.exe --versioninfo` ran and printed
  `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_151003/events.jsonl`.
- The existing dev-loop runtime is still build 2594, and its server process
  was already not alive. No current-build gameplay scenario was run, and no
  claim of runtime hit timing or visual acceptance is made. The existing
  client/server processes were not stopped.
- An earlier build request failed because the checkout build lock was held;
  after the active lock owner completed, retrying through the canonical dev
  loop succeeded. No direct compiler path was used.
- Focused skills: `spec-behavior-review-v1` — PASS for the clip-reference
  contract, with other actions still awaiting migration; `asset-checker-v1` —
  PASS for the referenced shared JSON path and existing avatar include.
- Human review still needed: play `a_ground_punch` in the current build and
  confirm pose presentation and active hit feel.
