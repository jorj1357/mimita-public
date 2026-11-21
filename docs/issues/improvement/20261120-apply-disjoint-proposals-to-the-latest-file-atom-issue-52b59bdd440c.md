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
