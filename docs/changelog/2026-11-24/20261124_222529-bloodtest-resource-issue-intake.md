# Bloodtest resource and issue triage

- Task ID: `bloodtest-quality-triage`
- Session ID: `session-84c90c6a5dcd`
- Owner: `bloodtest-quality-triage`
- Focus: Bloodtest frame-time, host resource attribution, issue intake
- Summary: Added five complete, claimable issue records and updated existing Bloodtest, CPU, decal, and agent-awareness issues with the user's latest requirements and runtime evidence. No source code or gameplay settings were changed.
- Status: `PASS_WITH_HUMAN_REVIEW`
- Date, time, timezone: 2026-11-24T22:25:29Z, UTC
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `d06fc46482d25e1a7de225774cb3273c4bfdcb52`
- Final commit: `d06fc46482d25e1a7de225774cb3273c4bfdcb52` (unchanged)

# Pre-existing changes

- The shared checkout was already dirty before this issue-intake work. The tracked diff included `config/accounts/default.json`, `config/analytics.json`, `config/camconfig.json`, `config/gamemodes/bloodtest.json`, `config/vehicles/octane_rl.json`, `devscripts/dev_loop/game_build.py`, `src/effects/effect-part-blood.cpp`, `src/main-systems.cpp`, `src/physics/physical-entity.cpp`, and other issue/changelog files. This session did not edit those source/config/build files.
- Files not created or modified by this session: the pre-existing source, config, build, and unrelated issue/changelog edits above; active FCS and Vehicle Arena runtime processes.

# Requested behavior

Investigate Bloodtest camera/frame choppiness toward the 240 FPS target; check memory, CPU, Swap, and config polling; capture the user's camera, lens, hit-feedback, limb/corpse, muzzle-origin, decal, shader, slope, fullscreen mode 6, unrestricted-config-value, and OpenCode/ChatGPT communication requirements as claimable work. The user additionally asked for automatic host and process resource context in an often-called Python coordination path.

# Specification alignment

- Current specification paths: `docs/workflows/issue-intake-and-triage.md`, `docs/skills/issue-triage-v1.md`, `docs/specs/performance/performance.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-runtime-registry.md`, `docs/specs/dev-loop/agent-context-contract.md`, `docs/specs/gamemodes/bodycam.md`, `docs/specs/gamemodes/bloodtest.md`.
- Exact requirements: preserve user quotes; make complete, claimable issue records; distinguish observations from inferences; behavior requires pinned runtime evidence; do not stop a process without verified ownership/use.
- Why the change follows the specification: existing issues were reused for bodycam, Bloodtest combat/collision, decals, CPU measurement, and agent awareness; new issues cover distinct frame diagnosis, config validation policy, opaque-black rendering, fullscreen 6 composition, and automated resource context.
- Conflicts or decisions: the user's request to remove every config limit is recorded as an audit/warning-first proposal; the issue retains point-of-use safeguards against overflow, unbounded work, OOM, and invalid simulation state. 240 FPS remains a target, not an observed result.

# Exact implementation changes

## Coordination brain and issue records

