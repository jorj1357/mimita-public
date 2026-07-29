# Mimita Executable Size Investigation

## Executive Summary

- Raw executable size: **433.71 MB** (454,776,640 bytes)
- ZIP-compressed size: **86.98 MB** (91,200,756 bytes)
- Compression ratio: **20.05%** (5:1)
- Most likely primary cause: **DWARF debug information embedded in the EXE**
- Confidence: **99%**
- Is this currently blocking launcher distribution?: No — the installer compresses to 108 MB and the launcher can serve a release build
- Recommended immediate action: **Build with `--release` flag to produce a ~14 MB executable**, or add stripping to the debug build

## Largest PE Sections

| Section | Raw Size | % of EXE | Likely Contents |
|---|---:|---:|---|
| `.debug_info` | 380.16 MB | 87.7% | DWARF type information, variable/function declarations |
| `.debug_loclists` | 18.60 MB | 4.3% | DWARF variable location lists |
| `.debug_line` | 10.73 MB | 2.5% | DWARF line number mappings |
| `.debug_str` | 4.09 MB | 0.9% | DWARF string table (source paths, symbol names) |
| `.debug_abbrev` | 2.11 MB | 0.5% | DWARF abbreviation table |
| `.debug_line_str` | 1.14 MB | 0.3% | DWARF line strings |
| `.debug_frame` | 0.91 MB | 0.2% | DWARF call frame information |
| `.debug_aranges` | 0.63 MB | 0.1% | DWARF address ranges |
| `.debug_rnglists` | 0.79 MB | 0.2% | DWARF range lists |
| `.text` (code) | 4.17 MB | 1.0% | Executable machine code |
| `.rdata` (ro data) | 1.10 MB | 0.3% | Read-only data, constants, strings |
| `.pdata` + `.xdata` | 0.49 MB | 0.1% | Exception handling data |
| `.data` (rw data) | 0.19 MB | 0.0% | Read-write initialized data |
| Other (`.reloc`, `.idata`, `.tls`, `.eh_frame`) | < 0.05 MB | 0.0% | Relocation, import, TLS tables |

**Debug total: 419.16 MB (96.6%)**
**Non-debug total: 5.99 MB (1.4%)**
**Estimated code + data without debug: ~14.5 MB** (accounting for PE overhead and alignment)

## Largest Build Contributors

| Category | Size | Evidence |
|---|---:|---|
| DWARF debug (all `.debug_*` sections) | 419.16 MB | PE section headers via `dumpbin /headers` |
| Precompiled header (`pch.h.gch`) | 127.53 MB | On-disk; not linked into EXE |
| Object files (`build/obj/*.o`) | 899.79 MB total | Each `.o` has its own copy of DWARF info |
| Hot-reload DLL (`build/mimita-game.dll`) | ~175 MB | Separate file, not embedded in EXE |
| Static library code linked into EXE | ~4 MB (estimated) | MinGW runtime (libgcc, libstdc++, libwinpthread) |

## Embedded Asset Findings

- **`assets.pak` is NOT embedded.** The string `assets.pak` appears once in the EXE — it is a source-code reference, not the 182 MB PAK file.
- **No assets are embedded.** A binary scan found zero actual PNG, JPEG, GLB, WAV, or OGG file headers. String matches (`.png`, `.obj`, `.json`, etc.) are all source file path references within the DWARF `.debug_str` section.
- **No content duplication.** All asset files are standalone files on disk (`assets.pak`, `Characters/`, `config/`, `shaders/`).

## Debug and Linker Findings

- **Build type:** Debug (default mode in `build.py`, line 60: `MODE = "debug"`)
- **Compiler:** MinGW-w64 GCC 15.2.0 (posix-seh, winlibs distribution)
- **Debug format:** DWARF (GCC `-g` flag produces DWARF debug info)
- **Debug location:** Embedded in the EXE as PE sections (`.debug_info`, etc.) — no separate PDB file
- **Optimization:** `-Og` (optimize for debugging, minimal optimization)
- **Dead-code removal:** Not enabled — no `-Wl,--gc-sections` or `-ffunction-sections -fdata-sections` in linker flags
- **Identical code folding:** Not applicable (MinGW/GCC toolchain, no `/OPT:ICF`)
- **Static linking:** MinGW runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) are used as DLLs, not linked statically
- **Whole-archive behavior:** Not present in the build script
- **Release mode** exists in `build.py` lines 77-85: no `-g`, uses `-O2`

## Repository Size vs Executable Size

Large repository files that **do NOT** affect executable size:
- `assets.pak` (182 MB) — loaded at runtime, not embedded
- Replay files (~200+ files, various sizes) — runtime output, not compiled in
- Logs — runtime output
- `.obj` map source files — loaded at runtime for map collisions
- `pch.h.gch` (127 MB) — precompiled header, used during compilation but not linked
- Website `node_modules` — not part of the game binary
- Installer — packaging artifact

