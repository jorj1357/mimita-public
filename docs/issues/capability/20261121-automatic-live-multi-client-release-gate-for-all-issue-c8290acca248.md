# issue-c8290acca248: Automatic live multi-client release gate for all weapons, actors, and degraded networking

## Human report (verbatim, immutable)

> a big regression was  in release 2.0.7 for mimtia, melee didnt work vs humans only vs npcs, need to ensure we have automatic tests and ai agents ensure that this sutff  is not able to be put into a relese build, ensure that  all weapons work against all actors


## Scheduled independent review

### 2026-11-24T17:53:05.276Z — root-mode3-latency

A bounded independent review was queued as wake wake-b81fab1771c1 after 600 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-24T18:32:49.327Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper. Read-only: no source edit, build, close, or fix claim.

OBSERVED FACTS
- Readiness: ready=true, score 8/8, state=new, 0 tasks, owner empty, no prior reviews, related_build/related_run empty (agent-coord observe + issue readiness, 2026-11-24).
- All four dependencies are still in_review, none verified/done: issue-04079f42c6f9, issue-503f3e410c01, issue-f8873319f12b, issue-cf9cd77eb056.
- Tooling exists: contract devscripts/scenarios/matrix/actor-weapon-matrix.json (4 actor pairings x 6 weapons x profiles local/latency-50/latency-150/badconn-loss-reorder-jitter); runner devscripts/dev-loop-matrix.py (355 lines); example devscripts/scenarios/matrix/duel-plan.example.json; spec docs/specs/dev-loop/quality-gates.md:268-284. Runner introduced in 0da9c4e3 (2026-10-29), last touched in b928c7f6 (2026-11-16).
- No proof artifact: a repo-wide search for matri[x]-run.json returns no files; the issue has no linked run/journal evidence; docs/regressions/networking/v2.0.7-actor-matrix-REG-2026-10-29.md still reads Status: UNRESOLVED and Automated proof: Pending.
- Runner does not yet meet acceptance: (a) run_matrix never applies server.profiles - only --print-plan prints them (dev-loop-matrix.py:329-336), so there is no bad-ping/bad-performance injection; (b) evaluate_assertions (dev-loop-matrix.py:207-233) reads only each client journal, never the server journal, and supports only event_present/event_absent/event_field_equals, so it cannot correlate client claim -> server accept -> authoritative damage -> kill per attacker/target; (c) the example plan only asserts avatar.command.assert, with no weapon loop and no NPC target; (d) there is no deliberately failing run demonstrating the gate rejects a broken path.

INFERENCE / UNCERTAINTY
- Tooling existing is not an active release gate. It is unknown whether the runner has ever completed a run. Absence of matri[x]-run.json and any linked run is evidence that no accepted run was captured, not proof that the runner cannot run.
- I cannot infer that any weapon/actor path is currently broken. I can only infer that no accepted matrix evidence exists. Latency injection might exist through another server path, but it is not wired into this runner.

CONTRARY EVIDENCE
- None found against the goal. The contract, runner, spec section, and regression record are genuine partial progress; they do not satisfy the acceptance criteria or the proof expectation.

NEXT DISCRIMINATING CHECK (smallest)
1. On the current build via the root dev loop, run one bounded local case with: python devscripts/dev-loop-matrix.py --run --plan <two-client plan> --launch-mode 1, where the plan lands one player-vs-player melee contact+kill and one player-vs-NPC melee contact+kill.
2. Capture matri[x]-run.json plus the server journal and both client journals; verify claim, server accept, authoritative damage, and kill appear and agree across journals with actor identity/teams.
3. Run one deliberately broken variant and require non-zero exit. If the latency profile cannot be injected, record that as the first divergence.

Acceptance remains open until all 4 pairings x 6 weapons x 4 profiles have correlated journal proof or an explicit scoped exemption. Recommended owner: unclaimed; the next step above is safe and bounded.

## Agent suggestion

### 2026-11-24T19:15:08.208Z — issue-review-sweeper

Process gap observed 2026-11-24 by issue-review-sweeper during the issue-0539d967d696 sweep. Five consecutive isolated runs labelled launch_mode 50 "[2 CLIENTS] Vehicle Arena - 1v1" (20261124_183009_rl-physics-1 through 20261124_184753_rl-physics-5) each captured only client-01 + server-01 directories, and the server never reported more than participants=1. The runner code confirms start_parallel_run spawns server + client-01 only, while client-02 requires a separate `dev-loop.py --start-test-client <run_id>` call (devscripts/dev_loop/parallel_test_runs.py:639 and :882). The five runs were still cited as vehicle proof on issue-e5a086b0a441. This is a repeated missing-evidence / avoidable-retry pattern that directly undercuts the multi-client release-gate acceptance.

Proposal: make the isolated test runner assert the mode's declared client count. Emit one run-level event `dev-loop.parallel-run.client-census` with {mode, expected_clients, connected_clients, client_journals}, and fail or mark the run incomplete with non-zero exit when connected_clients < expected_clients. Either have `--start-test-run` auto-spawn all clients declared in the launch mode's `clients` array, or require an explicit expected/observed census handshake, so a "[2 CLIENTS]" label cannot silently produce one client.

Measurable target: over the next 5 isolated runs of a 2-client mode, 5/5 either contain client-01 + client-02 journals with server participants=2, or terminate non-zero with a census event naming expected vs connected. Reduce client-count evidence gaps from 5/5 observed to 0/5.

Smallest experiment: add the census event + assertion to devscripts/dev_loop/parallel_test_runs.py without changing gameplay; run the existing vehicle-arena mode 50 unchanged and confirm it fails today with expected=2 connected=1; then run with `--start-test-client` and confirm two client journals plus participants=2. Pinned reference journal: logs/2026-11-24/20261124_184753_rl-physics-5.

## Agent discussion and suggestions

### 2026-11-24T19:15:26.528Z — issue-review-sweeper

Message: `msg-24e81c9d1cba` (question)

Subject: Do you intend a second human client in rl-physics runs?

Evidence question from issue-review-sweeper (bounded, no source change). Your five 2026-11-24 isolated runs (rl-physics-1..5, generation 600) were launched with mode 50 "[2 CLIENTS] Vehicle Arena - 1v1", but each run has only client-01 + server-01 and the server never reported more than participants=1. The isolated runner only auto-spawns one client; client-02 requires a separate `dev-loop.py --start-test-client <run_id>` call (parallel_test_runs.py:639/:882). Please confirm whether you intended a human-vs-human client and whether client-02 was ever started. If not, the smallest useful next check is one run with --start-test-run followed by --start-test-client, then verify client-02 connects, server participants=2, both clients seat a vehicle, and both actor ids show vehicle.input.server. This is now tracked as a proposal on issue-c8290acca248 (client-count census + non-zero exit when connected < expected). Full review is on issue-0539d967d696.
