# Live evidence observer

Status: desired behavior; prototype exists in `devscripts/evidence-watch.py`.
Scope: read-only discovery, mapping, and querying of every running MiMITA
process in this checkout, whether started by the dev-loop, an agent, or a human.

## Purpose

Agents need to use the live game the user is actually playing. Evidence must not
depend on the dev-loop being the starter. The observer finds running MiMITA
processes, identifies their canonical journal, and returns bounded evidence
without adopting or controlling them.

## Non-goals

- It does not start, stop, restart, attach to, or edit a process.
- It does not create a run ID, instance ID, build, or proof record.
- It does not infer a healthy client from process liveness.
- It does not bypass journal identity, rotation, or quota rules.

## Discovery

Use native Windows process identity (`devscripts/process_identity.py`) to find
executables under this checkout. A process is MiMITA only when its executable
path matches a known checkout executable.

Role evidence comes from:

1. the active run manifest (`run.json` instance row);
2. `logger.started` fields (`process_role`/`process`, `pid`, `path`, build ID);
3. an explicit command-line marker (`--server`, `--connect`) only as a hint.

Unknown or contradictory evidence stays `unverified`; do not present it as a
confirmed server or client.

## Journal mapping

For each PID, resolve the journal in this order:

1. `logs/**/run.json` instance whose PID matches;
2. `logger.started` in recent canonical journals whose PID/start time/executable
   matches;
3. the process’s exported events path when available.

A journal directory is one instance journal. Read `events.jsonl` and rotated
`events-NNNN.jsonl` segments in numeric order. The active file is last.

## Commands

```text
evidence-watch list [--json] [--write-registry]
evidence-watch show <pid|run-id>
evidence-watch tail <pid|run-id> [--event E] [--actor A]
             [--since UTC] [--until UTC] [--lines N] [--follow]
evidence-watch watch [--interval S] [--once]
evidence-watch capture <pid|run-id> --out <bundle> [--max-bytes N]
```

- `list` is a bounded snapshot; it must not scan multi-gigabyte files fully.
- `tail` keeps per-file offsets and does not re-read from the start on every
  poll.
- `capture` writes only the manifest, selected journal segments, and an excerpt;
  it must be bounded and must record SHA-256 and byte sizes.

## Registry

Optional generated index:

`.dev/live-evidence/registry.json`

Fields per process: PID, executable, start time, role, run ID, build ID, mode,
map, journal segments, journal bytes, last event time/name, event age, proof
links, source of identity, diagnostics.

The registry is a view. It never becomes the source of truth for process
ownership or gameplay state.

## Safety

- Read-only access to journals and process metadata.
- No secrets, packet payloads, tokens, or credentials.
- Do not adopt, relabel, stop, or kill an external process.
- Do not infer a run identity for a standalone process that has none.
- Keep capture size bounded; large journals are excerpted, not copied whole by
  default.

## Acceptance

- A human starts MiMITA manually; `list` finds it within one refresh.
- `show` names the exact journal, build, mode/map, and role evidence.
- `tail` returns selected events and follows rotation.
- Two live instances are distinguished.
- No external process is adopted or controlled.
- `capture` produces a bounded bundle with manifest and checksums.
- A missing journal is reported as a diagnostic, not a fabricated healthy run.

## Related

- `docs/specs/dev-loop/agent-runtime-registry.md`
- `docs/specs/dev-loop/multi-instance.md`
- `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/specs/dev-loop/dashboard.md`
