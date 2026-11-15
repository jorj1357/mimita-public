// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# hot reload spec consolidation and automation

# Task

- Task ID: agent-a-20261114-hotreload-spec
- Session ID: agent-a-20261114-hotreload-spec
- Owner: agent-a
- Focus: spec, generated status, changelog automation
- Summary: Archive competing hot-reload docs, install one current spec, add generated status/audit JSON and a changelog filler.
- Status: complete
- Date, time, timezone: 2026-11-14T22:22:23+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: da4133c1423f992ab5f61a35683229e5576c98ed
- Final commit: da4133c1423f992ab5f61a35683229e5576c98ed

# Pre-existing changes

- Exact status output: see `## Captured git state`.
- Files not created or modified by this session: `config/analytics.json`,
  `devscripts/blender-ai`, `docs/changelog/2026-11-13/20261113_082500-agent-c-whole-game-entry-fixed-tick.md`,
  `src/game/server-command-dispatch-module.cpp`,
  `src/game/server-runtime-module.cpp`, `src/network/packets.h`,
  `src/network/server.cpp`, `src/network/server-ice.cpp`,
  `src/network/server-packets.cpp`, `src/network/schema/`, and the untracked
  changelog/handoff files created by concurrent work.

# Requested behavior

Make hot reload use one static specification, stop hand-maintaining status,
generate current facts as JSON, add strong changelog automation, and prepare
agent coordination without claims JSON or lock files.

# Specification alignment

- Current specification paths: `docs/specs/hotreload/hotreload.md`,
  `docs/skills/spec-writing-v1.md`,
  `docs/operations/task-completion/task-completion.md`.
- Exact requirements: spec defines desired behavior; current status is generated
  JSON; changelogs carry history and next work.
- Why the change follows the specification: competing hot-reload status/plan
  documents were archived and replaced by one current specification.
- Conflicts or decisions: claims JSON and `live.py` authority are deprecated;
  the root dev-loop is the intended build authority, with the game-DLL request
  still to implement.

# Exact implementation changes

## File: `docs/specs/hotreload/hotreload.md`

- Lines/functions/headings: complete replacement.
- Old content or behavior: historical append-only document with current status
  and plan claims mixed into one file.
- New content or behavior: current specification with authority rules,
  Cutover-1/2/3, cold core, hot behavior, all-networking-hot, server split,
  state, input, rendering, ABI, live loop, evidence, and generated paths.
- Reason: one current source of truth.
- Why unrelated behavior is preserved: old content was moved to
  `docs/archive/hotreload/hotreload-history-2026-11-14.md`.

## File: `devscripts/audit_triage.py`

- Lines/functions/headings: parser and audit output.
- Old content or behavior: wrote CSV/Markdown/service map only.
- New content or behavior: also writes `build/hot-reload/audit.json` with rows,
  counts, high-risk rows, services, and limits.
- Reason: automatic unload-safety audit, not manual review.
- Why unrelated behavior is preserved: existing CSV/report/service outputs stay.

## File: `devscripts/hot_reload_status.py`

- Lines/functions/headings: new read-only aggregator.
- Old content or behavior: no single generated status file.
- New content or behavior: writes `build/hot-reload/status.json` from source
  manifest, build state, packet/state ledgers, and audit output.
- Reason: current facts must be generated, not hand-written.
- Why unrelated behavior is preserved: it does not build, run, or edit source.

## File: `devscripts/changelog.py`

- Lines/functions/headings: new template filler and evidence collector.
- Old content or behavior: changelogs were hand-written.
- New content or behavior: captures git state, build identity, generation,
  journal path, changed paths, and reverse commands; fills the existing
  template; `--check --session-id` verifies one changelog per session.
- Reason: make handoffs and reverse steps strong and automatic.
- Why unrelated behavior is preserved: narrative sections remain human-written;
  full diff is opt-in via `--include-diff`.

## File: `docs/changelog/TEMPLATE.md`

- Lines/functions/headings: Task, changed files, ABI, state/schema, deleted
  paths, agent handoff, automatic evidence, and reverse sections.
- Old content or behavior: no reverse/ABI/state/handoff fields.
- New content or behavior: merged template additions without duplicating the
  existing evidence sections.
- Reason: any agent can see what changed and reverse it.
- Why unrelated behavior is preserved: existing sections remain.

## File: `docs/handoffs/hot-reload-cutover-checklist-20261114.md`

- Lines/functions/headings: new temporary checklist.
- Old content or behavior: a concurrent draft appended a Cutover-1 checklist and
  current blockers directly to the specification.
- New content or behavior: preserved that checklist as a handoff scratchpad and
  removed current blockers/ownership from the specification.
- Reason: keep the spec static and keep current blockers/plans outside it.
- Why unrelated behavior is preserved: no checklist content was discarded.

## File: `docs/handoffs/hot-reload-agent-focus.md`

- Lines/functions/headings: new temporary focus scratchpad.
- Old content or behavior: coordination lived in claims/plan JSON and a dated
  sync document.
- New content or behavior: A owns spec/scripts/ABI/build; B owns server
  networking; C owns client networking; shared rules prevent same-file edits.
- Reason: coordination outside the specification without claims JSON.
- Why unrelated behavior is preserved: it is explicitly temporary and not a
  source of truth.

## File: `docs/skills/spec-writing-v1.md`

- Lines/functions/headings: new specification-writing skill.
- Old content or behavior: no general spec-writing rule.
- New content or behavior: defines what belongs in a spec and what belongs in
  generated status, changelogs, or coordination scratchpads.
- Reason: prevent status documents from becoming specifications again.

