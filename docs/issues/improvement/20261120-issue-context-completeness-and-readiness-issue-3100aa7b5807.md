# issue-3100aa7b5807: Issue context completeness and readiness

## Human report (verbatim, immutable)

> can u do these as much as possible like as a test to ensure that each issue has all releveant info for it


## Task linked

### 2026-11-20T05:59:49.390Z — work-metrics

Task: issue-context-schema (step 0)

## Task linked

### 2026-11-20T05:59:51.411Z — work-metrics

Task: issue-readiness-cli (step 0)

## Task linked

### 2026-11-20T05:59:52.658Z — work-metrics

Task: issue-backfill (step 0)

## Task linked

### 2026-11-20T05:59:53.878Z — work-metrics

Task: issue-context-tests (step 0)

## Context

### 2026-11-20T06:23:57.238Z — work-metrics

reproduction: Run issue needs-info against the open issue list; observe missing reproduction, expected behavior, acceptance, paths, or evidence.
expected_behavior: Every active issue exposes complete context and is ready or explicitly needs_info with named missing fields.
acceptance: issue readiness returns ready=true for backfilled active issues; incomplete issues list exact missing fields.
paths: ['devscripts/agent_coord.py', 'devscripts/quality/test_agent_coord.py', 'docs/specs/dev-loop/agent-coordination.md', 'docs/workflows/issue-intake-and-triage.md']
evidence: {'spec': 'docs/workflows/issue-intake-and-triage.md', 'command': 'agent-coord.py issue needs-info'}

## Context

### 2026-11-20T14:47:27.209Z — context-bus

reproduction: Run gent-coord issue lint over the live brain and review every returned issue.
expected_behavior: Every new issue is rejected until all required fields exist; legacy issues are either completed from evidence or explicitly marked needs-info.
acceptance: Issue creation rejects each missing field; lint reports zero incomplete open issues after backfill.
paths: ['devscripts/agent_coord.py', 'devscripts/agent-issue.py', 'docs/specs/dev-loop/agent-coordination.md', 'docs/workflows/issue-intake-and-triage.md']
evidence: {'implementation': 'strict issue_add rejects missing quote, summary, reproduction, expected behavior, acceptance, paths, evidence, proof expectation, risk'}
risk: Automated placeholder text would create false readiness; require real context or explicit needs-info.
proof_expectation: Unit rejection tests plus live issue lint output before/after backfill.

## Agent suggestion

### 2026-11-22T01:26:42.522Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-67b1c7c3ad91, 2026-11-22T01:26Z): this sweep found one concrete missing-evidence completion. Session coordination-region-leases marked task disjoint-file-patch-handoff implementation complete (`docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md:19`) and committed it in `ce0009d5`, yet issue-c55b45bd457f still has readiness 1/8, empty task evidence and method, no proof row, and zero `coord.proposal.*`/`coord.shared.*` events in `.dev/agent-coord/events.jsonl` (despite 507 lease acquisitions). The code exists, but the acceptance-required real workflow was never recorded. Whether this is a repeated pattern is not yet measured.

Measurable target: `agent-coord issue lint` (or the board) reports zero issue-linked tasks in state complete/done that have empty task evidence and no proof record. Start by making this issue's case visible. Smallest experiment: add one read-only lint query over tasks joined to proof rows, run it on the live brain, and confirm it flags issue-c55b45bd457f / disjoint-file-patch-handoff while proven tasks are not flagged. No source or build change; if the count is nonzero, decide separately whether session end or task completion should warn or block.

## Agent suggestion

### 2026-11-22T01:29:02.219Z — issue-review-sweeper

Clarification to the process proposal appended at 2026-11-22T01:26Z (issue-review-sweeper, wake-67b1c7c3ad91): the observable is a session/changelog claim of completion, not a task in state complete. In this case the linked task `disjoint-file-patch-handoff` is still `open` even though the owner session changelog says status complete; a query filtered on task state alone would miss it. Use this measurable target instead: `agent-coord issue lint` reports zero issue-linked tasks that have empty evidence, no proof row, and an owner session that has ended, starting with issue-c55b45bd457f. Smallest experiment: a read-only query over tasks, proof rows, and ended work sessions that flags this issue/task while leaving tasks with proven or exempt proof unflagged. No source or build change.

