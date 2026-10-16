# 2026-10-16 weapon scenario and logging specifications

- Canonical time: 2026-10-16T22:59:40Z
- Display time: 2026-10-16 18:59:40 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD: `3a34520009d91d10a571dfd08ca3ee90c9c56639`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Make the weapon respawn/equip task and scenario process sufficiently explicit
for an implementation agent; determine what scenario automation already
exists; specify one-command launch/join, assertions, live inspection and
steering, reusable networking scenarios, JSON-first component weapons, and
whether the log-storage limit is resolved. No runtime code was changed.

## Pre-existing repository state

The relevant specs were already modified in this checkout, and
`docs/specs/open-gameplay-issues-2026-10-16.md` was an existing untracked
working file. Prior content was preserved. The router-referenced
`docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` are
absent. Source files were inspected but not edited.

## Specification changes

- `docs/specs/dev-loop/dev-loop.md:653` — corrected the scenario feature status.
  Before: “the scenario queue/file and assertion runner are the next slice.”
  After: `--scenario-file` already queues setup/steps into a ready client,
  while the client marks `scenario.finished` with
  `assertions_evaluated=false`; it does not return a verdict.
- `docs/specs/dev-loop/dev-loop.md:693` — specified a versioned end-to-end
  scenario schema and lifecycle: ensure one daemon, launch/reuse a matching
  run, wait for server room registration and client/map readiness, run ordered
  command/event/state/snapshot/assert steps with bounded waits, and return a
  correlated pass/fail record.
- `docs/specs/dev-loop/dev-loop.md:826` — specified live inspection and
  intervention correlation. Current queueing does not yet serialize
  interventions at step boundaries or mark an intervened scenario.
- `docs/specs/networking/networking.md:445` — made respawn behavior
  unambiguous. Before: the spawn policy could preserve the old selection “or
  publish[] the new starting selection.” After: preserve/reapply the previous
  weapon if still owned and allowed; only select the documented default if it
  is no longer valid, and replicate selection before firing.
- `docs/specs/networking/networking.md:500` — added the full weapon identity
  regression scenario: launch/join readiness, distinct hotbar/native positions,
  deterministic NPC setup, equip/fire/death/respawn/fire without manual equip,
  repeat across lives and sets, and verify invalid identity rejection. Added
  the reusable networking scenario catalog criteria at line 561.
- `docs/specs/weapons/weapons.md:194` — specifies JSON-defined item/tool
  definitions composed from reusable behavior components, stable definition
  and instance IDs, hotbar positions as presentation mappings, server-owned
  equipped identity, parity-checked migration, and acceptance cases.
- `docs/specs/weapons/weapons.md:261` — specifies JSON-only addition for
  existing component capabilities, validated atomic hot reload, shared server
  revision/hash, stable behavior for in-flight actions, and asset-loader
  ownership. New mechanics still need one shared component implementation.
- `docs/specs/weapons/weapons.md:289` — records the current mixed architecture.
  Before: JSON modified C++-created definitions, without tracing slots and
  attacks together. After: the spec names built-in constructor registration,
  set-list order as logical hotbar position, per-definition native slots,
  translation owners, and attack validation by network definition ID followed
  by native-slot equipped-state comparison.
- `docs/specs/debug-logging/debug-logging.md:14` — added current source and
  disk evidence. Before: source caps were described, while runtime enforcement
  remained unverified. After: source guards and pruning exist, but the
  inspected journal is 100,004,632 bytes (4,632 bytes above the per-file
  limit), and quota enforcement remains unproven under live concurrent writes.
  At 2026-10-16T22:59:28Z, `logs/` contained 78 files totaling 349,784,205
  bytes; this snapshot was below the 1 GB tree ceiling.
- `docs/specs/open-gameplay-issues-2026-10-16.md:79` — linked priority 2 to
  the executable scenario spec and records that current runner limitations
  prevent the scenario from producing an automated verdict yet.

## Implementation facts checked

- `devscripts/dev-loop.py::_scenario_json()` parses repository-local JSON or
  fenced JSON; `queue_agent_scenario()` queues `setup` and `steps` commands
  only into an already-ready client. `--scenario-file` does not currently
  launch/join a session or evaluate assertions.
- `src/main.cpp` emits `scenario.step` and final `scenario.finished` for queued
  steps, with `assertions_evaluated=false`. The control call returns a queue
  acknowledgement, not the final scenario verdict.
- `src/main-systems.cpp::registerAgentCommands()` provides movement, aim,
  action, shoot, and snapshot adapters. `npc_spawn` exists, but a deterministic
  NPC kill/attack scenario still needs an explicit test profile/control path.
- `src/debug/structured-log.cpp` defines 100,000,000-byte file and
  1,000,000,000-byte tree ceilings, named cross-process coordination, pre-write
  checks, pruning, and dropped-record accounting. It does not rotate an
  oversized active JSONL file into segments. Direct/category diagnostic sinks
  remain in the logging inventory.
- The current workspace snapshot is under the tree limit, but contains a file
  above the file limit. Therefore log storage is **not confirmed resolved**.
  No live quota/concurrent-writer validation was performed this session.

## Validation and remaining work

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/documentation-checker-v1.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/workflows/runtime-scenario-validation.md`, and
`docs/operations/task-completion/task-completion.md`. The router-required
review and regression docs are missing. `git diff --check` found only a
pre-existing trailing-whitespace line in the user's networking clarification
at line 3482; it was preserved. No build, test, or gameplay runtime was run.

A next agent can begin the weapon fix as a trace-first implementation task,
but must first implement/record missing per-life identity diagnostics and run
this scenario through the real dev-loop. The scenario runner itself still needs
cold-start lifecycle orchestration, event/state waits, assertions, timeout and
intervention results, and a final verdict. Log storage requires concurrent
runtime/quota-boundary validation and full writer inventory before declaring
resolved.
