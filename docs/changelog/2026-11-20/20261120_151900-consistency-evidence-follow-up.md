# End-to-end consistency evidence follow-up

Date: 2026-11-20
Issue: `issue-cf9cd77eb056`
Tasks: `lifecycle-first-failure-trace`, `reproduce-lifecycle-payload-evidence`

## Evidence recorded

- TDM isolated run `20261120_144622` (EXE build `2026-11-20T07:47:09.981634Z`, game DLL generation 401) reached `join.accepted` about 0.9 seconds after the connect request and applied authoritative snapshots through 14:51:48.856Z. This is one successful baseline only.
- The same run's client process PID 26580 later disappeared while its run manifest still reported online. The logger stopped at 14:51:49.519Z; an explicit stop updated the state at 15:04:10Z. `devscripts/dev_loop/parallel_test_runs.py::list_parallel_runs` returns persisted manifest state without checking process liveness; command queue checks liveness only when called. This is a specific stale-status window and likely missing reconciliation owner, not proof that the in-game network disconnected.
- Payload server run `20261120_090706_01`, PID 3940, mode Payload / FunWorld3, reported 42 NPCs and 0 players. Over 92 telemetry windows from 15:13:19 to 15:17:06Z, 525 sampled ticks yielded an estimated median cadence of 2.68 Hz (max 5.33 Hz); NPC simulation average cost had median 346 ms and max 1,520 ms, with reported total-window p95 as high as 3,579.8 ms. This is a severe loaded development-run symptom; it is not isolated causality or a production benchmark.
- 993 of 1,021 fall samples lacked grounded/world-contact flags in the inspected window. These events do not establish that an NPC was visibly floating above the Payload cart. The available `payload.cart-state` records are sparse and lack cart transforms, preventing same-tick actor/cart correlation.

## Evidence artifact and follow-up

Performance plot: `C:\Users\guita\.codex\visualizations\2026\11\20\01a1f96b-21a0-71f3-b2f0-39da5c68b607\payload-server-tick-performance.png`.

For a conclusive visual reproduction, capture a pinned run's screenshot/video together with server tick/frame ID, cart authoritative position/velocity, NPC feet/world position, support/contact state and reason, NPC role/nav goal, and build/run/map/mode identity. Correlate those values at the exact capture tick. Then measure server cadence and per-phase costs at representative NPC counts. Keep the run manifest tied to OS process identity and reconcile exits automatically so UI state cannot remain online after process death.

No code was changed and no build or new runtime was started in this evidence-only follow-up. Generation 404 and Arcade's coordinated visual run were active; the healthy run `20261120_150433` was left untouched. The issue and tasks remain open/in progress pending repeated lifecycle proof and synchronized Payload visual evidence.
