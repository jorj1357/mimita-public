// 2026-11-21
/* purpose
* record the pinned live C++ development dependencies and their verified limits
* help the next isolated hot-reload prototype choose a bounded integration path
* this file DOES NOT authorize a compiler switch or game-architecture migration
* this file DOES NOT claim runtime compatibility, layout migration, or latency
*/

# Live C++ dependency import and compatibility

Audience: contributors planning a bounded live C++ development prototype.

This record describes three imported upstream repositories and their fit for
MiMITA experiments. The repositories are pinned as Git submodules in
`.gitmodules`; the root Git tree records each checked-out commit. No dependency
is added to the active game build by this import.

## Source and license manifest

| Dependency | Upstream | Pinned commit | License in upstream checkout | Build/runtime notes |
|---|---|---|---|---|
| blink | <https://github.com/crosire/blink.git> | `19b1560a8bcfb4ccdcd62cfc52ea904ca868025a` | BSD 2-Clause, `external/blink/LICENSE.md` | Standalone Windows tool; its README requires Windows SDK and MSVC 2017 or later with C++17. |
| cr | <https://github.com/fungos/cr.git> | `1c3f8302320dee8206cf85d6aeb9e9a9cd78b527` | MIT, `external/cr/LICENSE` (license terms also embedded in `external/cr/cr/cr.h`) | Header-only host/guest reload support; current upstream documents Windows and a CMake integration. The guest is a DLL; no separate runtime DLL is required by the API. |
| Flecs | <https://github.com/SanderMertens/flecs.git> | `85188ed19f971c201af61c26143fa69877fe418b` | MIT, `external/flecs/LICENSE` | C99 core, optional C++17 API. On Windows, non-MSVC builds may need `dbghelp` and `ws2_32`; Flecs can be compiled into the program or provided as a library. |

Keep each upstream license file with its submodule. Reproduce these sources with
`git submodule update --init --recursive`; the root tree pins the commits above.

## Compatibility comparison

| Capability | blink | cr.h | Flecs |
|---|---|---|---|
| Windows | Yes; Windows-only tool | Yes; Windows is an upstream tested target | Yes |
| Compiler fit | Requires MSVC-style PDB/debug information; documented build is MSVC 2017+, C++17 | Guest DLL can use a Windows compiler/toolchain; no MSVC-only PDB dependency is documented | GCC/MinGW, Clang, or MSVC; compile core as C99 and use C++17 or later for its C++ API |
| Requires extra DLLs | No dependency DLL stated; blink is an external injector/watcher executable | Uses the game/plugin DLL being reloaded; no second runtime DLL required | No DLL required; can be statically linked or compiled into the application |
| Updates existing functions | Yes, patches changed functions to jump to their replacements | Reloads a whole plugin/DLL; it does not patch individual function entries | No code reload facility |
| Adds/removes functions | Not a general live ABI/schema migration. Recompiled references can resolve new symbols, but existing callers, function pointers, and references to removed code require care | A rebuilt DLL may change its internal functions; exported entry points and host ABI must remain valid | Not applicable to code; systems and component types can be registered or removed at runtime subject to ECS rules |
| Changes object layouts | No general migration. It preserves addresses for matching globals, which is not a transformation of existing objects | No general migration. It has automatic static-state transfer, but changes to state addresses/sizes are safety-sensitive and user callbacks must manage state | Supports adding/removing component data on entities. It does not rewrite compiled C++ layouts or automatically migrate a component whose C++ definition changes |
| Persistent game state | Existing global addresses are reused; optional `__blink_sync` and `__blink_release` callbacks let the app coordinate | Host context persists; `CR_LOAD`/`CR_UNLOAD` callbacks and `CR_STATE` transfer support static state. Application-owned migration is still needed for schema changes | World/entity/component values persist while the same Flecs world remains alive; save/load and schema conversion are application concerns |
| Automatic rebuild | Watches source and derives compiler commands from PDB data; can also watch object files with `--no-compile` | Detects changed/rebuilt plugin DLL; compilation is external | None; use a build system such as Ninja |
| Principal risk | PDB and `/ZI` workflow conflicts with current MinGW build; function patching and process injection need careful runtime validation | Whole-DLL swap is closer to MiMITA's current experiment, but requires an adapter/export seam and safe tick-boundary activation | ECS can make data composition dynamic, but is a broad game-state architecture decision and does not make native C++ functions hot |

### blink compiler/debug-symbol blocker

MiMITA currently selects MinGW `g++` in `build_toolchain.py` (with `clang++`
only as a fallback if no `g++` is found). blink locates a matching PDB in the
executable, reads source compilation commands from that PDB, and expects
MSVC-compatible COFF objects. Its README specifically recommends `/ZI` when
command-line discovery fails. The active build does not currently produce that
PDB format or use MSVC Edit-and-Continue flags. Therefore blink is not an
integration candidate for the current executable without a separately approved
MSVC or compatible `clang-cl` build experiment. `clang-cl` compatibility with
blink's PDB reader and the game's flags remains unverified; do not infer it from
Clang's general PDB support. Switching the production compiler is out of scope.

