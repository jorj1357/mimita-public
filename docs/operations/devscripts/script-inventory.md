# Development script inventory

As recorded: 2026-11-17  
Scope: `devscripts/` tooling and the repository entry points that invoke it.

## How to read this document

These categories describe authority, not importance:

- **Canonical**: the normal development path. Changes here affect every run.
- **Gate**: a repository check. A gate may block publication when authoritative.
- **Report-only**: diagnostic output. It must not be treated as runtime proof
  or as a Cutover-1 blocker.
- **One-off**: a named migration, content generator, release, or repair tool.
- **Deferred**: retained for a paused workflow, such as Mode 2 lanes.

A build is not runtime proof. A report-only script is not acceptance. A
changelog is history, not current status.

## Canonical development loop

| Script | Category | What it does | Reads | Writes | Run cadence |
|---|---|---|---|---|---|
| `devscripts/dev-loop.py` | Canonical | Root orchestration: status, control, source watching, builds, launch, scenarios, dashboard, and instance lifecycle. This is the primary development entry point. | source tree, `.dev/state.json`, build outputs | `.dev/state.json`, journals, published builds | continuously while developing |
| `devscripts/dev-loop-watch.py` | Canonical | Supervises exactly one checkout-owned dev-loop daemon; restarts or replaces a stale daemon by script hash. | daemon lock/state, script hashes | watcher lock/state/log | always-on when using the loop |
| `devscripts/dev-loop-matrix.py` | Canonical | Runs multi-client/launch-mode matrices through `dev-loop.py`. | launch modes, scenario data | journals, matrix result | on demand for matrix validation |
| `devscripts/dev_loop/` | Canonical | Package for loop artifacts, build, launch, state, runtime identity, status, GUI, terminal, and tests. `dev-loop.py` remains the public entry. | source/build state | loop state and artifacts | used by dev-loop |
| `devscripts/open-dev-loop-dashboard.py` | Canonical | Opens the canonical dashboard entry. | `dev-loop.py` | dashboard process | on demand |
| `devscripts/dev_loop_gui.py` | Canonical wrapper | Small GUI entry shim. | GUI package | GUI | on demand |
| `devscripts/dev_loop_terminal.py` | Canonical wrapper | Small terminal entry shim. | terminal package | terminal | on demand |
| `devscripts/repair-dev-loop-ownership.py` | One-off | Repairs identifiable dev-loop/watchdog ownership after orphaned or stale processes. | process table | process changes, log | explicit repair only |
| `devscripts/process-snapshot.py` | Report-only | Captures a process inventory for diagnosis. | Windows process table | `.dev/process-snapshots/` | on demand |
| `devscripts/process_identity.py` | Canonical helper | PID identity/liveness helper shared by loop tooling. | process table | none | imported by scripts |
| `devscripts/dev-launch-modes.json` | Canonical data | Named launch modes, maps, server arguments, client arguments, and validation requirements. | none | none | edited when adding modes |
| `devscripts/dev-profiles/` | Canonical data | Dev-loop server/client profiles. | none | none | edited when adding profiles |
| `devscripts/scenarios/` | Canonical data | Runtime scenario and recording inputs. | none | scenario outputs under run logs | used by scenario commands |

## Build, ownership, and code generation

| Script | Category | What it does | Reads | Writes | Run cadence |
|---|---|---|---|---|---|
| `build.py` | Canonical | Builds the EXE from the cold source set, handles PCH/ccache/incremental objects, links, and publishes. | source manifest, build toolchain | `build/obj-*`, `build/link-*`, EXE artifacts | whenever a cold source changes |
| `devscripts/game_build_worker.py` | Canonical | Builds an immutable whole-game candidate DLL from a coherent manifest snapshot. | `build/source-manifest.json` | `build/game/generation-*` | on every eligible game source change |
| `devscripts/build-note.py` | Canonical | Queues an agent-written change summary and optional paths for the next whole-game DLL build. | CLI arguments | `.dev/build-notes/pending/` | before a related game source build |
| `devscripts/build_authority.py` | Canonical | Guards EXE/module builders so compilation goes through the dev-loop authority. | environment/locks | none | imported by builders |
| `devscripts/build_scheduler.py` | Canonical | Enforces machine-wide EXE/module build slots. | scheduler state | scheduler state | imported by builders |
| `devscripts/build_exclusive.py` | Canonical | Checkout-level build mutex. | lock file | lock file | imported by `build.py` |
| `devscripts/module_ownership.py` | Canonical | Single source for module/EXE source ownership and the cold source set. | module catalog, source manifest | none | imported by build/gates |
| `devscripts/source_manifest.py` | Canonical | Captures one coherent source revision, headers, dependencies, and exactly one owner per source. | `config/source-owners.json`, module catalog, source tree | `build/source-manifest.json` | before builds and ownership checks |
| `devscripts/dependency_graph.py` | Report-only | Reports compiler-derived direct/transitive dependents, header fan-in, stale `.d` files, and hot-to-cold include edges. | `.d` files, `build/source-manifest.json` | optional `build/reports/dependency-graph.json` | before shared-header or hot-boundary changes |
| `devscripts/resident_state_codegen.py` | Canonical | Generates resident state domain structs/masks from the state schema. | `config/hot-reload/state-schema.json` | `src/host/generated/resident-state-v1.h`, state ledger | when state schema changes |
| `devscripts/packet_registry_codegen.py` | Canonical | Generates packet paths/registry from actual dispatch and references. | source dispatch, packet schemas | packet ledger/registry | when packet code changes |
| `devscripts/module-build-worker.py` | Deferred | Legacy subsystem ModuleV1 DLL worker. Module lanes are deferred; do not use for normal work. | module catalog | module DLL artifacts | deferred |
| `devscripts/build-minimal-ffmpeg.sh` | One-off | Builds/stages the minimal FFmpeg dependency for release/runtime packaging. | toolchain, FFmpeg sources | FFmpeg artifacts | dependency setup only |

