---
name: repo-hygiene-checker
description: Prevent repository debris — temporary files, backups, logs, crash dumps, generated artifacts.
---

# Repository Hygiene Checker

## Purpose
Prevent repository debris, temporary artifacts, random outputs, backups, logs, crash dumps, generated reports, and abandoned files from accumulating over time.

A repository should contain only files that intentionally belong there.

## Debris Patterns

Check for and flag any of the following:

**Logs:**
`err.txt`, `err2.txt`, `out.txt`, `out2.txt`, `debug.txt`, `debug.log`, `crash.log`, `trace.log`

**Temporary files:**
`*.tmp`, `*.temp`, `*.cache`

**Backup files:**
`*.bak`, `*.old`, `*.backup`, `*.copy`

**Generated junk:**
`test_output.txt`, `output.txt`, `results.txt`, `dump.txt`, `report.txt`

**Editor leftovers:**
`*.orig`, `*.rej`

**AI leftovers:**
`ai_report.txt`, `audit_output.txt`, `analysis_output.txt`

**Duplicate source copies:**
Any file matching `*_copy.cpp`, `*_backup.cpp`, `*_old.cpp` patterns.

## Allowed Files

The following are NOT debris:
- Source code (`*.cpp`, `*.h`, `*.hpp`, `*.c`, `*.py`, etc.)
- Assets (under `assets/`)
- Documentation (`*.md`, `*.txt` under `docs/`)
- Build configuration (`CMakeLists.txt`, `*.cmake`, `Makefile`, etc.)
- Build output directories (under `build/`)
- Intentionally-versioned reports or logs documented as permanent

Use judgment: if a file looks like it was generated during a test run or debugging session, flag it.

## Severity

Any debris found: **BLOCKER**

## Output

**PASS:**
```
Repository Hygiene: PASS
No debris detected.
```

**BLOCKER:**
```
Repository Hygiene: BLOCKER
Found N temporary artifacts:
  path/to/file1
  path/to/file2
Cleanup required before completion.
```

## Action

Report only. List all offending files with their paths.
Do not delete files. Report them.