- Created `issue-16500baeb0cc` for Bloodtest client frame-time diagnosis; it depends on existing 240 FPS instrumentation issues `issue-549680e53b26` and `issue-13d080318e1c`.
- Created `issue-9e73745d3154` for warning-first, best-effort config values; `issue-3c1894ca50ec` for opaque-black shader visibility; `issue-e0a6bcbacad0` for full-frame/UI stretch in fullscreen mode 6; and `issue-b9edd402e658` for automatic host/process resource context in agent sync/status.
- Added one unassigned follow-up task to each new issue so the work is claimable. Linked the full user context to related existing issues `issue-e9f582f8d2d0` (bodycam), `issue-2e7b0cd6f868` (Bloodtest combat/collision/gore/slope), `issue-3eaa6c834b9a` (decals), `issue-91e3384b219b` (agent awareness), and `issue-ed3ac775283d` (CPU measurement).
- Appended the latest measured process/resource evidence to `issue-ed3ac775283d` and `issue-16500baeb0cc`. Appended the new camera/FOV, projectile/gore/slope, decal, and cross-agent communication requests to their existing issue records. `docs/issues/README.md` is regenerated from the shared issue brain.
- No source, config, shader, spec, build, or runtime files were edited by this session.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md` | absent | new complete issue with run evidence and task | track measured Bloodtest frame misses | use issue workflow to cancel/close only with a resolution; preserve quote/evidence |
| `docs/issues/capability/20261124-replace-arbitrary-configuration-limits-with-warn-issue-9e73745d3154.md` | absent | new complete issue with warning-first acceptance and safety risks | track the user's broad validation request | use issue workflow; do not delete append-only record |
| `docs/issues/bug/20261124-opaque-black-world-surfaces-must-not-become-pass-issue-3c1894ca50ec.md` | absent | new reproduction and alpha/material acceptance issue | distinguish opaque black RGB from authored transparency | use issue workflow; do not delete append-only record |
| `docs/issues/bug/20261124-preserve-fullscreen-mode-6-full-frame-stretch-th-issue-e0a6bcbacad0.md` | absent | new mode 6/UI stretch proof issue | document and verify full-frame composition after shader changes | use issue workflow; do not delete append-only record |
| `docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md` | absent | new complete issue and unassigned sync-snapshot task | expose why processes run and their measured host cost | use issue workflow; do not delete append-only record |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` | existing | appended direct process/resource sample | preserve observed CPU/RAM evidence | append a correction if later evidence supersedes it |
| `docs/issues/improvement/20261123-complete-the-bloodtest-bodycam-presentation-spec-issue-e9f582f8d2d0.md`, `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md`, `docs/issues/bug/20261121-surface-marks-stretch-to-contact-plane-and-remai-issue-3eaa6c834b9a.md`, `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` | existing | appended linked requirements/evidence | reuse existing owners and avoid duplicate work | append correction/evidence through issue workflow |
| `docs/issues/README.md` | generated index | regenerated with new issue records | keep index aligned with shared brain | regenerate from coordination brain |
| `docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md` | absent | this session record | required final changelog | generated session record; preserve for audit |

# Contract / ABI changes

- Contract: no runtime contract or ABI changed.
- Version bump: none.
- Compatibility: unchanged.
- Reverse: no source rollback is needed; issue records are append-only.

# State / schema changes

- Domain: shared coordination issue/context/task records only.
- Old schema: existing issues did not contain this exact request or this run/process sample.
- New schema: five complete issue entries, related-context links, and one claimable follow-up task per new issue.
- Migration: none.
- Reverse: none; issue history remains append-only.

# Deleted paths

- None.

# Diagnostics

- Owner/category: existing `performance.fps`, `performance.frame-budget`, `performance.resource-window`, process manifests, and `run.started` events.
- Input: client frame windows, OS process CPU-time deltas, working sets, host logical CPU count, available physical RAM, run manifests, and journals.
- Decision: create a Bloodtest-specific performance issue and a separate automatic-resource-context capability issue; link related requests to existing owners.
- Output: issue IDs above; evidence appended to CPU and Bloodtest performance issues.
- Failure or rejection reason: unavailable during the later process sample: Bloodtest process CPU, paging/hard-fault rate, GPU/display utilization, and source-function CPU attribution.
- Rate limiting: no runtime diagnostics were added.

# Validation

- Focused skill paths and results: `docs/skills/issue-triage-v1.md` and `docs/workflows/issue-intake-and-triage.md` reviewed; new issues include required fields and proof expectations.
- Tests and exact commands: no tests run; no code changed.
- Build status: no build requested. Observed root dev-loop after the sample reported generation 626 published, with the EXE artifact marked stale for current source.
- Runtime or hot-reload evidence: read-only inspection of run manifests and per-process journals; no runtime was launched or stopped.
- Output files: five new issue records; existing issues updated as listed above.

# Measured evidence