## Authoritative hot/cold gates

| Script | Category | What it does | Reads | Writes | Gate authority |
|---|---|---|---|---|---|
| `devscripts/build_target_gate.py` | Gate | Validates EXE cold-core target, game/test sources in EXE, and optional cold-trial zero. | actual link inputs, source manifest, cold trial | `build/hot-reload/build-target-gate.json` | authoritative |
| `devscripts/exe_cutover_gate.py` | Gate | Checks that no game-owned source is in the actual EXE linker input. | actual link inputs, source manifest | `build/hot-reload/exe-cutover.json` | authoritative |
| `devscripts/exe_cold_link_trial.py` | Gate | Links the cold EXE candidate without game sources and reports unresolved symbols. | source manifest, build objects | `build/hot-reload/cold-link-trial.json` | authoritative |
| `devscripts/hot_client_gate.py` | Gate | Validates a live client journal for DLL render path, swap, frames, connection, and stalls. | client `events.jsonl` | optional gate JSON | authoritative runtime check |
| `devscripts/cap_render_gate.py` | Gate | Detects raw game-owned GL/GLAD that bypasses the render capability. | source manifest | `build/hot-reload/cap-render.json` | authoritative |
| `devscripts/hot_reload_status.py` | Status | Aggregates current build/run/packet/state/gate facts into `status.json`. | generated manifests and gates | `build/hot-reload/status.json` | authoritative status; not runtime proof |
| `devscripts/game_static_gate.py` | Gate | Rejects banned raw patterns in game sources. | `config/game-static-gate.json`, source tree | optional report | authoritative if enabled |
| `devscripts/cold_change_guard.py` | Gate/preflight | Classifies changed files as hot, cold, module, tooling, or unknown before editing/building. | source manifest, git changes | optional JSON | advisory/preflight; exit nonzero on cold/unknown by default |

## Optional report-only census and audit

These tools document the migration. Their outputs are not Cutover-1 acceptance
and must not be used as the current status without checking their source
revision and timestamp. They remain available on demand.

| Script | Category | What it does | Reads | Writes | Notes |
|---|---|---|---|---|---|
| `devscripts/audit_triage.py` | Report-only | Static classification of source rows into unload/state/runtime/adapter/non-game classes. | source inventory or manifest | `build/hot-reload/audit*.json`, report/CSV | Do not treat class counts as bugs |
| `devscripts/audit_passes.py` | Report-only | Generates static quiesce/capability pass plans from audit data. | audit/inventory data | handoff reports | Folded into audit triage conceptually |
| `devscripts/state_owner_pass.py` | Report-only | Per-symbol state-owner decisions for state-review rows. | inventory data | handoff decisions | Historical |
| `devscripts/render_query_inventory.py` | Report-only | Inventories render/query call sites. | source/inventory | handoff data | Historical support for audit |
| `devscripts/protocol_inventory.py` | Report-only | Inventories protocol/packet call sites. | source/inventory | handoff data | Historical support for audit |
| `devscripts/cpp-source-inventory.py` | Report-only | Enumerates C++ sources, dependencies, and risk candidates. | source tree | inventory JSON/CSV | Expensive; not needed every edit |
| `devscripts/game_link_worklist.py` | Report-only | Historical undefined-symbol/worklist reporter. | linker output/ownership | handoff worklist | Superseded by actual link-input sidecars |
| `devscripts/hot_reload_plan_registry.py` | Report-only | Validates/generates planning registry views. | plan JSON, source manifest | generated planning docs | Planning only; no acceptance authority |
| `devscripts/live.py` | Deprecated | Earlier standalone watcher/builder coordinator. | source tree | old game ready/current files | Superseded by `dev-loop.py`; do not start a competing loop |

## Scenarios, performance, and diagnostics

