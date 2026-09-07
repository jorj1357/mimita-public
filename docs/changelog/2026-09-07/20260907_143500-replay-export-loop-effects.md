# Replay export loop and effect re-presentation fix

- Branch: `8292026stash`
- Starting commit: `fe983bc`
- Session timestamp: `2026-09-07T18:45:00Z`
- Scope: stop export from restarting the replay at the end and re-presenting historical effects indefinitely.
- Pre-existing changes: user/other-agent configuration edits, replay camera/event-cursor edits, existing regression entries, and existing changelog files were preserved.

## Exact change

### `src/engine/engine-tick-combat.cpp`

- Old condition for the normal FinalKillReplay loop allowed playback to seek to tick 0 whenever the replay ended, including while an export subprocess was rendering.
- New condition adds `!isReplayExportActive()`.
- Added `#include "replay/replay-export.h"` so the export-state guard uses the existing replay-export ownership/API.
- Effect: export now reaches the clip end once instead of repeatedly restarting it and clearing/re-delivering its historical effects. Normal in-game replay looping remains unchanged.

## Evidence

- `logs\\09-07-2026\\ReplayExport_log_142914.txt` showed a 900-tick clip producing approximately 4,967 `projectile_spawn`, 4,064 `damage_number`, 18,541 `footstep`, and 63,994 `net_rocket_trail` replay dispatch lines. The source replay contained only 19 projectile-spawn events, proving the export loop was re-presenting the history.
- The screenshot supplied in the request is consistent with repeated historical presentation: expired visuals were followed by another replay pass recreating them.

## Validation

- `python build_agent.py`: `Status: SUCCESS`; canonical `mimita.exe` linked.
- `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator`: `26/26 passed`.
- No fresh visual export was run in this session.

## Remaining work

- Replay still has replay-only projectile/effect reconstruction in `src\\engine\\engine-tick-camera.cpp`.
- The shared collision-aware rocket path, shared hit-effect entry path, exact lifetime/lighting/tracer behavior, and left-leg basis still require implementation and live acceptance.
