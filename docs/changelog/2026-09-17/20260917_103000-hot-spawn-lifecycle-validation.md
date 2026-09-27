# Hot spawn lifecycle validation guard

Date: 2026-09-17

## Outcome

Hot movement policy no longer converts lifecycle-invalid movement reports into
accepted transforms. Pre-spawn, not-active, movement-disabled,
spawn-generation-mismatch, and transform-epoch-mismatch reports preserve the
cold validation decision, preventing stale client coordinates or `(0,0,0)`
from overwriting the authoritative spawn.

## Evidence

- The server log showed the authoritative spawn at
  `(-604.895,28.294,2366.225)` followed by accepted stale reports including
  `(0,0,0)`.
- The cause was `rocket-behavior.cpp` forcing every movement validation result
  to `Accept` without preserving lifecycle rejection.
- `python devscripts/live-build.py --generation 1000000` produced a successful
  hot DLL candidate without writing `mimita.exe`.
- The server log also recorded `[SERVER POST-LEAVE] players=0 — server
  continuing`, proving the leave path did not intentionally close the server.

## Remaining acceptance

The candidate must activate in the running server/client pair, then be tested
with join, leave, rejoin, repeated explode/respawn, and the first movement
report. Acceptance requires authoritative and client positions to agree without
an origin or multi-kilometer correction.
