# Game DLL incremental skip: why a no-change build compiles nothing

Recorded: 2026-11-12
Scope: `devscripts/game_build_worker.py`, the whole-game `game.dll` candidate builder.
Evidence class: source and build only. No runtime proof.

## The contract

A no-change build must compile **zero** translation units and go straight to the link. The worker prints:

```text
[SKIP] 12/575 src/network/server.cpp
[CXX ] 13/575 2.26s src/npc/npc-targeting.cpp
```

and writes live `build/game/progress.json` plus `build/game/progress.log`.

## What makes it skip

Five rules, matched to `build.py::source_changed`:

1. **Persistent object cache.**
   Objects live in `build/game/objects/<flags-hash>/`, not `generation-N/obj`.
   A new generation reuses the same objects. The flags hash covers the compiler
   path, build mode, PCH mode, include dirs, and defines.
2. **Per-TU key.** `object_key = sha256(path + source_sha)[:20]`, so an edited
   source gets a new object. Unedited sources keep their object.
3. **Valid object + dependency.** Both `.o` and `.d` must exist and be non-zero.
   Zero-byte files are aborted-build artifacts and force a rebuild.
4. **Correct `.d` parsing.** GCC emits a Makefile rule with a Windows drive-letter
   target, `-MP` phony targets, and backslash line continuations:
   - join `\` + newline,
   - drop the target prefix up to `.o:`,
   - split dependencies on whitespace,
   - **skip tokens ending in `:`** (`-MP` phony targets),
   - keep backslashes inside paths (do not replace them with spaces).
5. **Timestamp comparison.** Rebuild if any dependency is `>=` the object mtime,
   or if the shared PCH (`src/pch.h.gch`) is `>=` the object mtime.

If all rules pass, the TU is `[SKIP]`.

## Parallel and priority

- `ThreadPoolExecutor` with `jobs = os.cpu_count()` by default.
- `SetPriorityClass(HIGH_PRIORITY_CLASS)` for the worker and its children.
- Link only after every object is present. The response file uses forward slashes
  (`path.as_posix()`), because MinGW `@response` parsing strips backslashes.

## Why the first build is still slow

Native C++ must compile once. The first full build is unavoidable. The cache makes
every later build incremental. Measured on the 575-source diagnostic:

| Run | Compiled | Skipped | Compile | Link |
|---|---:|---:|---:|---:|
| First full | 575 | 0 | ~11 min | failed |
| First rerun | 10 | 565 | 18.2 s | 12.0 s |
| No-change rerun | **0** | **575** | **0 s** | 15.3 s |

## How to verify the skip

```powershell
python devscripts/game_build_worker.py --manifest build/source-manifest.json `
  --out build/game --priority high
Get-Content build/game/progress.json -Raw | ConvertFrom-Json |
  Select-Object compiled, skipped, compile_s, link_s
```

Expected on a no-change tree: `compiled=0`, `skipped=575`, `compile_s=0`.

## Pitfalls this reference prevents

- **Per-generation object dirs** defeat incrementality even when the parser is
  correct.
- **Splitting `.d` on the first `:`** splits the Windows drive letter
  (`C:`), treating the target as a dependency.
- **Replacing backslashes with spaces** shreds Windows paths.
- **Ignoring `-MP`** makes every phony `header.h:` token look like a missing file.
- **Skipping the PCH check** can reuse objects compiled against an old shared PCH.
- **Backslash response files** make the linker report paths with the separators
  removed.

## References

- `build.py::source_changed` — the canonical skip rules.
- `devscripts/game_build_worker.py` — the whole-game worker.
- `docs/regressions/dev-loop/incremental-recompile-REG-2026-11-12.md` — the bad
  pattern that caused the full recompile.
- `docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md` — the measured result.