## Agent suggestion

### 2026-11-22T02:21:44.954Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-28e67b8e79f8, 2026-11-22T02:2xZ): this sweep found a human-only completion gate with no durable request/decision artifact. Issue-93bd29a36d24 (architecture, spec-only) already contains the requested target contract at docs/specs/networking/contact-authority.md:76-160 and no src/ contact path changed, yet the issue is ~46h old, readiness 1/8, state new until this review, task contact-spec open with empty evidence, and no recorded human pre-review. The review scheduler selected it at readiness 1/8. This overlaps the existing proposal for complete tasks with empty evidence but adds a distinct gate: spec-only acceptance names human pre-review with no recorded request timestamp, owner, or decision.

Measurable target: 100% of spec-only tasks whose acceptance names a human pre-review have a recorded request timestamp and an explicit decision (approve/edit/defer) before a second review sweep; the board shows the count of such tasks with no decision. Secondary: 0 scheduled reviews of issues with readiness score < 4 unless the issue is explicitly flagged review-only.

Smallest experiment: add one read-only lint over issue-linked tasks whose acceptance/summary contains human pre-review or spec only, joined to reviews/quotes/clarifications; run it on the live brain and confirm it flags issue-93bd29a36d24/contact-spec while approved tasks are not flagged. No source or build change.

## Agent suggestion

### 2026-11-22T04:11:05.442Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-788722969fee, 2026-11-22T04:1xZ): third measured instance of the same record-drift pattern, with a distinct trigger. issue-75220121d993 (architecture, spec-only) has its requested target spec committed at docs/specs/architecture/nodes-and-connections.md (301 lines, commit d3368058) and a changelog entry, yet after 232 completed work sessions the issue is still state=new, readiness 1/8, reviews/reviewers empty, and task graph-spec open with empty evidence and no proof row. Unlike the 2026-11-22T01:26Z proposal (task complete but empty evidence) and the 02:21Z proposal (human pre-review gate), here the artifact exists and appears to meet the task acceptance, so the cost is a wasted review wake and a task that cannot be closed from its own record.

Measurable target: within one completed work session of a commit that adds or changes a spec/issue artifact, 100% of linked open tasks whose acceptance is plausibly satisfied by that artifact have a recorded evidence link or an explicit not-linked reason; the board count of such unlinked tasks drops from at least 2 (issue-c55b45bd457f, issue-75220121d993) to 0.

Smallest experiment: add one read-only check to the existing session-end/work-metrics path (or a standalone agent-coord query) that takes the ending session's changed docs/specs and docs/issues paths and reports open issue-linked tasks with empty evidence and no proof row. Run it against this wake's session and confirm it flags issue-75220121d993/graph-spec while leaving tasks with a proof row or recorded evidence unflagged. No source or build change until human review; never auto-close.

## Agent suggestion

### 2026-11-22T06:23:20.864Z — issue-review-sweeper

Process evidence from issue-82cdc1277704 review (2026-11-22T06:19Z): the issue is still not readiness-ready (missing proof_expectation and risk) yet sits triaged/owned, and its only cited evidence artifact logs/2026-11-20/20261120_040807 is missing while the linked re-review run 20261120_064201 has run.json but no server-01/events.jsonl. Proposal: extend issue lint/readiness to (a) list the proof_expectation/risk gap and (b) verify each evidence.runtime/run_id path exists and is non-empty, marking an issue not-ready when evidence is unverifiable. Measurable target: over all open issues, report the exact count with missing fields or missing/nonexistent evidence runs; target 0 unverifiable open issues after backfill (seed: issue-82cdc1277704). Smallest experiment: run the current readiness over open issues, add the evidence-path existence check, and re-run; compare flagged counts and confirm the two missing fields plus the absent cited run are caught. Risk is low: read-only check, no schema change.

