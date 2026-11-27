# Build, process, and event implementation handoff (items 1–7)

- **From:** `spec-event-build-volumes`
- **Date:** 2026-11-27
- **Checkout:** `C:\mimita-v9`, branch `checkpoint/one-checkout-condensation-20261111`
- **Base commit:** `33ea17d5`
- **Current published generation:** 805 (`build/game/generation-805/game-g805.dll`)
- **Handoff reason:** specs are complete; implementation is larger than the
  remaining context. No code for items 1–7 exists yet.
- **Continuation owner:** the next top-level OpenCode chat that reads this file.

Read `AGENTS.md`, `docs/ROUTER.md`, and this handoff before editing.

---

## 1. Current state

### Done (specified only)

These specs are written and routed. They are the authority for the work:

- `docs/specs/dev-loop/spec-authoring.md`
- `docs/specs/dev-loop/build-preflight-and-admission.md`
- `docs/specs/dev-loop/build-performance-budget.md`
- `docs/specs/dev-loop/hot-modules-and-header-boundaries.md`
- `docs/specs/dev-loop/process-lifecycle-and-release-events.md`
- `docs/specs/dev-loop/event-driven-runtime.md`
- `docs/specs/dev-loop/agent-broadcast-and-shared-context.md`
- `docs/specs/networking/visible-hit-volumes.md` (deferred item 8)
- `docs/specs/animations/animation-and-weapon-pose-editor.md` (deferred item 9)

Extended: `event-driven-coordination.md`, `agent-runtime-registry.md`,
`system-resource-measurement.md`, `contact-authority.md`,
`animation-editor.md`, `docs/ROUTER.md`.

Changelog: `docs/changelog/2026-11-27/20261127_1910-build-process-event-hit-volume-specs.md`.

### Not done

- No source, config, build, or runtime change for items 1–7.
- No `check` request, `process-guard`, frame cap, taskbar policy, audio policy,
  Job Object, fast profile, or hot module split.
- Item 8 (visible hit volumes) and item 9 (animation/weapon editor) are
  deferred until 1–7 are in and measured.
- Current generation 805 only reflects another agent's
  `src/npc/recast-navigation.cpp` change.

---

## 2. Evidence to preserve

Build measurements used by the specs:

| Case | Result |
|---|---|
| Generation 804 total | 192 s |
| `src/npc/recast-navigation.cpp` | 123.5 s (single TU) |
| lld link, 646 objects / 506 MB → 237 MB | 46.2 s under load |
| Generation 640, one changed TU | 95.9 s compile, 11.4 s lld |
| Generation 634, 55 changed TUs | 95.0 s compile wall, 15.4 s lld |
| Header fan-in | `pch.h` 636, `structured-log.h` 360, `physics-types.h` 312, `movement-types.h` 292, `map-mesh-types.h` 287, `weapon-types.h` 286, `config.h` 286, `avatar-rig.h` 280, `actor-body.h` 272 |
| Source ownership | 626 game, 42 resident, 3 bootstrap, 25 unknown/stale |
| Client frame cap | `config/video-settings.json` `maxFrames: 999` |
| Observed client rate | `avg_frame_ms 3.55` (~282 FPS), roughly 0.85–1.0 logical core |
| Audio | `Audio::AudioUpdate` median under 1 ms; first-play spikes 100–270 ms |
| Toolchain | `C:\important\msys64\mingw64\bin\g++.exe`, `ld.lld.exe`; `ccache` at `C:\ProgramData\chocolatey\bin\ccache.exe` |
| Hidden test windows | `MIMITA_AGENT_HEADLESS_GUI=1` in `devscripts/dev_loop/parallel_test_runs.py` (lines 770, 930); `src/renderer/renderer.cpp` creates the window hidden and never shows it |
| Hidden test audio | clients never call `setServerAudioMode(true)`; only the server does (`src/game/server-policy-module.cpp`) |

Caveat: the parallel CPU investigation reports that `maxFrames` alone "hasn't
done much". Treat the frame cap as a measured per-client factor, not a proven
sole cause. Item 1 exists to settle this.

---

## 3. Ordered work items

Execute in order. Do not ask for per-item confirmation. Stop only for a
scope-changing decision, an unbuildable tree, or an unanswered shared-file
lease after one short window.

### Item 1 — Automatic measurement with reasons

**Goal:** explain CPU/memory per process before changing behavior.

**Do:**

- Reuse `devscripts/dev_loop/build_resource_sampler.py` measurement mechanics
  where practical.
- Add a low-overhead sampler surfaced by `agent-coord sync` and the dashboard.
- For every MiMITA PID report: role (client/server), run ID, mode/map, owner
  agent, issue, build/generation, DLL SHA, command line, parent, start time,
  journal, last event age, CPU normalized per logical CPU and whole host,
  working set/private memory, frame cap, actual FPS, audio state, NPC count.
