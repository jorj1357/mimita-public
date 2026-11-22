// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Fix isolated test run process identity

# Task

- Task ID: session-20261122-011809
- Session ID: session-20261122-011809
- Owner: Codex network-mode-launch
- Focus: dev-loop test-run ownership and Fun Counter Strike launch evidence
- Summary: Normalize process creation-time comparison to millisecond manifest precision; verify pinned Fun Counter Strike join and safe owned-run stop; record NPC tick-budget lead.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-22T01:18:11+00:00
- Local display time: 2026-11-21 20:18 EST
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 935c0e7ed4a702055b553aea19ea6c51b6c31a94
- Final commit: 935c0e7ed4a702055b553aea19ea6c51b6c31a94

# Pre-existing changes

- The checkout already had numerous modified and untracked files from other work. The captured full status and diff snapshot later in this file is repository-wide; those changes are not part of this fix and were preserved.
- This session's source edit is limited to `devscripts/dev_loop/parallel_test_runs.py`. The existing consistency issue `issue-cf9cd77eb056` was appended through agent-coord with this session's evidence. The user's exact request is recorded once as context `ctx-8d499a00d7b3` and linked to that issue and related metadata issue `issue-c481ab36713a`.

# Requested behavior

> can u open mimita exe and  start random modes aand track what is mkaing it not launch&#x20;
>
> like, i open miitma exe thru dev loop in some mode, the mode is&#x20;
>
> dev loop side note  the mode that you launch it in, thats  stuck adn ai thinks the mode is still that mode even when the exe u opened in taht mode, u switch to a new  gamemode in the exe, its stale doenst refelct reality
> i try to launch fun counterstrike and it doesnt work&#x20;
>
> disconnected, room not found, stuck on creating room,&#x20;
> in game ill just have a slow down then stop and reconnecting disonnectd issue then i reconnect to the same room and its fine
>
> networkign overall i think needs consistency work
>
> can u open the game or loko at events and mkae fixes like cahnge the code adn test it again

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-proof-and-verification.md`, `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/dashboard.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/networking/networking.md`, `docs/specs/gamemodes/funcounterstrike.md`, `docs/skills/dev-loop-restart-v1.md`, `docs/operations/task-completion/task-completion.md`.
- Exact requirements: preserve the user's report; use root dev-loop and pinned run journals; keep process identity checks safe; do not claim launch/build success as gameplay proof; leave unrelated healthy processes alone; produce one final changelog.
- Why the change follows the specification: isolated-run ownership previously compared timestamps with mismatched precision and could reject a process created by the same run. Comparing at the manifest's recorded millisecond precision retains PID, executable, and role checks while allowing the run controller to safely identify its own processes.
- Conflicts or decisions: the current EXE artifact was stale relative to checkout source. The isolated FCS run explicitly pinned that artifact and current DLL generation 437. It validates launch/join and this Python lifecycle fix, not current EXE source or the user's reported reconnect symptom.

# Exact implementation changes

## File: `devscripts/dev_loop/parallel_test_runs.py`

- Lines/functions/headings: `_same_process_start_time` at line 44; `stop_parallel_run` identity guard at line 868.
- Old content or behavior: exact string inequality compared the manifest timestamp (written at millisecond precision) with `windows_process_identity`'s microsecond timestamp. The two strings differed even for the same Windows process.
- New content or behavior: parse both ISO timestamps, convert to UTC, truncate the fractional component to milliseconds, and compare. Invalid values fail closed. The existing PID liveness, executable path, required manifest timestamp, and role/command-line checks remain in place.
- Reason: a precision-format mismatch caused test-run control to refuse its own server/client and leave ownership metadata stale.
- Why unrelated behavior is preserved: the comparison changes only timestamp normalization; run construction, executable verification, process role verification, and stop scope are unchanged.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `devscripts/dev_loop/parallel_test_runs.py` | Added millisecond-normalized UTC creation-time comparison used by isolated-run stop validation. | Revert only this hunk after review; do not reset the whole file because other work may be present. |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` | Appended timestamp-identity finding and FCS tick-budget evidence through the coordination issue workflow. | Keep append-only evidence; supersede with a later entry if disproved. |
| `docs/changelog/2026-11-22/20261122_011811-fix-isolated-test-run-process-identity.md` | Final record for this session. | Retain as session history. |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results: `docs/skills/dev-loop-restart-v1.md` reviewed; no daemon restart was needed. The healthy primary FCS session was preserved. Runtime workflow and multi-instance requirements were reviewed.
- Tests and exact commands: targeted precision check covered equal timestamps truncated to the same millisecond, timestamps one millisecond apart, and malformed input; all 3 passed. Isolated FCS run `20261122_011502` (mode 45) and TDM run `20261122_012331` (mode 41) both started and reached Connected on `dust2cyberiav4`, each pinned to the same old EXE and DLL generation 437. `--stop-test-run` stopped each run's two PIDs with no failures. Proof ledger `task-proof-197a6b56fafc` verifies the exact FCS `dev-loop.parallel-run.stopped` event.
- Build status: no game build requested. Root dev-loop reported `published_stale`; latest EXE artifact was `2026-11-21T02:26:50.306117Z`, SHA-256 `8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871`. DLL generation 437 was pinned with SHA-256 `bfa93e8fea787fba38248b2e4f0dfddb3aef11c6c69844666c8496c804609c11`.
- Runtime or hot-reload evidence: FCS run `20261122_011502`, server PID 716/client PID 30948. Client reached Connected at `2026-11-22T01:15:11.810Z`, recorded 36 snapshots and no reconnect events. TDM run `20261122_012331`, server PID 31660/client PID 26620, reached Connected at `2026-11-22T01:23:44.579Z`; its client journal recorded `network.client.join.accepted` twice. Both runs stopped cleanly. The duplicate TDM welcome is another lead, not a demonstrated disconnect cause. Exact client/server/lifecycle journals are under `logs/2026-11-22/20261122_011502/` and `logs/2026-11-22/20261122_012331/`.
- The primary run `20261122_011031` remained on the FCS server (PID 26748, `dust2cyberiav4`). My add-client request at `2026-11-22T01:23:04.866Z` was rejected because the published EXE was stale. A later dashboard request was accepted at `01:23:55.849Z` and created PID 30496; I did not initiate or stop that process. Current instance listing shows that client online, but the journal has no `network.connection-state.changed` record for PID 30496, so I do not claim it joined.
- Output files: isolated run manifests `logs/2026-11-22/20261122_011502/run.json` and `logs/2026-11-22/20261122_012331/run.json`; proof `task-proof-197a6b56fafc`.

