// 2026-11-11
/* purpose
* state that Mode 2 agent lanes are deferred
* define the active path toward one replaceable whole-game DLL
* make NPC behavior the first proof slice, not the permanent DLL scope
*/

# Whole-game DLL migration plan

As of **2026-11-11 18:41 EST**, Mode 2 agent lanes are deferred. The target is
one replaceable `game.dll` for ordinary game implementation, not one DLL per
source file and not a permanent NPC-only DLL. Completion means the actual game
runs with its ordinary game code in that DLL; an NPC-only reload demo is not
completion.

## Active working rules

- Use the current root checkout, current branch, canonical root dev-loop, and
  existing executable.
- Do not create, start, build, sync, promote, or clean up Mode 2 lanes. The
  agent workflow scripts reject lane mutations. Read-only lane status/list
  remain available.
- Preserve existing lane worktrees, branches, metadata, artifacts, and
  journals as parked historical evidence. Review and migrate useful changes
  deliberately later; do not bulk merge or delete them.
- The repair launcher is the process-control path for already-running root or
  lane watchdogs, daemons, and build-only workers. This policy does not mean
  they are stopped until the launcher has run and confirmed it.
- Only the root dev-loop owns source builds. Do not start another watcher or
  request a build from an agent.

See [dev-loop policy](../../specs/dev-loop/agent-lanes-build-authority.md),
[dev-loop operation](../../specs/dev-loop/dev-loop.md), and
[workspace policy](self-contained-workspace.md).

## Where hot reload is now

There is a focused game DLL containing only `src/effects/effect-part.cpp` and
`src/hot-reload/quick-hit-debug-visual.cpp`. The source inventory records 582
translation units assigned to the EXE and 12 to subsystem modules in the
snapshot; re-count before acting because it can change. `npc-behavior` and
`npc-perception` are useful existing hot subsystem modules and host adapters,
but they are not a resident-runtime boundary and do not make the full game
replaceable. The current build still needs an EXE for most C++ changes.

The desired architecture is:

| Part | Owns |
|---|---|
| Launcher EXE | Process entry and startup only |
| Resident `runtime.dll` | Stable ABI, persistent live state, services, safe generation switching |
| One `game.dll` | Ordinary changeable game behavior and implementation |

A source `.cpp` is compiled into a target; it does not become an individual
DLL. Keep one large game DLL unless measured link time or failure isolation
proves a split is useful. Keep explicit exceptions for bootstrap/runtime,
platform/device, transport, third-party, and build/test code.

## Migration order

1. **Inventory the whole game.** Account for every active game `.cpp` and
   header: current target, dependencies, durable/static state, callbacks/jobs,
   and proposed owner. The source inventory is a current snapshot; it is not
   yet a complete reload-safety map. Work can start on the runtime seam while
   owners are classified, but the end target remains every eligible game
   implementation in `game.dll`.
2. **Install the runtime/game seam.** Add the stable C ABI, resident runtime,
   generation validation, callback/job quiescence, safe activation, and
   rollback. This needs one deliberate initial EXE build. The aim afterward is
   that ordinary game C++ edits do not relink or restart the EXE.
3. **Prove the DLL lifecycle.** Use one NPC decision as the first bounded
   probe that same-process activation, failure fallback, state continuity, and
   rollback work. This only validates the loader mechanism; it does not mean
   the game is migrated or ready.
4. **Cut over the running game into one DLL.** Finish semantic state/lifetime
   tracing for every eligible game source and linked game dependency, then
   prepare one complete owner manifest. Move all eligible implementation
   sources to one `game.dll` in a single ownership cutover, removing each from
   its former EXE/subsystem target in the same change. Preparatory loader and
   runtime work can be developed before this cutover; do not leave ordinary
   game sources split across old and new owners as a long-lived migration
   state. Keep every feature present. The completion check covers the real
   launch path and modes, not only NPC behavior. Bootstrap/runtime and
   documented platform/build exceptions remain outside the replaceable game
   code.
5. **Automate the one-click root loop.** A single launcher opens the canonical
   Python GUI/coordinator. It detects saves, snapshots coherent inputs,
   compiles affected translation units, links an immutable candidate, validates
   it, and signals safe activation. Coalesce edits during a build; retain the
   prior generation when compile, validation, or activation fails. Editing any
   eligible game `.cpp` must select the DLL target automatically; no manual
   per-file DLL request.
6. **Verify whole-game completion.** Confirm the game launches and its ordinary
   systems run from the active `game.dll`; then edit representative sources
   across the inventory and confirm each edit builds/activates a new DLL
   generation in the same process. Measure compile, link, activation, and
   save-to-live time. Performance is a later tuning target. Keep one DLL unless
   a measured limitation requires splitting it.

## Completion evidence

Report source/build evidence separately from runtime behavior and human review.
For each proof, record the root revision, source hash, candidate generation,
process ID, exact `events.jsonl`, before/after state, and observed fallback or
rollback. A successful link or generation swap alone does not prove behavior.
Do not claim process cleanup unless the repair launcher confirms it.

## Related documents

- [Hot-reload specification](../../specs/hotreload/hotreload.md)
- [Live-development invariant](live-development.md)
- [Hot kernel](hot-kernel.md)
- [Agent lanes and build authority](../../specs/dev-loop/agent-lanes-build-authority.md)
- [Build and EXE](../../operations/build-and-exe/build-and-exe.md)
- [Self-contained workspace](self-contained-workspace.md)