- Label unregistered processes `unknown`; never stop them.
- Record sample duration, host logical CPU count, and concurrent build phase.

**Acceptance:** a single sync/status call prints the table; an unregistered
process is visible as `unknown`; overhead is measured and bounded; no process
is terminated.

**Likely files:** `devscripts/agent_coord.py`,
`devscripts/dev_loop/build_resource_sampler.py`, `devscripts/dev-loop.py`,
`devscripts/evidence-watch.py`, dashboard readers.
**Coordinate with:** `cpu-investigation`, `npc-cost-and-measurement`.

### Item 2 — Frame cap, taskbar, and audio policy

**Goal:** stop unseen clients from eating a core and making sound; make agent
runs discoverable.

**Do:**

- Add `--window hidden|taskbar|visible` to test-run and dev-loop launch;
  default **taskbar** for agent runs.
- In `src/renderer/renderer.cpp`, `taskbar` shows the window minimized (or
  iconifies it) and leaves a taskbar entry; `hidden` never shows it.
- Window title: `MiMITA [<run_id>] [<mode>] [<owner or issue>]`.
- Agent test clients run at 60 FPS; human clients may use 144/240. Do not
  enable VSync.
- Add an explicit client mute path (for example `setClientAudioMuted(true)` or
  `setAudioEnabled(false)`), distinct from `setServerAudioMode`.
- Hidden and taskbar runs are muted unless the lease says `audio_policy: on`.
- Record `fps_cap`, actual FPS, and `audio_policy` in `performance.fps` or a
  nearby event.
- Dashboard: list live windows with run/mode/owner/issue/generation/journal and
  a Watch action; a Stop action gated by ownership.

**Acceptance:** an agent run appears in the taskbar; a hidden/taskbar run is
silent; one client at 60 FPS stays under 0.25 logical core; the journal proves
the cap and audio state.

**Likely files:** `src/renderer/renderer.cpp`, `src/video/frame-pacer.*`,
`src/video/video-settings.*`, `src/main-systems.cpp`,
`src/audio/audio.cpp`, `src/main.cpp`, `devscripts/dev-loop.py`,
`devscripts/dev_loop/cli.py`, `devscripts/dev_loop/parallel_test_runs.py`,
dashboard UI.
**Note:** the current `MIMITA_AGENT_HEADLESS_GUI=1` behavior is the cause; keep
it only as an explicit `hidden` policy, not the default.

### Item 3 — Process guard, leases, and Job Objects

**Goal:** every process proves why it exists and cannot outlive its work.

**Do:**

- Add `devscripts/process-guard.py`:
  `scan`, `explain <pid>`, `stop --run <id>`, `watch <run_id>`, `orphans`.
- Extend the runtime lease with `purpose`, `issue_id`, `proof_target`,
  `keep_until` event, `owner_session`, `window_policy`, `audio_policy`.
- Release events: `issue.proven`, `task.ended`, `owner.session.ended`,
  `run.superseded`, `build.finished`, `build.failed`, `run.stop_requested`.
- No time-based kill. Time only raises `process.stale`/`NEEDS_REVIEW`.
- Run build workers and test runtimes in Windows Job Objects with
  `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`.
- On `build.finished`/`failed`/cancel, terminate the job and verify no
  `cc1plus`, `g++`, or `ld.lld` remains.
- Every activation runs `process-guard scan`; task end stops own runs.
- Bun/OpenCode: report parent/session/port; never kill a live worker.

**Acceptance:** start two runs, end the session, both stop or are flagged with
the exact reason; a finished build leaves no compiler child; a process without
a lease is visible; no unrelated process is stopped.

**Likely files:** new `devscripts/process-guard.py`,
`devscripts/dev_loop/runtimes.py`, `devscripts/dev_loop/parallel_test_runs.py`,
`devscripts/agent-runtime.py`, `.dev/runtimes` readers,
`devscripts/dev_loop/game_build.py` (job object).

### Item 4 — Compile-check gate

**Goal:** the build request is the seal, not the first compile.

**Do:**

- Add `dev-loop.py --request check` and `--check-status <id>`.
- Ladder C0–C5 from `build-preflight-and-admission.md`:
  - C0 snapshot/revision;
  - C1 dependency/fan-out (`dependency_graph.py`);
  - C2 include lint;
  - C3 `-fsyntax-only` on changed TUs with real flags/PCH, memory-aware jobs,
    below-normal priority;
  - C4 symbol/link preflight from an object symbol index;
  - C5 estimate and admission.
- Write a `check` record with revision, changed paths, stage results, estimate,
  admission, and `result`.
- A build request MUST name a passed `check_id` with the same revision.
- Any source/header/flags/PCH change invalidates the check.
- Events: `check.requested/started/stage/passed/failed/invalidated`,
  `admission.blocked/cleared`.
