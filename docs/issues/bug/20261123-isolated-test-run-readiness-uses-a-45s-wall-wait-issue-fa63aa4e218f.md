# issue-fa63aa4e218f: Isolated test-run readiness uses a 45s wall wait and misclassifies intermission/server stalls

## Human report (verbatim, immutable)

> test client 31072 did not observe active mode; client journal network.mode.start stage=phase phase=4; server performance.server-tick-stall total_ms=4494.053


## Review (evidence)

### 2026-11-24T00:58:50.528Z — npc-elite-fcs-5

Reviewer: npc-elite-fcs-5

Generation 576 readiness attempts: run 20261124_005506 reported 'test client exited before active mode' after the client journal reached t=25.4 s in phase 4 with no network.mode.active; run 20261124_005656 reported the new health-gate classification 'server stalled before active mode: tick 308->1083 in 51.8s, max_tick_stall_ms=409.3, over_100ms_percent=5.7'. This proves the gate distinguishes a stalled server from a healthy mode transition, but neither run produced behavior proof. Retry when the host is free.