## Scheduled independent review

### 2026-11-22T09:41:13.590Z — cpp-latency-linker-investigation

A bounded independent review was queued as wake wake-5ac16a86265e after 271 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (changes-requested)

### 2026-11-22T09:54:57.453Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review — issue-review-sweeper, wake-5ac16a86265e, 2026-11-22T09:5xZ. Read-only: no source edit, build, close, or fix claim.

Observed facts (commands run in this checkout ~2026-11-22T09:42-09:55Z):
1. Own record: issue-3100aa7b5807 is state=new, owner=work-metrics, with 6 linked tasks all `open` and all carrying empty `evidence`/`method`, and 0 rows in task_proofs (SQL join issue_tasks→task_proofs). No implementation artifact or runtime evidence is attached; the issue "evidence" is a JSON claim string only.
2. Required-field sets disagree in three places:
   - issue_add enforcement (devscripts/agent_coord.py:1770-1780): title, quote, summary, reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation. It does NOT enforce `risk`.
   - issue_readiness (devscripts/agent_coord.py:2803-2811): human_quote, reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. It does NOT check `summary`, `title`, or `source`.
   - issue_brief (devscripts/agent_coord.py:1908-1909) checks only title, summary, source.
   Live consequence: `issue brief issue-3100aa7b5807` returns missing=["summary"] while `issue readiness issue-3100aa7b5807` returns ready=true score=8/8 missing=[], at the same instant. The gate accepts an issue that its own brief calls incomplete.
3. Live counts (read-only SQL on .dev/agent-coord/brain.db; 240 issues, 232 non-closed): 6 non-closed issues have an empty `summary` (issue-6772ff949164, issue-89efbbc86535, issue-549680e53b26, issue-5f49047dae49, issue-89c11130189a, issue-3100aa7b5807). Two of them report ready=true 8/8: issue-89c11130189a and issue-3100aa7b5807.
4. `issue lint --limit 500` currently returns 41 non-ready issues across states: in_review=16, assigned=10, needs_clarification=7, triaged=3, new=2, done=2, deferred=1. The issue acceptance is "lint reports zero incomplete open issues after backfill"; lint neither filters to open (it includes done and deferred) nor reaches zero.
5. Context-integrity defect in this issue's own record: `reproduction` holds byte 0x07 (BEL) where "a" belongs ("Run <BEL>gent-coord issue lint ..."). Verified via character codes at docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md:46. This is the only issue in brain.db with a control character in `reproduction`. Neither readiness, lint, nor brief flags non-printable/corrupt field content.
6. Test coverage: devscripts/quality/test_agent_coord.py:556-578 (test_issue_context_readiness) and :615-620 (test_issue_add_rejects_missing_required_fields) cover readiness and summary-at-add, but the add_issue helper always sets summary=title (test file:51), so no test covers an empty summary in readiness or any control-character/whitespace corruption.

Inference / uncertainty:
- Most likely cause of "ready but incomplete" is that readiness — the only gate used by lint and next-work — carries a field list that diverged from both creation and brief and never includes `summary`. High confidence this inconsistency is real (verified by two live commands on one issue). Medium confidence on intent: summary could be argued optional for legacy issues, but the human request ("ensure that each issue has all relevant info") and issue_add both treat summary as required.
- The 0x07 byte is probably a terminal/shell escape artifact written into the DB, not deliberate. Whether other fields or source docs hold hidden corruption is not measured; I checked `reproduction` only.
- Fixing the field-set divergence alone may not drive the 41 lint rows to zero: several are genuinely missing human-supplied context (needs_clarification). So "zero incomplete open issues" may be unreachable without an explicit needs-info carve-out. This is the main ambiguity in the acceptance wording.