## File: `src/main.cpp` (attempted and reverted)

- Lines/functions/headings: `configureWholeGameHost`.
- Old content or behavior: normal full client forced the resident frame unless
  `MIMITA_GAME_HOT_CLIENT`/`MIMITA_GAME_DLL` was set.
- New content or behavior: attempted a hot-client default, exercised it in run
  `20261114_225952`, observed a stall after frame 1, then reverted the default.
- Reason: prove the actual hot-client blocker instead of assuming the code move
  was sufficient.
- Why unrelated behavior is preserved: the resident client path is restored.
- Evidence: `logs/2026-11-14/20261114_225952/client-01/events.jsonl` contains
  `presentation.framebuffer_probe` with a 1x1 viewport and repeated
  `performance.stall-watchdog` events (`last_completed_frame=1`).

## File: `docs/specs/hotreload/hotreload.md`

- Added the packet-ledger meaning: `client=dll|resident` counts DLL-side
  dispatch overrides, not all normal client packet handlers. The normal client
  handlers are game-owned in `multiplayer-tick.cpp` and
  `multiplayer-packets.cpp`.
- Added the 2026-11-14 17:28 provisional-status TODO.

## File: `src/game/host-compat-bridge-module.cpp`

- Removed duplicate `MimitaNet::nowMs`, `normalizeMapId`,
  `mapIdsReferToSameMap`, `parseAddress`, and `addressToString` definitions.
- Kept the crash-breadcrumb bridge, which still uses `MimitaNet::nowMs` from the
  new `net-common-module.cpp` owner.
- Reason: the dev-loop game-DLL link failed on duplicate net-common symbols.
- Result: game generation 163 links and publishes; generation 164 was built after the diagnostics below.

## File: `devscripts/dev-loop.py` and `devscripts/dev_loop/cli.py` hot-client flag

- Added `--hot-client` to the dev-loop control surface. It sets
  `MIMITA_GAME_HOT_CLIENT=1` only on the launched client process, so the hot
  client can be tested without changing the normal resident default or relying
  on daemon environment inheritance. It also works with
  `--request start-client` so a hot client can attach to an existing server.
- Added `game-dll` to the wait-for-build path so the request can be polled.
- Documented the deterministic command in
  `docs/handoffs/hot-reload-agent-focus.md`.

## File: `src/main.cpp` queue polling

- Added a file-size/mtime signature gate around the host-owned agent command
  queue. The hot-client stack samples showed repeated file open/read frames in
  `processAgentCommandQueueForDll` while the queue was empty.
- Reason: avoid hot-path filesystem I/O every frame.

## File: `src/host/host-runtime.cpp` and `.h` published-generation tracking

- Added `activePublishedGeneration_` so `poll()` compares the ready manifest to
  the last published generation, not the module's local generation field.
- Added a same-DLL identity guard: when the ready manifest names the DLL that is
  already active, only update the published generation and do not reload the
  module. Reloading the same DLL shut down the active generation and left its
  statics initialized, which caused the hot-client stall.
- Verified with `client-hot6`: 1,700+ frames, `path=dll`, 1920x1080 viewport,
  responsive, no stall.

## File: `src/perf/perf-frame.cpp`

- Changed the stall watchdog from one stack capture to a periodic capture every
  5 seconds while the frame remains stalled.
- Reason: identify the repeated hot-client hang path. Run `20261115_000136`
  captured repeated `CreateFileA` / `sopen` / `basic_filebuf` stacks in the
  queue poll and confirmed `last_completed_frame=1`.

## File: `devscripts/dev-loop.py` and `devscripts/dev_loop/game_build.py`

- Lines/functions/headings: `--request game-dll`, `queue_game_dll_request`,
  `start_game_build_worker`, `drain_game_build_completions`, game-build status.
- Old content or behavior: no dev-loop game-DLL request; `live.py` owned the
  separate game build.
- New content or behavior: dev-loop snapshots the source manifest, runs codegen
  checks, invokes `game_build_worker.py` internally, publishes
  `build/game/ready.json`, and reports game-build state. EXE and game builds are
  serialized.
- Reason: make the root dev-loop the intended build authority.
- Why unrelated behavior is preserved: the existing EXE build path and
  `live.py` remain temporarily available.

## File: `devscripts/live.py`

- Lines/functions/headings: header and `main` startup warning.
- Old content or behavior: described as the live coordinator without a
  deprecation notice.
- New content or behavior: marks the file deprecated and points to the root
  dev-loop game-DLL request as the intended authority.
- Reason: begin the dev-loop consolidation without breaking the temporary path.
- Why unrelated behavior is preserved: the existing coordinator still runs
  until the dev-loop request exists.

## File: `docs/ROUTER.md` and related references

- Lines/functions/headings: hot-reload route and live coordination references.
- Old content or behavior: routed to archived hot-reload documents, claims JSON,
  `live.py`, and outdated architecture docs.
- New content or behavior: routes to `hotreload.md`, spec-writing skill, focus
  scratchpad, generated status, and latest changelog.
- Reason: remove competing current authority.
- Why unrelated behavior is preserved: historical documents remain on disk.

# Changed files and reverse steps

