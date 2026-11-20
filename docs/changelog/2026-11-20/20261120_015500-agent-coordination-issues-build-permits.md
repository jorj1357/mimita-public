// 2026-11-20T01:55:00Z
/* purpose
* record the shared-brain issue/activity/board and agent build/test permit work
* separate source/config, tooling tests, build status, and runtime evidence
* this file DOES NOT claim live gameplay or human acceptance
* this file DOES NOT claim candidate parallel builds or warm runtime completion
*/

# Agent coordination, issues, and build/test permits

## Task

- Task ID: session-20261120-agent-coordination-issues-build-permits
- Owner: coordination-v2
- Focus: shared agent awareness, append-only issues, self-improvement,
  build-friction latency, and per-agent build/test authority
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Status: source/config and focused tooling tests complete; live build/test
  permit and runtime path not yet exercised

## What changed

### Shared brain

- Added `issues`, `issue_tasks`, `issue_reviews`, and `activity` tables to
  `devscripts/agent_coord.py`; added task metadata for issue, priority,
  acceptance, evidence, and deferral.
- Added `issue add/list/show/triage/assign/defer/clarify/close/review/link-task`.
- Added `activity set` and `board`; `status` and `sync` include activity and
  open issues; `format_brief` prints a compact board summary.
- Issue detail documents are append-only under `docs/issues/<kind>/`; the
  original human quote is immutable and interpretations/evidence/reviews are
  appended. `docs/issues/README.md` is generated from the brain.
- Added issue intake and self-improvement workflows/skills.
- Added `devscripts/build_friction.py` to detect pending builds, cold changes,
  build causes, and slow phases and optionally file duplicate-checked
  `build-friction` issues.

### Build/test authority and latency specs

- `AGENTS.md`: agents may request builds and isolated test runs through the
  root dev-loop; direct compilers and second loops remain forbidden. One EXE
  build at a time; candidate DLL requests are bounded by PC resources.
- Added `docs/specs/dev-loop/change-to-evidence-latency.md` and
  `docs/specs/dev-loop/agent-self-improvement.md`.
- Updated `agent-coordination.md`, `agent-lanes-build-authority.md`,
  `build-time-observability.md`, `runtime-scenario-validation.md`,
  `build-and-exe.md`, the router, and `open-gameplay-issues.md`.
- Recorded that per-agent source isolation is deferred; one shared source tree
  remains current policy.
- Added measured per-class budgets/outliers instead of a zero-second promise.

### Dev-loop tooling

- Removed the CLI/daemon refusal of agent build requests. Requests now carry
  `MIMITA_AGENT_ID` in `requested_by`/`agent`; empty identity is still rejected.
- Added `--test-build-id` and `--test-game-generation` to pin an isolated run.
- `parallel_test_runs` can select a published build by ID and a pinned game-DLL
  generation/path; it verifies the executable hash and records both identities.
- A pinned EXE + pinned game-DLL run does not wait for an unrelated pending EXE
  build; the no-pin path keeps the existing current-generation checks.
- Updated the manual-build guard tests for the new agent authorization policy.

## Evidence

- `python -m unittest devscripts.quality.test_agent_coord`: 21 tests passed.
- `python -m unittest devscripts.quality.test_manual_build_guard`: 3 tests passed.
- `python -m py_compile` passed for changed Python tooling.
- CLI smoke: `issue add`, `activity set`, and `board` produced the expected
  append-only issue doc, generated index, and activity record.
- Two real issues were opened and triaged:
  - `issue-67f2be3d7012`: improvement for auto-intake, parallel candidate
    DLLs, and warm pinned tests.
  - `issue-346c68de182d`: build-friction for the currently pending root build.
- Source/config changes were not built as a game DLL or EXE by this session.
  The dev-loop already reported a pending build; no live test run was started.

## Not yet proven / remaining work

- The agent build/test permit and pinned run path are source-reviewed and
  unit-smoke-tested, not live-exercised against a published build.
- `build_friction.py` is available and created a real issue, but the automatic
  post-build hook is not wired yet; task `auto-build-friction`.
- Isolated parallel candidate game-DLL output is specified but not implemented;
  task `candidate-dll-pool`.
- Warm runtime plus fast event assertions are specified but not implemented;
  task `warm-runtime`.
- Existing `open-gameplay-issues.md` entries are pointed at the new system but
  not yet bulk-migrated into individual append-only issue docs.
- No runtime, deployment, or human acceptance is claimed.

## Build/runtime status

The root dev-loop had a pending game build while this work was completed. This
session did not invoke a compiler, `build.py`, a module worker, or a second
loop. The build-friction issue records the observed wait and is owned for
diagnosis.
