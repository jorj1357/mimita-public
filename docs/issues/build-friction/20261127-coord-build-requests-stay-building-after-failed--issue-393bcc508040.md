# issue-393bcc508040: Coord build requests stay building after failed/queued builds; isolated test command router reports live clients dead

## Human report (verbatim, immutable)

> Observed 2026-11-27 during the incremental NPC patch: coord build requests 2792da10f482, 498a7be48a4d, b141943a885e, a05998af8e28, 5668f6f41797 remained status=building after dev-loop failures, daemon restarts, or no build attempt. Request 498a7be48a4d and a05998af8e28 also carried updated_utc about 5 hours in the future. Isolated test run 20261127_172415 had client-01 process alive with a live journal, but --command returned 'test client client-01 is not alive'.

