// 09 03 2026, 15 40
/* purpose
* route AI agents and humans to the correct authoritative project documents
* explain the document order before code is inspected or changed
* prevent agents from guessing when the repository already has written guidance
* this file DOES NOT contain the full game specifications
* this file DOES NOT replace the documents it routes
* this file DOES NOT make archived material authoritative
*/

# MiMITA Documentation Router

This is the map for the project documentation.

## Document hierarchy

- `AGENTS.md`: project introduction and permanent working rules.
- `docs/ROUTER.md`: this map and the required reading order.
- `docs/specs/`: desired behavior; the main source of truth for what the game should do.
- `docs/architecture/`: ownership and structural rules.
- `docs/architecture/time-and-formatting/time-and-formatting.md`: universal
  UTC, ISO 8601, folder, and generated filename rules.
- `docs/workflows/`: methods for common kinds of work.
- `docs/workflows/runtime-scenario-validation.md`: required real-executable,
  live-`events.jsonl` workflow for behavior investigations and changes.
- `docs/operations/`: build, deployment, assets, and completion procedures.
- `docs/skills/`: focused review checklists used for the task.
- `docs/regressions/`: confirmed failures and their permanent lessons;
  `docs/regressions/<domain>/README.md` is the domain index; flat files in that folder own detail.
- `docs/issues/`: append-only user reports, TODO surfacing, build-friction, and
  improvement/capability proposals; `README.md` is generated from the shared
  coordination brain.
- `docs/changelog/`: one final record for every AI work session.
- `docs/features/`: stable feature records linking goals, owners, tests, logs,
  changelogs, regressions, and human acceptance.
- `docs/gold/`: examples of complete evidence and changelog quality.

The specification describes the desired behavior. If code disagrees with it,
the code is wrong unless a human changes the specification first. Quote the
specification, explain the disagreement, and align the implementation as far as
the task safely allows.

## Every task

1. Read `AGENTS.md`.
2. Read this router.
3. Classify the task.
4. Read the relevant specification first.
5. Read related architecture and workflow documents.
6. Read the required focused skills, including
   `docs/skills/spec-behavior-review-v1.md` for behavior changes or spec review.
7. Inspect the current code and pre-existing changes.
8. Compare the implementation with the specification.
9. Make the smallest correct change.
10. Run focused validation and build when required.
11. Write one changelog file immediately before completion.
12. Report evidence separately from human review still needed.

## Route priority

When several routes apply, check the dependency most likely to break other
systems first. Use this default order:

1. documentation and specification
2. logging and diagnostics
3. performance
4. terminal commands
5. in-game chat
6. moderation
7. assets

## Common routes