This table is auto-captured and includes pre-existing worktree changes from other
agents. See “Pre-existing changes” for what this session did not touch.

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `devscripts/audit_triage.py` |  |  |  | git checkout -- devscripts/audit_triage.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/archive/hotreload/hot-cold-split-2026-11-14.md` |  |  |  | git checkout -- docs/archive/hotreload/hot-cold-split-2026-11-14.md |
| `docs/archive/hotreload/hotness-contract-2026-11-14.md` |  |  |  | git checkout -- docs/archive/hotreload/hotness-contract-2026-11-14.md |
| `docs/archive/hotreload/hotreload-history-2026-11-14.md` |  |  |  | git checkout -- docs/archive/hotreload/hotreload-history-2026-11-14.md |
| `docs/archive/hotreload/input-layer-2026-11-14.md` |  |  |  | git checkout -- docs/archive/hotreload/input-layer-2026-11-14.md |
| `docs/archive/hotreload/state-domains-2026-11-14.md` |  |  |  | git checkout -- docs/archive/hotreload/state-domains-2026-11-14.md |
| `docs/archive/hotreload/status-2026-11-14.md` |  |  |  | git checkout -- docs/archive/hotreload/status-2026-11-14.md |
| `docs/changelog/2026-11-13/20261113_082500-agent-c-whole-game-entry-fixed-tick.md` |  |  |  | git checkout -- docs/changelog/2026-11-13/20261113_082500-agent-c-whole-game-entry-fixed-tick.md |
| `docs/changelog/TEMPLATE.md` |  |  |  | git checkout -- docs/changelog/TEMPLATE.md |
| `docs/operations/task-completion/task-completion.md` |  |  |  | git checkout -- docs/operations/task-completion/task-completion.md |
| `docs/specs/debug-logging/canonical-jsonl.md` |  |  |  | git checkout -- docs/specs/debug-logging/canonical-jsonl.md |
| `docs/specs/dev-loop/agent-lanes-build-authority.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-lanes-build-authority.md |
| `docs/specs/dev-loop/quality-gates.md` |  |  |  | git checkout -- docs/specs/dev-loop/quality-gates.md |
| `docs/specs/networking/networking.md` |  |  |  | git checkout -- docs/specs/networking/networking.md |
| `src/game/server-command-dispatch-module.cpp` |  |  |  | git checkout -- src/game/server-command-dispatch-module.cpp |
| `src/game/server-runtime-module.cpp` |  |  |  | git checkout -- src/game/server-runtime-module.cpp |
| `src/network/packets.h` |  |  |  | git checkout -- src/network/packets.h |
| `src/network/server-ice.cpp` |  |  |  | git checkout -- src/network/server-ice.cpp |
| `src/network/server-packets.cpp` |  |  |  | git checkout -- src/network/server-packets.cpp |
| `src/network/server.cpp` |  |  |  | git checkout -- src/network/server.cpp |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `src/game/host-compat-bridge-module.cpp` |  |  |  | git checkout -- src/game/host-compat-bridge-module.cpp |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/dev_loop/game_build.py' |
| `devscripts/live.py` |  |  |  | git checkout -- devscripts/live.py |
| `devscripts/changelog.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/changelog.py' |
| `devscripts/hot_reload_status.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/hot_reload_status.py' |
| `docs/changelog/2026-11-14/20261114_171448-networking-single-authority-packet-schema-split.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-14/20261114_171448-networking-single-authority-packet-schema-split.md' |
| `"docs/handoffs/2026 11 14 1706 .md"` |  |  |  | Remove-Item -LiteralPath '"docs/handoffs/2026 11 14 1706 .md"' |
| `docs/handoffs/hot-reload-agent-focus.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/hot-reload-agent-focus.md' |
| `docs/handoffs/hot-reload-cutover-checklist-20261114.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/hot-reload-cutover-checklist-20261114.md' |
| `docs/handoffs/hot-reload-agent-focus.md` |  |  |  | git checkout -- docs/handoffs/hot-reload-agent-focus.md |
| `docs/skills/spec-writing-v1.md` |  |  |  | Remove-Item -LiteralPath 'docs/skills/spec-writing-v1.md' |
| `docs/specs/hotreload/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/hotreload/' |
| `src/network/schema/` |  |  |  | Remove-Item -LiteralPath 'src/network/schema/' |

# Contract / ABI changes

- Contract: none.
- Version bump: none.
- Compatibility: no ABI or host source changed.
- Reverse: not applicable.

# State / schema changes

- Domain: status/spec document roles; no runtime state domain changed.
- Old schema: hand-maintained status and plan Markdown.
- New schema: generated `build/hot-reload/status.json` and `audit.json`.
- Migration: archive old docs; scripts generate JSON.
- Reverse: restore archived docs and remove the generators; not recommended.

# Deleted paths

- Path: none deleted from history; six hot-reload documents were moved to
  `docs/archive/hotreload/`.
- Before: competing current-looking status, hot/cold, hotness, state, and input
  documents.
- Why: one current specification; archive preserves history.
- Reverse: `git mv` each archived file back to `docs/specs/hotreload/`; this
  would restore the previous competing authority and is not recommended.

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results: `docs/skills/spec-writing-v1.md` applied;
  `docs/skills/documentation-checker-v1.md` used for routing/reference checks.
- Tests and exact commands:
  - `python -m py_compile devscripts/audit_triage.py devscripts/changelog.py devscripts/hot_reload_status.py`
  - `python devscripts/audit_triage.py --from-manifest --json build/hot-reload/audit.json`
  - `python devscripts/hot_reload_status.py`
  - `python devscripts/changelog.py ... --dry-run`
  - `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/cli.py devscripts/dev_loop/game_build.py`
  - `python devscripts/dev-loop.py --help`
  - `python devscripts/dev-loop.py --status`
  - `git diff --check`
- Build status: EXE build succeeded through the dev-loop
  (`2026-11-14T23:13:13.529312Z`). A hot-client default experiment was built,
  run in mode 57, stalled after frame 1, and was reverted; the resident path was
  rebuilt. The dev-loop `--request game-dll` then published generation 163 after
  resolving duplicate net-common symbols in `host-compat-bridge-module.cpp`;
  generation 164 then published.
