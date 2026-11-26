# Dashboard CPU use and MiMITA process purpose

**Date:** 2026-11-25 UTC  
**Issues:** [issue-acf28dc22c85](../../issues/performance/20261125-dev-loop-dashboard-should-cost-under-1-percent-c-issue-acf28dc22c85.md), [issue-b9edd402e658](../../issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md)

## Change

Reduced the dashboard coordination snapshot polling interval from 2 seconds to
30 seconds. The main dashboard status refresh cadence remains unchanged.
MiMITA process rows now display UTC start time, run ID, purpose, mode, and map
when those values are available. External processes with no launch metadata are
marked with an unknown launch reason instead of being assigned a guessed
purpose.

## Evidence

- Source: `devscripts/dev_loop/gui.py`.
- `git diff --check -- devscripts/dev_loop/gui.py` passed.
- Relaunched the checkout-owned dashboard through
  `python devscripts/dev-loop.py --dashboard --no-terminal-relaunch`.
- The dashboard window showed the managed server PID 15988, run
  `20261125_221046`, start `2026-11-25 22:10:46 UTC`, Vehicle Arena Free Play,
  and map `arena1`. It showed one server and zero clients at that observation.
- CPU sample for dashboard PID 35740, 2026-11-25 22:35:59.660–22:36:14.778Z:
  0.672 CPU seconds over 15.03 seconds on a 12-logical-CPU host, or 4.47% of
  one logical CPU and 0.37% of whole-machine capacity. The active runtime was
  run `20261125_221046`, Vehicle Arena Free Play, map `arena1`, zero NPCs; DLL
  generation 654 was published and no build was active. Frame/tick data and
  top competing processes were not sampled for this window.
- Earlier samples were 2.44% whole-machine CPU after a 10-second poll interval
  (PID 34728) and 11.52% before the polling change (PID 31064). These samples
  were not perfectly workload-matched, so the lower CPU is promising evidence,
  not proof of sole causality. Runtime journal:
  `logs/2026-11-25/20261125_221046/dev-loop/events.jsonl`.

## Remaining proof

Repeat matched dashboard CPU samples under idle and active workloads, and
check that the 30-second coordination refresh lag is acceptable. Process
purpose is only as specific as the metadata available from the runtime; an
unknown launch reason does not establish that a process is unused. No game
process was stopped.
