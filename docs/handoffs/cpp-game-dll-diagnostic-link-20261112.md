# Whole-game DLL diagnostic link (2026-11-12)

**Status:** diagnostic only. The whole-game `game.dll` candidate compiled and linked far enough to enumerate missing symbols; it did not produce a usable DLL. This is not runtime evidence.

**Build-authority exception:** this chat's human owner explicitly authorized the game-DLL worker builds for this investigation. This is a session exception, not a standing agent-build permission. The standing target is Option C (a dev-loop `game-dll` request), which is documented but not implemented.

## Command

```powershell
python devscripts/game_build_worker.py `
  --manifest build/source-manifest.json `
  --revision 5f3adf5749b2396a7a039e3f9c9348e467becc1b6382bf38e9c95e3fc9cae639 `
  --out build/game --priority high
```

- Revision: `5f3adf5749b2396a7a039e3f9c9348e467becc1b6382bf38e9c95e3fc9cae639`
- Sources: 575 `owner == game`
- Jobs: 12 (all logical cores)
- Priority: high
- Object cache: `build/game/objects/b10e3a31b3ae8637`

## Fast incremental result

First full build: 575 compiled (no compile errors) in ~11 minutes serial/12-way.

After fixing the `.d` parser to ignore `-MP` phony targets and adding the PCH/zero-byte checks:

```text
first rerun:  total=575 completed=575 compiled=10 skipped=565 failed=0
              compile=18.2s link=12.0s total=25.9s
second rerun: total=575 completed=575 compiled=0  skipped=575 failed=0
              compile=0.0s link=15.3s
```

The worker now prints `[SKIP]` / `[CXX ]` per file and writes live `build/game/progress.json` and `build/game/progress.log`. A no-change run compiles nothing and reaches the link in about 15 seconds.

## Link result

```text
undefined references: 3398 total, 389 unique
multiple definitions: 0
```

Missing symbols by family:

| Family | References |
|---|---:|
| `StructuredLogger` (resident logging) | 2100 |
| `MimitaNet` (transport/network) | 363 |
| `ModuleHost` (resident module host) | 219 |
| other | 144 |
| audio services (`AudioManager`/`MusicManager`) | 142 |
| effect system (`EffectPartSystem`) | 94 |
| GLAD (vendored C) | 87 |
| audio helpers (`playWorldSound`, `computeImpactAudio`, listener) | 74 |
| ICE transport (`IceAgent`) | 59 |
| duel queue (`DuelQueue`) | 43 |
| process globals (`gpWorld`, `gpNpcSystem`, `gpMpContext`, …) | 36 |
| `HotReloadSystem` | 18 |
| `LogManager` | 10 |
| `Renderer` (device) | 9 |

Top individual symbols:

| Symbol | References |
|---|---:|
| `StructuredLogger::instance()` | 1054 |
| `StructuredLogger::writeEvent(...)` | 886 |
| `MimitaNet::nowMs()` | 243 |
| `ModuleHost::instance()` | 111 |
| `StructuredLogger::shouldLog(...)` | 57 |
| `playWorldSound(...)` | 48 |
| `StructuredLogger::writeFormatted(...)` | 39 |
| `MusicManager::instance()` | 36 |
| `EffectPartSystem::spawn(...)` | 34 |
| `DuelQueue::instance()` | 28 |
| `AudioManager::instance()` | 24 |
| `StructuredLogger::write(...)` | 24 |
| `recordCrashBreadcrumb(...)` | 22 |

Full unique list: `docs/handoffs/game-dll-undefined-symbols-20261112.txt`.

## What this proves

- All 575 `game` sources compile into DLL-flavored translation units; there are no compile errors.
- The whole-game link is blocked only by unresolved **resident symbols and vendored objects**, not by source syntax or duplicate definitions.
- The fast loop works: after the parser fix, a no-change run skips 565–575 files and reaches the link in seconds.
- Logging is the dominant seam (2100 references), exactly as the service map predicted.

## What this does not prove

- That the DLL runs, loads, activates, or preserves state.
- That the missing symbols are the only blockers; new ones can appear after the first groups are linked.
- That the current `game.dll` ABI is complete.

## CAP_LOG result (2026-11-12)

`CAP_LOG` is implemented as a versioned capability plus a game-side
`StructuredLogger` bridge:

- `HostServicesV1::get_capability(HostCapabilityId::Log)` returns a bounded
  `LogCapabilityV1` (`text`, `event`, `should_log`).
- The resident EXE forwards events to the canonical `StructuredLogger`.
- `src/game/host-log-bridge-module.cpp` defines the `StructuredLogger` symbols
  inside the game generation and forwards to the host. It opens no second journal.

Measured effect:

| Metric | Before CAP_LOG | After CAP_LOG |
|---|---:|---:|
| Undefined references | 3398 | **1247** |
| Unique symbols | 389 | **372** |
| `StructuredLogger` references | 2100 | **0** |
| Compile (incremental) | 10 files / 18.2 s | 3 files / 13.0 s |
| Link | 12.0 s | 12.2 s |

The probe DLL also links with the bridge (`build/probe/npc-target/generation-4/game-g4.dll`).
The `HostServicesV1` version is now 2; the EXE-side `host-runtime.cpp` was compiled
standalone to verify it. A no-change relink after the compat bridge skipped all 578 game
sources (0 compiled, 15.2 s link).

The compat bridge then added game-side `MimitaNet::nowMs` and `recordCrashBreadcrumb`, reducing
undefined references to **554 / 300 unique**. `nowMs` and `recordCrashBreadcrumb` are 0.

Remaining top families: `ModuleHost` 76, audio 216, `MimitaNet` 95, effects 94,
GLAD 87, ICE 59, duel 43, process globals 36, `LogManager` 10, `HotReloadSystem` 18,
`Renderer` 9.

## Next worklist (ordered by reference count)

1. ~~**Logging capability** (`CAP_LOG`)~~ — done; `StructuredLogger` is 0.
2. **Network/transport** (`CAP_TRANSPORT`): 313 + 59 + 43. Opaque payload/session capability or resident transport imports.
3. **Module host** (`CAP_MODULES` resident bridge): 219.
4. **Audio services/helpers** (resident bridge or capability): 216.
5. **Effect system** (resident effects or capability): 94.
6. **GLAD vendored object**: 87. Add the GLAD object to the DLL link or import dispatch from the host.
7. **Process globals**: 36. Replace with stable handles or resident accessors.
8. **Crash breadcrumb / LogManager / HotReloadSystem / Renderer**: resident bridges/imports (31 + 18 + 9).

After each group, relink. The incremental cache makes this a seconds-scale loop.

## Evidence separation

- Source: 575 game sources compile as DLL TUs.
- Build: link failed with the counts above; no usable DLL.
- Runtime: not run.
- Human review: build-authority exception recorded for this chat only.