- Runtime or hot-reload evidence:
  - `20261114_225952`: hot client loaded DLL, then stalled after frame 1.
  - `20261115_003100/client-hot3`: hot client with `MIMITA_GAME_READY` pointed
    at a missing file reached 1,500+ frames with `path=dll` and a 1920x1080
    viewport. This proves the hot frame works without a pending swap.
  - `20261115_005633/client-hot2`: the same hot client activated generation
    167 during startup and stalled, identifying the live-swap path as the
    remaining blocker.
  - `20261115_005633/client-hot6`: after the same-DLL guard, the hot client
    stayed responsive, rendered from the DLL at 1920x1080, and passed 1,700
    frames. Connection/room proof is still pending.
- Output files: `build/hot-reload/status.json`,
  `build/hot-reload/audit.json`. The status JSON now carries a
  `validation.validated=false` marker and the 2026-11-14 17:28 TODO note.

# Measured evidence

- Before values: one 200 KB history/spec/status mix in `hotreload.md`; no
  generated JSON status.
- After values: 11.8 KB current spec; generated status/audit JSON; one template
  and one changelog generator.
- Timestamps: session UTC timestamps in Task and generated JSON.
- Tick/frame/network measurements: not applicable.

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review: not applicable.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified: true different-DLL live swap, hot-client connection/join to
  the intended server, live networking coverage, gameplay live-fix acceptance.

# Agent handoff context

- Owner: agent-a
- Focus: spec, generated status, changelog automation
- Files to check: `docs/specs/hotreload/hotreload.md`,
  `docs/handoffs/hot-reload-agent-focus.md`, `devscripts/changelog.py`,
  `devscripts/hot_reload_status.py`, `devscripts/audit_triage.py`.
- What another agent should not redo: do not create another hot-reload spec,
  status Markdown, claims JSON, audit script, or build coordinator.
