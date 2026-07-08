---
name: overseer
description: Final quality gate. Run after every implementation task. Scans repo for issues, validates build, checks architecture rules, verifies specialized skills were used. Must return CLEAN PASS before work is complete.
---

# Overseer: Final Quality Gate

Run this skill after every implementation task before declaring completion.

## Discovery Notice

The Overseer SKILL.md is always at `.opencode/skills/overseer/SKILL.md` relative to the workspace root.
If the glob tool returns "No files found" for this path, that is a known tool limitation — the file exists.
To verify: use `Test-Path` via PowerShell, or glob with `**/SKILL.md` (which searches all subdirectories).

## Report Format

```
=== OVERSEER REPORT ===
FINAL STATUS: CLEAN PASS | ISSUES FOUND
```

## Zero-Tolerance

Any finding = failed review. Fix and re-run until `FINAL STATUS: CLEAN PASS`.

## Checks

### 1. Build Check

- If code was modified, confirm `build/changelog.txt` shows `Status: SUCCESS`.
- Scan build log for `warning:` — report any compiler warnings.
- If the build was not run, note this as a **FAIL**.

### 2. Test Check

- If tests exist in the repository, run them.
- Report any test failures.
- If no test framework exists, note: "No tests configured — manual verification required."

### 3. Regression Check

- Verify changed files still compile.
- Verify changed logic paths are exercised.
- Check that existing behavior is preserved (not silently broken).

### 4. Duplicate Code Check

- Search for newly-introduced duplicate functions.
- If new code duplicates an existing function, flag it.
- If the user added a function that already existed, flag it.
- Reference the **duplicate-state-checker** and **single-responsibility-checker** skills if changes involve state or ownership.

### 5. Dead Code Check

- Flag any new functions, variables, or files that are defined but never used.
- Flag any `.h` files with only declarations and no corresponding implementation.

### 6. Missing Assets Check

- If the task adds audio or visual assets, verify the asset file exists at the expected path.
- Audio assets should be under `assets/sound/entity/`, `assets/sound/ui/`, or `assets/sound/weapon/`.

### 7. Missing Includes Check

- Scan new `.cpp` files for missing `#include` directives that would cause compilation errors.
- Verify headers referenced in new code exist.

### 8. TODO Check

- Search new/modified files for leftover `TODO`, `FIXME`, `HACK`, `XXX`, `WORKAROUND` comments.
- If any exist, the task is **NOT** complete.

### 9. Debug Code Check

- Search for `printf(`, `cout <<`, `std::cerr`, `Debug::log`, `OutputDebugString`, `MessageBox` that appears to be debug-only (not intentionally permanent).
- Flag any temporary debug visualizations.
- Flag any "debug" bools or `#ifdef DEBUG` blocks that were added as part of the task and not intended to remain.

### 10. Temporary Hack Check

- Search for `// TEMP`, `// HACK`, `// XXX`, `quick fix`, `workaround`, `temporary` in new/modified code.
- Flag any hardcoded paths, IPs, ports, credentials, or magic numbers that should be configurable.

### 11. Stray Output Files Check

- Search for unexpected output files: `*.tmp`, `*.bak`, `err.txt`, `out.txt`, `debug.txt`, `test_output.txt`, `dump.txt`, `analysis_output.txt`, `ai_report.txt`.
- Reference the **repo-hygiene-checker** skill.

### 12. Stray Logs Check

- Search for log files in the repository root or source directories: `*.log`, crash dumps.

### 13. Merge Marker Check

- Search for `<<<<<<<`, `=======`, `>>>>>>>` merge conflict markers in source files.

### 14. Syntax Check

- Verify new `.cpp`, `.h`, `.hpp`, `.c` files have no obvious syntax errors (unmatched braces, missing semicolons).

### 15. Security Check

- Flag any hardcoded secrets, API keys, passwords, tokens.
- Flag any unsafe string handling (e.g., `sprintf` without length limits, `strcpy`).
- Flag any command injection vectors (system calls with unsanitized input).

### 16. Specialized Skills Check

- If the task involved **collision work**: verify the `collision-review` prompt was consulted.
- If the task involved **physics/movement**: verify relevant architecture rules were checked.
- If the task involved **build system changes**: verify the build was tested.
- If the task involved **state variables**: verify the **duplicate-state-checker** or **ai-drift-prevention** agent was used.
- If the task involved **file/function size**: verify the **file-size-checker** or **function-size-checker** was used.
- If the task involved **dependency changes**: verify the **dependency-checker** was used.
- If none of these categories apply, note: "No specialized skills required for this task."

## Diagnostics (when a skill cannot be found)

If any skill fails to load, print:

```
[OVERSEER DIAGNOSTIC]
  Current working directory: <dir>
  Workspace root: <root>
  Expected path: .opencode/skills/<name>/SKILL.md
  File exists: yes/no (verified via Test-Path)
  Glob result (**.opencode/**): found/NOT FOUND
  Glob result (.opencode/...): found/NOT FOUND
  Glob result (**/SKILL.md): found/NOT FOUND
  Every discovered skill: <list>
  Available skills in system prompt: <list>
  Failure reason: <explanation>
```

This diagnostic ensures debugging takes seconds instead of guessing.
