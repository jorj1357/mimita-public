# Development Quality Gates

Time created: `2026-10-29T17:13:46Z`

Status: TARGET CONTRACT

## Purpose

Define the automated checks MiMITA keeps in `devscripts/` so the project can
find unused code, duplicate logic, forbidden ownership paths, and untested
actor/weapon/network combinations over time.

These checks do not replace runtime scenarios. They narrow the search before a
real executable proves behavior.

## Spec-first rule

For behavior changes:

1. update the relevant specification first
2. update the authoritative function map
3. implement code to match the specification
4. prove it with the runtime scenario matrix
5. record the result in the changelog and, when applicable, the regression
   record

The specification is the desired behavior. Code is wrong when it disagrees until
a human changes the specification.

## Required repository tooling

All tooling lives under `devscripts/` and is versioned with the code.

### `devscripts/quality/lint.py`

Orchestrates the quality checks.

Modes:

- `--changed`: checks only files changed against a base revision
- `--all`: reports the whole tree
- `--gate`: fails on configured blocking findings
- `--baseline`: writes/compares the accepted baseline

Initially `--all` and `--gate` are report-only for the existing tree.

### `devscripts/quality/check_damage_authority.py`

Fails when combat damage bypasses `applyActorDamage`.

Forbidden outside the allowlist:

```text
npc.health -=
npc.health =
player.health -=
player.health =
knockbackImpulse +=
```

Allowlist:

- `src/network/server-damage.cpp`
- actor spawn/reset/death lifecycle
- tests
- explicitly documented gamemode scripted damage during migration

### `devscripts/quality/check_contact_ownership.py`

Detects new parallel contact/damage paths:

- duplicate melee/hitscan/projectile damage formulas
- weapon-ID branches in shared contact code
- new direct packet handlers for existing contact behavior
- a second damage application owner

This is a source-pattern check, not a proof of correctness. It exists to force
an explicit review when a new path appears.

### `devscripts/quality/duplicate_report.py`

Runs a duplicate-code detector when available (`jscpd`, PMD CPD, or an
equivalent local tool) over:

- `src/combat`
- `src/network`
- `src/physics/movement`

The report is advisory. Duplicate geometry helpers may be acceptable; duplicate
damage/validation owners are not.

### `devscripts/quality/dead_code_report.py`

Combines:

- `cppcheck --enable=unusedFunction`
- compiler/linker warnings
- linker map or coverage data when available

Output is a candidate list. External linkage and callback-driven code require
caller proof before deletion.

### `devscripts/quality/coverage_report.py`

Reads coverage from real scenario runs and lists never-executed functions in
combat and network owners.

Coverage is evidence, not permission to delete. A function may be reached only
through a mode or configuration not present in the current scenario set.

## Tool configuration

The repository keeps preset configs for:

- `.clang-format`
- `.clang-tidy`
- `cppcheck-suppressions.txt`
- duplicate-detector config

C++ checks focus first on:

- `bugprone-*`
- `performance-*`
- `readability-*`
- `modernize-*`
- `misc-unused-*`

Formatting and style checks do not prove behavior. They exist to keep diffs
small and reviewable.

## Multi-client scenario matrix

The dev-loop must support one server and N clients, each with:

- a unique name/player ID
- a unique command queue
- a unique client journal
- its own scenario or role
- its own assertions
- an optional bad-connection profile

The runner must:

1. start the one server
2. start all clients
3. wait for every client to be ready
4. synchronize scenario start
5. collect each client's result and the server result
6. keep journals separate and linked by run/session IDs

A client journal records what that client observed plus the server events it
received. The server writes its own journal. They are never silently merged.

## Required actor matrix

Every weapon family is tested against every actor pairing:

| Attacker | Target |
|---|---|
| NPC | NPC |
| NPC | Player |
| Player | NPC |
| Player | Player |

Weapon families:

- mace
- swordsword
- chainsaw
- hitscan
- rocket/projectile
- deathball

Network profiles are RTT-based:

- local
- RTT below 100 ms
- RTT 100 ms or more
- packet loss/reorder/jitter preset

Accuracy targets:

- RTT below 100 ms: 99.99% claim acceptance and exact damage match
- RTT 100 ms or more: 99% claim acceptance and exact damage match

Assertions:

- exactly one damage result per target/tick
- no duplicate damage or death
- friendly-fire policy respected
- no wall penetration
- client prediction and server confirmation converge
- contact and damage events carry attacker/victim identity and teams
- each client journal proves what that client saw

## Dev-loop module split

`devscripts/dev-loop.py` remains the only main entry point. The implementation
is split into a package so the runner is readable and testable:

```text
devscripts/dev-loop.py
devscripts/dev_loop/
  cli.py
  build.py
  launch.py
  control.py
  status.py
  state.py
  scenario.py
  matrix.py
  runtime.py
```

The entry command, flags, and scenario behavior remain compatible. The package
split is a refactor, not a second launcher.

## Baseline and rollout

1. Add all tooling in report-only mode.
2. Record the baseline.
3. Delete confirmed inert code.
4. Make changed-file checks blocking.
5. Make the full gate blocking after the legacy baseline is clean.
6. Never add a new special case to satisfy a gate; fix the owner.

## Relationship to regression records

A quality gate exists because a previous class of failure was found. The
regression record explains the failure and names the gate that prevents it from
returning.

Do not use the quality-gate document as a changelog. It defines standing rules;
changelogs record work, and regression files record confirmed failures.
