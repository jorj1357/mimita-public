# Fix Big Shotgun Include

Time UTC: 2026-10-02T01:32:04Z
Display timezone: America/New_York
Branch: afad20a-rebuild
Commit: working tree; no commit created

## Result

Status: PASS_WITH_HUMAN_REVIEW

Fixed the dev-loop compiler failure in `src/combat/weapon-fire-hit.cpp` by
including the header that owns `MAX_PELLETS_PER_BLAST`:

```cpp
#include "combat/pellet-pattern.h"
```

The `pelletDirs` errors were cascading diagnostics caused by the missing
constant declaration, not separate missing variables.

## Validation

- Focused source ownership check: PASS.
- `git diff --check` on the corrected source: PASS.
- Existing dev-loop retry completed and published
  `C:\mimita-v9\.dev\builds\0870\mimita.exe` at 2026-10-01 21:31:01 local.
- The two existing `mimita.exe` processes remained running and were not
  closed, restarted, killed, relinked, or replaced.

The legacy `build/build-result.json` / `build/changelog.txt` still reflects an
older failed run and was not used as proof for this retry; the dev-loop state
and published build artifact are the current evidence.

## Cold-build rule locations requested by the user

The active rule is stated in:

- `docs/operations/build-and-exe/build-and-exe.md:9-17` — running MiMITA must
  remain running; `build_agent.py` and `buildv3.py` are cold-build paths.
- `docs/architecture/live-development/live-development.md:14-43` — the live
  invariant and cold-versus-live build split.
- `docs/architecture/live-development/live-development.md:115-118` — live
  entry versus `build_agent.py` / `build.py` cold-only status.
- `docs/operations/build-and-exe/build-and-exe.md:129-131` — do not overwrite
  a running executable.
- `docs/gold/2026-09-12-live-jsonl-ai-observability.md:143-149` — no killing or
  silently restarting MiMITA during live development.
- `docs/operations/task-completion/task-completion.md:63-67` and
  `docs/regressions/README.md:332-360` — every intentional cold build must be
  recorded in the dated cold-build debt record.
- `docs/architecture/live-development/hot-audio-contract.md:9` — the audio
  hot-reload contract also forbids relinking a running executable.

These are active documentation paths; older archived regression/spec files may
mention the historical rule but are not the current routing authority.

## Pre-existing work

All unrelated worktree edits and prior changelogs were preserved. This session
added only this changelog and the direct include correction to the feature's
already modified source file.
