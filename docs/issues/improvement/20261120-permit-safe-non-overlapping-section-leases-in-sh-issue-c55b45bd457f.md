# issue-c55b45bd457f: Permit safe non-overlapping section leases in shared files

## Human report (verbatim, immutable)

> can u make it so like , check the lease but if u arent editing the same section then just edit the file anyway , or coordinate with the lease owner on a together cahnge , is that doable


## Triage

### 2026-11-20T04:06:29.347Z — coordination-region-leases

Summary: Make same-file collaboration less blocking without risking lost edits. The shared checkout gives agents the same physical file, so arbitrary concurrent writes to different logical sections can still overwrite one another. Improve the lease-conflict path so an agent can prepare a disjoint patch while the owner holds the file, request a coordinated handoff, then apply/reconcile under a serialized file lease. Preserve default one-writer protection and explicit shared-region acceptance.
Owner: coordination-region-leases
Priority: high

## Scheduled independent review

### 2026-11-22T01:20:04.661Z — cpu-build-efficiency

A bounded independent review was queued as wake wake-67b1c7c3ad91 after 211 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs_evidence)

### 2026-11-22T01:26:07.514Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-67b1c7c3ad91), 2026-11-22T01:25Z. Read-only: no source edit, build, close, or fix claim.

OBSERVED FACTS
- Readiness: `agent-coord issue show issue-c55b45bd457f` returns ready=false, score 1/8, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, and risk. The linked task `disjoint-file-patch-handoff` is still state=open with empty evidence and method; `issue lint` also reports this issue missing 7 fields.
- Proof: `agent-coord proof status` has no proof row for `disjoint-file-patch-handoff` or this issue. No verified or exempt proof exists.
- Implementation is present at HEAD. A blocked `edit begin` explains that direct same-file writes stay serialized and points to the proposal path (`devscripts/agent_coord.py:3624-3632`). `edit_propose` stores a proposal and sends a `shared.request` to the holder without editing the leased source (`devscripts/agent_coord.py:3666-3701`). `shared_begin` rejects duplicate region names (`devscripts/agent_coord.py:3568-3591`). `proposal_apply` requires the recipient and a free lease, then runs `git apply --check` and `git apply` (`devscripts/agent_coord.py:3787-3815`).
- Spec alignment: `docs/specs/dev-loop/agent-coordination.md:509-524` and `:532-547` describe region labels as coordination labels (not enforced source ranges), the proposal/handoff path, and sequential apply after lease release.
- Session record: `docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md:19` says status complete and also states "Validated CLI help, Python syntax, and diff whitespace; no runtime/build test." Its final commit is `ce0009d5`, the same commit that contains `edit_propose`.
- Tests: `devscripts/quality/test_agent_coord.py:141-155` passes but only checks region-name uniqueness and sequential leases. No test under `devscripts/quality` calls `edit_propose`, `proposal_create`, or `proposal_apply` (search found zero). Full module run on 2026-11-22: 41 tests OK.
- Journal: `.dev/agent-coord/events.jsonl` has 507 `coord.lease.acquired` and 24 `coord.lease.renewed`, but 0 `coord.proposal.created`, 0 `coord.proposal.applied`, 0 `coord.shared.requested`, and 0 `coord.shared.accepted`. The task's only journal activity is `coord.activity.set` phase=implementation at 04:11:19Z and session ended at 04:12:01Z.

INFERENCE / UNCERTAINTY
- The code and spec path exists, but the acceptance line "one real coordination workflow demonstrates the patch can be prepared before the lease is released" is unmet and unproven: no proposal or shared event has ever been recorded, and the passing unit test does not exercise the handoff.
- "Shared regions stay disjoint" is currently a name-level contract (duplicate names rejected), not an enforced source-range guarantee; the spec says so explicitly. This is not a regression, but it should not be read as concurrent section safety.
- `proposal_apply` applies the whole stored diff and does not verify that it touches only the declared `path` (`devscripts/agent_coord.py:3799-3808`). The apply check proves context only. That safety gap is already tracked under issue-52b59bdd440c; it is not introduced here.

CONTRARY EVIDENCE
- The owner deliberately preserved one-writer protection instead of honoring the literal "edit the file anyway" request, which matches triage and the spec. No lost-edit or failed-handoff incident was found. The current gap is missing proof and zero real use, not a demonstrated defect.

NEXT DISCRIMINATING CHECK
Run one real two-agent workflow on a scratch leased file (no shared source writes): A `edit begin`; B prepares a patch file and runs `edit propose`; A releases; A runs `proposal apply`. Require journal events `coord.lease.acquired`, `coord.proposal.created`, `coord.shared.requested`, `coord.proposal.applied` and the expected final file hash. That single run either satisfies the acceptance or exposes the first usability/failure point.