| Task type | Read first |
|---|---|
| Documentation | the relevant docs folder, `docs/skills/documentation-checker-v1.md` |
| Current product priority, feature order, or roadmap | `ROADMAP.md` |
| Time, dates, filenames, or generated artifacts | `docs/architecture/time-and-formatting/time-and-formatting.md` |
| In-game UI, HUD, menus, text | `docs/specs/gui/guiv2.md`, `docs/skills/documentation-checker-v1.md` |
| Vehicles, skateboards, BMX, vehicle controls, and the vehicle arena | `docs/specs/vehicles/vehicles.md`, `docs/specs/vehicles/vehicles-skateboard.md`, `docs/specs/vehicles/vehicles-bmx.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/vehicle-arena.md` |
| Replay, in-game video creation, or distribution | `docs/specs/replay/replay.md`, `docs/specs/creator-tools/video-editor.md`, `docs/specs/distribution/distribution.md`, `docs/specs/performance/performance.md` |
| Generated character speech or voice chat | `docs/specs/live-voice-generation/live-voice-generation.md`, `docs/specs/ingame-chat/voice-chat.md`, `docs/specs/moderation/moderation.md` |
| Damage, weapons, knockback | `docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md`, `docs/specs/networking/contact-authority.md` |
| Destructible world, map cuts, physical fragments, map reset | `docs/specs/destructible-world/destructible-world.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/networking/networking.md` |
| Movement, physics, collision | `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md` |
| NPC behavior, combat profiles, or authored NPC movement | `docs/features/npc-behavior.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md` |
| Creating monster avatars, limb collision, attacks, sounds, or shareable game-content packages | `docs/specs/creators/monster-authoring.md`, `docs/specs/animations/animations.md`, `docs/specs/avatar/avatar-editor.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/distribution/distribution.md` |
| Actor presets, `roles.json`, actor loadouts, or mode team rosters | `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md` |
| Creator world projects, project versions/forks, distributed content, world-scoped saves | `docs/specs/world-projects/world-projects.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/networking/networking.md` |
| Creator entry flow, My Creations, local project launch, Community/Server browser UI | `docs/specs/creators/monster-authoring.md`, `docs/specs/gui/guiv2.md`, `docs/specs/world-projects/world-projects.md`, `docs/specs/distribution/distribution.md` |
| Infinite RPG world authoring or connected procedural chunks | `docs/specs/gamemodes/rpg.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/procedural-infinite-world/procedural-infinite-world.md`, `docs/specs/networking/networking.md` |
| Team Deathmatch rules | `docs/specs/gamemodes/teamdeathmatch.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/networking/networking.md` |
| Fun Counter Strike, respawning bomb rounds, grenades, Deathball | `docs/specs/gamemodes/funcounterstrike.md`, `docs/specs/gamemodes/counterstrike.md`, `docs/specs/weapons/weapons.md`, `docs/specs/weapons/deathball.md`, `docs/specs/entity-editor/entity-editor.md` |
| Objective/round HUD for any mode | `docs/specs/gamemodes/objective-hud.md`, `docs/specs/gui/guiv2.md`, `docs/specs/gamemodes/gamemodes.md` |
| Zombie Tower phases, encounters, checkpoints, or monster roles | `docs/specs/gamemodes/zombie-tower/zombie-tower.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/npc-behavior/npc-behavior.md` |
| Bloodtest body layers, capsule locomotion, blood/destruction test bench | `docs/specs/gamemodes/bloodtest.md`, `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`, `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`, `docs/specs/creators/monster-authoring.md` |
| Animation, rigs, monster/NPC clips, arbitrary body parts, flying/crawling actors | `docs/specs/animations/animations.md`, `docs/specs/avatar/avatar-editor.md`, `docs/skills/spec-behavior-review-v1.md` |
| Multiplayer or server behavior | `docs/specs/networking/networking.md`, `docs/specs/gamemodes/gamemodes.md` |
| Logging or diagnostics | `docs/specs/debug-logging/debug-logging.md`, `docs/skills/logging-checker-v1.md` |
| Performance | `docs/specs/performance/performance.md`, `docs/specs/performance/perf-report-overlay.md`, `docs/specs/performance/culling-and-work-avoidance.md`, `docs/specs/hotreload/hot-layer-latency-and-language.md`, `docs/skills/efficiency-checker-v1.md` |
| Visibility culling, LOD, offscreen work, or efficiency backlog | `docs/specs/performance/culling-and-work-avoidance.md`, `docs/specs/performance/performance.md`, `docs/specs/gui/guiv2.md`, `docs/architecture/collision/collision.md` |
| Screenshots at a specific tick or visual proof | `docs/specs/dev-loop/visual-proof-screenshots.md`, `docs/specs/dev-loop/scenario-input-recording.md`, `docs/workflows/runtime-scenario-validation.md` |
| Terminal commands | `docs/architecture/terminal-commands/terminal-commands.md`, `docs/skills/terminal-command-checker-v1.md` |
| Runtime behavior validation | `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/quality-gates.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/terminal-commands/terminal-commands.md` |
| Human-recorded scenarios, replay checkpoints, or collision trace capture | `docs/specs/dev-loop/scenario-input-recording.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/specs/dev-loop/quality-gates.md` |
| Quality gates, lint, dead/duplicate code, multi-client matrix | `docs/specs/dev-loop/quality-gates.md`, `docs/specs/dev-loop/dev-loop.md` |
| Dev-loop dashboard UI, process discovery, or dashboard controls | `docs/specs/dev-loop/dashboard.md`, `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/dev-loop.md` |
| Multi-server/client presentation, per-executable journals, or dev-loop process table | `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/dashboard.md`, `docs/specs/debug-logging/canonical-jsonl.md`, `docs/specs/dev-loop/dev-loop.md` |
| One server + multiple clients, same-machine interaction tests, or dev-loop process controls | `docs/workflows/multi-client-dev-testing.md`, `docs/specs/dev-loop/multi-instance.md`, `docs/specs/networking/bad-connection-simulation.md` |
| Bad-connection presets, latency, loss, reorder, blackout, or impairment evidence | `docs/specs/networking/bad-connection-simulation.md`, `docs/specs/networking/networking.md`, `docs/specs/debug-logging/debug-logging.md` |
| In-game chat | `docs/specs/ingame-chat/ingame-chat.md`, `docs/skills/chat-checker-v1.md` |
| Moderation | `docs/specs/moderation/moderation.md`, `docs/skills/moderation-checker-v1.md` |
| Assets or sound | `docs/operations/asset-management/asset-management.md`, `docs/skills/asset-checker-v1.md` |
| Audio behavior, hot audio policy, sound recipes or sound resources | `docs/architecture/live-development/hot-audio-contract.md`, `docs/specs/hotreload/hotreload.md`, `docs/skills/logging-checker-v1.md` |
| Build or EXE | `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/hotreload/hotreload.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/architecture/live-development/module-migration-plan.md` |
| Dev scripts, script inventory, dependency fan-in, or cold-change preflight | `docs/operations/devscripts/script-inventory.md`, `docs/operations/devscripts/dependency-graph.md`, `docs/operations/devscripts/cold-change-guard.md` |
| Header fan-out, include hygiene, or reducing rebuild fan-out | `docs/architecture/headers/header-hygiene.md`, `docs/operations/devscripts/dependency-graph.md`, `docs/operations/devscripts/cold-change-guard.md` |
| Merge agent lanes into one EXE, or find pending lane compile/runtime validation | `docs/operations/build-and-exe/agent-lane-integration-and-pending-validation.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/dev-loop.md` |
| Build duration, phase progress, or compile-time diagnosis | `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/specs/hotreload/hot-layer-latency-and-language.md` |
| Agent work, isolated lanes, or build authority | `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/architecture/live-development/self-contained-workspace.md` |
| Agent build/test requests, candidate DLL builds, pinned runs, or change-to-evidence latency | `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/dev-loop/build-time-observability.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md` |
| Proof records, runtime proof, unproven/stale changes, or completion gates | `docs/specs/dev-loop/agent-proof-and-verification.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/workflows/runtime-scenario-validation.md` |
| Concurrent agents, shared-file coordination, activity/board, messages, edit leases, or autonomous decisions | `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`, `docs/specs/debug-logging/canonical-jsonl.md` |
| Build queue, rotating build owner, per-agent/cohort DLL pointers, preview overlays, or wait budgets | `docs/specs/dev-loop/agent-build-queue.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/change-to-evidence-latency.md` |
| Isolated test runtimes, runtime health, warm runtimes, DLL pinning, or proof links | `docs/specs/dev-loop/agent-runtime-registry.md`, `docs/specs/dev-loop/agent-build-queue.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/workflows/runtime-scenario-validation.md` |
| Live evidence from any running MiMITA process (human- or agent-started), external process journal discovery | `docs/specs/dev-loop/live-evidence-observer.md`, `docs/specs/dev-loop/agent-runtime-registry.md`, `docs/specs/debug-logging/canonical-jsonl.md` |
| Save/restore a live game state, session snapshot, or one-command state replay | `docs/specs/simulation/session-snapshot.md`, `docs/specs/dev-loop/scenario-input-recording.md`, `docs/specs/dev-loop/dev-loop.md` |
| Simulation time, tick rate, slow motion, event ordering, or primitive-state emergence | `docs/specs/simulation/time-and-ordering.md`, `docs/specs/architecture/nodes-and-connections.md`, `docs/specs/networking/networking.md`, `docs/specs/tas/tas.md` |
| Quantum-inspired primitives or literal-quantum research branch | `docs/specs/simulation/quantum-inspired-and-literal-quantum-research.md`, `docs/specs/simulation/time-and-ordering.md`, `docs/architecture/ecs-entity-etc/ecs.md` |
| Parent/subagents, cohorts, delegation, shared cohort pointers, or budgets | `docs/specs/dev-loop/agent-subagents.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-build-queue.md` |
| Waking an idle agent, `wake-check`, wake adapters, or wake safety | `docs/specs/dev-loop/agent-wake.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-self-improvement.md` |
| Universal issue context, automatic user-message broadcast, context routing repair, handoff completeness, or claiming unowned work | `docs/specs/dev-loop/agent-context-contract.md`, `docs/specs/dev-loop/agent-coordination.md` |
| Stale dev-loop daemon, wedged control path, owned process restart, or source-change pickup | `docs/skills/dev-loop-restart-v1.md`, `docs/specs/dev-loop/dev-loop.md` |
| Launch timers, one-button launch, launch failure codes, reports, or launch doctor | `docs/specs/dev-loop/launch-feedback.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/dashboard.md`, `docs/workflows/runtime-scenario-validation.md` |
| Agent self-improvement, autonomy classes, retro, improvement scanner, or capability proposals | `docs/workflows/measurement-hypothesis-driven-improvement.md`, `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/workflows/issue-intake-and-triage.md` |
| Measurement, falsifiable hypotheses, before/after evidence, or gold evidence behavior | `docs/workflows/measurement-hypothesis-driven-improvement.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/change-to-evidence-latency.md` |
| User reports, issue intake/triage, append-only issue docs, or TODO surfacing | `docs/workflows/issue-intake-and-triage.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/skills/issue-triage-v1.md`, `docs/issues/README.md` |
| Agent self-improvement, capability proposals, build-friction review, or coordination improvements | `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/dev-loop/change-to-evidence-latency.md`, `docs/skills/self-improvement-v1.md`, `docs/workflows/issue-intake-and-triage.md` |
| Live code, hot reload, or replaceable game modules | `docs/specs/hotreload/hotreload.md`, `docs/specs/hotreload/hot-layer-latency-and-language.md`, `docs/skills/spec-writing-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/skills/logging-checker-v1.md` |
| Live C++ dependency import or evaluation (Blink, cr.h, Flecs, Ninja) | `docs/specs/hotreload/live-cpp-dependencies.md`, `docs/specs/hotreload/hotreload.md`, `docs/specs/hotreload/hot-layer-latency-and-language.md` |
| Nodes, connections, graph knowledge, live dimensional projections, or graph-native code experiments | `docs/specs/architecture/nodes-and-connections.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/networking/contact-authority.md` |
| Entity, component, or ECS migration | `docs/architecture/ecs-entity-etc/ecs.md`, `docs/architecture/ecs-entity-etc/ecs-migration.md`, `docs/architecture/live-development/hot-kernel.md`, `docs/architecture/player-npc-systems/player-npc-systems.md` |
| Reusable collision/volume queries or soft-fit movement | `docs/architecture/collision/collision.md`, `docs/specs/movement/movement.md`, `docs/specs/gamemodes/gamemodes.md` |
| VPS inspection or repository-to-production comparison | `docs/operations/vps-audit.md`, `docs/operations/vps-deployment/vps-deployment.md`, `docs/operations/task-completion/task-completion.md` |
| Persistent progression, rewards, profiles, leaderboards | `docs/specs/data-saving/data-saving.md`, `docs/operations/vps-audit.md`, `docs/operations/todo-sort-these/persistence-recovery.md` |
| Website architecture, VPS website operations, database availability, and site recovery | `docs/architecture/website/website-architecture.md`, `docs/operations/website-operations.md`, `docs/operations/website-agent-deployment.md`, `docs/operations/vps-deployment/vps-deployment.md`, `docs/operations/vps-audit.md`, `docs/operations/todo-sort-these/persistence-recovery.md` |