# Measured evidence

- Before values: manifest timestamps were millisecond precision while the process inventory returned microseconds; direct string comparison rejected matching process identities.
- After values: targeted comparison passed all 3 precision/invalid-input cases. A real FCS isolated run started and joined; its stop controller recognized and stopped both manifest-owned processes, `failures=[]`, and wrote terminal status `stopped`.
- Timestamps: run created `2026-11-22T01:15:02.750Z`; client became Connected `2026-11-22T01:15:11.810Z`; run stopped `2026-11-22T01:15:50.207Z`.
- Tick/frame/network measurements: the FCS client received 36 snapshots and had no reconnect transition; TDM connected but showed duplicate join acceptance. FCS server emitted 193 `performance.server-phase-budget` records, all with `npc_over=true` against a 10 ms budget; maximum observed `npc_ms=347.801`. This is a measured slowdown lead, not proof of cause for the user's disconnect report.

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression: the process timestamp mismatch is a confirmed dev-loop control defect from prior run evidence. It is not a confirmed gameplay/network regression from this session.
- Related regression paths: networking consistency issue `issue-cf9cd77eb056`; launch-vs-runtime metadata issue `issue-c481ab36713a`. No new regression file was added.

# Human acceptance

- Visual review: not performed.
- Gameplay review: isolated FCS join path reached Connected and snapshots; human visual acceptance was not performed.
- Multiplayer review: one client joined the isolated local server; remote/multiplayer behavior and reconnect were not exercised.
- Still unverified: the primary report's room-not-found/creating-room symptoms and in-game disconnect/reconnect were not reproduced. The pinned EXE is stale relative to source. The measured NPC budget spikes may explain slowdown but have not been causally linked to disconnects.

# Agent handoff context

