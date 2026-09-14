---
timestamp_est: 2026-09-14 19:17:39 -04:00
branch: 8292026stash
status: PASS_WITH_HUMAN_REVIEW
scope: investigation-only whole-repository architecture audit
---

# Whole-repository architecture audit

## Final state

No implementation source, configuration, or existing documentation was changed
for this audit. The only file created by this session is this changelog. The
worktree already contained unrelated/pre-existing edits in the hot-runtime,
ECS, combat, networking, and documentation areas; those changes were preserved.

## Governing documents and skills read

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/features/live-code-development/live-code-development.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/live-development/hot-kernel.md`
- `docs/architecture/live-development/hot-cold-audit.md`
- `docs/architecture/live-development/hot-kernel-next-steps.md`
- `docs/architecture/ecs-entity-etc/ecs.md`
- `docs/architecture/ecs-entity-etc/ecs-migration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`

The focused specification-review result is an audit finding set, not an
implementation approval. Logging review was applied to the evidence boundary:
the existing JSONL live journal is useful for local generation transitions, but
does not prove the full two-client protocol or visual acceptance.

## Evidence inspected

- Hot loading, staging, activation, rollback, and cold-boundary detection:
  `src/hot-reload/hot-reload-system.cpp:129-226,379-558,657-699`.
- Generic registration and activation: `src/hot-reload/generic-runtime.cpp:27-319`.
- Stable ABI and remaining fixed fields: `src/hot-reload/game-api.h:23-192,698-746,1286-1463`.
- Generic dynamic storage and schema migration: `src/ecs/dynamic-components.cpp`.
- Generic relationship storage: `src/ecs/relationship-store.cpp`.
- Generic component/relationship network envelope and sender/apply path:
  `src/network/dynamic-replication.cpp:82-328,328-505`.
- Remaining authoritative projectile owner: `src/network/server-projectiles.cpp:1743`.
- Remaining gamemode owner and phase/objective path:
  `src/network/server-gamemode.cpp:1988-2184`.
- Client packet, interpolation, and reconciliation owners:
  `src/network/multiplayer-tick.cpp:697-1572` and
  `src/network/multiplayer-interpolation.cpp:985`.
- Static command registration/discovery: `src/devtools/terminal.cpp:42-258,502-565`
  and `src/devtools/terminal-builtins.cpp:11-82`.
- Concrete rendering ownership: `src/render/render-player.cpp:30-249` and
  related `src/render/*` / `src/renderer/*` traversal.
- Source watcher and dependency primitives: `src/project/project-watcher.cpp`
  and `src/project/dependency-graph.cpp`.

## Claims and limits

The audit distinguishes source evidence, self-test/documentation claims, and
runtime claims. No live multiplayer, visual, deployment, or human acceptance
was performed. A build was not run because this was investigation-only and the
worktree contains pre-existing changes. Therefore no build or runtime claim is
made here.

## Recommended first migration

Migrate the remaining normal authoritative projectile path completely: move the
last NPC/unmigrated producers and compatibility policy behind the already-proven
generic projectile entity/system path, then prove a real two-client rocket and
grenade trace before deleting the legacy container/packet branch. This removes a
real gameplay cold owner while preserving the transport and collision kernel.

