# Scenario input recording and playback

- Time UTC: `2026-10-25T01:28:19Z`
- Display: `2026-10-24 21:28:19 EDT`
- Branch: `20261011zt`
- HEAD: `0472121b` plus the existing uncommitted worktree
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Added local per-tick input recording and fixed-60-Hz playback so a human or
agent can record movement, dashing, aiming, firing, reloading, and weapon
changes, then replay the same input through the normal fixed-tick gameplay
path. Also fixed the scenario-runner condition that reused a run with a live
server but no live client.

## Commands

New terminal commands:

```text
scenario_startrecord [name]
scenario_endrecord
scenario_status
scenario_play <path.jsonl>
scenario_stop
```

## Files added

- `src/devtools/scenario-recorder.{h,cpp}`
- `src/devtools/scenario-player.{h,cpp}`
- `src/devtools/scenario-commands.{h,cpp}`
- `src/input/scenario-input.{h,cpp}`
- `docs/specs/dev-loop/scenario-input-recording.md`
- `devscripts/scenarios/user/README.md`
- `devscripts/scenarios/recorded_input_smoke_v1.json`

Recordings are local-only under `devscripts/scenarios/user/*.jsonl` and are
gitignored.

## Input surface

`src/input/input-frame.h` now carries the per-tick data needed by both local
simulation and the authoritative input packet:

- camera-relative `wishSide`/`wishForward`
- `camForward`
- `primaryHeld`, `secondaryHeld`
- `shieldHeld`, `grabHeld`
- `slotHeldMask`

`src/input/input-poll.cpp` populates them in `buildInputFrame()`.
`src/input/scenario-input.cpp` provides the playback override used by combat,
camera FOV, and authoritative input.

## Recorder

`ScenarioRecorder` writes append-only JSONL under
`devscripts/scenarios/user/scenario-YYYYMMDD_HHMMSS[-name].jsonl`:

- header with schema, tick rate, map, mode, weapon set, player, avatar, source,
  and recorded start state
- one line per 60 Hz fixed tick with input intent and context
- footer with frame count, duration, and stop reason
- flush every 6 ticks
- 10-minute/36,000-frame cap

The hook is in `src/engine/engine-tick-replay.cpp` immediately before
`simulateTick()`.

## Playback

`ScenarioPlayer`:

- loads and validates the JSONL once
- rejects map, mode, or weapon-set mismatch
- restores recorded position, aim, weapon, and start velocity through the
  normal command/state path
- waits for the client and server to agree on the start position and for the
  teleport to settle
- runs at fixed 60 FPS through `FramePacer`
- feeds the recorded `InputFrame` to local simulation and the recorded intent
  to `MpInput`
- applies the recorded camera and mouse/key overrides before combat
- reports the first divergence above 1 m and `max_position_error_m`
- emits `scenario.playback.started`, `start_ready`, `divergence`, and
  `completed`

## Dev-loop integration

`devscripts/dev-loop.py`:

- added a first-class `input_playback` step
- reads the recording header before launch and uses its map and weapon set
- relaunches on missing/empty client, launch-mode mismatch, stale build, build
  mismatch, or requested weapon set
- waits for `scenario.playback.completed` using the step timeout rather than
  the 15-second command timeout

The earlier scenario failure reused a run with a live server but
`client_ready=false` and `client_pids=[]`, so commands timed out. The runner now
relaunches in that case.

## Validation

- Canonical dev-loop build: `SUCCESS`, final build `3151`.
- Record test: `devscripts/scenarios/user/scenario-20261025_012422-smoke3.jsonl`,
  163 frames, command stop.
- Playback test: `scenario.playback.completed` with `frames_played=163`,
  `start_restored=true`, first divergence at frame 62 (`1.72 m`), max position
  error `24.66 m` in final run `20261025_014608` on build `3151`. Repeated
  runs show the first divergence varying with live server state, which is why
  playback reports divergence rather than claiming deterministic parity.
- A local dev-loop playback scenario passed with four assertions. The reusable
  checked-in example is `devscripts/scenarios/recorded_input_playback.example.json`;
  it points at a local recording path by design.
- The max-error result is expected for input playback against a live server and
  possible state differences; it is recorded as divergence evidence, not a
  deterministic replay claim.

## Known limits

- Input playback is not a full deterministic world replay. NPCs, remote players,
  and server corrections can differ.
- Normal mouse/keyboard play is the supported capture path. Terminal-only
  actions that do not pass through `InputFrame` are not guaranteed.
- Melee shield/grab are recorded, but full melee combo fidelity is not claimed.

## Pre-existing work preserved

Existing uncommitted collision, networking, configuration, GUI, and Oct 24
changelog work was preserved. No commit was created.
