# Agent File Visibility Mismatch

Time created: 2026-09-21T01:23:53Z
Time last updated: 2026-09-21T01:23:53Z

Status: UNRESOLVED

Related specification:
`docs/architecture/time-and-formatting/time-and-formatting.md`

Related workflow:
`docs/ROUTER.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-21T01:23:53Z`

The user reported this path:

`C:\mimita-priv-v8\logs\2026-09-21\20260921\_011109\events.jsonl`

The agent checked that exact path and reported that it did not exist.

### Actual filesystem evidence

The repository contained this path instead:

`C:\mimita-priv-v8\logs\2026-09-21\20260921_011109\events.jsonl`

The difference is the extra leading underscore before `011109` in the user-provided path.
The discovered file existed, was 62,116 bytes, and had recent JSONL records. Its tail contained
`MOVEMENT` / `movement.preset.tuning` events for run `20260921_011109`.

### Expected behavior

When a user provides a log path, the agent should verify the exact path first, then search nearby
paths for likely naming differences and report both results clearly. The runtime should also print
the authoritative absolute log path so the user and agent are referring to the same file.

### Why this is bad

A small path-format difference can make a real live log appear missing. That causes incorrect
diagnosis, duplicate logging work, and loss of confidence in live debugging.

### Confirmed cause

The immediate cause is a path spelling mismatch:

```text
reported:  20260921\_011109\events.jsonl
actual:    20260921_011109\events.jsonl
```

This occurrence does not prove whether the user interface displayed the underscore or whether it
was added while copying the path. That remains unverified.

### Required prevention

1. `versioninfo` must print the absolute active `events.jsonl` path.
2. Log records must include `run_id` and the same absolute path in session metadata.
3. The investigation workflow must check the exact path, then use `rg --files` to find nearby
   `events.jsonl` files when the exact path is absent.
4. Diagnostics must distinguish `path does not exist` from `path exists but has no matching events`.
5. Future reports should paste the path from `versioninfo` rather than reconstructing it manually.

### Proof status

The corrected nearby path was found and searched with `rg`. The exact user-provided path was not
found. Runtime path reporting and the workflow change are still pending.
