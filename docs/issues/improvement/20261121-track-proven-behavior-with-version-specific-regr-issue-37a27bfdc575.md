# issue-37a27bfdc575: Track proven behavior with version-specific regression evidence

## Human report (verbatim, immutable)

> add a  issue that we need like auto regerssion tracking, like, once somthing works we nede to explain exact why it works and how to exnrue that it works and show the evidence that this specific version of htings worked


## Agent suggestion

### 2026-11-21T15:51:41.477Z — issue-review-sweeper

Process proposal from the issue-0cdac76fb006 independent sweep (2026-11-21): make proof staleness region-scoped so an unrelated same-file edit does not erase valid evidence, and require a named re-check command in the regression record.

Observed: task-proof-ed45c1b9e923 (dashboard monitor retention, verified 2026-11-20, three restart cycles) became `stale` solely because `devscripts/dev_loop/gui.py` changed. The recorded gui.py hash 20fac330... no longer matches current 5739625d..., but the current diff is a read-receipt display edit (+8/-2 near line 1452) and does not touch the proven geometry/persistence functions (`_monitor_work_area`, `_window_bounds`, `_tk_geometry`, `persist_window_geometry`, `toggle_maximize`). At this sweep the status line reports stale=6 proofs; this is one confirmed case where whole-file staleness did not correspond to a change in the proven behavior. That costs a re-investigation to distinguish "regression" from "unrelated edit" and weakens trust in version-specific proof.

Proposal: when recording a proof, store hashes for only the named owning functions/regions (or accept an explicit `--region`), and on later source change mark `needs_recheck` naming the changed region instead of blanket `stale`. Record the exact one-line re-check command in the regression record so any agent can re-prove without re-deriving the procedure.

Measurable target: false-stale proofs (stale although the owning region is unchanged) drop to 0 for the dashboard retention task within one sweep; time to classify stale-vs-regression for a proven behavior under 2 minutes; every durable regression record names its re-check command.

Smallest experiment: for task-proof-ed45c1b9e923 only, add function-level hashes for the five geometry/persistence functions and compare them after the current gui.py diff. If unchanged, mark the record `proven-unchanged` with the region hashes and log the decision. Do not change global staleness rules until this pilot shows the false-stale count and re-check time.