- Next exact step: restart/reuse the dev-loop, run
  `--request launch --launch-mode 57 --hot-client`, and verify frame count,
  `game.render.path path=dll`, and no stall. Then request a game-DLL build and
  prove same-PID activation. `12/78` is a DLL-dispatch-override count, not a
  client-handler migration count.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / da4133c1423f992ab5f61a35683229e5576c98ed / da4133c1423f992ab5f61a35683229e5576c98ed
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "building", "latest_build_stale": true, "latest_successful_build": "2026-11-14T22:16:46.128256Z", "latest_successful_build_id": "2026-11-14T22:16:46.128256Z:9a88fafa4344510f531f5f48cf233a96c26a9719e5c8ca15ed152b2bde081545", "latest_successful_source_generation": 22}, "game_current": {"build_mode": "debug", "code_hash": "93815fb4933d11966f781ffe1ef622163cc84d9e1c5fbf506ccdfac6e935e037", "dll": "build\\game\\generation-160\\game-g160.dll", "generation": 160, "revision": "c87323d32529a49f276a530af882c00194938a282d6526cc5f95aa2962d840bd", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-160\\game-g160.dll", "generation": 160, "published_utc": "2026-11-14T20:13:13.184Z", "revision": "c87323d32529a49f276a530af882c00194938a282d6526cc5f95aa2962d840bd", "status": "ready"}, "progress": {"elapsed_s": 244.0, "phase": "compiling", "updated_utc": "2026-11-14T22:22:22.891Z"}}
- Generation: 160
- Artifact: build\game\generation-160\game-g160.dll
- Journal path: C:\mimita-v9\logs\11-14-2026\20261114_171706\events.jsonl
- Session ID: agent-a-20261114-hotreload-spec

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M config/analytics.json
 M devscripts/audit_triage.py
 m devscripts/blender-ai
 M docs/ROUTER.md
R  docs/specs/hotreload/hot-cold-split.md -> docs/archive/hotreload/hot-cold-split-2026-11-14.md
R  docs/specs/hotreload/hotness-contract.md -> docs/archive/hotreload/hotness-contract-2026-11-14.md
RM docs/specs/hotreload/hotreload.md -> docs/archive/hotreload/hotreload-history-2026-11-14.md
R  docs/specs/hotreload/input-layer.md -> docs/archive/hotreload/input-layer-2026-11-14.md
R  docs/specs/hotreload/state-domains.md -> docs/archive/hotreload/state-domains-2026-11-14.md
R  docs/specs/hotreload/status.md -> docs/archive/hotreload/status-2026-11-14.md
 M docs/changelog/2026-11-13/20261113_082500-agent-c-whole-game-entry-fixed-tick.md
 M docs/changelog/TEMPLATE.md
 M docs/operations/task-completion/task-completion.md
 M docs/specs/debug-logging/canonical-jsonl.md
 M docs/specs/dev-loop/agent-lanes-build-authority.md
 M docs/specs/dev-loop/quality-gates.md
 M docs/specs/networking/networking.md
 M src/game/server-command-dispatch-module.cpp
 M src/game/server-runtime-module.cpp
 M src/network/packets.h
 M src/network/server-ice.cpp
 M src/network/server-packets.cpp
 M src/network/server.cpp
?? devscripts/changelog.py
?? devscripts/hot_reload_status.py
?? docs/changelog/2026-11-14/20261114_171448-networking-single-authority-packet-schema-split.md
?? "docs/handoffs/2026 11 14 1706 .md"
?? docs/handoffs/hot-reload-agent-focus.md
?? docs/skills/spec-writing-v1.md
?? docs/specs/hotreload/
?? src/network/schema/
```

## Captured diff stat

```text
config/analytics.json                              |   2 +-
 devscripts/audit_triage.py                         |  35 ++-
 devscripts/blender-ai                              |   0
 docs/ROUTER.md                                     |  14 +-
 .../hotreload/hotreload-history-2026-11-14.md      |   4 +
 ...3_082500-agent-c-whole-game-entry-fixed-tick.md |  19 ++
 docs/changelog/TEMPLATE.md                         |  60 ++++
 docs/operations/task-completion/task-completion.md |   5 +
 docs/specs/debug-logging/canonical-jsonl.md        |   2 +-
 docs/specs/dev-loop/agent-lanes-build-authority.md |  34 +--
 docs/specs/dev-loop/quality-gates.md               |   5 +-
 docs/specs/networking/networking.md                |  49 ++-
 src/game/server-command-dispatch-module.cpp        |  81 +----
 src/game/server-runtime-module.cpp                 |  20 ++
 src/network/packets.h                              | 331 +--------------------
 src/network/server-ice.cpp                         |  10 +-
 src/network/server-packets.cpp                     |   2 +
 src/network/server.cpp                             |  33 +-
 18 files changed, 232 insertions(+), 474 deletions(-)
```

## Captured changed paths

```text
M	config/analytics.json
M	devscripts/audit_triage.py
M	devscripts/blender-ai
M	docs/ROUTER.md
M	docs/archive/hotreload/hotreload-history-2026-11-14.md
M	docs/changelog/2026-11-13/20261113_082500-agent-c-whole-game-entry-fixed-tick.md
M	docs/changelog/TEMPLATE.md
M	docs/operations/task-completion/task-completion.md
M	docs/specs/debug-logging/canonical-jsonl.md
M	docs/specs/dev-loop/agent-lanes-build-authority.md
M	docs/specs/dev-loop/quality-gates.md
M	docs/specs/networking/networking.md
M	src/game/server-command-dispatch-module.cpp
M	src/game/server-runtime-module.cpp
M	src/network/packets.h
M	src/network/server-ice.cpp
M	src/network/server-packets.cpp
M	src/network/server.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "building",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-14T22:16:46.128256Z",
    "latest_successful_build_id": "2026-11-14T22:16:46.128256Z:9a88fafa4344510f531f5f48cf233a96c26a9719e5c8ca15ed152b2bde081545",
    "latest_successful_source_generation": 22
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "93815fb4933d11966f781ffe1ef622163cc84d9e1c5fbf506ccdfac6e935e037",
    "dll": "build\\game\\generation-160\\game-g160.dll",
    "generation": 160,
    "revision": "c87323d32529a49f276a530af882c00194938a282d6526cc5f95aa2962d840bd",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-160\\game-g160.dll",
    "generation": 160,
    "published_utc": "2026-11-14T20:13:13.184Z",
    "revision": "c87323d32529a49f276a530af882c00194938a282d6526cc5f95aa2962d840bd",
    "status": "ready"
  },
  "progress": {
    "elapsed_s": 244.0,
    "phase": "compiling",
    "updated_utc": "2026-11-14T22:22:22.891Z"
  }
}
```

# Follow-up: hot-client default, join, and live swap

- Date, time, timezone: 2026-11-15T03:00:00+00:00
- Owner: agent-a
- Status: live join and default proven; swap proven with a diagnostic follow-up

## Requested behavior

Make the normal hot client reach `Connected`, make hot frame ownership the
default, prove a different-DLL live swap, and continue toward the EXE cutover.

## Exact implementation changes

- `src/main.cpp`: removed the `MIMITA_GAME_HOT_CLIENT` opt-in gate. A normal
  client now sets `MIMITA_GAME_DLL`, `MIMITA_GAME_PROBE`, and
  `MIMITA_GAME_OWN_FRAME=1` whenever a built generation exists.
  `MIMITA_GAME_DISABLE=1` remains the resident escape hatch.
- `src/engine/whole-game-entry.cpp`: `shutdownWholeGameEntry()` now calls
  `perfStopResourceWatchdog()` before the generation is unloaded, so the old
  module cannot leave a watchdog thread reading unloaded state.
- `devscripts/dev-loop.py`: added the `--hot-client` client environment path and
  made `start-client` prefer the run's own published artifact, preventing a
  latest-build client from being rejected by an older server.
- `docs/handoffs/hot-reload-agent-focus.md`: recorded the join, default, and
  swap evidence and the remaining cutover work.

## Runtime evidence

- Normal client, no hot-client flag:
  `logs/2026-11-15/20261115_020351/client-01/events.jsonl`
  - `hot_client=false`
  - `network.connection-state.changed` to `Connected`, local player 1
  - `game.render.path path=dll`
  - more than 7,300 rendered frames
- Correct-build hot client against the current server:
  `logs/2026-11-15/20261115_005633/client-hot8/events.jsonl`
  - `Connected`, local player 9
  - `game.render.path path=dll`
  - more than 3,500 rendered frames
- Different-DLL swap:
  same `client-01` journal, sequence 132388
  - `game.generation.activated generation=168`
  - `dll=build\game\generation-168\game-g168.dll`
  - connection remained active and rendering continued.
- Build evidence:
  - EXE `2026-11-15T02:02:51.752618Z` includes the hot-client default change.
  - Game generation 168 was published at `2026-11-15T02:11:41.584665Z`.
  - Game generation 169 was published at `2026-11-15T02:49:12.720Z` and
    includes the watchdog-stop change.

## Remaining work

- Verify the watchdog-stop change on a clean client loaded from generation 169,
  then swap to a later generation.
- Refuse silent legacy fallback when a built generation is rejected; the new
  probe currently logs `game.host.probe.failed` and continues resident.
- Remove the resident normal-client/server fallback.
- Add the automatic hot-client acceptance gate.
- Perform the EXE cutover/link gate.
- Complete server pump/session split, state-domain wiring, safe unload, and
  CAP_RENDER cleanup.

## How to reverse

- Revert the `configureWholeGameHost` default in `src/main.cpp`.
- Remove the `perfStopResourceWatchdog()` call from
  `src/engine/whole-game-entry.cpp`.
- Revert the `start-client` artifact preference and `--hot-client` environment
  handling in `devscripts/dev-loop.py`.

# Follow-up: loud rejection, acceptance gate, cutover report

## Requested behavior

Reject incompatible hot generations instead of silently rendering the legacy
path, add an automatic hot-client acceptance gate, and add an EXE cutover/link
gate.

## Exact implementation changes

- `src/main.cpp`: sets `MIMITA_GAME_REQUIRE_HOT=1` when a built generation is
  selected. If the generation cannot load, the client writes
  `game.client.hot-required` and exits with status 1 instead of continuing.
- `devscripts/hot_client_gate.py`: validates a client journal for loaded
  generation, `path=dll`, `Connected`, frame progression, optional live
  activation, and no watchdog stalls after the post-activation bootstrap.
- `devscripts/dev-loop.py`: runs the hot-client gate after mode start, polls
  until enough frames exist, writes `hot-client-gate.json`, and fails launch
  readiness when the gate fails. It also records the EXE cutover report after
  successful builds.
- `devscripts/exe_cutover_gate.py`: compares `build/source-manifest.json`
  game-owned sources with the actual EXE source list; writes
  `build/hot-reload/exe-cutover.json`.
- `devscripts/hot_reload_status.py`: reports hot-frame default/runtime proof and
  state-domain wiring from generated/source facts.

## Evidence

- Loud rejection: `logs/2026-11-15/20261115_034509/client-failhot/events.jsonl`
  - `game.host.inactive`
  - `game.client.hot-required`
  - process exit code 1.
- Gate pass: `python devscripts/hot_client_gate.py
  logs/2026-11-15/20261115_034509/client-hotlive/events.jsonl`
  - generation 173, `path=dll`, `Connected`, 3,900+ frames, no
    post-bootstrap stalls.
- Gate fail: generation-168 journal fails `no_post_bootstrap_stalls`, proving
  the gate rejects the old watchdog defect.
- EXE cutover report: 568 game-owned sources remain linked into the 610-source
  EXE. The cutover is not complete; the gate correctly fails.

## Remaining work

- Finish the EXE source cutover; the resident client fallback and 568 linked
  game sources still exist.
- Verify the dev-loop hot gate on a fresh launch after the active build.
- Remove the server resident `MIMITA_GAME_DISABLE` fallback blocks.

# Follow-up: acceptance gate, repeated swaps, cutover blockers

## Verified

- Automatic hot gate now runs in the live dev loop. Run
  `20261115_132451` logged `hot_client_gate result=ready` and launch
  `complete result=succeeded`.
- Repeated safe swaps: the same client loaded generations 175, 176, and 177
  while staying connected and rendering. `hot_client_gate.py --require-swap
  --min-activations 3` passes at 10,000+ frames with no post-bootstrap stalls.
  Evidence: `logs/2026-11-15/20261115_132451/client-01/events.jsonl`.
- State domains: `state_domains_wired=true` from the generated ledger and host
  adapter coverage.
- Server fallback: `server_fallback_removed=true`; server startup refuses
  without the hot server generation and there is no `if (!hotPlayers)` /
  `if (!hotWorld)` resident simulation branch.
- CAP_RENDER blocker: `devscripts/cap_render_gate.py` reports 42 game-owned
  sources that still include raw GLAD/GL.

## Still blocked

- EXE cutover: `exe_cutover_gate.py` still reports 568 game-owned sources linked
  into the 610-source EXE. The actual source cutover is not complete.
- CAP_RENDER cleanup: 42 raw-GL game sources remain; the new gate reports them.
- Full safe-unload leak proof: three swaps pass, but long-run memory/handle
  leak measurement is not yet recorded.

# Follow-up: automatic game-source hot builds

## Implemented

- `devscripts/dev-loop.py` now classifies changed watched files as `game` or
  `cold` before requesting a build.
- A game-owned `.cpp` or game header automatically queues a whole-game DLL
  build; it does not set the EXE build pending flag.
- Cold, ABI, external, resource, and bootstrap changes still queue the cold EXE
  build.
- Mixed changes queue the game build first and serialize the cold build.
- The dev-loop journals `hot.build.plan` with generation, targets, changed
  paths, and whether the game/cold target was automatic.
- `build.py` and `game_build_worker.py` remain the private low-level engines;
  the dev-loop is the orchestration entry point.

## Evidence

- Touching `src/perf/perf-frame.cpp` produced:
  `hot.build.plan targets=["game"] game_auto=true cold_auto=false`.
- Generation 195 was published automatically without a new EXE build.
- The running client activated generation 195 in the same PID (`26648`), with
  `path=dll` and continued rendering.
- The existing EXE identity remained unchanged.

## Next

- Add the cold-core allowlist and make the cold target reject game sources.
- Fold the module catalog into the single game DLL.
- Add terminal/tips live views over `hot.build.*` events.

## Build target gate

- Added `devscripts/build_target_gate.py`.
- It reports `exe_sources`, `cold_core_sources`, `test_sources`,
  `game_sources_in_exe`, and legacy bridge files.
- Current result remains intentionally failing: 568 game-owned EXE inputs.
- `--require-cold` makes the gate fail; report-only remains the default while
  the cutover is in progress.
- This is the enforcement point for the future cold-core allowlist.

## Cold-link trial harness

- Added `devscripts/exe_cold_link_trial.py`.
- Added a dev-loop-only `--request cold-link-trial` action.
- The harness reuses the last normal `build/link.rsp`, removes game-owned object
  files, links a separate `build/out/mimita-cold-trial.exe`, and writes
  `build/hot-reload/cold-link-trial.json`.
- Current run removed 591 game-owned objects and attempted the trial link.
- The compiler driver currently returns `collect2.exe: error: ld returned 53
  exit status` with no linker diagnostics. The harness therefore records no
  undefined symbols yet. This is a tooling blocker, not evidence that the cold
  link is clean.
- The normal EXE is untouched; no canonical build behavior changed.

## Next

- Diagnose the trial linker invocation (likely response-file/driver handling),
  then use the undefined-symbol output to remove the first game cluster.
- Test/selftest source removal remains separate from the game-source count.

# Follow-up: cold-link symbol parsing and first resident→game slice

## Cold-link report

- Fixed the undefined-symbol parser in `devscripts/exe_cold_link_trial.py`: it
  now captures the full linker symbol instead of truncating at the escaped
  newline, and records the referring object, source, and source owner.
- The report now includes `undefined_by_referrer_owner`,
  `undefined_by_referrer_combination`, `unknown_referrers`, and
  `undefined_referrers`.
- Baseline trial: 411 unique undefined symbols (1,282 refs), 591 removed
  game-owned objects, referrers 216 resident / 156 bootstrap / 74 test;
  185 were resident-only.
- The first resident boundary defect chosen was `src/hot-reload/module-host.cpp`
  directly owning 36 `MapEditor` calls. The host may not link or call game
  implementation once the game source leaves the EXE.

## MapEditor host bridge

- Added `mimita::host::kEventMapEditorHost` and bounded
  `MapEditorHostCallV1` / `MapEditorHostOp` to
  `src/host/abi/game-host-abi.h`.
- Added `HostRuntime::invokeMapEditorHost()` to deliver one editor callback to
  the active generation without re-entering the host barrier (the callback can
  be reached from an already-entered game tick).
- Converted the resident `EditorServicesV1` callbacks in
  `src/hot-reload/module-host.cpp` to forward bounded requests instead of
  calling `MapEditor` directly.
- Added game-owned `src/game/map-editor-host-bridge.cpp` implementing those
  operations against `MapEditor`, and dispatched them from
  `src/game/game-module.cpp`.
- Fixed the game-side module startup bridge in
  `src/game/module-host-bridge-module.cpp`: it now forwards
  `ModuleOp::Startup` to the resident host instead of doing nothing. Without
  this, hot-client runs never started the module catalog, so `map-editor` and
  the other modules stayed unloaded.

## Evidence

- Build: EXE artifact `2026-11-15T20:00:25.068387Z`
  (`376a5d5a...`) published current; game generation 219 published.
- Hot client PID `24668` in run `20261115_200646` loaded
  `generation-219`, `path=dll`.
- Module startup after the forward-the-startup fix: `module.host.started`,
  8 modules; `map-editor` activated at generation 41, boundary
  `main_thread_editor_boundary`.
- MapEditor bridge runtime proof:
  `map-editor.command command=set_position handled_by_module=true`, followed by
  `map-editor.module.position` `handled_by_module=true` with object
  `ci_bridge_probe3`, before `[0,0,1.75]`, after `[239,-89,408]`.
- Journal: `logs/2026-11-15/20261115_200646/client-01/events-0039.jsonl`
  (rotated 100 MB segment; the active `events.jsonl` has since rotated past
  these startup/module records).
- Cold trial after the bridge: 375 unique undefined symbols (down 36), 592
  removed game objects, referrers 180 resident / 156 bootstrap / 74 test;
  resident-only 149 (down 36). No `MapEditor` symbols remain in the undefined
  list. Report: `build/hot-reload/cold-link-trial.json`.
- Cleanup: loaded the client map from disk after the probe chunks; no map save
  was issued.

## Rejected experiment

- Re-owning `src/debug/debug-log.cpp` and `src/utils/path_utils.cpp` as
  resident links them into the EXE but not into `game.dll`. The game build
  failed with undefined `Debug::*` and `resolveAssetPath` references. Reverted;
  generation 210 restored. Shared services need an EXE export/import or a host
  capability before they can move resident.

## Caveats and next slice

- The hot-client gate on the rotated journal fails `connected`: this run
  disconnected at `20:08:26` (`ReconnectFailed`), before the module startup and
  MapEditor probe. The disconnect is not caused by the bridge, but the run is
  not a full connected acceptance pass.
- Next resident-only cluster: 23 `EffectPartSystem` calls in
  `src/host/host-runtime.cpp`, then the `MimitaNet::*` server policy calls in
  `src/network/server.cpp`.
- Test/bootstrap-only undefined symbols remain prerequisites for a truly cold
  link, not reductions in `game_in_exe`.

# Follow-up: effects host boundary

## Implemented

- Added `kEventEffectsHost`, `kEffectsHostVersion`, and bounded
  `EffectsHostCallV1` to `src/host/abi/game-host-abi.h`.
- Added `HostRuntime::invokeEffectsHost()`; it delivers `EffectsOp`, input, and
  output pointers to the active game generation without re-entering the host
  barrier.
- Resident `effectsInvoke()` in `src/host/host-runtime.cpp` now forwards; the
  old direct `EffectPartSystem` body is compiled out pending deletion.
- `src/game/effects-bridge-module.cpp` now provides the game-side
  `handleEffectsHostCall()` implementation against the real `EffectPartSystem`
  and no longer forwards `EffectPartSystem::spawn*` back to the host.
- `src/effects/effect-part.cpp` compiles the real effect implementation for
  both the EXE and the whole-game DLL; the old `GameAPI` shim remains
  `MIMITA_GAME_DLL`-only.
- `src/game/game-module.cpp` dispatches `kEventEffectsHost`.

## Evidence

- Cold-link trial: 375 → 352 unique undefined symbols; resident referrers
  180 → 157; resident-only 149 → 128. No `EffectPartSystem` symbols remain in
  `undefined_symbols`. 592 game objects removed.
- Game generation 229 is published and activated in client PID `24668`
  (`game.generation.activated`, `generation-229`). The client stayed alive
  after activation.
- `hitfx_test 25` completed with `terminal.command.completed success=true` and
  no crash; the game generation now owns the effect pool.

## Link-failure diagnosis

- The silent link was not caused by the effects slice. Reverting
  `host-runtime.cpp` and `effect-part.cpp` to HEAD reproduced the same failure;
  restoring them changed nothing.
- Disk pressure was real: the checkout had ~1.36 GB free. Removing old
  `build/game/generation-*` artifacts ≤ 219 and the stale
  `build/mimita-check.exe` raised free space to ~8.57 GB.
- The actual EXE linker failure is an unrelated concurrent input/window bridge
  change:
  - `input_mouse-lock.o` references undefined
    `mimita::game::setHostCursorVisible(bool)` from `src/input/mouse-lock.cpp`.
  - `src/input/mouse-lock.cpp` also uses undeclared
    `mimita::game::hostCursorMode()`.
  - `src/input/input-poll.cpp` uses undeclared `InputRouter`.
  - `src/audio/music-manager-widget.cpp` uses undeclared `win` in a lambda.
- `src/game/window-bridge-module.cpp`, `src/game/window-bridge-module.h`, and
  `src/host/abi/window-capability.h` are new untracked files from the concurrent
  input/window work. That work must finish before the EXE can link.
- The previously published EXE artifact (`2026-11-15T20:00:25.068387Z`) remains
  current; the dev-loop is running that EXE plus generation 229. Do not claim a
  clean cold build or cutover yet.

## Cleanup

- Deleted the compiled-out old `effectsInvoke` body (`#if 0` block) from
  `src/host/host-runtime.cpp`; only the forwarding call remains.

