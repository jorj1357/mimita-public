# MiMITA live C++ edit-to-active latency investigation

**Purpose:** identify the best measured route toward fast edit-to-active C++
changes and document what the imported libraries can and cannot contribute.

**Audience:** the owner of `issue-136d4553ff92` and agents continuing the
isolated live-edit latency investigation.

**Evidence captured:** 2026-11-21 UTC.

**Scope:** current MinGW/game-DLL pipeline, existing live runtime journal, and
the imported Blink, `cr.h`, and Flecs dependencies.

**Does not:** change the compiler or game architecture, claim a new edit was
activated, or establish a latency SLO from a small sample.

## Executive finding

The imported libraries do not combine into a ready-made solution for
“edit C++ → intended behavior appears in the same running game.” None removes
the current build cost by itself:

- **Blink** can patch changed functions after compiling their translation
  unit, but its documented Windows workflow depends on MSVC PDB debug data and
  `/ZI`; it does not fit the active MinGW build as-is.
- **`cr.h`** can reload a rebuilt DLL and transfer supported static state, but
  it does not compile source. MiMITA already has a DLL loader/module host, so
  adopting it before measuring the current loader would risk duplicating the
  same responsibility without reducing the dominant compile work.
- **Flecs** makes entity/component composition dynamic. It does not reload C++
  functions or migrate arbitrary changed C++ layouts.
- **Ninja** can execute an accurate incremental dependency graph; it cannot
  determine a wrong dependency graph, make an invalidated translation unit
  compile faster, or patch a running process. `ninja`, `cl`, and `clang-cl`
  were not resolvable from the current command search path.

The strongest measured successful same-day DLL attempt took **620.249 s** and
compiled **170** translation units while skipping **472**. Its worker phases
recorded **418.840 s compiling**, **86.354 s scanning the object cache**, and
**34.736 s linking**. Other same-day attempts took **338.688–810.257 s** and
failed; a further attempt took **2,309.492 s** and failed during changed-source
preflight. A separate fresh attempt failed after **41.218 s** in packet
registry codegen, before compiling game sources. The bottleneck therefore
varies by attempt; “DLL reload” alone is not an adequate explanation.

The current best direction is to first make the existing build’s invalidation
and per-translation-unit decisions explainable, then measure a warm one-`.cpp`
change through compile, link, activation, and first behavior event. If the
large DLL link remains expensive, move frequently edited policy into a dynamic
behavior layer or a small number of measured coarse modules. Keep native C++
for mechanisms and performance-sensitive code. The existing deferred hot-layer
spec already describes this split and warns that 50 changed C++ translation
units cannot meet a one-second target by compiling them on save.

## Measured build evidence

All timings below come from root dev-loop per-attempt JSON/JSONL records. Build
success is not runtime proof.

| Attempt | End-to-end | Build result | Observed work |
|---|---:|---|---|
| `game-dll-20261121T134938183745Z-c0c1da` | 620.249 s | Published generation 420 | 170 translation units compiled; 472 skipped; object scan 86.354 s; compile 418.840 s; link 34.736 s. |
| `game-dll-20261121T155427526457Z-6240d5` | 810.257 s | Failed at link (47.206 s) | 215 compiled and 427 skipped; changed-source preflight 21.471 s; object-cache scan 109.188 s; compile 594.109 s. The linked review for this attempt reports only `src/actor/player-ai.cpp` as the changed `.cpp`, but the log does not explain a verified invalidation reason for every compiled translation unit. |
| `game-dll-20261121T162506366821Z-0079b9` | 338.688 s | Failed at link (33.688 s) | 28 compiled and 614 skipped; object-cache scan 125.656 s; compile 59.640 s. |
| `game-dll-20261121T150703129448Z-23d2a4` | 748.941 s | Failed compilation | Four translation units failed; the worker reported 214.666 s in its compile-failure summary. Its recorded changed-source preflight was 128.022 s and object-cache scan 180.831 s. |
| `game-dll-20261121T034654937752Z-3c82d2` | 2,309.492 s | Failed in changed-source preflight | Preflight recorded 1,488.033 s; packet-registry codegen 224.529 s; manifest snapshot 224.122 s; source snapshot 296.796 s. The available record does not establish why those individual phases stretched. |
| `game-dll-20261121T164228470355Z-4ae172` | 41.218 s | Failed at packet-registry codegen | The codegen check ran 40.312 s and failed. No game C++ translation units were compiled and no new generation was produced. |

The successful attempt demonstrates that the current pipeline can skip many
units, but it does not show that a one-file edit only rebuilds necessary
dependents. The 810-second attempt is a concrete counterexample to assuming
that unchanged work is cheap. The records expose skip/compile outcomes, but
they do not consistently give a useful stale/invalidation reason per
translation unit. A correct header dependency can justify rebuilding
dependents; an unexplained compile cannot be called necessary.

