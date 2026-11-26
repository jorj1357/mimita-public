# issue-0843a94eae4b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (genuine-over-budget-not-stall;build-succeeded-gen648;reason-clean;identity-stale;wake-queued-once-fallback-late)

### 2026-11-25T22:01:43.230Z — build-wait-reviewer

Reviewer: build-wait-reviewer

**Build-wait review — build-wait-reviewer, wake-25ef593488d5, 2026-11-25T21:5xZ UTC.**
Read-only: no source edited, no build/test started, no fix claimed, no proof recorded.

**Verdict: genuine over-budget wait, NOT a stall. The build was healthy and published generation 648. The single changed TU alone (73.8s) already exceeds the 60s budget, and the monolithic link (68.1s) pushed the total to 189s. One acceptance defect remains (stale/partial build identity) plus a late reviewer fallback. The `reason` field was clean this time (no corruption).**

Observed facts (pinned)
- `wait_id` `game-dll-build:2026-11-25T21:25:09.090Z`, stage `game_build`, budget 60.0s, elapsed **90.177s**, `started_utc` 21:25:09.090Z; detected ~21:26:39.27Z; one `dev-loop.wait.over_budget` recorded `21:26:56.815Z` (`created=true`).
- Evidence `reason` was a readable stage summary: `[GAME BUILD] revision=4d985e5… sources=644 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0` — NOT the corrupted mid-JSON tail seen in sibling waits.
- Actual attempt `game-dll-20261125T212509367039Z-c77aed`, `source_generation` 27, request `2026-11-25T21:25:08.870Z-9eef37a1:mode3:game`, notes by `rl-mode-focus` (two vehicle-system.cpp notes).
- Result: **success**, generation **648**, revision `4d985e576181a18ba6ec9122af54c9031b255892178368fb09787c040d1f8d56`, code_hash `0addc5535588eafe21b161091be03ed377c153560dba04fbcf7ffb634d9f0442`, DLL `build\game\generation-648\game-g648.dll`, completed `21:28:18.507Z`, `total_seconds` **189.141**, `worker_total_seconds` 146.86, `compiled` 2, `skipped` 643.
- Phase timing (`.dev/build-timings/game-dll-20261125T212509367039Z-c77aed.json`): `changed_source_preflight` **73.938s**, `compile` 2.781s (identity provider only), **`link` 68.063s**, `publish` 0.469s, `coherence_validation` 0.312s, `identity_and_cache` 0.032s, `object_cache_scan` 0.015s, `toolchain_setup` 0.078s.
- Changed TU `src/vehicles/vehicle-system.cpp` compiled **73.848s** (started elapsed 40.593s / 21:25:50.163Z, completed 21:27:04.016Z). Compiler ftime: preprocessing **30.05s (50%)**, phase parsing **42.86s (72%)**, template instantiation 7.85s. `module-identity-provider.cpp` 2.739s.
- Link inputs: `link_input_count` 644, `link_object_input_bytes` **495,466,690** (~472 MiB), `link.lld_linker` 68.0s.
- Journal `logs/2026-11-25/20261125_212558/dev-loop/events.jsonl`: exactly **2** lines contain this `wait_id` — one `dev-loop.wait.over_budget` (`created=true`) and one `dev-loop.wait.reviewer-wake` (`queued=true`). No other journal contains it.

Resource evidence (same attempt, `.resource.json`/`.resource.jsonl`)
- Logical CPUs **12**, jobs 10, `duration_seconds` 189.125, `mean_host_cpu_percent` **99.249**, `peak_host_cpu_percent` **100.0**, `minimum_available_memory_bytes` **123,723,776** (~118 MiB free; total RAM 16.80 GB), 142 samples @1s.
- At sample 56 (elapsed 74.485s, phase `changed_source_preflight`): host CPU **99.5%**, available memory 277,626,880 B (~265 MiB, 98.3% used). Top competing whole-host share: `MsMpEng.exe` (Defender) **8.48%**, `pythonw.exe` 6.64%, `System` 3.62%, `dwm.exe` 3.02%, `cpptools.exe` 1.97%, `Taskmgr.exe` 1.89%, `opencode.exe` 1.80% + 1.78%. Tracked `mimita.exe` 20.37% / 6.02% / 5.44%.
- Runtime context (context only; `mode_verification_status=client_mode_unverified`): run `20261125_212558`, server `bloodtest`, 1 player / 1 NPC, client ~110.9 fps. The pair launched ~21:25:58, i.e. mid-build, so it added CPU pressure but is not the primary cause.