Contrary evidence:
- Some of the 41 lint rows are legitimately awaiting human clarification (state needs_clarification), and this issue's own acceptance allows "needs-info with a question"; nonzero lint is therefore not by itself proof of failure. The sharp failure signal is the ready=true-with-empty-summary contradiction and the unfiltered "open" definition.
- issue_add does reject a missing summary (unit test proves it), so newly created compliant issues will have one; the empty-summary population is small (6/232) and likely legacy.

Next discriminating check (one, bounded):
- Add a read-only check that (a) lists issues where readiness.ready is true but brief.missing is non-empty (expect 2 now), and (b) issues here risk is required by readiness but not by issue_add, so a fully compliant `issue add` can be born non-ready. Run it against the live brain and report both counts plus the lint count restricted to state in ('new','triaged','assigned','in_review') (31 by the distribution above). This separates a field-contract bug from genuine missing human context. No source change without owner approval.

Confidence: high on the measured contradiction; medium on the intended fix; low on the origin of the 0x07 byte.

## Agent suggestion

### 2026-11-22T09:55:54.066Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-5ac16a86265e, 2026-11-22T09:5xZ): this review found the record-drift class this issue already tracks, now localized to a single-source-of-truth defect: creation, readiness, and brief each validate a different field set (devscripts/agent_coord.py:1770-1780 vs :2803-2811 vs :1908-1909). Measured effect: 6 non-closed issues have an empty summary and 2 of them — including issue-3100aa7b5807 itself — report ready=true 8/8, while a fully compliant `issue add` can be born non-ready because risk is required by readiness but not by add. This is a safe, reversible Python-validation change (no build, no runtime, no auto-close).

Measurable target: 0 issues where `issue readiness` returns ready=true while `issue brief` returns a non-empty missing list (currently 2: issue-89c11130189a, issue-3100aa7b5807), and 0 issues created by `issue add` that are non-ready immediately after creation (currently possible via omitted risk). Secondary: `issue lint` gains an explicit --open-only filter so its count matches the "incomplete open issues" acceptance wording.

Smallest experiment: define one module-level ISSUE_REQUIRED_FIELDS set in devscripts/agent_coord.py and have issue_add, issue_readiness, and issue_brief read it; add --open-only to issue lint; add one unit test that creates an issue with every required field, asserts readiness.ready and brief.missing==[], then blanks summary and asserts both gates flag it. Run devscripts/quality/test_agent_coord.py and re-run readiness/lint on the live brain; expect ready-with-empty-summary to go 2→0 and lint --open-only to drop by at least 2 without changing any non-empty issue's verdict. Read-only until the owner accepts.

## Agent suggestion

### 2026-11-22T14:46:33.133Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-e4f8e54f9733, 2026-11-22T14:3xZ) from the independent review of issue-c9d8a98dd241.

Observed defect class: one coordination command is documented with two different meanings, and the implementation matches only one. docs/specs/dev-loop/agent-coordination.md:218 says `issue needs-info` lists every open issue that is not ready; :247-252 says the user-facing coordinator runs `issue needs-info` to find the exact open questions. devscripts/agent_coord.py:4683-4687 implements only the readiness meaning. Live baseline: 22 issues returned (in_review 17, assigned 4, triaged 1), 0 of the 7 open `needs_clarification` questions included, so a coordinator following :247-252 asks zero of the real questions. This has already delayed the high-priority capability issue-c9d8a98dd241 (~2 days, owner inactive) without any review flagging the missing queue.

Measurable target: 0 documented coordination commands whose described filter differs from the implementation; the user-question queue returns 7/7 live `needs_clarification` issues, and remains 7/7 after a second question is added to an existing issue. Secondary: `issue readiness` and the question queue stay distinguishable in docs and CLI help.

Smallest experiment: add one unit test that seeds two `needs_clarification` issues and asserts the new/split queue returns exactly 2 while `issue needs-info` keeps returning only readiness gaps; update the :247-252 text to name the actual command. Run `devscripts/quality/test_agent_coord.py` and the live queue; expect 7/7 today with no change to the 22 readiness rows. No source edit made by this review.