These attempts are not matched save-to-runtime samples. There is no valid
edit-to-first-event p50/p95 from them: most attempts failed, and the successful
attempt is not correlated here to a fresh same-process behavior event.

The 15:54 attempt's worker input-validation row reports 643 sources, while its
recorded translation-unit outcomes total 642 (215 compiled plus 427 skipped).
The current generated manifest reports 643 game-owned sources. This one-unit
reconciliation is unresolved in the inspected evidence. Treat the exact source
revision and manifest identity as part of every matched benchmark rather than
comparing counts from different attempts as if they were one snapshot.

## Live runtime evidence inspected

I inspected the existing server process and its exact journal read-only; I did
not launch another game or request another build.

- Run: `20261121_144350`
- Process: server PID `7556`, still alive in the latest dev-loop snapshot
- Executable build ID: `2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871`
- Run manifest marks `build_match=false`; source generation is `0` and the
  published EXE is stale relative to the current source.
- The run has a server and **no client**, so it cannot establish visible
  player behavior or human acceptance.
- The live server journal records NPC owner events from `src/npc/npc.cpp` with
  `module_generation=420`. At `2026-11-21T16:46:56.468Z`, one sample reports
  `performance.server-tick-stall` at 509.223 ms total, with 42 NPCs and
  405.488 ms attributed to NPC work. Nearby NPC phase telemetry reports a
  440.437 ms phase-window total. These are actual runtime observations from
  this loaded run, not measurements of an edit or reload and not a clean
  performance baseline.
- No matching `game.generation.activated` record was found in the searched
  current run journals. The generation-420 NPC events prove that NPC code is
  executing in the process; they do not prove that the current source or a new
  edit was activated.

Thus there is live runtime telemetry to anchor the report, but **no controlled
edit-to-visible-result test**. Current dev-loop state reports the executable
stale, no client, and the latest candidate build failed before C++ compilation.
Starting a test against that stale executable would not prove the current edit
path. Build evidence, runtime evidence, and human review remain separate.

## Current owners and path

The current source manifest marks `src/npc/npc.cpp` as `game` owned, so a
normal edit belongs in the replaceable whole-game `game.dll` path. The same
manifest marks `src/hot-reload/hot-reload-system.cpp` and
`src/hot-reload/module-host.cpp` as resident mechanisms. The whole-game DLL
worker consumes hundreds of game-owned translation units; it is not equivalent
to the small focused DLL in `src/hot-reload/hot-reload-system.cpp`.

Relevant owners to measure in the next controlled run:

- `build_toolchain.py` / `build.py`: MinGW tool selection and the incremental
  EXE build path; the candidate game-DLL worker is separate.
- `devscripts/dev_loop/game_build.py`: root dev-loop request/coherence and
  publication orchestration.
- `devscripts/game_build_worker.py`: source scan, dependency decisions,
  compilation, linking, and worker phase records.
- `build/source-manifest.json` and `config/source-owners.json`: current source
  inventory and ownership; regenerate the manifest through its owner.
- `src/hot-reload/module-host.cpp` / `.h`: existing generic module catalog,
  reload, safe-point activation, and event path.
- `src/hot-reload/hot-reload-system.cpp` / `.h` and `game-api.h`: focused
  effect/debug-visual DLL experiment. That implementation watches only its
  small source list and invokes `python build_game_dll.py` directly; it is not
  evidence of the root dev-loop whole-game build path and must not be adopted as
  a second builder.

## Dependency and tool fit

Exact URLs, commit pins, and licenses are recorded in
[`live-cpp-dependencies.md`](../specs/hotreload/live-cpp-dependencies.md).

| Tool | What it could contribute | Why it does not solve the whole problem |
|---|---|---|
| Blink | Function-level replacement after compiling a changed source file; shared globals keep their existing addresses; optional synchronization callbacks. | Its upstream README requires PDB-backed source command discovery and recommends MSVC `/ZI`; MiMITA currently selects MinGW `g++`. A changed TU still must compile. It does not migrate arbitrary object layouts. A separate compiler compatibility proof is required before a Blink prototype. |
| `cr.h` | DLL reload, load/unload callbacks, rollback support, and guarded static-state transfer through `CR_STATE`. It is the closest API shape to MiMITA's current DLL experiment. | The DLL must already be rebuilt; `cr.h` is not an incremental source compiler. Its host/guest ABI would overlap MiMITA's existing module host. State-size/address validation is not schema conversion. Compare its reload cost and safety against the existing host before choosing either. |
| Flecs | Runtime addition/removal of components on entities, and ECS storage for a future dynamic state model. Its core is C99; its C++ API requires C++17. | It does not replace C++ functions or migrate compiled component layouts when a C++ struct changes. MiMITA does not currently use Flecs as authoritative game state, so adoption would be a large architecture decision. |
| Ninja | Runs changed build rules from a precise dependency graph and may reduce repeated orchestration/scan overhead if its graph is correct. | It is not a compiler, patcher, or cache by itself. The current process must first expose why units are invalidated; otherwise Ninja just executes the same oversized rebuild accurately. It was not found on the current PATH. |
| MSVC / `clang-cl` | MSVC is Blink's documented toolchain. `clang-cl` is a possible isolated PDB/COFF compatibility candidate. | The existing production toolchain is MinGW. The availability check did not resolve `cl` or `clang-cl`; Visual Studio directory presence does not prove the required compiler, SDK, or Blink compatibility. No compiler change is recommended from this evidence. |

