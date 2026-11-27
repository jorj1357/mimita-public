# Independent bounded review of issue-0cdac76fb006

- Task ID: wake-676a14cb924a
- Agent: issue-review-sweeper
- Focus: one read-only review of the small-desktop proof standard issue, its task/proof, the exemplar dashboard monitor-retention proof, gui.py region drift, the dashboard live/saved state and restarts, the keepalive journal, and build identity
- Summary: Appended one independent review (review id 555, verdict `inconclusive`) and one evidence-backed process addendum to `issue-0cdac76fb006`, plus one issue-linked message to owner `dashboard-position-proof`. No source edit, build request, test run, close, restart, proof exemption, owner change, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-27T23:06Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents (config files, `devscripts/blender-ai`, `docs/issues/README.md`, and many changelogs). This session edited no source, reverted nothing, and touched only the issue record through `agent-coord.py`, coordination state, and this changelog. No compiler, `build.py`, module worker, second dev-loop, or game runtime was invoked.

# Requested behavior

Run one bounded independent review of `issue-0cdac76fb006`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observations separated from inference; no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/dashboard.md`: the dashboard must persist its normal size/position and restore them on startup; keepalive must leave a user-placed window where it is; monitor coordinates may be negative.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build or a live process is not behavior proof; acceptance requires a pinned run journal with the required event.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` | 6 reviews (ids 46/191/288/372/435/491) + 2 suggestions | + review id 555 + one process addendum + one issue-discussion message | required independent review record | remove only the appended review/addendum/message text |
| `.dev/agent-coord/brain.db` | no review id 555 | review id 555, suggestion, issue-linked message | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-27/20261127_1906-issue-0cdac76fb006-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue_review` added review id 555 to `issue-0cdac76fb006` (verdict `inconclusive`; state stays `in_review`, owner unchanged).
- One `issue suggest` addendum appended to the existing continuous-dashboard-health proposal on `issue-0cdac76fb006`.
- One issue-linked `send` message posted to owner `dashboard-position-proof`.

# Diagnostics

- Readiness: `ready=false`, score 1/8, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk; the only linked task `small-desktop-state-evidence-standard` is still `open`.
- Proof `task-proof-bac2b095c7af` still `unproven`, `verified_utc` empty, evidence empty. Pinned doc hashes still stale and unchanged since 11-24: `issue-intake-and-triage.md` 56d48ea9... vs current f3d370b5...; `agent-proof-and-verification.md` 9d2eaefb... vs current eaef36e8....
- Proof `task-proof-ed45c1b9e923` still `stale` ("source changed: devscripts/dev_loop/gui.py"). Current gui.py sha256 994c577b... (11-26 sweep: 5ad5428f...), changed by commit 17c7a67d (2026-11-27T15:21Z). Direct byte extraction vs proven d3368058: `_window_bounds`, `_tk_geometry`, `persist_window_geometry` IDENTICAL; `close_view` DIFFERS. `dashboard-keepalive.py` still matches recorded d3cc3e94.... Latest build generation 805 changed only `src/npc/recast-navigation.cpp`.
- New live fact - dashboard now on the PRIMARY monitor: PID 24360 (`pythonw dev-loop.py --dashboard --no-terminal-relaunch`, created 2026-11-27 01:14:09 local / 06:14:09Z) has exactly one TkTopLevel window, hwnd 67380, visible, not minimized, rect [78,31,859,783] = 781x752 at (78,31); `.dev/dashboard-ui.json` is 781x752+78+31, saved==live. Differs from 11-26 saved/live secondary 775x810+-1769+129.
- New restart fact: `dev-loop.dashboard.attach` logged dashboard_pid=1828 at 2026-11-27T00:00:13.635Z (run 20261126_235953) and dashboard_pid=35600 at 2026-11-26T22:43:10.940Z (run 20261126_201711); other 11-26/27 attach PIDs include 1328, 34728, 35008, 36052. At least two replacements after the 11-26 observation.
- Keepalive still not continuous: `.dev/dashboard-keepalive/events.jsonl` ends at 2026-11-26T22:44:09.589Z (6,042 lines); zero 11-27 samples, no keepalive process, no startup-geometry record. This sweep did NOT run keepalive because `--once` can select/ShowWindow the ConsoleWindowClass console; the dashboard window was enumerated directly by class instead.
- Deliverable (the reusable small-desktop/persistent-desktop proof standard) is still absent from docs; issue state unchanged.

# Validation

- `agent-coord.py begin/sync/observe`, `issue brief`, `issue readiness`, `proof status`, `proof list --issue`, source/doc SHA-256 via Python, direct Win32 window enumeration with class names, dev-loop journal reads, and `git status`.
- `issue review` appended review id 555; `issue suggest` appended one addendum; `send --issue` posted one owner message.
- No compiler, build request, module worker, second dev-loop, or game runtime was invoked. No dashboard close/restart performed.

# Findings

1. The standard deliverable is still unimplemented; readiness remains 1/8 and the only task is open with an unproven, stale-pinned proof.
2. The dashboard moved from the 11-26 saved/live secondary position to the primary (x=78) and was replaced at least twice, but no keepalive sample or startup-geometry event brackets any transition, so the move cannot be classified as restore vs snap. Current saved==live and the known exemplar fix remains FIXED, so no snap is demonstrated.
3. gui.py changed again (17c7a67d) but the three geometry primitives remain byte-identical to the proven commit; only close_view still differs and is untested for save-before-exit.
4. The exemplar proof remains whole-file stale even though the geometry functions are unchanged; the pinned workflow doc hashes for the unproven task also remain stale.

# Confidence

High on readiness/proof-hash facts, the live/saved primary geometry, the attach PIDs/timestamps, and the keepalive/journal timestamps (direct reads). Moderate on the cause and date of the move to primary. Unknown: whether retention survives three close/reopen cycles from a known secondary position, and whether the changed close_view path affects save-before-exit.

# Proof

No proof claim. Read-only review; evidence is source-tier, runtime-state-tier, and journal-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; observations recorded on the issue.

# Human acceptance

- Visual, gameplay, multiplayer review: none (not applicable)
- Still unverified: three close/reopen cycles captured with class-filtered TkTopLevel rects, `.dev/dashboard-ui.json` before/after, one dashboard PID per cycle, and geometry written before exit.

# Agent handoff context

- Owner: `dashboard-position-proof` (messaged).
- Next exact step: fix/avoid the title-substring window match, move the dashboard to the secondary, run `python devscripts/dashboard-keepalive.py --interval 5` (no `--focus`) for 10 minutes, do 3 close/reopen cycles, and per cycle record class-filtered TkTopLevel hwnd+rect, `.dev/dashboard-ui.json`, one dashboard PID with the old PID exited, and the `dev-loop.dashboard.attach` line. User context ctx-2d62389bbf7f authorizes dashboard restart/run for measurement. Then re-pin `task-proof-bac2b095c7af` to f3d370b5.../eaef36e8... and re-verify `task-proof-ed45c1b9e923` by the named geometry+close_view region.
- What another agent should not redo: do not treat a title-matched keepalive sample as dashboard evidence, do not infer a snap from saved==live or from the move to primary, and do not claim retention from natural restarts alone.

# Automatic evidence

- Git status: shared dirty tree preserved; this session added one issue review, one issue suggestion, one owner message, coordination DB changes, and this changelog.
- Build identity: latest published game-DLL generation 805 (`src/npc/recast-navigation.cpp`); unrelated to this issue; no build requested.
- Journal paths checked: `.dev/dashboard-keepalive/events.jsonl`, `.dev/dashboard-ui.json`, `logs/2026-11-26/20261126_201711/dev-loop/events.jsonl`, `logs/2026-11-26/20261126_235953/dev-loop/events.jsonl`; live windows via Win32 on PID 24360.

# How to reverse

- Remove only the appended review (id 555), addendum, and message text from the `issue-0cdac76fb006` doc; do not rewrite prior text.
- Restore `.dev/agent-coord/brain.db` from backup if the message/review is withdrawn.
- Delete this changelog. No commit to revert.
