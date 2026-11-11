# Continuous live development plan clarification

- Local time: 2026-11-11 14:54:34 -05:00 (America/New_York)
- Branch: `checkpoint/pre-live-kernel-20261108`
- HEAD: `9bfbaeff` (`feat: install hot NPC hooks into one host`)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Updated `docs/specs/hotreload/hotreload.md` to put the agreed runtime/game architecture and phased migration plan first. The user's pre-existing dated notes in this file were consolidated into an authoritative plan; other existing working-tree changes were left untouched.

The new plan defines:

- one large `game.dll` as the initial code target, with splitting deferred until build/link/activation measurements justify it;
- a resident `runtime.dll` as a generic, versioned state store and coordinator, with ordinary mutable behavior in `game.dll`;
- game-owned schema migrations for component fields, stable handles/ABI across the DLL boundary, and free signature changes within the rebuilt game DLL;
- automatic snapshot-based incremental compilation, validation, safe activation, old-generation retention, and rollback;
- an NPC live-reload vertical proof before defaulting the remaining source inventory to the game DLL;
- 240 FPS as a frame-budget goal, server 60 Hz evidence, trusted host-approved native code sharing, and same-process graphics-device recreation for supported backends;
- current source status as of 2026-11-11 and a clear distinction between current subsystem modules and the future whole-game DLL target.

The remaining subsystem-module sections are explicitly labeled as historical/implementation evidence so their older module-first target does not conflict with the new plan.

## Exact wording changed

Prior user note included: “no more start-sleep 45 sec for AI to wait for a build to finish” and “edit .cpp => save => build into one .exe => test .exe => trial/error until success”. It also proposed “compile all into one game.dll” and preserving state through `runtime.dll`.

The new contract says: “For ordinary game-code edits, ‘no build’ means no manual build request, no full EXE relink, and no process restart. Native C++ still has to compile and link to machine code.” The architecture specifies `runtime.dll` as persistent state/service ownership and `game.dll` as replaceable behavior. The phases begin with source inventory and an NPC vertical proof, then expand default ownership to ordinary implementation `.cpp` files.

## User decisions recorded

- Start with one large `game.dll`; split only if measurements justify it.
- Keep runtime data in versioned generic records so field additions use game-owned migration instead of fixed runtime-struct edits.
- Prove one complete NPC path before scaling across the repository.
- Measure 240 FPS by the proportion of frames at or under 4.17 ms and report percentiles/worst frame.
- Renderer/backend changes may recreate the device in the same process and are limited to supported platform/GPU combinations.
- Native-code collaboration is limited to trusted, host-approved sessions.
- Measure 99.99% accepted legitimate hit claims over the full test session; invalid claims remain rejected.

## Evidence and validation

- Read the current hot-reload specification, `docs/architecture/live-development/module-migration-plan.md`, `docs/operations/task-completion/task-completion.md`, and focused spec/documentation review guidance.
- Source inspection and `python devscripts/module_ownership.py`: 594 source `.cpp` files; 12 assigned to subsystem modules and 582 to the EXE. `build_game_dll.py` currently lists two source files; the current `GameAPI` is a narrow feature interface.
- `git diff --check -- docs/specs/hotreload/hotreload.md`: passed; Git emitted only its line-ending conversion warning.
- No build, automated tests, or runtime acceptance were run because this change updates the plan only. The implementation and every runtime acceptance gate remain pending.
- Documentation TODO scan found pre-existing unrelated notes, left unchanged. Examples: `docs/architecture/vision-mentality/mapping.md:2` (“jorj todo eplxain map editor conenciton”) needs a map-data-flow explanation; `docs/architecture/player-npc-systems/npc-movement.md:11` (“2026 10 13 2116 jorj todo -”) needs a concrete replacement or historical label; `docs/operations/vps-deployment/vps-deployment.md:6` (“todo make this prettier and update docs to look nicer”) belongs to a separate documentation cleanup.

## Working-tree preservation

Pre-existing changed paths observed before this edit: `config/analytics.json`, `docs/specs/hotreload/hotreload.md`, `docs/specs/open-gameplay-issues.md`, and untracked `build-identity.json`. Only the requested hot-reload specification was edited in this session. No build artifacts or runtime state were created.

## Human review still needed

The document now records an agreed target, not an implementation claim. Review the phase order and the runtime/game boundary before implementation. Runtime evidence is still required for live NPC behavior, state continuity, failure retention, rollback, performance, multiplayer agreement, and graphics-backend recreation.