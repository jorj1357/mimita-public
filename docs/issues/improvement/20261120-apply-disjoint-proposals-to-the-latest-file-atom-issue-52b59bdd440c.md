# issue-52b59bdd440c: Apply disjoint proposals to the latest file atomically

## Human report (verbatim, immutable)

> The current edit propose workflow uses the separate-patch approach, but the final apply still requires a free file lease. To allow truly concurrent final updates, the coordinator would need to apply each patch to the latest file under a brief lock, then write the merged result atomically.


## Context

### 2026-11-21T05:25:26.919Z — build-wait-reviewer

reproduction: Agent A holds an edit lease on devscripts/agent_coord.py. Agent B creates a disjoint patch with 'agent-coord edit propose' and then runs 'agent-coord proposal apply <id>'. proposal_apply rejects while A's lease is active ('is leased by', devscripts/agent_coord.py:3420-3421), and only the recipient can apply (:3416-3417), so B cannot apply until A releases. The proposals table stores patch_sha but no base file hash, and there is no short serialized apply lock or atomic-replace step.
expected_behavior: A disjoint proposal prepared while another agent holds the file lease can be applied against the latest file contents under a brief serialized lock: verify the declared target path and base context, reject overlapping patches, replace the file atomically, and record the resulting hash and conflict status. Direct edits keep the existing one-writer lease rule.
acceptance: Proposal stores target path and base file hash at create time; apply takes a short serialized lock without requiring the full edit lease; apply rejects when current file hash/context does not match the proposal base; overlapping proposals are rejected with a recorded conflict and unchanged file; file replacement is atomic and the resulting hash is stored and journaled; a two-patch integration test proves both disjoint changes survive and the conflicting one is rejected.
paths: ['devscripts/agent_coord.py', 'docs/specs/dev-loop/agent-coordination.md']
evidence: {'code': 'proposal_apply at devscripts/agent_coord.py:3410-3438 requires _active_lease(relative) is None (:3419-3421); proposal_create at :3374-3395 stores patch_sha but no base file hash; proposals schema at :280-291 has no base_hash or applied_hash column', 'spec': 'docs/specs/dev-loop/agent-coordination.md:479-483 and acceptance :638-640 currently require proposal apply to need a free file lease and a clean patch check', 'gap': 'issue asks to change that to short-lock apply against latest contents, which is a spec change needing human review before code change'}
risk: Touches the coordination write path. A lost update or partial write could corrupt shared files; require temp-file plus atomic rename, base-hash recheck under the lock, and no weakening of the direct-edit one-writer rule.
proof_expectation: Focused integration test with two agents and two disjoint patches on one file: both apply sequentially, resulting hash is recorded, and events show applied hashes; an overlapping patch is rejected with a conflict and the file hash is unchanged.

## Agent suggestion

### 2026-11-21T05:31:13.611Z — build-wait-reviewer

Bounded-activation review (build-wait-reviewer, wake-37ac86bffbeb): do not implement yet. Current code and spec deliberately require a free lease for apply (agent-coordination.md:479-483, :638-640), so this issue is a spec change first. Suggested slice: (1) add base_hash/applied_hash columns and store base hash in proposal_create; (2) add a short sqlite/atomic lock helper that serializes apply without holding the edit lease; (3) re-check target path plus base hash and run 'git apply --check' inside the lock; (4) write temp file and os.replace for atomicity; (5) record applied hash and a conflict row on mismatch/overlap; (6) extend the coordination self-test with two disjoint patches and one overlapping patch. Owner should get human approval for the acceptance-line change before editing code.

## Context

### 2026-11-21T05:38:40.376Z — build-wait-reviewer

reproduction: Acquire a lease as agent A with edit begin on a shared file, then have agent B run edit propose. While A still holds the lease, run proposal apply as A: agent_coord.py:3419-3421 rejects because an active lease exists. Also run two concurrent proposal apply calls for distinct proposals targeting one file and observe the unsynchronized git apply --check followed by git apply sequence.
expected_behavior: A recipient may apply a reviewed disjoint proposal to the latest file under a short coordinator lock without forcing the sender to wait for a full lease release. Apply verifies the proposal target and base context, rejects overlap with other proposals or regions, atomically replaces the file, and records the new file hash, proposal state, and any conflict.
acceptance: 1) proposal_apply succeeds while a non-overlapping lease is active, using a short lock. 2) Overlapping or stale proposals are rejected with an explicit conflict record and no file change. 3) The declared path is verified and patches touching other paths are rejected. 4) Concurrent apply attempts serialize and the final file hash plus proposal state are recorded. 5) The existing one-writer lease path remains the default and unchanged.
paths: ['devscripts/agent_coord.py', 'docs/specs/dev-loop/agent-coordination.md', 'docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md']
evidence: {'text': 'Code inspection 2026-11-21 by build-wait-reviewer. edit_propose (agent_coord.py:3289-3324) stores no region or base hash and only messages the lease holder. proposals schema (agent_coord.py:280-291) has path, sender, recipient, patch path, sha, state, timestamps but no region, base_hash, or resulting hash. proposal_apply (agent_coord.py:3410-3438) refuses when any active lease exists (3419-3421), runs git apply --check then git apply at ROOT without a coordinator lock or path allowlist (3422-3431), and records only state plus applied_utc with no new file hash or conflict record. FileLock is used only for the events.jsonl append (agent_coord.py:620).'}
risk: A careless concurrent apply could overwrite an unrelated region, apply a patch outside the declared path, or interleave check and write. Keep one-writer lease behavior as the default and gate the new path behind explicit disjoint-region verification.
proof_expectation: Runtime-proven coordinator CLI evidence: run the reproduction on a scratch file, show proposal apply succeeding under a non-overlapping lease, show an overlap rejection with a conflict id and unchanged file hash, and show the recorded resulting hash. A unit or synthetic test alone does not count as behavior proof.

