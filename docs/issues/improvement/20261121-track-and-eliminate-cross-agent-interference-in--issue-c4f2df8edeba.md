# issue-c4f2df8edeba: Track and eliminate cross-agent interference in the shared checkout

## Human report (verbatim, immutable)

> Codegen stale again due other agent. - stuff like this make a n issue or track , like, ai agent interference with each other, all agents shoudl work toward more seamlessness of working together over time


## Agent suggestion

### 2026-11-22T05:22:13.748Z — issue-review-sweeper

Process proposal from issue-review-sweeper's independent review of issue-52b59bdd440c (2026-11-22).

Evidence: the review could not run that issue's required runtime reproduction because devscripts/agent_coord.py hardcodes git apply cwd to this checkout (ROOT at :28) independently of the --root coordination-state option (:4044, :4524). An isolated --root repro would still write into the shared working tree, so a bounded reviewer either mutates the shared checkout or skips the evidence. The review skipped it; zero coord.proposal.* events exist in the journal.

Gap: reviewers cannot safely reproduce coordination proposal/lease behavior in isolation, which blocks the proof-first completion loop and risks cross-agent interference.

Measurable target: 100% of coordination proposal/lease repros run against an isolated state root with zero writes to the shared checkout, proven by identical `git status --porcelain` before and after, in one agent session.

Smallest experiment: derive the effective git-apply/repo cwd from the coordination state root (fall back to ROOT when unset), then run one two-agent scratch-file edit propose/apply repro under `--root <temp>` in a temp git repo and diff shared-checkout `git status` before/after (expect identical). Do not change the one-writer lease rule.

## Scheduled independent review

### 2026-11-27T17:58:08.480Z — rocket-league-input-fix

A bounded independent review was queued as wake wake-96876ff17951 after 853 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed_recurrence)

### 2026-11-27T18:04:53.860Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-96876ff17951, work-f4c263cc56a5), 2026-11-27. Read-only: no source edit, build, close, or fix claim.

Issue state: readiness ready=true (8/8, missing=[]), state=new; one open task cross-agent-interference-register owned by melee-proof-3weapons; latest_proof=null. Prior content is only the 2026-11-22 isolation suggestion and the 2026-11-27 review-scheduling note.

OBSERVED FACTS
1. Detection already exists at the build stage. devscripts/dev_loop/game_build.py:1720-1754 runs `python devscripts/packet_registry_codegen.py --check` as stage `codegen:packet_registry_codegen.py`, then fails the attempt with `codegen check failed: <script>` and re-verifies a validation fingerprint (game_build.py:85-126 keys on manifest revision + direct inputs/outputs). packet_registry_codegen.py --check prints the specific `stale <path>` / `missing <path>` (lines 429-444).
2. The user complaint still reproduces on 2026-11-27, hours before this review, across two different requesting agents:
   - events seq 22067: build-request-2f5299e2b097 (session map-authored-parity) status=failed 16:28:41.869Z, wait_reason "codegen check failed: packet_registry_codegen.py"; attempt game-dll-20261127T162836506030Z-8f226c error same, total 5.289s.
   - 16:30:26 request b141943a885e "retry after packet ledger refresh"; 16:31:22 request 5bc00473fa70 (session rocket-rl-settle) "Retry after packet_registry_codegen prebuild check failure; codegen rerun is clean"; 16:33:31 request 498a7be48a4d same.
   - events seq 22135: build-request-c5f7b1c86bd9 (map-authored-parity) failed 16:44:25.818Z with the identical wait_reason. Attempt game-dll-20261127T164418202582Z-35c5b8: stage codegen:packet_registry_codegen.py started 16:44:22.524, failed 16:44:25.225 exit_code=1; build.failed 16:44:25.658; total 7.509s; changed_count=2, changed_header_count=1, source mirror mismatch_count=0, manifest_validation passed.
   - 16:46:42 request 5668f6f41797 "packet ledger refreshed"; generation 798 published 17:01:30.
   Net: at least 4 build requests by 2 distinct agents re-failed on the same stale generated packet registry inside ~18 minutes, each requiring a manual codegen refresh and retry, and the same failure returned at 16:44 after the 16:30 refresh.
3. Attribution is incomplete. The build note/postmortem records only the requesting agent (`map-authored-parity`); the recorded error/wait_reason names the generator script, not the interfering agent or the specific stale output path (the `stale <path>` line is printed by codegen but not carried into build.failed/wait_reason). Source-manifest "owner" is module ownership ("game"/"resident"), not agent identity. No agent-level owner is resolvable from the build record.
4. No repeatable interference metric was found: `rg interference` over devscripts and docs/specs matches only this issue doc. 35 `.dev/agent-coord/reviews/conflict-*.md` files (2026-11-20..25) exist but are per-event artifacts, not a counter/gauge; stale-codegen failures appear only as transient build events.

INFERENCE / UNCERTAINTY
- Inferred (not proven): one or more other agents edited packet-registry inputs without regenerating, leaving the generated header/ledger stale; the exact owning agent is unknown because no attribution is recorded and the stale path is not in the recorded error.
- Unknown: whether the 16:30 "refresh" was incomplete or a later edit re-staled the artifact before 16:44; both fit the events.
- The behavior is intermittent, not constant: the 17:56:42-44 gen-800 attempt shows resident codegen cache hit and packet_registry --check completing exit 0 in 1.589s.

CONTRARY EVIDENCE
- Detection is explicit and stage-attributed (named stage + nonzero exit + fingerprint guard), not silent. Build notes now carry an `agent` field (note agent=map-authored-parity). At 16:44 the source mirror was consistent (mismatch_count=0) and manifest validation passed, so only the generated artifact was stale, not the source snapshot.
- The latest published generation (798, 17:01:30) and the in-flight gen-800 attempt are not blocked by this fault right now.

NEXT DISCRIMINATING CHECK (cold-pickup)
Run one bounded two-agent scenario: Agent A takes an edit lease on a packet-registry input (e.g. src/network/schema/packet-vehicle.h or src/network/packets.h) and saves a content change WITHOUT running codegen; Agent B requests an integration build through the root dev-loop. Record the exact build attempt/journal. Expected current behavior: the build fails at codegen:packet_registry_codegen.py and the record names only the script, not A or the stale path. Acceptance for this issue is that same scenario either auto-repairs deterministically from the pinned source snapshot or fails naming A's agent + stale path + message, and that the interference count/retry count is recorded. Do not mark this improvement done without that pinned two-agent artifact (no gameplay runtime_proven event applies); no fix is claimed here.
