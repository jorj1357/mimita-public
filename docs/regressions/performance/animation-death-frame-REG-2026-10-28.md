# Animation and death frame choppiness

Time created: 2026-10-28T13:34:00Z
Time last updated: 2026-10-28T13:34:00Z

Status: ATTEMPTED FIX (1) — major measured improvement; full performance acceptance remains open

Related specification:
`docs/specs/performance/performance.md`

Related changelog:
`docs/changelog/2026-10-28/20261028_041903-plan-a-gameplay-diagnostics.md`

Related gold note:
`docs/gold/20261028_093400-animation-and-death-choppiness.md`

## Regression Occurrence 1

### Observed

During 2026-10-28 animation/death performance investigation, gameplay became
severely choppy as animation and corpse work accumulated. In a Juggernaut run,
walk/run state flapping produced 75,689 `anim.state.changed` records in about
two minutes. A music change also produced a 701.9 ms `Audio` scope. See the
related changelog's Animation and death choppiness continuation for the source
trace and build/run details.

### Expected behavior

Animation, corpse presentation, diagnostics, and music changes should not
introduce large gameplay-thread stalls or repeated per-actor work. Work that
does not need the fixed simulation tick should run outside it; per-tick work
should avoid needless allocations and transitions.

### Actual behavior

Each actor's per-tick animation sampling allocated temporary containers, the
selector built a JSON candidate array with debugging disabled, hot-reload
polling performed filesystem timestamp scans on the gameplay thread, and
walk/run repeatedly restarted at one threshold. Corpse updates used an
expensive alive-body solver and emitted frequent traces/effects. Opus track
changes could synchronously invoke FFmpeg on the gameplay thread.

### Confirmed causes and corrected owners

- `src/entities/player-animation.cpp`: `interpolateAnimClip` used a map and
  string vector per actor per tick; candidate JSON was unconditional. Sampling
  now uses fixed `AnimOverlayBuffer` storage, and candidate JSON is built only
  when `anim_priority_debug` is enabled.
- `src/entities/player-avatar-animations.cpp` and weapon configuration
  reload owners: periodic gameplay-thread file scans, including duplicate
  weapon polling, were replaced by a shared background JSON watcher and a
  single weapon-definition scan.
- `src/entities/animation-movement-state.h`: the 12 m/s walk/run boundary
  caused clip flapping; hysteresis keeps 12 m/s to enter run and requires
  dropping below 10 m/s to return to walk. State-change diagnostics are also
  rate-limited per actor.
- `src/ragdoll/ragdoll-mode.cpp` and ragdoll configuration: corpse simulation
  now has a reduced corpse-only solver budget, less frequent blood, and
  high-volume diagnostics behind trace logging.
- `src/audio/music-manager.cpp`: Opus decode/cache work moved to a background
  thread so track changes do not synchronously launch FFmpeg on the main
  gameplay thread.

### Proof and human report

The related changelog records successful dev-loop builds through build 3633
and live runtime evidence:

- Build 3633, Juggernaut mode 9, 32 active avatars, journal
  `logs/2026-10-28/20261028_132142/events.jsonl`.
- `anim.state.changed` decreased from 75,689 to 1,244 records.
- In steady 30–45 second windows, average frame time was 5.5–8.8 ms and p95
  was 8–16 ms per 60-frame window. Corpse step averaged about 0.55 ms/frame.
- The prior 701.9 ms music spike was absent.
- The player reported at 2026-10-28 09:34 EDT that the previously very bad
  choppiness “now seems much better.”
- Follow-up human confirmation at 2026-10-28 09:45 EDT — jorj:
  “its smooth i ahvent had issues choppiness etc yet.” This is human smoothness
  acceptance for the reviewed session, not proof of 240 FPS for 60 seconds.

These observations support a substantial improvement and confirm the traced
costs were reduced. The 240-FPS-for-60-seconds goal was not achieved, so this
record remains an attempted fix rather than a claim of complete performance
acceptance. Snapshot/NPC reset bursts, first-time avatar binding, 32-actor
rendering, and mass-death sound startup remained candidates in the recorded
run. Reprofile a representative busy scene and ask the player to confirm the
improvement persists before marking the issue fully solved.

### Permanent lesson

Evaluate work by its execution frequency and multiplicity. A small operation
inside every actor's 60 Hz update can become a major cost as actor count grows.
Avoid per-tick allocation, repeated serialization, filesystem polling, and
unbounded diagnostic output. Use measured owner-level evidence and comparable
runtime windows to separate a confirmed cause from remaining candidates.