## Build unblock and permanent linker diagnostics

- Added `src/host/window-host-fallback.cpp`: EXE-side implementations of the
  window/input capability using the resident `InputRouter` window. This covers
  the window functions the game DLL now calls, plus `setHostCursorVisible`.
- Fixed the incomplete input/window bridge compile errors:
  `mouse-lock.cpp` now includes the window bridge header; `input-poll.cpp`
  includes `input-router.h`; `music-manager-widget.cpp` uses `nullptr` instead
  of the removed `win` local.
- `build.py` now captures linker stdout/stderr, appends it to
  `.dev/link-output.log`, and prints it on failure. This permanently removes the
  silent-link failure mode.
- Normal EXE build succeeded and published current:
  `2026-11-15T22:35:50.214260Z:dbed60fa...`.
- Fresh launch `20261115_223719`: client PID `30780`, game generation 233,
  `path=dll`, `Connected`; `hot_client_gate.py --latest` passes all checks
  (6,056+ frames, no post-bootstrap stalls).

## Follow-up: build stable and map-editor folded

- The normal EXE build now succeeds and publishes current:
  `2026-11-15T23:25:37.026076Z:a077efd7...`.
- Permanent linker diagnostics are active in `build.py`; the failure output is
  appended to `.dev/link-output.log`.