The existing deferred language/latency spec lists C#/.NET Hot Reload,
Cling/LLVM ORC, AngelScript JIT, LuaJIT, TinyCC, and a JSON/DSL layer as future
bake-off candidates. They are not imported or evaluated here. That spec's
important architectural point is that a 50-file, one-second target cannot route
all changed files through a native C++ compile and DLL link. It calls for a hot
behavior layer plus a small number of stable native modules, with performance
and real-game acceptance measured separately.

## Recommended route to the goal

1. **Establish a trustworthy one-file baseline.** When the root dev-loop has a
   coherent source generation and a current runnable EXE, run matched warm
   no-op, one `.cpp` function-body edit, and high-fan-out header samples. Record
   intent, save, request, queue, worker, every phase, per-TU compile/skip
   reason, link, publish, same-process activation, and first owner event. Use
   the root dev-loop only.
2. **Fix the proven bottleneck for each sample.** If a body edit changes one
   TU but many are rebuilt, report the dependency or identity reason for each
   one and correct overly broad invalidation only after checking freshness.
   If scan/preflight dominates, optimize the index and snapshots. If compile
   dominates, avoid recompiling behavior code on save or reduce translation
   unit cost. If link dominates, compare the existing linker and a small
   capability module. Do not treat a cache hit, published DLL, or successful
   build as proof of live behavior.
3. **Keep flexible decisions out of the large native DLL.** For frequently
   edited NPC policy, prototype a hot behavior layer or a measured coarse
   module with host-owned, versioned state. The current hot-layer spec's
   language bake-off is the route for edits that must be active in about one
   second without invoking a C++ compiler. Keep physics, collision, and other
   measured performance-critical mechanisms native.
4. **Evaluate imported libraries only where they target the measured cost.**
   Compare `cr.h` to `ModuleHost` only if loader/reload work is material after a
   small module build. Evaluate Blink only in a separate compiler compatibility
   prototype, because it cannot currently consume MiMITA's MinGW PDB-less
   build. Defer Flecs until there is an approved dynamic-state model. Try Ninja
   only after comparing its scan/scheduling overhead against the existing
   cache and dependency graph.
5. **Add explicit state versioning before layout changes.** Keep long-lived
   match state in resident/host-owned storage behind versioned handles or
   serialized records; migrate with prepare/commit/reject callbacks and keep
   the old generation/state available for rollback. None of the imported
   projects provides arbitrary safe C++ object-layout migration.

## Latency targets and how to measure them

Treat 0.1 s, 1 s, and 5 s as **targets to test by change class**, not guarantees
from these dependencies:

- **0.1 s:** plausible only for a small already-compiled policy/data update or
  a prevalidated activation. It is not supported by current native compile/link
  evidence as a general `.cpp` save target.
- **1 s:** align with the deferred hot-layer target by avoiding native C++
  compile/link on the flexible behavior path. Measure cold and warm behavior,
  p50/p95 over at least ten samples, and steady-state game tick cost.
- **5 s:** an initial aspirational target for a warm one-function edit using a
  genuinely small compiled module. Current native translation-unit examples
  and large links do not establish that it is attainable yet.

For each accepted sample, use `edit_to_evidence_ms`: the time from saved change
to the expected event in the exact pinned process journal. Keep separate
columns for compile, link, publish, activation, and first visible/result event.
Exercise the same NPC behavior before and after, retain the same PID/session
when claiming hot activation, verify the changed source resolves to the game
owner, and record rollback/reload safety. Report p50 and p95 per change class;
do not average a configuration reload together with a cold EXE change.

## Work not performed and next proof gate

No source, build configuration, compiler, or game architecture was changed.
No new build or game process was started. The evidence in this report is
read-only analysis of existing dev-loop attempts and a live server journal.

The next proof gate is a current, non-stale executable plus a coherent
published candidate DLL and a client/server test run. Then perform one small
NPC behavior edit, observe the exact compile set and activation in the same
server PID, and inspect the first owner event and visible result. If the normal
build cannot provide a current candidate, record that failure and its stage;
do not substitute the old running server's generation-420 telemetry as proof
of a new edit.
