# Runtime DLL staging lock was reported as a failed build

Time created: 2026-09-27T01:31:34Z
Time last updated: 2026-09-27T01:31:34Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/operations/build-and-exe/build-and-exe.md`

Related changelog:
`docs/changelog/2026-09-27/20260927_013134-runtime-dll-staging-lock.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-26 local development session; recorded 2026-09-27T01:31:34Z`

The linker completed successfully, but the build then failed while staging a
runtime DLL:

```text
[LINK] mimita.exe
Traceback ...
PermissionError: [WinError 32] The process cannot access the file because it is being used by another process
[DEV] build failed with exit code 1; current game was left running
```

### Expected Behavior

A locked convenience copy beside the root executable must not turn a
successful link into a failed development build. The numbered published build
must receive its own runtime DLL copies from the configured MinGW/GLFW
installations.

### Actual Behavior

`build.py` copied runtime DLLs directly into the repository root after linking.
If a running process had one of those root files mapped, `shutil.copy2` raised
`PermissionError` and terminated the build with exit code 1.

### Why This Is Bad

The code compiled and linked, but the dev loop treated the post-link staging
failure as a code-build failure. That prevented publication/restart and made a
file-lock condition look like a compiler or missing-DLL problem.

### Wrong Code

File:
`build.py`

```python
destination = os.path.join(ROOT, name)
shutil.copy2(source, destination)
print("[RUNTIME] staged %s" % name)
```

### Confirmed Cause

The failure occurred in post-link runtime staging, not compilation or linking.
Windows rejected replacement of the destination DLL because another process
had it open. The numbered dev builds already run with DLLs beside their own
EXE, so root staging is convenience setup rather than proof that the link was
valid.

### Attempted Fix 1

Time:
`2026-09-27T01:31:34Z`

Change:

- `build.py` now catches a locked runtime-DLL destination and keeps the
  existing root copy instead of converting the completed link into exit code 1.
- `devscripts/dev-loop.py` resolves runtime DLLs from the current compiler and
  GLFW installations and copies those directly into each `.dev\builds\####`
  directory.
- Missing per-build DLLs are reported explicitly rather than silently skipped.

### Corrected Code

File:
`build.py`

```python
try:
    shutil.copy2(source, destination)
    print("[RUNTIME] staged %s" % name)
except PermissionError:
    print("[RUNTIME] destination locked; keeping existing %s" % name)
```

File:
`devscripts/dev-loop.py`

```python
compiler_dir = Path(resolve_compiler()).parent
runtime_sources.update({
    "libgcc_s_seh-1.dll": compiler_dir / "libgcc_s_seh-1.dll",
    "libstdc++-6.dll": compiler_dir / "libstdc++-6.dll",
    "libwinpthread-1.dll": compiler_dir / "libwinpthread-1.dll",
})
runtime_sources["glfw3.dll"] = Path(resolve_glfw_lib()) / "glfw3.dll"
```

### Fix

Root runtime staging is now best-effort. The actual dev-loop artifact is
self-contained: its EXE and runtime DLLs come from the current configured
toolchain and are copied into the versioned build directory. A running root
process can no longer make a successful link look like a failed build.

### Proof

Automated proof:

- `python devscripts/dev-loop.py --help`: passed after adding the toolchain
  fallback import.
- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python build.py build-only`: passed while the numbered MiMITA server/client
  processes were still running.
- The build reached `BUILD SUCCESS` after linking and staged all four runtime
  DLLs.

Human review:

Still required: leave the dev-loop server/client running, edit a watched source
file, and confirm the next published build starts without a staging-lock
failure and has all four DLLs beside its numbered EXE.

### Solution

Not confirmed yet.
