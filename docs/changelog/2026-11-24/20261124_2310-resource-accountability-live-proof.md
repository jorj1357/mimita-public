# Live resource-accountability evidence for the four MiMITA processes

- Task ID: `hot-reload-latency-root`
- Session ID: `session-0b9a7744c23d`
- Owner: `hot-reload-latency-root`
- Focus: prove why each running MiMITA process exists; route the high-priority resource-mindfulness context
- Wake: `wake-08063b85f5b7`, context `ctx-4eda88d4210c`
- Summary: Acknowledged and routed the user's resource-consumption context, then recorded a live, read-only inventory that identifies the four running `mimita.exe` processes as two runtimes and shows two real accountability defects: an isolated test run with no recorded owning agent, and a status surface that reports a dead run as online. No source code, config, build, or runtime process was changed.
- Status: `PASS_WITH_HUMAN_REVIEW`
- Date, time, timezone: 2026-11-24T23:10Z, UTC
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `eb92b02b21e62fd5ec496eee4dcad832f901bb7a`
- Final commit: `eb92b02b21e62fd5ec496eee4dcad832f901bb7a` (unchanged)

# Pre-existing changes

- The shared checkout was already dirty before this session. Tracked and untracked pre-existing edits included `config/analytics.json`, `docs/issues/README.md`, several 2026-11-24 issue documents, `docs/changelog/2026-11-24/20261124_222529-bloodtest-resource-issue-intake.md`, `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md`, `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md`, and others. This session did not edit those.
- Active runtime processes pre-existed this session and were left running. The unowned isolated pair is deliberately not stopped.

# Requested behavior

User (verbatim): "we also need to mkae it so ai agents overall are mindful of resource consumtoion of teh cmoptuer and alaso everthing should prove why its running , mimita like rightnow thereis 4 procesess running, 3 , but nothing in task bar, just  eating cpu int ehbackground, not sure  why , still i dont know wht to dobaotu cpu usage, i feel the goal pursuing i didnt do it acurate, i wanted to see what is  mkaing hte cpu usage used so much but  we didnt really figure it out i think"

A follow-up in the same thread asked for a passive investigation of multi-agent build memory pressure, with the target that MiMITA and AI processes have functionally negligible PC impact.

# Specification alignment

- Paths: `docs/specs/dev-loop/agent-runtime-registry.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-context-contract.md`, `docs/specs/performance/performance.md`, `AGENTS.md` resource-evidence handoff.
- Requirement: every runtime must be identifiable, link runtime → issue → build → DLL → journal → proof, and a runtime whose identity cannot be verified is reported `NEEDS_USER_ACTION`; never stop a process that was not verified as owned.
- Requirement: append resource-pressure evidence with UTC/sample duration, logical CPU count, per-PID CPU normalized per logical CPU and whole host, memory/paging state, live mode, workload, and artifact paths, labelling direct observations vs inferences vs unknowns.
- Compliance: no process was stopped; the unattributable pair is explicitly labelled unknown/NEEDS_USER_ACTION; observations and inference are separated.

# Exact implementation changes

No code, config, build, or runtime change.

## Coordination records

