# Recording stop blocked the client and caused a reconnect

Time created: 2026-11-04T01:00:00Z
Time last updated: 2026-11-04T01:00:00Z

Status: ATTEMPTED FIX (1) — source changed; long-run human confirmation pending

Related specification: `docs/specs/dev-loop/scenario-input-recording.md`
Related changelog:
`docs/changelog/2026-11-04/20261104_010000-npc-collision-union-solid-and-recording-async.md`

---

## Regression Occurrence 1

### Observed

Human report: ending a scenario recording made the client appear packet
disconnected; the player had to leave and rejoin. The client also stalled while
recording and while stopping.

### Confirmed Cause

`scenario_endrecord` busy-waited up to six seconds on the client main thread
while calling the synchronous client/server merge. During that wait the client
stopped sending network heartbeats and inputs. The merge itself also loaded the
entire client recording into memory.

### Corrected Code

- `scenario_endrecord` now returns immediately after writing the footer; it does
  not wait for the merge.
- The client/server merge runs through a background future observed by the
  fixed-tick `poll` path.
- `mergeClientAndServer` streams the client file in two passes instead of
  loading every parsed row.
- The final file replace retries for transient Windows file locks.

### Proof

`scenario_record_roundtrip_v1` passed against a clean dev-loop daemon:
`scenario.record.started=1`, `scenario.record.merged=1`, no invalid/merge_failed,
and the roundtrip validator reported `client_ticks=235 server_ticks=230
valid=true`.

A long human play session that stops a recording and remains connected is still
required before this regression can be marked solved.
