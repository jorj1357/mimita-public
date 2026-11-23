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

## Agent suggestion

### 2026-11-23T18:12:32.732Z — issue-review-sweeper

Process proposal from issue-review-sweeper's independent review of issue-be1adb51b6ac (2026-11-23T18:10Z), evidence used in that review.

Observed gap: owner npc-elite-fcs appended a textual 'Phase 1 CT NPC defuse proof' at 2026-11-23T17:35:59Z citing pinned run 20261123_172947 (generation 567, 4 objective.defused, all CT round wins). Yet at 2026-11-23T18:10Z the issue's structured evidence JSON still described run 20261122_081330 on generation 478 with objective.defuse-start=0 and objective.defused=0, and proof status still listed task fcs-objective-progress-after-dash-guard=unproven. Result: a cold reader of proof status/issue.evidence cannot see a proven acceptance event that already exists in a stopped journal. This is version-specific proof-link lag, not a missing experiment.

Proposal: when an isolated pinned run stops, compare its journal events against each linked open issue's declared required/proof events; on a full match, auto-create the structured evidence record (or emit exactly one owner-addressed 'evidence-due' reminder carrying issue id, run id, generation, dll sha256, journal path). Do not mark the issue fixed.

Measurable target: for the next 10 behavior proofs, >=8 have a structured evidence record with evidence.created_utc within 5 minutes of the proving run's run.json updated_utc at stop (current observed lag here is >=35 minutes and unbounded because the owner must remember a separate step).

Smallest experiment: add a dry-run-only check to the one-shot agent test path that writes one 'evidence-due run=<id> issue=<id> generation=<n>' line to the dev-loop events journal when a stopped run matches a linked issue's required events; no change to build paths or runtime processes. Run it over the next 10 pinned behavior tests and report the lag distribution before deciding whether auto-create is safe.
