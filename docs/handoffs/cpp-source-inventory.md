# C++ source and reload-risk inventory

Generated from the repository on **2026-11-12T01:03:40.877Z** by
`devscripts/cpp-source-inventory.py`. The tool invokes no compiler and does not
change ownership.

## Coverage

| Group | `.cpp` count | Current path |
|---|---:|---|
| All repository C++ files | 731 | Full list below |
| `src/` game translation units | 594 | 582 EXE, 12 subsystem-module DLLs |
| Recast sources compiled by `build.py` | 20 | EXE object files |
| **C++ translation units directly compiled into `mimita.exe`** | **602** | 582 `src/` + 20 Recast |
| C translation units directly compiled into `mimita.exe` | 22 | `src/glad.c` + 21 libjuice `.c` files; exact list in the EXE link-input report |
| Manifold sources listed by the existing Ninja static-library manifest | 18 | Build.py links `libmanifold.a`; this generated source manifest is dated 2026-09-30 and freshness is unverified |
| Other repository `.cpp` files | 99 | Launcher, development tool, header implementation, tests/demos, or other vendored code; not part of the canonical game build |
| `.cpp` sources across current game targets | 614 | 594 under `src/` (582 EXE + 12 subsystem DLL) + 20 Recast EXE sources |
| Additional sources listed by a saved linked-library manifest | 18 | Manifold Ninja manifest; archive freshness is unverified |
| **`.cpp` sources represented by game targets or saved link manifests** | **632** | 594 `src/` (including 12 module DLL sources) + 20 Recast + 18 manifest-only |

The repository `.cpp` inventory includes 731 unique paths; every path existed
when the report was written. The CSV has one row per path. The JSON contains each
source's direct includes, transitive repository-header scan, available
compiler-dependency snapshots, source hash/size, current target, proposed
target category, and per-file reload-risk locations. The CSV also has explicit
semantic-review columns for state ownership, callback/job quiescence, external
resource lifetime, safe-unload evidence, and cutover group. These intentionally
start as `NOT_REVIEWED`/`NOT_PROVEN`; the lexical scan does not complete them.

- [Sortable per-file CSV](cpp-source-inventory.csv)
- [Full per-file JSON and aggregate evidence](cpp-source-inventory.json)
- [First manually source-traced lifetime findings](cpp-lifetime-review.md)
- [Detached-worker blocker review batch 1](cpp-lifetime-batch-detached-20261112.md)
- [Owned-worker lifetime review batch 2](cpp-lifetime-batch-owned-workers-20261112.md)
- [Actual `mimita.exe` link-input inventory](mimita-exe-link-inputs.md)

## What the map says about DLL ownership

- Current `src/` ownership has no overlaps in the module ownership catalog:
  582 sources are EXE-owned and 12 are module-owned.
- `build_game_dll.py` compiles two files that are also assigned to the EXE:
  `src/effects/effect-part.cpp` and
  `src/hot-reload/quick-hit-debug-visual.cpp`. This is a duplicate target
  declaration, not whole-game reload.
- `CMakeLists.txt` declares a separate six-source `mimita-game` DLL target.
  The canonical `build.py` does not use that target. All six overlap current
  EXE ownership.
- The current source-to-target map classifies 629 game-source candidates,
  two hot-reload/runtime-owner files for review, one `src/main.cpp` bootstrap
  split, and the separate launcher exception. These are proposed migration
  labels, not implemented ownership changes.
- The canonical build script compiles 20 Recast `.cpp` files into EXE objects.
  It links `libmanifold.a`; the saved Ninja source manifest lists 18 Manifold
  `.cpp` files, but is dated 2026-09-30 and does not prove the archive was
  rebuilt from those sources. They are recorded as linked-library source
  candidates, not direct game-build translation units. Verify archive
  provenance before claiming their edits hot reload.

## Reload-lifetime leads

The lexical scan found 2,191 `static`/`extern` declaration candidates, 174
callback candidates, and 139 thread/async/job candidates across source files
and reachable project headers. Per-file findings include line numbers and code
snippets in the JSON and per-file counts in the CSV.

For the 632 `.cpp` files in this map (614 files across current app/game targets
plus 18 sources listed only by the saved Manifold manifest), automated triage
currently buckets 278 files with mutable `static`/`extern` candidates, 58 with
callback or thread/job candidates, 25 with static candidates whose declaration
scope needs review, and 271 with no lexical lifetime hit. These buckets are
prioritization only and may overlap at the code level; each file is assigned to
one bucket for the CSV. **All 632 still require semantic lifetime review.** The
22 C files directly compiled into the EXE are listed separately in the EXE
link-input report and also need lifetime review. A
`static` local can be safe, a callback match can be only a declaration, and no
lexical hit does not prove statelessness. CSV fields explicitly separate
heuristic triage from state-owner, quiescence, resource-lifetime, and safe
unload evidence.

Source review also anchors live state in the dedicated-server loop, listen
server state, `NpcSystem`/NPC records, `ServerNpc` projections,
`MultiplayerContext`, loaded `World` data, and the existing `HotReloadSystem`.
Those are clues to state ownership, not a complete field-level schema. Each
callback/job candidate still needs its capture, registration, cancellation,
join, and shutdown path traced before its module can unload safely.

## Dependency evidence and limits

The inventory found compiler `.d` snapshots for 603 sources; 128 have no
matching snapshot. **Zero snapshots were freshness-verified.** Their headers
are historical compiler evidence only. The Manifold Ninja manifest is also a
saved build configuration, not proof of the current static archive contents.
The current-source include scan found 3,101 unresolved-or-external spellings;
standard-library and platform headers,
conditional/macro includes, and true missing project includes are not separated
perfectly, so this count does not mean 3,101 broken includes.

The include graph is not a function-call or linker-symbol graph. The text scan
can miss macros, multiline constructs, and generated behavior. A candidate
`static` does not automatically mean persistent mutable state. This is a
complete source enumeration and a useful review map, but semantic lifetime,
ABI compatibility, safe unload, and runtime behavior remain unproven.

## Next mapping work

1. Confirm the source list against current canonical build commands and
   regenerate compiler dependencies for the current revision through the
   authorized build workflow.
2. For active sources in dependency-connected batches, replace heuristic
   signals with reviewed state owners, callback/job lifetimes, and ABI boundary
   decisions.
3. Resolve the six duplicate CMake target claims and the two focused DLL/EXE
   overlaps so each active source has one clear owner before routing ordinary
   edits to one `game.dll`.



