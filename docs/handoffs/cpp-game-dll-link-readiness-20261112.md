# Game DLL link readiness (2026-11-12)

**Status:** readiness check only. No build, link, or run was performed by an agent. The root dev-loop owner or human runs the link.

## Verified without building

- Compiler resolves: `C:\important\msys64\mingw64\bin\g++.exe`
- ccache resolves: `C:\ProgramData\chocolatey\bin\ccache.EXE`
- GLFW include: `C:\important\glfw-3.4.bin.WIN64\include`
- GLFW lib: `C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64`
- Probe source set resolves: revision `9e24f2f11add85e2d23bc64bf6c6072024c538b31c5bae115cf8470f627109bf`, 2 sources:
  - `src/game/probe/npc-target-probe-module.cpp`
  - `src/npc/npc-targeting.cpp`
- No existing `build/probe/npc-target` artifact.
- Dev-loop status: `build_succeeded`, `run=(none)`; the latest EXE build does not prove the probe DLL exists.

## Owner-run: probe DLL link (ready)

```powershell
python devscripts/game_build_worker.py --sources-file config/probe/npc-target.json `
  --out build/probe/npc-target
```

Expected on success:

```text
[GAME BUILD] generation=1 revision=9e24f2f... compiled=... skipped=... dll=build/probe/npc-target/generation-1/game-g1.dll
```

Artifacts:
- `build/probe/npc-target/generation-1/game-g1.dll`
- `build/probe/npc-target/current.json`

Then the probe run (owner):

```powershell
$env:MIMITA_GAME_PROBE = "1"
$env:MIMITA_GAME_DLL = (Get-Content build/probe/npc-target/current.json | ConvertFrom-Json).dll
```

Run the full client/listen server and check the journal for `game.host.probe.started` and `npc.target.probe`.

## Whole-game diagnostic link (completed)

The diagnostic 575-source link was run under a session build-authority exception. All 575 sources compiled; the link failed with **3398 undefined references / 389 unique / 0 duplicate definitions**. Full classification: `docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md`.

The worker now has a persistent object cache, parallel all-core compile, high priority, and live `[SKIP]` / `[CXX ]` progress. A no-change rerun skips all 575 files and reaches the link in ~15 seconds (0 s compile, 15.3 s link), instead of a full rebuild.

This proves the compile side and gives the missing-symbol worklist. It does not produce a usable DLL: resident services, vendored GLAD, network/audio/effects, and process globals still need capabilities/imports.

## Build authority

Agents must not invoke build scripts, `ccache`/compiler commands, or dev-loop build requests. The owner runs the command above and reports the exact output and artifact hash. The probe runtime proof remains separate from link success.
