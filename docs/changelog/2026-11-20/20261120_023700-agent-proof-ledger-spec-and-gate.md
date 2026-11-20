// 2026-11-20T02:37:00Z
/* purpose
* record the proof ledger specification, completion gate, and issue exemption
* separate source/tooling tests from live runtime proof
* this file DOES NOT claim live gameplay acceptance
* this file DOES NOT claim candidate builds or warm runtime are implemented
*/

# Agent proof ledger: spec and completion gate

## Task

- Task ID: session-20261120-agent-proof-ledger-spec-and-gate
- Owner: proof-system
- Focus: readable proof specification; runtime-proof completion gate;
  explicit exemption with reason; unproven/stale visibility
- Status: spec and tooling tests complete; live game proof not run

## What changed

- Added `docs/specs/dev-loop/agent-proof-and-verification.md`. It defines the
  proof ledger, states, completion gate, evidence classes, build/test
  capability, feature flags, build-friction loop, metrics, boundaries, and
  acceptance.
- Linked the spec from `AGENTS.md`, `ROUTER.md`, and
  `agent-coordination.md`.
- Aligned the spec and workflow with the existing task proof and issue evidence
  implementation:
  - `proof task-add` / `task-verify` for task runtime proof;
  - `proof record` / `verify` for issue integration/unit evidence;
  - `proof status` for stale/proven counts;
  - `proof exempt` for reasoned task exemptions.
- Added proof visibility: task exemptions create exempt proof records,
  `proof status` reports proven/stale/exempt, and the board/status summary shows
  unproven/stale proof rows.
- Added issue-level proof exemption:
  - `issue_exempt` with a required reason and optional paths;
  - durable `proof_exempt_reason` / `proof_exempt_paths` issue columns;
  - `issue close` accepts proven evidence or a reasoned issue exemption;
  - `agent-coord issue exempt` command.
- Updated the issue-intake workflow to record task proof and issue evidence
  before closing.
- Removed a duplicate helper experiment and kept the proof logic in the brain.

## Evidence

- `python -m unittest devscripts.quality.test_agent_coord`: 28 tests passed,
  including:
  - task completion blocked without proof;
  - verified runtime proof unblocks completion;
  - source change marks proof stale;
  - exemption requires a reason;
  - issue integration proof requirements;
  - issue exemption unblocks close.
- `python -m py_compile` passed for changed Python.
- `git diff --check` passed for changed files.
- No live game run, build, deployment, or human acceptance is claimed.

## Remaining work

The existing improvement issue `issue-67f2be3d7012` still tracks:

- automatic build-friction intake;
- isolated parallel candidate game-DLL builds;
- warm runtime and fast event assertions;
- live pinned proof of the agent build/test path;
- OGI migration.

The proof gate is tooling-complete but has not been exercised against a real
pinned game run in this session.

## Build/runtime state

No compiler, `build.py`, module worker, or second dev-loop was invoked. The root
dev-loop has its own active/pending build state; this work is specification and
coordination tooling only.