- Owner: Codex network-mode-launch
- Focus: dev-loop test-run ownership and Fun Counter Strike launch evidence
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 935c0e7ed4a702055b553aea19ea6c51b6c31a94 / 935c0e7ed4a702055b553aea19ea6c51b6c31a94
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-21T02:26:50.306117Z", "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "bfa93e8fea787fba38248b2e4f0dfddb3aef11c6c69844666c8496c804609c11", "dll": "build\\game\\generation-437\\game-g437.dll", "generation": 437, "revision": "a3ef72a08c291c3491e0e193eddbb21eb09e79f51e8648f22ac3da575c12fed5", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-437\\game-g437.dll", "generation": 437, "published_utc": "2026-11-21T23:57:39.423Z", "revision": "a3ef72a08c291c3491e0e193eddbb21eb09e79f51e8648f22ac3da575c12fed5", "status": "ready"}, "progress": {"compiled": 17, "elapsed_s": 2727.517, "phase": "build_complete", "result": "success", "skipped": 70, "updated_utc": "2026-11-21T02:26:10.895Z"}}
- Generation: 437
- Artifact: build\game\generation-437\game-g437.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_011031\client-01\events.jsonl
- Session ID: session-20261122-011809

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
M config/accounts/default.json
 M config/analytics.json
 M config/movement/movement-source.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/packet_registry_codegen.py
 M devscripts/source_manifest.py
 M docs/changelog/2026-11-21/20261121_234000-daemon-status-journal-cpu-reduction.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md
 M docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md
 M src/npc/npc.cpp
?? docs/changelog/2026-11-21/20261121_235111-bloodtest-full-mode-issue-and-implementation-plan.md
?? docs/changelog/2026-11-22/
?? docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-1759b2cc6e65.md
?? docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-f8b4b3a4716d.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-421e4a6513c2.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d220df93f7cb.md
?? docs/issues/capability/20261122-enable-bounded-authenticated-ai-website-fixes-an-issue-260667351e04.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   4 +-
 config/analytics.json                              |   2 +-
 config/movement/movement-source.json               |   6 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/game_build.py                  | 187 +++++++++++++++++++--
 devscripts/dev_loop/gui.py                         |  32 ++++
 devscripts/dev_loop/parallel_test_runs.py          |  20 ++-
 devscripts/packet_registry_codegen.py              |  59 ++++---
 devscripts/source_manifest.py                      | 159 +++++++++++++++---
 ...1_234000-daemon-status-journal-cpu-reduction.md |  32 ++++
 docs/issues/README.md                              |  23 ++-
 ...asses-through-and-is-not--issue-04079f42c6f9.md |  26 +++
 ...nch-opencode-api-key-expi-issue-36579a10979a.md |  42 +++++
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |  45 +++++
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |  42 +++++
 ...move-avoidable-build-and--issue-e044a931c54b.md |  12 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  16 ++
 ...nd-observe-context-bundle-issue-6907072bb155.md |  16 ++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  12 ++
 ...-and-build-to-proof-links-issue-b396205aaecf.md |  12 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  49 ++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |   6 +
 docs/regressions/gameplay/README.md                |   4 +-
 .../website-auth-database-REG-2026-09-16.md        |  72 ++++++++
 src/npc/npc.cpp                                    |   2 +-
 26 files changed, 798 insertions(+), 84 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/packet_registry_codegen.py
M	devscripts/source_manifest.py
M	docs/changelog/2026-11-21/20261121_234000-daemon-status-journal-cpu-reduction.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md
M	docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-21T02:26:50.306117Z",
    "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "bfa93e8fea787fba38248b2e4f0dfddb3aef11c6c69844666c8496c804609c11",
    "dll": "build\\game\\generation-437\\game-g437.dll",
    "generation": 437,
    "revision": "a3ef72a08c291c3491e0e193eddbb21eb09e79f51e8648f22ac3da575c12fed5",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-437\\game-g437.dll",
    "generation": 437,
    "published_utc": "2026-11-21T23:57:39.423Z",
    "revision": "a3ef72a08c291c3491e0e193eddbb21eb09e79f51e8648f22ac3da575c12fed5",
    "status": "ready"
  },
  "progress": {
    "compiled": 17,
    "elapsed_s": 2727.517,
    "phase": "build_complete",
    "result": "success",
    "skipped": 70,
    "updated_utc": "2026-11-21T02:26:10.895Z"
  }
}
```

