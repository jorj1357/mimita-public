# Landing animation priority and playback

- Status: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-18T20:38:15Z (2026-10-18 16:38:15 EDT)
- Branch: `20261011zt`
- HEAD: `3bf384e4c6c0f5378b42f29762fef7e828c800df`
- Scope: make an authored landing clip override movement immediately, play from tick zero, and remain selected for its clip duration.

## Finding

The state selector already chose `land` before run/walk, so locomotion did not own a higher selection priority. The failure was the hard-coded 0.12-second landing window, which ended before a longer authored clip could finish. Also, a repeated landing while `currentAnimName` was already `land` did not reset `animStateTime`. The selector now documents and enforces this order from low to high: idle, walk, run, crouch, land, fall, jump, hurt, respawn, death. An explicit locked action clip continues to override the state selector.

## Changes

1. `src/entities/player-animation.cpp`
   - Replaced the unconditional `landAnimTimer = 0.12f` with the selected land clip's `durationTicks / 60.0f`; keeps 0.12 seconds only when no land clip is available.
   - Kept landing above crouch and walk/run, and moved jump/fall ahead of landing so an airborne action can still interrupt it.
   - On landing, sets the animation clock so that after this tick's normal `dt` increment, the evaluator samples tick 0. Resets again if the same landing clip is already active.
2. `docs/specs/animations/animations.md`
   - Added the built-in state precedence, full-duration one-shot behavior, tick-zero start, and repeated landing restart contract.

## Exact before and after

- `src/entities/player-animation.cpp`, `updateProceduralAnimation`, around lines 416–425.

Before:

```cpp
    // Brief landing pose timer for the optional `land` state clip.
    if (ground.didLand)
        landAnimTimer = 0.12f;
    else
        landAnimTimer = std::max(0.0f, landAnimTimer - dt);
```

After:

```cpp
    // Landing outranks locomotion and holds the whole authored one-shot. Keep
    // the short legacy fallback only when this profile has no landing clip.
    const AnimClip* landingClip = sm ? findClip(sm->land) : nullptr;
    const float landingDuration = landingClip
        ? (float)std::max(1, landingClip->durationTicks) / 60.0f
        : 0.12f;
    if (ground.didLand) {
        landAnimTimer = landingDuration;
    } else {
        landAnimTimer = std::max(0.0f, landAnimTimer - dt);
    }
```

- In the same function, the old selector chose `land` before `jump`/`fall`; the new selector chooses airborne `jump`/`fall` first, then `land`, then `crouch` and `run`/`walk`. The landing clock is set to `-dt` on a landing transition or same-clip retrigger, then the existing `animStateTime += dt` makes the first sample exactly tick 0.
- `docs/specs/animations/animations.md`, State machine, around lines 592–598. Before this task the built-in actor-state selector's precedence was not specified. Added:

```text
For the built-in actor-state selector, precedence from lowest to highest is
`idle < walk < run < crouch < land < fall < jump < hurt < respawn < death`.
An explicit locked action clip may replace the selected state clip. A landing
state interrupts walk/run immediately, starts at clip tick 0 on the landing
tick (including a retrigger while the same clip is active), and holds until its
non-looping clip duration has elapsed. If the profile has no landing clip, the
existing brief landing fallback remains.
```

## Validation

- Canonical dev-loop build succeeded: build 2609, request `2026-10-18T20:37:09.682Z-fef572dd`.
- Ran the exact published executable `C:\mimita-v9\.dev\builds\2609\mimita.exe --versioninfo`; it reported `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_163724/events.jsonl` and exited after identity reporting.
- The profile's existing landing clip is 7 ticks. The new owner logic will hold that animation for all seven fixed ticks, restarting at tick 0 on each landing.
- The active dev-loop client was preserved: it remains build 2607, PID 2268; auto-restart is false. No new interactive client was launched, so the new source's gameplay behavior has not yet been exercised in a live match. The earlier build 2608 was superseded by 2609 after the final priority-order edit.
- `git diff --check` passed for the source and specification changes. Git reported only its LF-to-CRLF normalization notices.
- No test was added or run. No regression was confirmed; the code fix still needs a live visual trial.

## Review and preserved work

- Read: `docs/ROUTER.md`, `docs/specs/animations/animations.md`, `docs/specs/movement/movement.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/features/live-code-development/live-code-development.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/operations/build-and-exe/build-and-exe.md`, and `docs/operations/task-completion/task-completion.md`.
- Also reviewed `docs/regressions/README.md`, the prior Captain Falcon landing and walk/run changelogs, and `docs/skills/spec-behavior-review-v1.md`.
- Focused review: `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW; priority and interruption behavior now match the requested state ordering, but visuals remain unverified.
- The worktree already contained unrelated edits, including earlier animation specification changes, Captain Falcon JSON clips, melee code/configuration, and other project docs. They were preserved. This task only added the priority section to the already-dirty animation spec.