Function replacement only takes effect for future function entries. Code
already executing in a replaced function continues until it returns. Global
state preservation by address does not make changed C++ object layouts safe.

## Build-system options

The current build uses MiMITA's incremental object cache and `ccache`; do not
replace it during dependency import. Ninja is a build executor, not a compiler
or live patcher. With an accurate generated dependency graph it can schedule
only invalidated object rules, but it does not avoid recompiling a translation
unit when one of its inputs changes, migrate runtime state, or guarantee a
one-second save-to-activation path. Evaluate it later against the root dev-loop
and current cache rather than creating a competing build watcher.

MSVC is needed for blink's documented workflow. `clang-cl` is a possible
separate compatibility investigation because it can target the MSVC ABI and
emit PDB data, but blink integration remains unproven. Ordinary Clang/MinGW
does not produce the exact MSVC PDB workflow blink expects. Neither compiler
should be selected for MiMITA's active build by this import.

## Recommended first prototype

Start with one NPC behavior function and a dedicated candidate module while
leaving the active game architecture and MinGW configuration intact. Keep the
NPC's durable state in host-owned storage with an explicitly versioned payload;
pass it through a narrow C ABI callback. Build the candidate through the root
dev-loop, activate only at a safe simulation boundary, and record compile,
activation, and first-visible-result times plus an owner event in that exact
run's journal.

Use the existing game-DLL experiment as the baseline and compare it with a
small `cr.h` host/guest adapter. `cr.h` is the most relevant first dependency
because it reloads a DLL and is not tied to PDB. Keep it an experiment: it is
not a drop-in replacement for `HotReloadSystem`, and its static-state copying
must not be treated as arbitrary layout migration. Test a real NPC function,
rollback, repeated reload, and retained state before expanding scope. Do not
start with Flecs migration or Blink compiler changes. The approximately one
second target remains a measurement goal, not a dependency guarantee.

## Likely MiMITA files for the next isolated prototype

Confirm ownership and generated inventories at prototype time; this is a
candidate list, not authorization to edit all of these files in one migration.

- `src/npc/npc.cpp` and, only if required by the selected behavior, its owning
  NPC header. Choose one frequently called function with a visible result.
- `src/hot-reload/hot-reload-system.cpp` and
  `src/hot-reload/hot-reload-system.h` for the existing focused loader/rebuild
  seam, or the generic `src/hot-reload/module-host.cpp` and `.h` if the chosen
  owner is an existing module.
- `src/hot-reload/game-api.h` and the candidate DLL's existing export source
  for a versioned test ABI and explicit state handoff.
- `build_game_dll.py` / `devscripts/dev_loop/game_build.py` only if the
  dev-loop cannot build the isolated candidate through its supported request
  path. Prefer making a root dev-loop request over adding a second coordinator.
- `config/source-owners.json` and generated `build/source-manifest.json` only
  after confirming the chosen function's owner. The generated manifest must be
  regenerated by its owning workflow, never hand-edited.
- `devscripts/build_toolchain.py` and `devscripts/game_build_worker.py` only
  for a later Blink/MSVC compatibility prototype; they are not part of the
  recommended first prototype.

## Blockers and risks

- MiMITA's existing `src/hot-reload/hot-reload-system.cpp` rebuilds its focused
  DLL by invoking `python build_game_dll.py` directly. The required next proof
  must use the root dev-loop authority; this import leaves that code untouched.
- The current focused DLL API exports effect/debug-visual functions, not NPC
  behavior. A one-function NPC prototype needs an explicit boundary and state
  ownership decision.
- A raw C++ object containing pointers, STL containers, callbacks, or jobs is
  unsafe to copy across a DLL generation. Use versioned, host-owned state and
  quiesce callers before unloading old code.
- DLL replacement does not make header edits cheap: all affected translation
  units may still need rebuilding. The small-function prototype must report
  the actual dependency fan-out and wall-clock result.
- Blink requires a different debug-symbol/toolchain path and uses code patching
  and remote-process mechanisms. This raises compiler compatibility, security,
  unwind/exception, thread-safety, and function-pointer risks.
- Flecs introduces an ECS representation that MiMITA does not currently use as
  its authoritative gameplay state. Importing it does not justify an ECS
  migration.
- None of the three projects guarantees arbitrary struct migration or
  approximately one-second save-to-visible activation.

## Scope verification

This import changes `.gitmodules` and adds the three pinned external submodule
entries plus this documentation and its session changelog. It does not edit the
active compiler selection, build worker, game source, hot-reload implementation,
dev-loop, source ownership, runtime configuration, or game architecture. No
game build or additional game process was started.
