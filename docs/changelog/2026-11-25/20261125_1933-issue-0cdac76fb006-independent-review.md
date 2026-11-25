# Independent bounded review of issue-0cdac76fb006

- Task ID: wake-9746dc6759a2
- Agent: issue-review-sweeper
- Focus: one read-only review of the small-desktop proof standard issue, its linked task/proofs, the exemplar dashboard monitor-retention evidence, current dashboard live/saved state, the keepalive journal, and one live keepalive sample
- Summary: Appended one independent review (review id 435, verdict `inconclusive`) to `issue-0cdac76fb006` and one issue-linked message (msg-fc4f6d3cff2e) to owner `dashboard-position-proof`. Executed the documented single-shot `dashboard-keepalive.py --once` sample (no `--focus`) and captured the first `dashboard.healthy` event since 2026-11-21. No source edit, build request, test run, close, owner change, or fix claim. No new process proposal; existing proposals cover the observed gaps.
- Status: complete (review only)
- Date, time, timezone: 2026-11-25T19:33Z, ISO 8601

# Pre-existing changes

- The shared checkout retains one modified issue doc (this review's append) and the dirty `devscripts/blender-ai` submodule. This session modified only `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md`, the coordination DB, `.dev/dashboard-keepalive/events.jsonl` (one sample), and this changelog. No compiler, build.py, module worker, second dev-loop, or runtime test run was invoked.

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
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` | 4 reviews (ids 46/191/288/372) | + one review (id 435) | required independent review record | remove only the appended review |
| `.dev/agent-coord/brain.db` | no review id 435 | review id 435 + issue-linked message msg-fc4f6d3cff2e | coordination record | restore from backup if withdrawn |
| `.dev/dashboard-keepalive/events.jsonl` | last event 2026-11-21T22:28:36Z | + one `dashboard.healthy` sample at 2026-11-25T19:31:01Z | bounded live check | remove only the last line |
| `docs/changelog/2026-11-25/20261125_1933-issue-0cdac76fb006-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue_review` added review id 435 to `issue-0cdac76fb006` (verdict `inconclusive`; state stays `in_review`, owner unchanged).
- One issue-linked `message` msg-fc4f6d3cff2e sent to owner `dashboard-position-proof` (thread thread-84513dc430dc, appended to the issue discussion).

# Diagnostics

- Readiness: `ready=false`, score 1/8, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk; one linked task `small-desktop-state-evidence-standard` still `open`.
- Proof `task-proof-bac2b095c7af` still `unproven`, `verified_utc` empty, evidence empty. Pinned doc hashes stale and unchanged from 11-24: `issue-intake-and-triage.md` 56d48ea9... vs current f3d370b5...; `agent-proof-and-verification.md` 9d2eaefb... vs current eaef36e8....
- Proof `task-proof-ed45c1b9e923` still `proven` in the store but dynamically stale for `devscripts/dev_loop/gui.py` (recorded 20fac330... vs current 267e69ea...); current gui.py hash unchanged since 11-22/11-23/11-24 and `dashboard-keepalive.py` d3cc3e94... matches. Build generation 642 (2026-11-25T08:43Z) changed only `src/npc/npc-combat.cpp`, so it did not trigger new gui.py staleness.
- New live fact: dashboard restarted. Current dashboard is PID 4460 (`dev-loop.py --dev-loop-role=dashboard --profile npc-navigation --no-terminal-relaunch --visible`, started 2026-11-25 00:16:55 local), replacing PID 16864 (running since 2026-11-23 00:56:56 per the 11-24 sweep). Window handle changed 67644 -> 67656. Live rect is [384,20,1156,779] (primary, x=384); `.dev/dashboard-ui.json` (mtime 2026-11-25 14:05:22 local / 19:05Z) is `772x759+384+20`, matching live.
- New live sample: `python devscripts/dashboard-keepalive.py --once` (no `--focus`) exited 0 and appended `dashboard.healthy` at 2026-11-25T19:31:01.311Z (pid 4460, hwnd 67656, visible, minimized=false, rect [384,20,1156,779]) — the first keepalive sample since 2026-11-21T22:28:36Z (rect [-986,40,-200,788]). No persistent keepalive process exists.
- Restart retention still unobserved: no sample at the 00:16:55 restart, and the 19:05Z save reflects a later `<Configure>` (gui.py persists geometry 400 ms after Configure, `persist_window_geometry`/`queue_geometry_save` lines 925-958), so whether the restart restored x=-1525 or snapped to primary is unrecorded.
- Deliverable (the reusable small-desktop/persistent-desktop proof standard) is still absent from docs; the issue doc changed only by scheduled-review text.

# Validation

- `agent-coord.py begin/sync/observe`, `issue readiness`, `proof list`, source/doc SHA-256 via Python, direct live window rect via Win32 `GetWindowRect`, journal reads, and `git status` reviewed.
- `dashboard-keepalive.py --once` (no `--focus`) executed; one `dashboard.healthy` event emitted.
- `issue review` appended review id 435; `send` posted one issue-linked owner message.
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked.

# Findings

1. The standard deliverable is still unimplemented; readiness remains 1/8 and the only task is open with an unproven proof whose pinned doc hashes are stale.
2. A real dashboard restart occurred at 2026-11-25 00:16:55 (PID 16864 -> 4460, hwnd 67644 -> 67656) while the last saved position was x=-1525; the current position is primary x=384, but whether the restart retained or snapped is still not recorded.
3. The keepalive gap is now partially closed: a bounded `--once` sample captured live health at rect [384,20,1156,779], proving the tool works today; continuous/inline sampling through a restart remains the missing piece.
4. The exemplar proof's whole-file staleness is unchanged this sweep; the gui.py close-path difference found on 11-23 is still unresolved and untested.

# Confidence

High on readiness/proof-hash facts, the live/saved geometry values, PID/hwnd identity change, the new keepalive sample, and journal timestamps (direct reads). Moderate on attributing the current x=384 to a human/agent move at 19:05Z (a human move, automated repositioning, and a startup snap are still distinguishable only by a sample through a restart). Unknown: whether retention survives a restart, and whether the changed close_view path affects save-before-exit.

# Proof

No proof claim. Read-only review; evidence is source-tier, runtime-state-tier, and journal-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; observations recorded on the issue.

# Human acceptance

- Visual, gameplay, multiplayer review: none (not applicable)
- Still unverified: one close/reopen (or natural restart) captured inline with `dashboard.healthy` rect and `.dev/dashboard-ui.json` compared before/after, one dashboard PID (no duplicate), and geometry written before exit.

# Agent handoff context

- Owner: `dashboard-position-proof` (messaged, msg-fc4f6d3cff2e).
- Next exact step: run `python devscripts/dashboard-keepalive.py --interval 5` (no `--focus`) persistently across the next natural dashboard restart (or one owner/human-approved close/reopen), then compare the first post-restart `dashboard.healthy` rect to the pre-restart rect and confirm one PID. Then re-pin `task-proof-bac2b095c7af` to f3d370b5.../eaef36e8... and re-verify `task-proof-ed45c1b9e923` by the named geometry+close_view region.
- What another agent should not redo: do not re-derive inventory drift, do not infer a snap from saved==live, do not over-read the `--once` sample as restart proof, and do not claim retention from the pre-move 11-20 proof alone.

# Automatic evidence

- Git status: shared dirty tree preserved; this session added one issue review, one owner message, one keepalive sample, coordination DB changes, and this changelog
- Build identity: latest published game-DLL generation 642 (`src/npc/npc-combat.cpp`); unrelated to this issue; no build requested
- Journal paths checked: `.dev/dashboard-keepalive/events.jsonl`, `.dev/dashboard-ui.json`; live window via Win32 on dashboard PID 4460

# How to reverse

- Remove only the appended review (id 435) from the `issue-0cdac76fb006` doc; do not rewrite prior text.
- Remove only the appended 2026-11-25T19:31:01Z `dashboard.healthy` line if the sample is withdrawn.
- Restore `.dev/agent-coord/brain.db` from backup if the message/review is withdrawn.
- Delete this changelog. No commit to revert.