Also read `docs/regressions/README.md` for any bug or human-reported
break. Read JSON architecture when settings or hot reload are involved.

For active configuration, resolve the runtime-loaded path before inspecting or
editing candidates. Files marked `.bak`, `.archive`, `.experimental`, dated
copies, or free-form historical text are archive material unless an active
configuration manifest explicitly identifies them.

## Historical documents

Documents under historical or archive locations are background reference only.
They must not override current specifications, architecture, workflows,
operations documents, skills, or regression records.

## Regression rule

If human review discovers that previously working behavior broke, connect the
failure to the changelog, record the exact wrong and corrected code, explain the
cause, and add a careful append-only regression entry.

## Spec/doc TODOs

If there is a comment in any document or spec like "todo: explain this better", the agent notes this, and in it's response, it notes the exact file path and line number that has that todo, and then suggests an expansion or a fix for that specific todo, but does not edit the document/spec.

## Live hot-reload work coordination

Before starting hot-reload code work, read:

- `docs/specs/hotreload/hotreload.md` (current specification)
- `docs/handoffs/hot-reload-agent-focus.md` (temporary coordination only)
- `build/hot-reload/status.json` after it exists (generated current status)
- the latest relevant `docs/changelog/` record

Exact ownership and generated matrix output come from
`build/source-manifest.json` and `config/source-owners.json`, never from a
hand-maintained path list. Generated views are not edited by hand. Do not add
claims JSON, lock files, or a second status document.

