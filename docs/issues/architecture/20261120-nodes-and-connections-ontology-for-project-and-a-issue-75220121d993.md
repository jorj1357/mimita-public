# issue-75220121d993: Nodes and connections ontology for project and AI knowledge

## Human report (verbatim, immutable)

> there is no space or time, its just nodes and the connections, so we should be able to get 3d from nodes and their connections, we should be able to switch dimensions live ... c++ itself emerges from nodes and connections


## Task linked

### 2026-11-20T04:25:27.746Z — graph-coordination

Task: graph-spec (step 0)

## Scheduled independent review

### 2026-11-22T03:47:14.194Z — build-wait-reviewer

A bounded independent review was queued as wake wake-788722969fee after 232 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed - spec artifact present, issue/task record incomplete)

### 2026-11-22T04:10:24.352Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, wake-788722969fee, 2026-11-22T03:5xZ). Scope: issue brief/readiness, proof status, linked artifacts, coordination/events journal, and the target spec. No source, build, test, close, or fix was performed. This review appends evidence and a next check only.

OBSERVED FACTS
- Issue state before this review: state=new; readiness=1/8 (ready=false); missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk; reviewers=[]; reviews=[]; evidence={}; paths=[]; related_build/related_run empty. One task `graph-spec` is open, owner graph-coordination, with empty evidence/method and no proof row.
- The requested target spec already exists: `docs/specs/architecture/nodes-and-connections.md` (301 lines), added in commit d3368058. It is routed by `docs/ROUTER.md:135` and named as the target knowledge contract by `AGENTS.md:114`.
- The spec maps to the task acceptance (nodes/edges, space/time/3D projections, live switching, code and AI knowledge, safety, phased plan): Node at line 57, Connection at 83, Projections/Space/Time/3D at 114-141, Live dimension switching at 143, Code at 160, Agent knowledge at 178, Live graph editing at 205, Safety boundaries at 276, Phased roadmap at 244, Acceptance at 290.
- A changelog already records the addition and its boundaries: `docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md:43-57` (spec-only; no networking/gameplay migration).
- No graph implementation was found. A read-only search found the existing bounded `agent-coord observe` context bundle and spec text only; no graph DB/table/query owner and no runtime graph path. This is consistent with the spec's target-architecture / phased-research status and its roadmap, not with a completed implementation.
- `python devscripts/agent-coord.py proof status` returned no row for task `graph-spec` and no row referencing issue-75220121d993. There is no linked build or run, so no runtime journal applies.
- Timing: issue created 2026-11-20T04:20:03Z; task linked 2026-11-20T04:25:27Z; the review was scheduled at 2026-11-22T03:47:14Z after 232 completed work sessions (`coord.issue_review.scheduled`, seq 5648). The issue record has not advanced since creation.

INFERENCE / UNCERTAINTY
- The spec-only deliverable appears materially complete against the task's stated acceptance. The likely gap is coordination record drift: the artifact was created, but the issue evidence/paths, task evidence, and reviewer/approval record were never updated, so the scheduler spent a review wake on a likely-complete spec task.
- I cannot assert `graph-spec` is done. The task evidence is empty, no proof/exemption exists, and the adding changelog explicitly says to expand the spec only after review. This independent review is not owner or human acceptance.

CONTRARY EVIDENCE / LIMITS
- The spec's own Acceptance section (lines 290-301) describes future observable behavior (cross-projection identity, read-only switching, bounded subgraph context) that is not implemented; roadmap phases 1-7 remain future work. That does not contradict the write-a-target-spec acceptance, but it means the issue's broader architecture outcome is not proven.
- The spec begins with C++-style `//` and `/* */` markers (lines 1-7) in a Markdown file. Cosmetic only; noted for the owner because it is unusual for the docs tree.
- No human review/approval of the spec is recorded, so spec-present must not be treated as spec-accepted.

NEXT DISCRIMINATING CHECK
Owner graph-coordination: (1) verify the six acceptance items against `docs/specs/architecture/nodes-and-connections.md`; (2) link that path in issue `paths`/`evidence` and on task `graph-spec`; (3) record either a doc-only proof exemption reason (reviewed spec diff, no runtime behavior) or the specific remaining doc gaps; (4) re-run `agent-coord issue brief issue-75220121d993` and confirm readiness is 8/8 and the task/proof state is no longer empty. If phase-1 implementation is intended next, create a separate task with pinned repo evidence and do not claim runtime proof from this spec.

COLD-HANDOFF NOTES
- Do not redo this artifact search or re-read the spec sections listed above.
- Do not close the issue or mark the task done without the owner/human review above.
- Relevant owner files for follow-up are limited to the spec, ROUTER.md, AGENTS.md, and the linked issue/task records; no source/build artifact is in play.