First divergence / cause (observed)
- The wait crossed 60s inside the **single changed-TU compile** of `vehicle-system.cpp` (running 21:25:50–21:27:04, 73.848s): startup/queue ~40.6s plus a 73.8s TU whose 50% is preprocessing (heavy include fanout). The TU compile alone exceeds the 60s budget before link even begins; the **monolithic DLL link then added 68.1s**, and identity-provider compile + publish brought the total to 189s.
- Inference (labeled): the flat 60s budget is below the structural minimum for a real incremental game-DLL build of this TU; the host was CPU-saturated (mean 99.2%) and memory-pressured, likely lengthening the link. Unknown: exact per-cc1plus CPU share (compiler rows were near 0 at the sampled instant; the TU was CPU-waiting on other cores/processes).

Acceptance status (not closing)
- wait_id / stage / elapsed / budget / evidence: **met**. One reviewer wake queued exactly once: **met** (single `wake.queued`, single `dev-loop.wait.reviewer-wake`).
- `reason`: **acceptable this occurrence** (clean `[GAME BUILD]` summary) — the issue-6e08d89b55e3 corruption did not recur here.
- **Build identity is stale/partial (defect recurs):** evidence records `build_id 2026-11-25T20:35:21.601166Z` (last published EXE artifact) and `generation 647` (last completed game build), while the in-flight attempt `game-dll-20261125T212509367039Z-c77aed` / generation 648 and its run id are absent. No first-class `build_attempt_id`/`run_id` in evidence. Owner: identity threading in `devscripts/dev-loop.py` per **issue-9a4503f6ed68**; occurrence appended there.
- Reviewer dispatch: exactly one wake queued, but arrival was late via fallback. `wake-25ef593488d5` queued 21:26:52.309Z (`.dev/agent-wake/events.jsonl`); primary adapter `codex` claimed/launched 21:26:56.505/56.581Z (pid 13272) then exited; `wake.retry` to `opencode` 21:47:26.846Z; opencode claimed/launched 21:54:44.997/45.041Z (pid 32348). Queue-to-final-launch **1672.7s (~27m53s)**, two `wake.launched` rows for one wake id. Occurrence appended to **issue-84c584957e65**.

`issue readiness` missing=[] so no field repair was needed. Left open; no duplicate issue created. No source change made or claimed.

Smallest next checks (unowned unless an owner claims)
- Budget: make the wait budget stage/TU-aware (e.g. >=180–240s for `game-dll-build`, or derived from measured per-TU p100) or require non-progressing evidence before filing friction; the 73.8s single-TU compile here alone proves the 60s flat budget is unreachable. Owner: **issue-9a4503f6ed68** / **issue-7d3dcfd7433b**.
- Identity: stamp `build_attempt_id`, in-progress generation 648, and `run_id` into the wait evidence at creation instead of the last published artifact/generation. Owner: **issue-9a4503f6ed68** (dev-loop.py).
- Link cost: 68.1s to link 644 objects / ~472 MiB is a structural latency source; track under the <=10s edit-to-live goal **issue-136d4553ff92**.
- Wake: detach the reviewer wake from the synchronous `run --once` primary/fallback retry so a dead `codex` adapter does not delay the working reviewer ~28 min; owner **issue-84c584957e65**.
- User context `ctx-81875ddb4419` ("continue, but put a temp limit raise to 9999 sec", unlinked, 21:55:17Z) may concern a wait/timeout limit but names no issue or setting; leave for the human/coordinator to route rather than inferring a policy change.
