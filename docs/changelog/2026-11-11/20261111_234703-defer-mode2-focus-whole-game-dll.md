# Defer Mode 2 and set whole-game DLL direction

- Set Mode 2 lanes as deferred from 2026-11-11 18:41 EST; lane CLI mutations and the Mode 2 workflow entry point now reject new lane operations. Read-only status/list remain; existing lane data is preserved.
- Rewrote the migration plan around one resident runtime and one broad `game.dll`. NPC behavior is the first continuity/reload proof, not the final migration scope.
- Updated the hot-reload spec's lane-specific runbook references to the canonical root checkout/dev-loop.
- Clarified completion: the NPC reload is only a loader proof; full completion means the running game uses one `game.dll` for ordinary game code, with automatic save-to-DLL rebuild/activation from one root GUI loop.
- Process shutdown: attempted through the repair launcher, but this execution environment denies CIM process access. The launcher did not start a new root loop because cleanup failed.
- Build/runtime validation: not run. No build was requested; source edits do not alter C++ binaries.