- `config/modules/modules.json` no longer declares `map-editor` as a separate
  module. `src/mapeditor/map-editor-module.cpp` is now a game-owned source in
  the whole-game DLL.
- `devscripts/game_build_worker.py` writes a game-build
  `module-identity.h` (`MIMITA_MODULE_GENERATION`,
  `MIMITA_MODULE_SOURCE_HASH`, `MIMITA_MODULE_STATE_SCHEMA`) and adds it only
  to the map-editor module compile include path.
- The game-side `ModuleHost` bridge initializes the folded map-editor module and
  dispatches its editor events locally. The editor host services still come
  from the resident module host capability through the new
  `HostCapabilityId::EditorServices`, whose callbacks forward to the game-owned
  MapEditor bridge.
- Evidence: fresh launch `20261115_232617`, client PID `1212`, generation 241;
  `module.host.started` lists 6 modules and no separate `map-editor`
  activation. `mapeditor add chunk mapfold_probe` created the chunk;
  `mapeditor select` and `set_position` both logged
  `handled_by_module=true` with `generation=241`. The map was reloaded after the
  probe and the probe chunks were removed.
- `hot_client_gate.py --latest` passes: `path=dll`, `Connected`, 9,154+ frames,
  no post-bootstrap stalls.

## Remaining

- `MimitaNet::*` server-policy split (55 undefined symbols, mostly in
  `src/network/server.cpp`) is not started. It needs an explicit cold
  socket/session pump and policy calls through `GameServerV1`.
- Test/self-test separation into a separate tool is not started; its 74
  test-referenced symbols and 156 bootstrap-referenced symbols still block a
  truly cold link.
- The map-editor fold removes the separate module activation, but the resident
  EXE fallback no longer has a map-editor module. That is intentional for the
  one-DLL end state and should be documented in the cold-entry record.