The only repository files that contribute to `mimita.exe` size are:
- Source `.cpp` files → compiled → `.o` files with DWARF → linked into EXE
- DLL dependencies (glfw3, MinGW runtime)

## Distribution Impact

| Artifact | Size | Notes |
|---|---:|---|
| Raw EXE (debug build) | 433.71 MB | Not for distribution |
| ZIP-compressed EXE | 86.98 MB | DWARF compresses well (5:1), but still large |
| Installer (LZMA2/ultra64) | 108.58 MB | Compresses everything including assets.pak |
| **Estimated release EXE** | **~14.5 MB** | Without debug symbols, with `-O2` |
| Full runtime folder (release) | ~250 MB | EXE 14 MB + assets.pak 182 MB + DLLs + config |
| Compressed runtime package | ~90 MB | assets.pak dominates at 182 MB → ~60 MB compressed |

## Root Cause Ranking

1. **DWARF debug info (96.6% of EXE)** — 99% confidence
   - PE section analysis proves `.debug_info` alone is 380 MB
   - Build script explicitly passes `-g` flag (line 93 of `build.py`)
   - No stripping or separate debug info is configured
   - All stage builds (412-416 MB) confirm this has been true since the build system was created

2. **No dead-code elimination** — 1% confidence
   - Without `--gc-sections`, unused functions/constants may remain in the binary
   - Would save at most a few MB, not the 419 MB debug issue

3. **No separate debug info / split DWARF** — 0% (already dominated by #1)
   - Even with split DWARF, the `.debug_info` section would still be embedded
   - The root cause is `-g` itself, not the absence of `-gsplit-dwarf`

## Recommended Fixes

### Do Now

Change the default build to produce a release executable for distribution:

| File | Change | Effect |
|---|---|---|
| `build.py` line 60 | Default `MODE = "release"` or produce `mimita-release.exe` | EXE drops from 433 MB to ~14 MB |
| `build.py` linker step | Add `-s` flag to GCC linker: strip all symbols | Removes all DWARF sections from EXE |
| Installer `setup.iss` line 43 | Point to release-build EXE | Installer drops from 108 MB to ~40 MB |

The safest immediate approach: add a `strip` step or `-s` flag to the linker in `build.py`, producing a debug EXE for development and a stripped EXE for distribution. Example:

```python
# After linking (line 546-559), add:
if MODE != "debug":
    cmd += ["-s"]  # strip all symbols in release mode
```

Or at the end of the build, run:
```python
subprocess.run(["strip", EXE_NAME])
```

### Do Later

- Add `-Wl,--gc-sections` + `-ffunction-sections -fdata-sections` to remove dead code
- Consider split DWARF (`-gsplit-dwarf`) for faster compilation without embedding debug info in object files
- Set up a CI release pipeline that builds with `--release` and signs the EXE
- The `assets.pak` at 182 MB is the actual distribution-size bottleneck — consider asset compression or streaming

## Exact Files and Build Flags to Change

| File | Symbol or Setting | Current Behavior | Recommended Change |
|---|---|---|---|
| `build.py:60` | `MODE = "debug"` | Default builds include full DWARF debug (~419 MB) | Change to `MODE = "release"` or add a `--dist` flag that adds `-s` to the linker |
| `build.py:90-97` | CXX_FLAGS in debug mode | `-Og -g` | Leave as-is for development, but strip after linking for distribution |
| `build.py:546-559` | Linker command | No stripping, no gc-sections | Add `-s` for distribution builds, add `-Wl,--gc-sections` |
| `installer/setup.iss:43` | `mimita.exe` | Includes debug info | Use stripped release EXE |

## Risks

- **Removing debug info:** Crash reports will not have file/line information. Keep the debug build available for developer testing.
- **`-O2` optimization:** Might expose optimization-related bugs not seen with `-Og`. Test the release build thoroughly before distribution.
- **Stripping:** Stripping removes all symbols, including those potentially needed for runtime exception handling or dynamic linking. With MinGW GCC on PE, stripping is safe for release executables.
- **`--gc-sections`:** Safe to add — removes unused code without changing behavior.

## Final Recommendation

Mimita should ship as an **Inno Setup installer containing a release-build EXE**.

- For developer testing: use the debug build (433 MB, full crash info)
- For end users: build with `python build.py release` → ~14 MB EXE → package via Inno Setup → ~40 MB installer (excluding assets.pak)
- The current 108 MB installer works because LZMA2 compresses DWARF to near-nothing, but distributing the release build is cleaner and faster to download/extract
