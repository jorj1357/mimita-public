# First Remote Test Build — Plan

## Overview

Take the current repository and produce a release-quality installer ready to send to one remote tester for a real cross-internet multiplayer test.

**Non-goals:** Chat UI, server browser, IPv6, stats, MMR, matchmaking, launcher self-update, or any gameplay feature work.

---

## Current State

| File | Issue |
|------|-------|
| `build.py:81` | Release mode uses `-march=native` — binary tied to developer's CPU |
| `build.py:77-85` | Release mode does not strip debug symbols (no `-s`) |
| `build.py:483-485` | libjuice C sources always compiled `-O0 -g` regardless of mode |
| `build_agent.py:151` | Always calls `build.py build-only` — never passes `release` |
| `devscripts/build-all.bat:20` | Calls `build_agent.py` without release arg — always produces debug EXE |
| `devscripts/build-all.bat` | Never calls `pack-assets.py` — installer ships stale `assets.pak` |
| Current `mimita.exe` | **455 MB** — debug build with ~419 MB of embedded DWARF symbols |

---

## Required Changes

### 1. `build.py` — Fix release mode

#### 1a. Replace `-march=native` with broad x86-64 target

```
Line 81:  "-march=native",   →   "-march=x86-64-v2",
```

**Why `x86-64-v2`:** Covers all CPUs from ~2009 onward (Intel Nehalem, AMD Bulldozer). Requires SSE3, SSE4.1, SSE4.2, POPCNT — available on nearly every 64-bit x86 CPU in use today. This avoids SIGILL crashes on older hardware while still enabling modern vectorization.

#### 1b. Add `-s` to strip debug symbols in release mode

```
After line 81, add:  "-s",
```

This tells GCC to strip symbol table and debug info from the output binary. Without this, even release builds may carry some DWARF sections.

#### 1c. Make libjuice C compilation respect build mode

Change lines 482-487 from:
```python
cmd += ["-std=c11", "-O0", "-g", "-pipe"]
```
To use the same optimization mode as the rest of the build:
```python
if MODE == "release":
    cmd += ["-std=c11", "-O2", "-pipe"]
else:
    cmd += ["-std=c11", "-O0", "-g", "-pipe"]
```

libjuice is performance-sensitive (STUN/TURN packet parsing in the networking hot path), so `-O2` for release is appropriate.

### 2. `build_agent.py` — Accept `release` argument

Modify line 151 to accept a `release` argument passed from the command line:

```python
release_flag = "release" if "release" in sys.argv else ""
result = subprocess.run(
    [sys.executable, "build.py", "build-only"] + ([release_flag] if release_flag else []),
    capture_output=True, text=True,
)
```

This allows `python build_agent.py release` to produce a release build via the agent lock system.

### 3. `devscripts/build-all.bat` — Orchestrate release build

#### 3a. Accept `release` argument and pass through

Add at the top:
```bat
set BUILD_MODE=%1
if "%BUILD_MODE%"=="" set BUILD_MODE=release
```

#### 3b. Pass release mode to build_agent

Change line 20 from:
```bat
python build_agent.py
```
To:
```bat
python build_agent.py %BUILD_MODE%
```

#### 3c. Add `pack-assets.py` step

Add after the game build (after line 24) and before the launcher build:
```bat
echo.
echo ==========================================
echo Packing assets...
echo ==========================================

python devscripts\pack-assets.py
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Asset packing failed
    exit /b 1
)
```

---

## Execution Order (to be done when implementation begins)

### Step 1 — Apply code changes

1. Edit `build.py:81` — `-march=native` → `-march=x86-64-v2`
2. Edit `build.py` — add `-s` to release CXX_FLAGS
3. Edit `build.py:482-487` — make libjuice flags conditional on MODE
4. Edit `build_agent.py:151` — pass through `release` arg
5. Edit `devscripts/build-all.bat` — add `pack-assets.py` step, pass `release` arg

### Step 2 — Clean and build

```bat
python build.py clean
python build_agent.py release
```

Or use the updated build-all:
```bat
devscripts\build-all.bat release
```

### Step 3 — Verify release EXE

Check file size:
```bat
dir mimita.exe
```
Expect: **~15 MB** (down from 455 MB)

Verify no DWARF sections (or confirm size is dramatically smaller):
```bat
C:\msys64\mingw64\bin\objdump.exe -h mimita.exe | findstr debug
```
Or just check the size is < 25 MB as a proxy.

### Step 4 — Pack assets (if not in build-all)

```bat
python devscripts\pack-assets.py
```

### Step 5 — Build launcher

```bat
launcher\build.bat
```

### Step 6 — Generate manifest

```bat
python devscripts\generate-manifest.py
```

### Step 7 — Build installer

```bat
iscc installer\setup.iss
```

### Step 8 — Verify installer contents

```bat
"C:\Program Files\7-Zip\7z.exe" l installer\MimitaSetup-*.exe
```

Check that it includes:
- [ ] `mimita.exe` (release build, ~15 MB)
- [ ] `MimitaLauncher.exe` (~1 MB)
- [ ] `assets.pak` (~191 MB)
- [ ] `glfw3.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- [ ] `shaders/` (6 files)
- [ ] `Characters/DefaultGuy/` (2 files)
- [ ] `config/` (all ~30 config files)

### Step 9 — Compute SHA-256

```bat
certutil -hashfile installer\MimitaSetup-*.exe SHA256
```

### Step 10 — Write report

Create `FIRST-REMOTE-TEST-BUILD.md` with exact results.

---

## Verification Gates

| Gate | Pass/Fail |
|------|-----------|
| `mimita.exe` < 25 MB | |
| Release build doesn't abort on startup | |
| Launcher builds and runs | |
| `assets.pak` exists and is recent | |
| Installer builds without errors | |
| Installer contains all required files | |
| Installer runs on clean Windows | |
| Hash matches between builds | |

---

## Files Touched

| File | Change | Risk |
|------|--------|------|
| `build.py:81` | `-march=native` → `-march=x86-64-v2` | Low — well-tested baseline |
| `build.py:77-85` | Add `-s` flag | Low — standard stripping |
| `build.py:482-487` | Conditional libjuice flags | Low — same flags used elsewhere |
| `build_agent.py:151` | Accept release arg | Low — backward compatible |
| `devscripts/build-all.bat` | Add pack step, pass mode | Low — adds missing step |

No multiplayer architecture is modified. No new features are added. The build chain is corrected to produce a proper release binary.
