---
name: overseer
description: Run after any code or config change. Scans repo for issues, validates build, checks architecture rules. Must return CLEAN PASS before work is complete.
---

# Overseer: Final Review Check

Run this skill after every implementation task before declaring completion.

## Checks

1. **Build** — Confirm `build/changelog.txt` shows `Status: SUCCESS`
2. **New files exist** — All expected files from the plan are present
3. **No compiler warnings** — Scan build log for `warning:`
4. **AGENTS.md rules** — No forbidden patterns (hardcoded paths that should be configurable, duplicate functions, etc.)
5. **Architecture** — One concept per file, one function per job, no scattered ownership

## Report Format

```
=== OVERSEER REPORT ===
Build: PASS/FAIL
New Files: PASS/FAIL (missing: ...)
Warnings: N
Architecture: PASS/FAIL (issues: ...)
FINAL STATUS: CLEAN PASS | ISSUES FOUND
```

## Zero-Tolerance

Any finding = failed review. Fix and re-run until `FINAL STATUS: CLEAN PASS`.