- Start with C1+C3; add C2/C4 after the first proof.

**Acceptance:** a syntax error is rejected without a build slot; a header edit
reports the exact affected TU count; a build without a matching check is
rejected; changing a file after the check invalidates it.

**Likely files:** `devscripts/dev-loop.py`, `devscripts/dev_loop/cli.py`,
`devscripts/dev_loop/game_build.py`, `devscripts/dependency_graph.py`,
`devscripts/build-note.py`, new check-record storage under `.dev/`.
**Honest limit:** C4 is a strong filter, not a link proof.

### Item 5 — Fast build profile and outlier split

**Goal:** make a one-file change cost seconds, not minutes.

**Do:**

- Add a `fast` build profile: `-Og -g0`, `-ffunction-sections`,
  `-fdata-sections`, `-Wl,--gc-sections`, `-Wl,--build-id=none`; optional
  stripped runtime artifact with an unstripped copy kept for symbols.
- Give each profile its own object-cache identity.
- Replace hardcoded `--jobs 10 --priority high` in `dev_loop/game_build.py`
  with memory-aware jobs and below-normal compile priority.
- Split `src/npc/recast-navigation.cpp` into build/config, tile adoption, and
  query adapter; no part may exceed a few seconds.
- Make the eleven module-identity wrappers use runtime identity instead of
  rebuilding on every generation.
- Measure ccache hit rate and wall time; keep it only with a measured win,
  otherwise disable it in the worker. Record the decision in the postmortem.

**Acceptance:** one leaf change ≤10 s p50 / ≤20 s p90; the recast split is under
10 s; the postmortem names the largest cause and the next action; ccache has a
recorded keep/disable decision.

**Likely files:** `devscripts/game_build_worker.py`,
`devscripts/dev_loop/game_build.py`, `devscripts/build_note`/postmortem,
`src/npc/recast-navigation.cpp` and new split files, identity generation.

### Item 6 — Header budgets and include lint

**Goal:** one header owns one concept and is included by as few files as
possible.

**Do:**

- Add the budget table and ratchet from
  `hot-modules-and-header-boundaries.md`.
- Report-only first: record the baseline in the check and postmortem.
- Add include lint for: new include of an over-budget header from a leaf, hot
  header including a cold/aggregate header, public header without an owner,
  upward dependency edge.
- Turn the lint into a gate for changed files after the baseline is clean.
- Split the top three headers first: `structured-log.h`,
  `physics-types.h`, `movement-types.h`.

**Acceptance:** a header edit reports its exact dependent count before
compiling; no over-budget header gains a dependent; the lint rejects an upward
edge and a new aggregate include; the top three are below half their 2026-11-27
fan-in.

**Likely files:** `devscripts/dependency_graph.py`, new
`devscripts/include_lint.py` (or an extension), `devscripts/build-note.py`,
the header files themselves.

### Item 7 — Hot module extraction

**Goal:** sub-10-second reload for a changed module.

**Do:**

- Follow the module map in `hot-modules-and-header-boundaries.md`:
  `core`, `gameplay`, `present`, `editor`, `net`, `content`.
- Extract `content` and `editor` first; then `present`, `net`, `gameplay`;
  `core` last.
- Keep one frozen snapshot and one generation manifest with per-module SHA-256.
- Only changed modules relink and reload; a runtime pins one manifest and
  rejects mixed module generations.
- Reuse `src/hot-reload/module-host.cpp` and the existing module API; do not
  invent a second loader.

**Acceptance:** a change in one module relinks/reloads only that module; the
other modules keep the same SHA; a one-module change meets the sub-10-second
budget; a mixed-manifest runtime is rejected.

**Likely files:** `src/hot-reload/*`, `src/game/*bridge-module.cpp`,
`src/mapeditor/map-editor-module.cpp`, `src/modules/api/*`,
`devscripts/game_build_worker.py`, generation manifest writers.

---

## 4. Deferred items (do not start yet)

### Item 8 — Visible hit volumes

Spec: `docs/specs/networking/visible-hit-volumes.md`.

Current code already has `actorHitVolumeContains()` and
`actorPartAtPoint()` in `src/network/server-contact.cpp`, per-part boxes in
`src/combat/weapon-fire-raycast.cpp`, and `validateHitVolume` for melee.
Missing: one cached `ActorHitVolume`, server projectile parts instead of
capsules, claim verification for every damage source, and per-part multipliers.

**Decide after 1–7:** whether the current part-box hitscan is good enough for
the user-visible contract, or whether the full `ActorHitVolume` extraction is
needed. Do not start it until the build/process work is measured.

### Item 9 — Animation and weapon pose editor

Spec: `docs/specs/animations/animation-and-weapon-pose-editor.md`.

