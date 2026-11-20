// 2026-11-19T15:15:20Z
/* purpose
* record mode-aware actor spawn handling for hot DLL generations
* separate source review from build, runtime, and human acceptance
* this file DOES NOT claim a successful DLL build or live gameplay acceptance
*/

# Mode Spawns After DLL Reload

# Task

- Task ID: session-20261119-mode-spawns-after-dll-reload
- Session ID: session-20261119-mode-spawns-after-dll-reload
- Owner: Codex
- Focus: hot-reload gameplay continuity and spawn policy
- Summary: New player joins now consult the active gamemode spawn resolver when mode spawn setup is ready; the generic world picker remains the startup fallback and emits a bounded event when mode spawn setup is not ready. The hot-reload specification now requires mode spawn continuity across generations.
- Status: source and documentation changed; DLL build and runtime validation pending
- Date, time, timezone: 2026-11-19T15:15:20Z; display time 2026-11-19 10:15:20 EST (America/New_York)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: not committed by this session

# Pre-existing changes

- The shared checkout contained extensive unrelated changes before this work.
- The dev-loop status showed run `20261119_151124`, no running server/client, and game-build generation 353 reported as building with a coherence rejection caused by `src/entities/player-animation.cpp` changing during that attempt. No build request or runtime launch was made by this session.

# Requested behavior

- After a DLL reload, new actors must use the active mode's spawn behavior instead of all being placed by a generic shared spawn path.
- Document the spawn continuity requirement.

# Specification alignment

- Current specification: `docs/specs/hotreload/hotreload.md`.
- The spec requires resident state to survive generation swaps and real behavior to remain visible in the same process. It now states that new joins, respawns, and mode-created actors must use active mode spawn rules, with a logged bounded startup fallback.
- `src/network/server-gamemode.cpp` already owned mode/team-aware spawn resolution. The join handlers were bypassing it and always calling the resident generic picker for new players. They now use the existing mode resolver after `spawnsAssigned`, preserving the generic picker before that state is ready.

# Exact implementation changes

## `src/game/server-command-dispatch-module.cpp`

- `dispatchHello` and `dispatchJoin`: resolve new player positions through `serverGamemodeSpawnPointForNewActor`; retain `pickHostSpawn` when mode selection is unavailable.
- Reason: a DLL generation swap leaves the active mode state resident, so new joins should use its authored team/role spawn selection rather than an unrelated generic map index.

## `src/network/server-gamemode.h` and `src/network/server-gamemode.cpp`

- Added the public new-actor spawn resolver, gated on active mode state and assigned spawns.
- Team resolution now also consults the existing `communityTeams` mapping.
- Added `gamemode.spawn-new-actor` and `gamemode.spawn-mode-not-ready` owner events with actor, mode/map, reason, team or fallback, and resulting position as applicable.

## `docs/specs/hotreload/hotreload.md`

- Added the reload continuity contract and runtime proof requirements for new actors and respawns.
- Clarified the intended audience and boundary with per-mode spawn layout specs.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `src/game/server-command-dispatch-module.cpp` | Use mode-aware spawn resolution for new joins | Restore generic picker calls in both join paths |
| `src/network/server-gamemode.cpp` | Add resolver, community-team lookup, and spawn events | Remove the helper/event and community-team lookup |
| `src/network/server-gamemode.h` | Declare resolver | Remove the declaration |
| `docs/specs/hotreload/hotreload.md` | Document generation-spawn continuity | Remove the added audience and continuity sections |
| `docs/changelog/2026-11-19/20261119_101520-mode-spawns-after-dll-reload.md` | Record this session | Remove only this session record if requested |

# Contract / ABI changes

- No host ABI or packet layout changed. A game-DLL internal API was added.
- Build note queued before compiler-input edits: note ID `0797861ff9c2`, covering the three game-DLL source inputs.

# Validation and evidence

- Source review: the current source manifest marks the edited C++ sources `game`-owned.
- Static review: `git diff --check` completed without whitespace errors; Git reported existing line-ending warnings in unrelated files.
- Build: not run or requested. The dev-loop remains the sole builder; the observed generation 353 coherence rejection is not evidence of this patch compiling.
- Runtime: not exercised. No active server/client was present in the observed run.
- Human review: mode-correct positions after same-PID DLL activation remain unaccepted.

# Documentation checker

- The broader docs scan found existing unrelated TODOs, including `docs/specs/gamemodes/gamemodes.md:499`: `- Todo explain this 9 2 2026 0848`. Suggested expansion: describe the authoritative mode spawn selection inputs and link to the gamemode-specific spawn rules. It was not changed because the router says to report, not edit, these TODOs during unrelated work.

