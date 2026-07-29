# First Remote Test Build Report

## Build Result: **SUCCESS**

## Exact Commands Run

| Step | Command | Result |
|------|---------|--------|
| 1. Clean old artifacts | `python build.py clean` | Cleaned (also ran game, user closed it) |
| 2. Remove objects + old exe | `Remove-Item ... build\obj; Remove-Item mimita.exe` | Cleaned |
| 3. Build release EXE | `python build.py release build-only` | **SUCCESS** — 390 compiled, 0 skipped, 349.87s |
| 4. Pack assets | `python devscripts/pack-assets.py` | **OK** — 414 files, 98.4 MB |
| 5. Build launcher | `launcher\build.bat` | **OK** — 1,064,960 bytes |
| 6. Generate manifest | `python devscripts/generate-manifest.py` | **OK** — 423 files, 115.8 MB |
| 7. Build installer | `iscc installer\setup.iss` | **OK** — 19.906s |

## Build Changes Made

### `build.py`

| Change | Reason |
|--------|--------|
| `-march=native` → `-march=x86-64-v2` (line 81) | Prevents SIGILL on CPUs without AVX2/AVX-512 |
| Added `-s` to release CXX_FLAGS (line 82) | Strips DWARF debug symbols from output |
| libjuice C flags now conditional on MODE (lines 482-493) | Release: `-O2`; Debug: `-O0 -g` (was always debug) |
| `build-only` no longer overrides release mode (line 67-69) | Fixed bug where `release build-only` built debug |

### `build_agent.py`

| Change | Reason |
|--------|--------|
| Passes `release` arg through to `build.py` when present (line 151) | Allows `build_agent.py release` to produce release build |

### `devscripts/build-all.bat`

| Change | Reason |
|--------|--------|
| Accepts `%BUILD_MODE%` arg (defaults to `release`) | Pipeline defaults to release builds |
| Passes mode to `build_agent.py %BUILD_MODE%` | |
| Added `python devscripts/pack-assets.py` step after game build | Ensures installer uses fresh `assets.pak` |

## EXE Size

| Build | Size | Reduction |
|-------|------|-----------|
| Previous (debug) | **454,776,640 bytes** (~434 MB) | — |
| **Release (new)** | **12,541,473 bytes** (~12 MB) | **97.3% smaller** |

## Installer Size

| File | Size |
|------|------|
| `installer\MimitaSetup-1.0.0.exe` | **47,129,133 bytes** (~45 MB) |

## SHA-256

```
49ca21016697009af732ea289fb0a6c0184979d14987afd7a5e4265a8f749d9e
```

## Packaged File List

| File | Size | In Installer | In Manifest |
|------|------|:---:|:---:|
| `mimita.exe` (release) | 12,541,473 | ✅ | ✅ |
| `MimitaLauncher.exe` | 1,064,960 | ✅ | ✅ |
| `version.txt` | 7 | ✅ | ✅ |
| `glfw3.dll` | 429,222 | ✅ | ✅ |
| `libgcc_s_seh-1.dll` | 922,573 | ✅ | ✅ |
| `libstdc++-6.dll` | 2,376,223 | ✅ | ✅ |
| `libwinpthread-1.dll` | 93,781 | ✅ | ✅ |
| `assets.pak` | 98,429,875 | ✅ | ❌ (uses individual assets/) |
| `shaders/` (6 files) | ~17,000 | ✅ | ❌ |
| `Characters/DefaultGuy/` (2 files) | ~10,700 | ✅ | ❌ |
| `config/` (~30 files) | ~50,000 | ✅ | ❌ (only 2 config files) |

## Installer Path

```
C:\important\mimita-priv-v8\installer\MimitaSetup-1.0.0.exe
```

## Ready to Send to One Remote Tester?

**YES**, with the following caveats:

1. Installer must be uploaded to `https://mimita.fun` or shared via file transfer (the launcher's update system expects it at `website/server/downloads/`)
2. The VPS coordinator must be running at `http://107.191.48.226:3001`
3. `MIMITA_TURN_PASSWORD` must be set on the VPS for TURN relay
4. Both host and remote client must be on the same game version (protocol 25)
5. The host should use the `--server` command or the in-game "Start Server" button
6. The remote client joins via room code in the Community menu

## Validation Checklist

- [x] Release EXE < 25 MB (12 MB ✅)
- [x] Debug symbols stripped (build uses `-s` flag ✅)
- [x] `-march=native` removed, `-march=x86-64-v2` used (broad CPU compat ✅)
- [x] `assets.pak` freshly built (414 files, 98.4 MB ✅)
- [x] Launcher builds successfully (1 MB ✅)
- [x] Update manifest generated (423 files, 115.8 MB ✅)
- [x] Installer builds successfully (45 MB ✅)
- [x] Installer includes all required files (checked ✅)
- [x] `devscripts/build-all.bat` updated for future builds (packs assets, defaults to release ✅)
- [x] `build_agent.py` accepts `release` arg (future agent builds ✅)

## Files Modified

| File | Change Type |
|------|-------------|
| `build.py` (lines 67-69, 81-82, 482-493) | Edit |
| `build_agent.py` (line 151) | Edit |
| `devscripts/build-all.bat` (lines 3, 8, 20, 29-37) | Edit |
