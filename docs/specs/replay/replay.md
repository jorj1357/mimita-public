# Replay Recording, Editing, and Export

**Purpose:** specify how MiMITA records gameplay and lets people inspect,
replay, edit, and export it.
**Audience:** simulation, networking, animation, audio, effects, rendering, and
creator-tool contributors.
**Status:** active desired behavior; implementation is partial and runtime
acceptance is incomplete.
**Does not:** promise deterministic simulation across different machines,
replace live gameplay, or define a separate video-editing application.

## Goals

- Record a short recent segment or a deliberately started and stopped session.
- Review a match, jump through time, and select useful moments.
- Combine ranges from multiple replay files into one creator project.
- Edit camera position and movement, field of view, playback speed, transitions,
  captions, audio, and supported effects.
- Export locally, with clear progress and failure information.
- Preserve a per-tick input-and-state recording for future tool-assisted
  playback and authored scenes.

The replay editor is part of MiMITA's in-game creator workspace. The broader
timeline, imported media, titles, and export workflow are specified in the
[Video Editor](../creator-tools/video-editor.md) document.

## Recording and project data

The user can start and stop a recording through a discoverable in-game action.
The recent-clip action remains a fast path; its current key binding and clip
duration must be read from the active configuration rather than duplicated in
this specification.

Replay data records the events and state required by the existing replay and
effects contracts. It must preserve event order, simulation tick identity,
actor state, relevant inputs, sounds, animation, and effect events. Use the
shared gameplay and presentation paths during playback rather than replay-only
implementations.

Long recordings must be written and read incrementally so duration is not
limited by holding the entire recording in memory. A project may refer to
multiple replay files and ranges without copying each source into a duplicate
large file. Missing or incompatible sources must be reported clearly.

## Editing behavior

- Scrub and play at normal, slowed, and accelerated speed.
- Select an interval, trim it, and arrange ranges from multiple replays.
- Place and animate replay cameras, including position, orientation, and field
  of view.
- Control supported effects, transitions, motion blur, and audio levels.
- Choose whether playback includes captions and relevant chat presentation.
- Save edits as a separate project; never rewrite the original replay when
  trimming or arranging a movie.
- Undo and redo project edits.

Simulation-tick timing is authoritative for recorded events. Rendered output
may use another frame rate by sampling replay time; output frame rate must not
alter gameplay or event order.

## Fidelity and tool-assisted playback

The first correctness target is playback fidelity for the same replay data and
compatible game content. Replays must identify incompatible content or
configuration rather than silently presenting a materially different result.
Ragdoll state, sound retriggering, animation, and effects each require separate
acceptance; a successful export does not prove those details are correct.

Future tool-assisted playback may replay recorded inputs through the normal
simulation and compare the resulting state against recorded checkpoints. Exact
replay on the same build, configuration, assets, seed, and fixed-tick sequence
is a staged engineering target.

The long-term vision is for an NPC to reproduce a long recorded performance
across machines with negligible positional error, with an aspirational
one-hour position error below 0.00001 world units. This is a research goal
only; it is not a current acceptance requirement or implementation milestone.
Cross-machine determinism must not be claimed until measured across supported
hardware, builds, configurations, and content.

## Export and performance

Interactive and headless export use the same render path and report
equivalent final status. Interactive export shows progress at least once per
60 source simulation ticks and reports completion or failure. Export must not
require every frame to be displayed in a visible window.

Editing input remains responsive while final frames are rendered. Measure
preview frame time, export throughput, memory use, and frame pacing on named
hardware. A 240-FPS export on a low-end device remains an aspiration to measure,
not a universal guarantee. Follow the fixed 60-Hz gameplay simulation rule;
do not raise the simulation tick to meet an export frame-rate target.

## Reported problems to verify

Earlier user notes reported missing ragdoll fidelity, sounds not retriggering,
and export failures. These are investigation targets, not proof of current
behavior. The 2026-10-27 export screenshot records an exit-code-3 failure with
unknown cause; retain its evidence and diagnose from the exact export request,
subprocess output, and live journal before assigning an owner.

## Acceptance scenarios

1. Start and stop a recording, reopen it, and verify actors, audio, animation,
   effects, and tick positions.
2. Export a recent clip and a long recording without unbounded memory growth.
3. Build one project from ranges in multiple replay files and preserve the
   source files.
4. Edit a replay camera, field of view, audio, and an effect, then confirm the
   export matches the project.
5. Interrupt or fail export and show a useful status while leaving the
   project and source replay recoverable.
6. Compare same-build input replay against recorded checkpoints; report
   cross-machine results separately.

## Related specifications

- [Video Editor](../creator-tools/video-editor.md)
- [Distribution](../distribution/distribution.md)
- [Effects and replay fidelity](../effects/effects.md)
- [Performance](../performance/performance.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