- Bloodtest run `20261124_212828`, client PID 25432, game generation 623, EXE build `2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63`. Journal `logs/2026-11-24/20261124_212828/client-01/events.jsonl` contains 477 one-second FPS windows. Recent windows were about 121–143 FPS; all frames exceeded 4.167 ms; p50 about 6.36–7.29 ms; sampled maxima 12.1–28.4 ms. Example `performance.frame-budget` samples included Networking, Replay, Camera, MusicConfigPoll, shadows, and one Swap sample. These samples do not identify one proven root cause.
- Host sample 2026-11-24T22:14:23.339Z–22:14:32.015Z, 8.676 s, 12 logical CPUs: four MiMITA processes consumed about 77.4% host CPU (PIDs 26492/27372 FCS client/server, 29052/37720 Vehicle Arena client/server); free physical RAM was 212,552 KiB of 16,409,100 KiB (~1.3%). Other contributors: dashboard PID 16864 ~5.07% host, Task Manager PID 30272 ~3.76%, daemon PID 19808 ~2.06%. The next dev-loop status reported generation 626 published; the exact in-sample build phase remains unknown.
- The active dev-loop run was Fun Counter Strike, not Bloodtest; the separate Vehicle Arena run was owned by agent `vehicle-ball-proof`. The preceding Bloodtest run had ended before the host process sample. Thus high host pressure is directly observed, but swap as the cause of Bloodtest choppiness is unknown.
- Before values: unavailable for a matched Bloodtest CPU/memory/paging sample.
- After values: not measured; this session created issues only.
- Timestamps: use exact UTC sample times above; see each issue for run IDs and journal paths.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: user-reported symptoms are captured, but no implementation change or controlled regression comparison occurred.
- Related regression paths: none created.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: root cause/fix for Bloodtest frame time, bodycam visual tuning, opaque-black rendering, fullscreen mode 6 composition, and all sound/effect/corpse/slope behavior.

# Agent handoff context

- Owner: new follow-up tasks are unassigned/claimable; existing issue owners remain unchanged.
- Focus: claim and reproduce one issue at a time; correlate run/process ownership, FPS scope evidence, RAM, paging, and build status before changing code.
- Files to check: `devscripts/agent_coord.py`, `devscripts/dev_loop/build_resource_sampler.py`, `docs/specs/dev-loop/agent-runtime-registry.md`, existing issue paths listed above.
- What another agent should not redo: do not repeat the initial 477-window Bloodtest journal scan or the 22:14 process/RAM snapshot without a new discriminating run; do not stop active FCS or Vehicle Arena processes without owner/runtime-registry confirmation.
- Next exact step: claim `bloodtest-frame-time-root-cause` or `auto-resource-context-on-sync`; run one pinned Bloodtest scenario while collecting synchronized per-PID CPU, working set/private memory, available RAM/commit/pagefile, journal FPS, and exact process/run identities.

# Automatic evidence

- Branch/base/final: `checkpoint/one-checkout-condensation-20261111` / `d06fc46482d25e1a7de225774cb3273c4bfdcb52` / unchanged.
- Git status: shared checkout remains dirty with unrelated pre-existing source/config/build/docs edits; this session added issue/changelog files only.
- Diff stat: mixed shared-worktree diff; do not attribute it wholesale to this session.
- Build identity: latest EXE build `2026-11-24T19:27:02.742428Z:7ba5fb9128425994e8d872c2a5449ec05d76410ce42d18294d8c98a9f94cc98` (runtime stale); game generation 626 published.
- Generation: 626 current published DLL; observed runs also pinned generations 623 and 626 as recorded in their manifests.
- Artifact: `build/game/generation-626/game-g626.dll`.
- Journal path: `logs/2026-11-24/20261124_212828/client-01/events.jsonl`; `logs/2026-11-24/20261124_221014/client-01/events.jsonl`; `logs/2026-11-24/20261124_221254_ball-proof-3/server-01/events.jsonl`.
- Session ID: `session-84c90c6a5dcd`.

# How to reverse

- No code/config was changed. Keep issue records append-only; if scope was routed incorrectly, use coordination context move/link with a reason rather than deleting history.
- Do not clean or stop any process based on this record alone; verify ownership and active use first.

# Related feature record

- Feature path: none.
