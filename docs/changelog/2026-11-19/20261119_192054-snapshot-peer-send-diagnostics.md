# Snapshot peer-send diagnostics

## Change

- Extended the sampled `network.server.snapshot.build` and
  `network.server.snapshot.send` events with per-player transport kind,
  transport-handle presence, spawn/stale state, sent/failed chunk counts, and
  bytes. The payload is collected only on the existing snapshot diagnostic
  interval.
- Reported the server event transport as `none` when there are no player
  recipients and `mixed` when the snapshot sender iterates a player roster;
  the prior unconditional `udp` label obscured ICE-backed player sends.
- Made isolated-run shutdown request fresh process identity before matching
  manifest-owned PIDs, with a 10-second bound rather than the cached 3-second
  status lookup that timed out under active compiler load.

## Motivation and evidence

The isolated run `20261119_190051` logged a client snapshot-starvation timeout,
10 reconnect attempts accepted by the server, and no client-applied snapshot
after server tick 12,381. Nine NPCs generally continued moving. One-second
world windows varied below 60 ticks during expensive simulation intervals,
and the server logged multi-second NPC-phase stalls. Existing snapshot send
events aggregated every player together and mislabeled their transport, so
they could not show whether a reconnecting peer was actually sent snapshots.

## Validation status

- `git diff --check -- src/network/server-packets.cpp`: passed.
- `python -m py_compile devscripts/dev_loop/parallel_test_runs.py`: passed.
- `python -m py_compile devscripts/dev-loop.py`: passed after the stop-timeout
  change.
- Isolated-run shutdown succeeded: the controller stopped only manifest
  PIDs 10884 and 29668 after their executable, command line, and creation time
  matched the run manifest.
- No build or runtime test of this diagnostic change was performed. The root
  dev-loop was already building DLL generation 370 when the edit was saved;
  agents may not invoke or wait for builds under the active repository gate.
- The previously running isolated test run is separate runtime evidence for
  the reproduced reconnect failure, not validation of this instrumentation.
