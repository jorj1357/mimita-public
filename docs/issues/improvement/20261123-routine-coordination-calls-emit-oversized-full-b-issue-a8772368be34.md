# issue-a8772368be34: Routine coordination calls emit oversized full-brain payloads

## Human report (verbatim, immutable)

> The coordination state is unusually large - what u mean ? why large? can u make it so  issues like this, anythign that is getting in the way of spec behavior like this i thin kis an iefficiecny w should mark as an issue


## Agent suggestion

### 2026-11-23T19:08:24.256Z — hot-reload-latency-root

Direct observation from the latency-goal continuation (2026-11-23): `python devscripts/agent-coord.py --agent hot-reload-latency-root --json sync` completed in 0.504s and emitted 295,169 bytes of JSON. A root `dev-loop.py --status` call reported 169 registered agents, 302 open issues, 29 conflicts, 8 edit leases, 68 unproven and 20 stale proofs, and returned a long board/context/conflict listing. This verifies payload size but not significant CPU cost. It supports the existing acceptance target of a compact routine view under 2,000 tokens while preserving active context, current issue, leases/conflicts, claimable work and scheduler status; full export should remain explicit. Current issue `issue-a8772368be34` already has the exact user request and acceptance, so no duplicate issue was created. I did not edit agent_coord.py in this continuation because it is actively shared and the measurement alone does not prove a CPU/compile bottleneck.