| Script | Category | What it does | Reads | Writes | Notes |
|---|---|---|---|---|---|
| `devscripts/scenario-analyze.py` | Canonical | Analyzes scenario recordings and produces assertions/reports. | scenario/run data | report output | Runtime evidence support |
| `devscripts/scenario_export.py` | Canonical | Exports/serializes scenario recordings. | scenario data | exported scenario | Runtime evidence support |
| `devscripts/scenario_replay_report.py` | Canonical | Replays/compares a scenario and reports results. | scenario data | report | Runtime evidence support |
| `devscripts/network_trace.py` | Report-only | Builds the reconnect/health/event chain from journals and checks configured attempt spacing, grace, and slot evidence. | run `events.jsonl`, `networkingconfig.json` | stdout/JSON summary | Network diagnosis, not acceptance |
| `devscripts/perf-watch.py` | Report-only | Samples performance evidence from journals. | `events.jsonl` | perf output | Measurement, not acceptance |
| `devscripts/perf-evidence.py` | Report-only | Collects/summarizes performance evidence. | journals | perf reports | Measurement |
| `devscripts/server_hotness_probe.py` | Diagnostic | Sends a bounded server probe and checks hotness/ping response. | server endpoint | probe result | Diagnostic |

## Configuration, content, and authoring validation

| Script | Category | What it does | Reads | Writes | Notes |
|---|---|---|---|---|---|
| `devscripts/jsonc.py` | Helper | JSON-with-comments loader. | JSONC files | none | Shared helper |
| `devscripts/config_selftest.py` | Report-only | Validates configuration files and unknown settings. | config tree | report | Run when config schema changes |
| `devscripts/animation_json_check.py` | Report-only | Validates animation definitions/profiles. | animation JSON | report | Content validation |
| `devscripts/gamemode_json_sync.py` | Report-only | Checks/synchronizes gamemode JSON data. | gamemode config | optional edits/report | Content validation |
| `devscripts/weapon_json_sync.py` | Report-only | Checks/synchronizes weapon JSON fields. | weapon config | optional edits/report | Content validation |
| `devscripts/world-project.py` | One-off | World-project inspection/authoring helper. | world project files | project output | Not a gameplay runtime path |
| `devscripts/migrate_avatar_animation_profiles.py` | One-off | Migrates avatar animation profile data. | animation data | migrated data | Historical migration |
| `devscripts/generate_zombie_tower_slice.py` | One-off | Generates the authored Zombie Tower slice. | authored inputs | generated map/content | Named content generator |

## Packaging, integrations, and deferred workflows

| Script | Category | What it does | Reads | Writes | Notes |
|---|---|---|---|---|---|
| `devscripts/package-release.py` | One-off | Packages release artifacts. | build artifacts, assets | release package | Release only |
| `devscripts/capture-windows-processes.bat` | Diagnostic | Convenience Windows process capture. | process table | snapshot | Prefer `process-snapshot.py` |
| `devscripts/open-dev-loop-dashboard.bat` | Wrapper | Windows shortcut for the dashboard entry. | `dev-loop.py` | dashboard process | On demand |
| `devscripts/repair-dev-loop-ownership.bat` | Wrapper | Windows shortcut for the ownership repair helper. | repair script | process changes | Explicit repair only |
| `devscripts/agent-lane.py` | Deferred | Historical Mode 2 lane manager. Mutating lane actions are disabled. | lane metadata | lane state | Preserve existing data |
| `devscripts/agent-workflow.py` | Deferred | Historical lane workflow wrapper. | lane config | workflow output | Not normal path |
| `devscripts/quality/` | Report-only | Lint, damage/contact ownership, dead/duplicate code, and coverage reports. | source tree | reports | Agent quality gate is not trusted |
| `devscripts/blender-ai/` | Integration | Blender AI/MCP integration and tests. | Blender/project data | Blender assets/commands | Separate owner |
| `devscripts/blender/` | Integration | Blender/Roblox control integration. | Blender/Roblox data | authored content | Separate owner |

## Suggested consolidation, not yet executed

Do not move scripts until call sites, docs, task profiles, and CI references are
audited.

1. **Optional audit front-end:** keep existing audit scripts report-only, later
   expose them through one `hot-audit` command with subcommands.
2. **Cutover gate wrapper:** later combine the three link/cold gates behind one
   `hot-cutover-check` command while keeping the focused scripts.
3. **Scenario front-end:** later combine analyze/export/replay reporting behind
   one `scenario` command with subcommands.
4. **Deprecated scripts:** `live.py` should be archived after references and
   workflows stop using it.
5. **Superseded worklist:** `game_link_worklist.py` should remain report-only
   until removed in favor of actual link-input sidecars.
6. **Quality scripts:** keep report-only, never a Cutover gate unless a future
   spec explicitly restores their authority.
7. **Planning outputs:** move generated planning/census artifacts out of
   authoritative status and mark them historical.

## Related documents

- [Development loop specification](../../specs/dev-loop/dev-loop.md)
- [Quality gates](../../specs/dev-loop/quality-gates.md)
- [Hot reload specification](../../specs/hotreload/hotreload.md)
- [Runtime scenario validation](../../workflows/runtime-scenario-validation.md)
- [Build and EXE](../../operations/build-and-exe/build-and-exe.md)
- [Cold-change guard](cold-change-guard.md)
- [Dependency graph](dependency-graph.md)
