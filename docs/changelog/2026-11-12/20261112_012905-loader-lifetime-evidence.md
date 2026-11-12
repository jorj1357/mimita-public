# Loader lifetime evidence pass

- UTC: 2026-11-12T01:29:05Z
- Local timestamp: 2026-11-11 20:29:05 -05:00
- Branch: unavailable; git status/branch inspection remains blocked by external/libjuice submodule access. Prior source/inventory edits are preserved; this pass changed documentation only.
- Scope: trace existing narrow HotReloadSystem and subsystem ModuleHost unload barriers against the proposed whole-game safe reload requirements.
- Exact changes: appended “Existing loader barrier evidence (2026-11-12)” to docs/handoffs/cpp-lifetime-review.md. No code, ownership manifest, loader behavior, or build setting changed.
- Evidence: HotReloadSystem validates candidate API and calls void beforeUnload before FreeLibrary, but shows no call quiescence and synchronously calls build_game_dll.py while watching only two source files and game-api.h. ModuleHost keeps shared locks through synchronous event dispatch and takes exclusive locks for activation; it validates module metadata and calls shutdown before FreeLibrary, but the ABI has no quiesce acknowledgement and module-created async work is outside the host lock. ModuleHost catalog supports declared module additions/removals, not arbitrary whole-repository source hot reload.
- Documents read: docs/ROUTER.md; docs/specs/dev-loop/agent-lanes-build-authority.md; docs/architecture/live-development/live-development.md; docs/specs/hotreload/hotreload.md; docs/handoffs/cpp-source-inventory.md; docs/architecture/time-and-formatting/time-and-formatting.md; docs/skills/documentation-checker-v1.md; docs/operations/task-completion/task-completion.md.
- Validation: manually inspected loader source, ABI declarations, and inventory counts/statuses. All 731 CSV rows currently report NOT_SEMANTICALLY_REVIEWED; 632 game-target or saved-manifest candidates require semantic disposition, and 22 active C translation units also need coverage. No build, test, or runtime launch.
- Result: PASS_WITH_HUMAN_REVIEW for this loader trace only. It does not complete the source audit, prove a universal reload barrier, or demonstrate that arbitrary add/remove/save actions update a running game.
- Human review: confirm the design requirement for a resident-owned generation barrier with refusal/rollback and whether active third-party sources are in scope for the game DLL.
