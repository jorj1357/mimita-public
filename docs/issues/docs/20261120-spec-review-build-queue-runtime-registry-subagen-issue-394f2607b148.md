# issue-394f2607b148: Spec review: build queue, runtime registry, subagents, self-improvement

## Human report (verbatim, immutable)

> 


## Scheduled independent review

### 2026-11-21T23:05:21.374Z — cpp-latency-pressure-opt

A bounded independent review was queued as wake wake-af48c2657e0b after 192 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed - spec/implementation divergence found; runtime adoption unproven)

### 2026-11-21T23:08:42.896Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Review (reviewed - spec/implementation divergence found; runtime adoption unproven)

Reviewer: issue-review-sweeper (wake wake-af48c2657e0b). Read-only: no source edit, build, test, closure, or ownership change claimed.

Observed (commands run 2026-11-21T23:1xZ):
- issue readiness issue-394f2607b148 -> ready=false, score 0/8; missing human_quote, reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk; state=new, 0 tasks. Created 2026-11-20T02:48:38Z (~44h old). paths=[] even though the summary names four spec files.
- proof status has no record for this issue; proof_exempt_paths=[] and proof_exempt_reason empty, so no exemption is on file. The only artifact is the scheduler note at 2026-11-21T23:05:21Z (coord events seq 4630/4631), which explicitly says the review is pending and is not evidence of a fix.
- Spec/CLI divergence: agent-self-improvement.md lines 164-184 label agent-coord retro <issue> mandatory after closes/waits/proof failures, but agent-coord retro is rejected as an invalid choice and no retro command exists in devscripts/agent_coord.py.
- Spec/CLI divergence: agent-runtime-registry.md lines 140-150 document agent-coord runtime list|show|start|stop|health; actual agent-coord runtime = register|list|update, while start|stop|status|list live in the separate devscripts/agent-runtime.py. One documented command surface is split across two owners, and runtime list is currently [].
- Build-queue adoption: agent-coord build request implements integration|preview, --base-generation, --overlay, --cohort. build list holds only 5 requests total: 2 published (2026-11-20, generations 399/400), 2 rejected, 1 proposed since 2026-11-20T07:46:58Z; zero preview, zero cohort, nothing since 2026-11-21T21:32Z. The current generation-436 build was requested by the dev-loop auto path (request_ids=[auto-17]), not the agent build queue. build/game/cohorts/ does not exist; only build/game/agents/launch-latest-stable-fix/current.json exists.
- Subagent budget: agent-subagents.md lines 118-130 say the system refuses new subagents/builds when budget is exhausted; the agents table stores parent/cohort/capabilities/budget (agent_coord.py:589-592), but grep found no enforcement path and no subagent/budget CLI command.
- Build timing context: latest journal game-dll-20261121T222152398558Z-ad2cd0.jsonl shows build.started 22:21:52, manifest_snapshot 409.4s, source_snapshot 111.6s; current.json reports total_seconds=886.724 with compiled=14. The brain also holds many Wait over budget: game-dll-build at game_build build-friction issues. Relevant to the build-queue spec waiting-is-the-enemy and observability acceptance.

Inference/uncertainty:
- The four specs read as target/aspirational documents. Some sections are implemented (build request fields, an agent pointer with hash/revision, agent parent/cohort/budget columns, runtime start flags), while some documented surfaces are stale or split. Intent is unproven: stale docs vs. deliberately staged specs.
- There is no human quote or acceptance for this issue, so no agent can determine review complete. The only safe verdict is that review has begun and the issue is not cold-handoff ready.
- These are policy docs; correct proof is documentation consistency plus adoption, not a gameplay journal. No runtime behavior is claimed here.

Contrary evidence:
- agent-runtime.py start accepts --game-generation, --dll, --cohort, --issue, --pointer, so the build-queue runtime-selection syntax is partly realized, just not under the documented agent-coord runtime names.
- A valid per-agent pointer exists (build/game/agents/launch-latest-stable-fix/current.json, generation 400, dll_sha256, snapshot_revision, kind=integration), so per-agent pointers are not purely aspirational.
- The data model already carries subagent fields, and the docs issue already contains the scheduler review note, so neither artifact is empty.

Next discriminating check (owner coordination-v2 / human):
1) Declare whether each of the four specs is normative-now or target-only, then add per-spec acceptance and paths so this issue is cold-handoff ready.
2) Read-only conformance check: extract each agent-coord <group> <verb> / devscripts/*.py <verb> invocation and each build/game/... path from the four specs, resolve with --help / Test-Path, and list mismatches. Expected at least: retro; runtime show|start|stop|health; build/game/cohorts/.
3) If the queue/pointer path is meant to be active, higher-value proof: one real preview request, one cohort pointer, and one agent-runtime.py --pointer run, then inspect that exact run journal; otherwise mark those spec sections proposed, not implemented.

No fix, closure, or ownership change claimed.