- Acknowledged `ctx-4eda88d4210c` (already linked as primary to `issue-b9edd402e658`).
- Linked duplicate-delivery context `ctx-83f9211aa3bf` as `related` to `issue-ca5eb16d9e21` with a reason, to prevent duplicate intake of the same request already linked via `ctx-275d9f0c136a`.
- Appended one evidence entry to `issue-b9edd402e658` (automatic host/process resource snapshot) with the live four-process attribution and the two status defects.
- Appended one evidence entry to `issue-ca5eb16d9e21` (multi-agent memory pressure) with the host memory sample and the MiMITA role split.
- Sent one issue-linked question to `all` on `issue-b9edd402e658` asking who owns run `20261124_224314` and whether the proposed smallest next check is agreed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md` | existing | one appended evidence suggestion | record live proof and defects for the snapshot acceptance | append a correction if superseded; never delete append-only history |
| `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md` | existing | one appended evidence suggestion | record measured host pressure and MiMITA role split | append a correction if superseded |
| `docs/changelog/2026-11-24/20261124_2310-resource-accountability-live-proof.md` | absent | this session record | required final changelog | generated audit record; preserve |

# Contract / ABI changes

- None.

# State / schema changes

- Coordination context link added (duplicate delivery) and issue discussion entries appended. No schema migration.

# Deleted paths

- None.

# Diagnostics

- Used existing surfaces only: `dev-loop.py --status`, `dev-loop.py --list-test-runs`, `agent-runtime.py list`, run manifests, `Win32_Process` command lines, `OpenProcess` liveness checks, `Win32_OperatingSystem` memory, and `Get-Process` CPU-time deltas.
- No new diagnostics code and no new test harness were added.

# Validation

- Read-only inspection; no tests, no build, no launch, no stop.
- Root dev-loop status at 22:59Z: lifecycle `game_build_succeeded`, run `20261124_221014` RUNNING, server pid 27372, client pids [26492], game generation 627 published, EXE artifact stale for current source.

# Measured evidence

Direct observations (12 logical CPUs, 16,025MB physical):
- 2026-11-24T22:48:12Z, 10s CPU-time deltas across four `mimita.exe` processes, combined 41.04% whole-host:
  - PID 27372 server + PID 26492 client = root dev-loop run `20261124_221014` (`.dev/current/mimita.exe`, funcounterstrike, dust2cyberiav4, 19 NPC/1 player). Server 12.03% host / 144.4% one core; client 17.12% host / 205.5% one core.
  - PID 36224 server + PID 26536 client = isolated `--start-test-run` run `20261124_224314` (`.dev/builds/2026-11-24T19-27-02.742428Z/mimita.exe`, Bloodtest/slaughterhouse1, 1 NPC). Server 3.79% host; client 8.10% host.
- Attribution defect: run `20261124_224314` has `test_run_kind=parallel_agent_test` but empty `build_identity.agent`, no top-level `agent`/`requested_agent`, no coordination `activity` row claiming it, and `runtime_registry` has 0 rows (`agent-runtime.py list` empty). The pair is genuinely running but unowned.
- Stale inventory: `--list-test-runs` reports run `20261124_212828` status `online` with PIDs 8940/25432; `OpenProcess` fails for both, so the run is dead.
- `--status` omits PIDs 36224/26536, which are exactly the extra processes behind the user's report.
- 2026-11-24T23:05:49Z host: free physical 831MB of 16,025MB (~5.2%); commit 36,825/52,536MB; `Memory Compression` working set 3,003MB. MiMITA private memory combined ~2.1GB; resident peers included 3× opencode, opencode-cli, Code, codex, ChatGPT, pythonw (72,708 CPU-seconds), cpptools.

Inference / unknown: MiMITA is a material but not dominant resident-memory consumer here; the machine is over-subscribed by the simultaneous agent/editor set. CPU figures are point samples and do not prove causation for build or frame latency.

# Regression review

- Regression entry appended: no.
- Why not: these are pre-existing accountability defects observed read-only, not newly introduced behavior changes; they are recorded in the linked issues with proposed fixes pending owner agreement.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Still unverified: whether the unowned Bloodtest pair was needed, why its agent was not recorded, and whether `--list-test-runs` "online" is stale for other runs.

# Agent handoff context

- Owner: follow-up is unowned; `issue-b9edd402e658` task `auto-resource-context-on-sync` is still unassigned and claimable.
- Next exact step (needs agreement): require or loudly warn on empty `--start-test-run` agent and register the run in `runtime_registry`; make `status`/`list` merge run manifests with a live-PID check so stale `online` rows become stopped/needs-action.
- What another agent should not redo: do not repeat this four-PID inventory without a new discriminating sample; do not stop run `20261124_224314` or PIDs 36224/26536 without verifying ownership and active use.

# Automatic evidence

- Branch/base/final: `checkpoint/one-checkout-condensation-20261111` / `eb92b02b21e62fd5ec496eee4dcad832f901bb7a` / unchanged.
- Build identity: EXE `2026-11-24T19:27:02.742428Z:7ba5fb9128425994c53d5e809f6615d709c9d64d63f27158de0093de72c2ad63`; game generation 627 published.
- Journals: `logs/2026-11-24/20261124_221014/`, `logs/2026-11-24/20261124_224314/`, `logs/2026-11-24/20261124_212828/`.
- Session ID: `session-0b9a7744c23d`.

# How to reverse

- No code or config changed. Use coordination context link/correction tooling with a reason rather than deleting append-only issue history.

# Related feature record

- None.