## Agent suggestion

### 2026-11-21T05:43:39.573Z — build-wait-reviewer

Suggestion (build-wait-reviewer, bounded wake): extend proposals with region, base_hash, and claimed_paths. In proposal_apply, take the existing FileLock, re-read the file, verify the patch touches only claimed_paths, then git apply --check to a temp index and os.replace the merged bytes; on any overlap or stale context, open a conflict and leave the file unchanged. Record result_sha in proposals and the coord.proposal.applied event. Keep edit begin (one-writer lease) as the default; only allow the short-lock path for explicitly disjoint regions. Proof must be a scratch-file two-agent CLI run, not a unit test alone.

## Scheduled independent review

### 2026-11-22T04:46:46.209Z — codex-ingame-gui-reconnect

A bounded independent review was queued as wake wake-357781f65dce after 244 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-unimplemented)

### 2026-11-22T05:21:27.742Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (independent, read-only). Scope: issue brief/readiness, proof status, linked artifacts, current code/spec, and the coordination journal.

OBSERVED FACTS (2026-11-22)
1. No implementation or proof exists. Task coord-proposal-apply-v2 is state=open with empty evidence; `proof list --issue issue-52b59bdd440c` returns `[]`. Readiness is ready=true, 8/8, 1 task.
2. Working tree is clean for devscripts/agent_coord.py and docs/specs/dev-loop/agent-coordination.md. The last commit touching the module is d89884f6 (2026-11-21T21:47Z), after the task was created, but it did not add the accepted behavior; the proposal schema and apply path are unchanged.
3. proposals schema (devscripts/agent_coord.py:286-297) has proposal_id, path, sender, recipient, patch_path, patch_sha, state, created_utc, applied_utc. No region, base_hash, result hash, or conflict column.
4. edit_propose (devscripts/agent_coord.py:3666-3701) requires --region, but region is used only in the notification subject/body. proposal_create (3751-3772) does not persist region or a base file hash; it stores only the patch sha.
5. proposal_apply (3787-3815) rejects whenever any active lease exists on the target (3796-3798), reproducing the reported symptom. It then runs `git apply --check` and a separate `git apply` (3799-3808) at the repo root with no coordinator lock, no check that the patch touches only the declared row path, and no atomic temp+replace. It records only state='applied' and applied_utc, with no resulting file hash and no conflict row.
6. FileLock is used only to append events.jsonl (agent_coord.py:688). Nothing serializes concurrent proposal_apply calls.
7. Spec still requires the old behavior: docs/specs/dev-loop/agent-coordination.md:520-524 says the holder may apply it after releasing the lease. Acceptance items 1 and 5 therefore need a spec change as well as code, matching the prior reviewer note.
8. Journal: .dev/agent-coord/events.jsonl has 5,921 events and zero coord.proposal.created, coord.proposal.applied, or coord.proposal.rejected events. The proposal path has never been exercised in this coordination state, so there is no runtime evidence in either direction.

INFERENCE / UNCERTAINTY
- Defect confidence is high from direct code read: the free-lease requirement, missing base hash, missing path allowlist, unlocked check/apply, and missing result hash are all present in current source. The prior evidence line numbers are stale (~377-line offset); use 3666-3701, 3787-3815, and spec 520-524, not the old 3410-3438 / 3289-3324 / 638-640.
- The conflict-record format and overlap rule are not merely unimplemented; they are undefined, so acceptance tests must define them first.
- This review did not run the runtime reproduction. ROOT for git apply is hardcoded to the checkout (agent_coord.py:28) and is independent of the --root state option (4044, 4524), so an isolated --root repro would still git apply into this shared working tree. A bounded reviewer cannot generate that evidence without mutating the shared checkout, which is a cross-agent interference risk.

CONTRARY EVIDENCE
- Region/lease support is otherwise present (edit begin --region, shared regions), and proposal show/reject exist. This does not change the apply-path defect.
- A claim that no commit landed since task creation would be false: d89884f6 landed after the task. The accurate claim is that the accepted behavior is absent in current HEAD and working tree.

NEXT DISCRIMINATING CHECK (owner, not this reviewer)
- In a disposable clone (so hardcoded ROOT is the clone), create a scratch file. Agent A edit begin it; Agent B edit propose a disjoint patch; then A proposal apply. Expected today: rejection at 3796-3798 while A's lease is active. Then run two disjoint applies concurrently and record the check/apply race. Capture file SHA-256 before/after and show the absence of a conflict row and result hash. This is the issue's stated proof expectation and remains unmet.

VERDICT: confirmed-unimplemented; do not close.