Fold the animation timeline and weapon/hand pose into the map editor; shared
pose function for first person, third person, cosmetics, muzzle, and hit origin;
per-avatar/clip overrides. Plan after 1–7.

---

## 5. Build, run, and proof instructions

- The root dev-loop is the only builder:
  `python devscripts/dev-loop.py`.
- Before the first compiler-input save, queue a build note:
  `python devscripts/build-note.py --changes ... --why ... --expected-seconds ... --files ...`.
- File a durable build request:
  `python devscripts/agent-coord.py build request --auto --kind integration --why ... --expected-seconds ... --paths ... --issue ...`.
- Only an approved request may build:
  `python devscripts/dev-loop.py --request agent-build --build-request <id>`.
- Start a pinned run:
  `python devscripts/dev-loop.py --start-test-run --test-launch-mode <id> --test-game-generation <gen>`.
- Drive commands:
  `python devscripts/dev-loop.py --command "<cmd>" --run-id <run> --client client-01 --expected-run-id <run>`.
- Observe evidence:
  `python devscripts/evidence-watch.py list --json` then `show`/`tail`.
- Never invoke `build.py`, `build_agent.py`, `module-build-worker.py`, a
  compiler, `ccache`, or a second dev-loop.
- Preserve other agents' runs. Four `mimita` processes were live at handoff;
  they are not this task's runs.
- If a link names an undefined symbol for existing source, follow the
  build-self-repair runbook in `AGENTS.md`; do not chase it file by file.

At handoff there is no in-progress build and no object-cache quarantine.

---

## 6. File ownership and coordination

This session changed only documentation:

- the nine new specs and five extended specs listed above;
- `docs/ROUTER.md`;
- `docs/changelog/2026-11-27/20261127_1910-build-process-event-hit-volume-specs.md`;
- this handoff.

Other active work at handoff:

- `cpu-investigation` and `npc-cost-and-measurement` — CPU/NPC measurement.
  Coordinate item 1 with them; do not duplicate their sampler.
- `rocket-rl-settle`, `rocket-league-*` — vehicle arena work; they touch
  `src/engine/engine-tick-camera.cpp` and vehicle files.
- `body-rig-ads-cont` — body sockets, rig, ADS, damage modes.
- `map-authored-parity` — navmesh/recast work (relevant to item 5's recast
  split; coordinate before editing `src/npc/recast-navigation.cpp`).
- `issue-review-sweeper` — issue review.

Before editing a shared file, run `agent-coord edit begin`; commit/abort
immediately. Run `agent-coord sync --agent <name>` at activation and after
each phase. Do not stop, adopt, or edit another agent's run.

Relevant issues:

- `issue-ed3ac775283d` — measure and reduce idle MiMITA CPU (the CPU finding
  was appended here).
- `issue-b9edd402e658` — automatically show host resource use and process
  reasons.
- `issue-2d4032ac8044` — socket lock/crouch/tick animation/editor; item 9
  relates.
- build-friction issues `issue-31ab18dedc96`, `issue-2235461bb31b`,
  `issue-c632de44f8d6`, `issue-1b08300935ae` — build waits over budget; item 4
  and item 5 address the cause.
- launch-friction `issue-150c895a1d99` — mode start timeout; item 2/3 may
  improve visibility.

Unanswered questions to settle with the user before item 8:

- Is the current per-part hitscan already sufficient for visible = hit, or is
  the full `ActorHitVolume` extraction required?
- Should server projectiles switch to parts even if it costs more contact time?
- Which per-part multipliers are final?

---

## 7. Known limits

- No item 1–7 code exists. Do not claim any of it is implemented.
- The CPU cause is not proven. The frame cap is a measured per-client factor;
  the sampler must attribute the rest.
- The 46 s link was measured under saturation; the 11–15 s link is the normal
  floor.
- `maxFrames` alone may not reduce host CPU if another process is the cost.
- No runtime proof exists for taskbar/audio, process guard, check gate, fast
  profile, header budgets, or hot modules.
- Human visual acceptance is still required for taskbar/audio and editor work.

---

## 8. Wake command

The handoff is ready but this session did not launch the next agent. To start
the continuation as a new top-level OpenCode chat:

```text
python devscripts/agent-wake.py add --agent build-process-event-impl --when manual --adapter opencode --prompt "Read AGENTS.md, docs/ROUTER.md, and docs/handoffs/2026-11-27-build-process-event-implementation-handoff.md. Continue items 1-7 in order. Preserve the listed file ownership. Do not start items 8-9 until 1-7 are measured. Run agent-coord sync and claim the work."
python devscripts/agent-wake.py run --once
```

The new chat becomes the continuation owner. It must run
`agent-coord sync --agent build-process-event-impl`, inspect the board, and
claim/continue the task.
