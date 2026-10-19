# Gold example: find an animation mismatch in the live journal

- Date recorded: 2026-10-19
- User's recognition time: 2026-10-19 17:49 EDT (UTC-04:00)
- Evidence run: `20261019_213626`
- Audience: AI agents and developers investigating behavior in a running MiMITA game
- Purpose: show how live runtime evidence can reveal the cause of a reported animation problem, even when the user did not mention logs.
- This document does not claim that a journal entry proves the animation looked right to a human. It records the runtime's selected clip and the configuration change that corrected that selection.

## The useful response

The response that motivated this example was:

> "I found the mismatch: Falcon’s walk state already points to the walking clip, but faster WASD movement switches to a separate `captainfalcon_run` clip. The live journal shows that switch happening. I’m pointing both moving states to the walking clip so movement keeps the animation you asked for."

The user had asked for WASD walking to use Falcon's walking animation instead of looking like the wrong animation. They had not asked us to inspect a journal or named the event that would prove which animation was selected. The running client had already recorded the answer.

## What the evidence showed

The evidence came from the active run's canonical journal:

`C:\mimita-v9\logs\2026-10-19\20261019_213626\events.jsonl`

Before the change, client event sequence `102449` recorded:

- avatar: `captainfalcon`
- event: `anim.state.changed`
- `moving: true`
- `to: captainfalcon_run`
- `movement_mode: walk`

The avatar profile already mapped `states.locomotion.walk` to
`captainfalcon_walk`. The animation selector switches to `states.run` when the
movement speed exceeds its run threshold, and that profile mapped `states.run`
to `captainfalcon_run`. The live event therefore exposed the precise mismatch:
the walk mapping was right, while the faster-movement mapping selected a
different clip.

## Small fix and live proof

The change was one profile value in
`assets/avatars/captainfalcon/animations.json`:

```diff
- "run": "captainfalcon_run",
+ "run": "captainfalcon_walk",
```

The existing dev-loop client hot-reloaded the profile. In the same run,
`anim.set.reloaded` sequence `102519` reported revision 3. Immediately after,
`anim.state.changed` sequence `102529` recorded `moving: true` and
`to: captainfalcon_walk`. Later movement events continued to select
`captainfalcon_walk` at fast movement speeds.

This before-and-after evidence connected the user's symptom to the runtime
selection, and then confirmed the intended clip was selected after the edit.
The configuration update did not need a code rebuild or a game restart.

## Reusable evidence-first approach

When a user reports that a visible behavior is wrong, look for existing live
evidence before guessing at the cause:

1. Identify the running dev-loop run and its exact `events.jsonl`; do not mix
   evidence from another run, process, or executable.
2. Search for the owner-level event that records the decision related to the
   symptom. Here, `anim.state.changed` recorded the chosen clip.
3. Read the event's surrounding fields, such as actor, movement state, selected
   clip, process, tick, and run ID. Follow the code and active config to explain
   why the owner made that choice.
4. Make the smallest change at the owner or configuration that controls the
   decision.
5. Observe a new event in the same live run that confirms the changed decision.
6. Report what the log proves, and say separately if visual or human acceptance
   still needs review.

If the expected event is not present, absence alone does not explain the bug.
Trace the input-to-output path and add a bounded diagnostic at the responsible
owner if the existing journal cannot identify the first wrong or missing step.

## What this proves

- The client selected `captainfalcon_run` for fast movement before the change.
- The profile reload succeeded in the active runtime.
- The client selected `captainfalcon_walk` for movement after the change.
- The runtime evidence pointed to a specific configuration mismatch without
  requiring the user to know which diagnostic to request.

It does not prove that the rendered walk cycle looked good in person. Runtime
state evidence and human visual acceptance are different claims.
