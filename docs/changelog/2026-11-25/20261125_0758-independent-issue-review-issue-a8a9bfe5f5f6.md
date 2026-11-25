// 2026-11-25 07:58 UTC
/* purpose
 * record one bounded independent issue-review activation
 * append evidence/counterevidence/next-check review to issue-a8a9bfe5f5f6
 * append one measured scheduler-coverage-telemetry proposal to issue-91e3384b219b
 * no source, build, close, or fix claim by this session
 */

# Task

- Wake ID: wake-0a7ac3d430e7 (scheduled bounded independent review, trigger=work_end)
- Session ID: opencode MIMITA_AGENT_ID=issue-review-sweeper
- Owner: issue-review-sweeper
- Focus: independent review of issue-a8a9bfe5f5f6 (on-demand private screenshots + OCR)
- Summary: The issue is ready and triaged with two open unproven tasks, but no
  implementation or proof exists. `agent.screenshot` still uses a GDI desktop
  copy (`GetDC(nullptr)` + `BitBlt` at the window rect), `screenshot_at_tick`
  has no source implementation, and no OCR adapter exists in `devscripts`.
  Appended one review (issue-a8a9bfe5f5f6, verdict evidence-insufficient) and
  one process proposal (issue-91e3384b219b).
- Status: bounded review complete; no code changes
- Date, time, timezone: 2026-11-25T07:58Z, ISO 8601
- Branch: (left as checked out by the shared dev-loop)
- Base commit: none recorded
- Final commit: none

# Pre-existing changes

- `git status --short` shows unrelated modified config, source, and issue files
  from other agents. None were attributed to, reverted, or edited by this session.
- Files added/modified by this session are listed below only.

# Requested behavior

- Complete one bounded independent review of issue-a8a9bfe5f5f6 starting with
  issue brief/readiness, proof status, linked artifacts, and the latest relevant
  journals; append one evidence-grounded review with observed facts,
  inference/uncertainty, contrary evidence, and the next discriminating check;
  then append at most one non-duplicate process improvement or state none found.
  No source edit, build, close, or claim a fix.

# Specification alignment

- `AGENTS.md` gate 0c: append-only issue evidence, no duplicate issue, no fix
  claim without proof.
- `AGENTS.md` gate 0b/0c self-improvement: hand off to an existing improvement
  issue with a measurable target and smallest experiment.
- `docs/specs/dev-loop/visual-proof-screenshots.md:57-76` already states
  `agent.screenshot` is a GDI desktop copy and not the framebuffer contract.

# Exact implementation changes

- No source or configuration changes.
- Added coordination/repository records only: one issue review on
  issue-a8a9bfe5f5f6, one suggestion on issue-91e3384b219b, and this changelog.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md` | scheduling note | review 430 appended | durable independent review evidence | remove appended review |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` | prior suggestions | one process suggestion appended | observed 4-day scheduler coverage gap | remove appended suggestion |
| `docs/changelog/2026-11-25/20261125_0758-independent-issue-review-issue-a8a9bfe5f5f6.md` | absent | added | one final session changelog | delete file |

# Validation

- `python devscripts/agent-coord.py begin`, `sync`, `observe`, `issue readiness`,
  `proof list`, `issue show`, `issue review`, `ask`, and `issue suggest`
  completed with agent issue-review-sweeper.
- No build, test run, or runtime was started or requested by this session.

# Measured evidence

- Readiness: ready=true, score 8/8, missing=[], state was triaged; after review,
  state=in_review with reviewer issue-review-sweeper.
- `proof list` contains zero rows for task_ids `devloop-visual-ocr-fallback` and
  `mi-mita-visual-capture-privacy` and zero rows for issue-a8a9bfe5f5f6.
- `src/main-systems.cpp:250-256` calls `GetDC(nullptr)` then `BitBlt` at
  `windowX, windowY`; `:270-272` errors with "ensure the client is visible".
- `screenshot_at_tick` occurs only as a fixture string in
  `devscripts/quality/test_agent_coord.py:569`, not in `src/`.
- No OCR adapter tokens (tesseract/pytesseract/easyocr/winocr) in
  `devscripts/*.py` outside that fixture.
- No `events*.jsonl` under `logs/` contains `agent.screenshot`.
- Reusable framebuffer-readback owner exists: `renderReadPixels` ->
  `glReadPixels` at `src/host/host-runtime.cpp:1495-1503`.
- Current ready build is generation 640 from
  `src/network/multiplayer-projectiles.cpp`; unrelated to this issue.

# Evidence gaps

- The occluder leak is inferred from source (GetDC/BitBlt) and was not
  runtime-confirmed in this sweep; no capture run was performed.

# Runtime evidence separation

- Source: read-only inspection of `src/main-systems.cpp`,
  `src/host/host-runtime.cpp`, `docs/specs/dev-loop/visual-proof-screenshots.md`,
  and coordination proof records.
- Build evidence: none claimed for this issue.
- Runtime evidence: none produced; the feature has no implementation to run.
- Human acceptance: none by this session.

# Regression review

- Regression entry appended: no. This is a bounded review of an unimplemented
  capability, not a new regression.

# Risks and unknowns

- Absence of implementation is evidence-of-absence; another agent could be
  working outside this checkout, though no relevant path is dirty here.
- The scheduler selected this ready high-priority unproven issue only after 712
  completed sessions; the reason for the delay is not recorded in events.

# Next steps

- Owner visual-access-and-issue-review: reuse `renderReadPixels` instead of GDI,
  add one explicit `screenshot_at_tick` that writes PNG + identity sidecar, then
  run one pinned client with a synthetic covering window and assert the occluded
  region shows the game framebuffer, not the occluder.
- Process proposal on issue-91e3384b219b: emit one
  `coord.issue_review.considered` event per sweep with candidate ids, rank
  inputs, and selected/skipped reason so coverage gaps are auditable.
