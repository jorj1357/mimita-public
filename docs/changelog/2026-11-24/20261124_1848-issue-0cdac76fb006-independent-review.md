# Independent bounded review of issue-0cdac76fb006

- Task ID: wake-63a8663e8d98
- Agent: issue-review-sweeper
- Focus: one read-only review of the small-desktop proof standard issue, its linked task/proofs, the exemplar dashboard monitor-retention evidence, current dashboard live/saved state, and the keepalive journal
- Summary: Appended one independent review (review id 372, verdict `inconclusive`) to `issue-0cdac76fb006` and one issue-linked message (msg-ab75e549a9c1) to owner `dashboard-position-proof`. No source edit, build request, test run, close, owner change, or fix claim. No new process proposal; existing proposals cover the observed gaps.
- Status: complete (review only)
- Date, time, timezone: 2026-11-24T18:48Z, ISO 8601

# Pre-existing changes

- The shared checkout retains dirty/untracked entries from other agents/sessions (config/*, src/vehicles/vehicle-system.cpp, several docs/issues, docs/changelog, assets/sound). This session modified only `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md`, the coordination DB, the issue discussion, and this changelog. No compiler, build.py, module worker, second dev-loop, or runtime test run was invoked.

# Requested behavior

Run one bounded independent review of `issue-0cdac76fb006`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observation separated from inference; no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/dashboard.md`: the dashboard must persist its normal size/position, restore them on startup, and the keepalive must leave a user-placed window where it is; monitor coordinates may be negative.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build or a process staying alive is not behavior proof; acceptance requires a pinned run journal showing the required event.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` | 3 reviews (ids 46/191/288) | + one review (id 372) | required independent review record | remove only the appended review |
| `.dev/agent-coord/brain.db` | no review id 372 | review id 372 + issue-linked message msg-ab75e549a9c1 | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-24/20261124_1848-issue-0cdac76fb006-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue_review` added review id 372 to `issue-0cdac76fb006` (verdict `inconclusive`; state stays `in_review`, owner unchanged).
- One issue-linked `message` msg-ab75e549a9c1 sent to owner `dashboard-position-proof` (requires-response, due 2026-11-24T20:48:51Z, appended to the issue discussion).

# Diagnostics

- Readiness: `ready=false`, score 1/8, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk; one linked task `small-desktop-state-evidence-standard` still `open`.
- Proof `task-proof-bac2b095c7af` still `unproven`, `verified_utc` empty, evidence empty. Pinned doc hashes stale: `agent-proof-and-verification.md` 9d2eaefb... vs current eaef36e8... (drifted further from 11-23's c05d407e... via commits 07ba9003/51306c08 adding gameplay-visual and website-auth text); `issue-intake-and-triage.md` 56d48ea9... vs current f3d370b5... (unchanged since 11-22).
- Proof `task-proof-ed45c1b9e923` still `stale` ("source changed: devscripts/dev_loop/gui.py"). Current gui.py sha256 267e69ea... unchanged since 11-22/11-23; `dashboard-keepalive.py` d3cc3e94... matches the recorded hash. Build generation 599 changed only `src/vehicles/vehicle-system.cpp`, so it did not trigger new gui.py staleness.
- New live fact: dashboard PID 16864 (`dev-loop.py --dashboard --no-terminal-relaunch`, started 2026-11-23T00:56:56Z) has visible window hwnd 67644 with rect L=-1525 T=210 R=-767 B=943; `.dev/dashboard-ui.json` (mtime 2026-11-24T18:42:50Z) is geometry `758x733+-1525+210`, window_bounds x=-1525, matching the live rect. The same PID was previously at x=455 on 2026-11-23, so the window moved to the secondary display while the process stayed alive. First sweep in four with the dashboard on negative-x.
- Keepalive still non-continuous: `.dev/dashboard-keepalive/events.jsonl` last wrote 2026-11-21T22:28:36Z (last `dashboard.healthy` rect [-986,40,-200,788]); zero samples on 11-22, 11-23, 11-24 while a dashboard has run; no keepalive process exists.
- Deliverable (the reusable small-desktop/persistent-desktop proof standard) is still absent from docs; the changed proof-doc sections are gameplay-visual residuals and website release authorization only.

# Validation

- `agent-coord.py begin/sync/observe`, `issue brief`, `proof status`, source/doc SHA-256 via Python, direct live window rect via Win32 `GetWindowRect`, journal reads, and `git log`/`git status` reviewed.
- `issue review` appended review id 372; `send` posted one issue-linked owner message.
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked.

# Findings

1. The standard deliverable is still unimplemented; readiness remains 1/8 and the only task is open with an unproven proof whose pinned doc hashes are stale (one drifted further).
2. The dashboard moved to the secondary display while the same process kept running; live rect and saved geometry now agree at x=-1525, but no restart or keepalive sample exists, so retention across restart remains unproven.
3. The keepalive has produced zero samples for a fourth consecutive day while a dashboard ran; live monitor retention is unobserved.
4. The exemplar proof's whole-file staleness is unchanged this sweep; the gui.py close-path difference found on 11-23 is still unresolved and untested.

# Confidence

High on readiness/proof-hash facts, the live/saved geometry values, PID identity, and keepalive journal timestamps (direct reads). Moderate on attributing the x=-1525 placement to a human move (a human move and a self-controlled move are equally consistent, and no keepalive sample exists). Unknown: whether retention survives a restart, and whether the changed close_view path affects save-before-exit.

# Proof

No proof claim. Read-only review; evidence is source-tier, runtime-state-tier, and journal-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; observations recorded on the issue.

# Human acceptance

- Visual, gameplay, multiplayer review: none (not applicable)
- Still unverified: three close/reopen cycles from the current x=-1525 placement with `dashboard.healthy` rect and `.dev/dashboard-ui.json` captured per cycle, one dashboard PID (no duplicate), and geometry written before exit.

# Agent handoff context

- Owner: `dashboard-position-proof` (messaged, reply due 2026-11-24T20:48:51Z).
- Next exact step: with the dashboard already at x=-1525, run `python devscripts/dashboard-keepalive.py --interval 5` (no `--focus`) for 10 minutes, close/reopen the dashboard three times, and record each `dashboard.healthy` rect and `.dev/dashboard-ui.json`; confirm a single PID. Then re-pin `task-proof-bac2b095c7af` to eaef36e8.../f3d370b5... and re-verify `task-proof-ed45c1b9e923` by the named geometry+close_view region.
- What another agent should not redo: do not re-derive inventory drift, do not infer a snap from saved==live, and do not claim retention from the pre-move 11-20 proof alone.

# Automatic evidence

- Git status: shared dirty tree preserved; this session added one issue review, one owner message, coordination DB changes, and this changelog
- Build identity: latest published game-DLL generation 599 (`src/vehicles/vehicle-system.cpp`); unrelated to this issue; no build requested
- Journal paths checked: `.dev/dashboard-keepalive/events.jsonl`, `.dev/dashboard-ui.json`; live window via Win32 on dashboard PID 16864

# How to reverse

- Remove only the appended review (id 372) from the `issue-0cdac76fb006` doc; do not rewrite prior text.
- Restore `.dev/agent-coord/brain.db` from backup if the message/review is withdrawn.
- Delete this changelog. No commit to revert.
