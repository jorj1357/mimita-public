// 2026-09-16T17:40:00Z
/* purpose
* record the confirmed regression where the server never tracked the player
* (stuck near spawn, server position error never returned to 0)
* capture the exact evidence, cause, and permanent prevention
* this file DOES NOT replace docs/regressions/regressions-v1.md
* this file DOES NOT rewrite historical regressions
*/

# Regression: server never tracks the player / stuck near spawn (client/server hot generation mismatch)

Status: ROOT CAUSE CONFIRMED

## Regression Occurrence 1

### Observed

- In a dedicated-server session (server pid 25376, client pid 26140) the player
  could move on the client, but the server's authoritative position did not
  follow. The client/server position error ("d") stayed non-zero and the server
  repeatedly showed a stale position (e.g. `p2 pos=(-625.5,106.1,2351.9)
  vel=(0.0,0.0,0.3) onGround=0`).
- Earlier in the same investigation the player was repeatedly snapped back to
  the spawn. That phase was caused by repeated deaths (NPC on the spawn point,
  then a lethal explosion-damage override); it is recorded here only to
  distinguish it from this occurrence.
- The live-code on-screen panel showed `CLIENT ... clientGeneration=10` and
  `SERVER STILL RUNNING GENERATION 76`.

### Expected Behavior

- The client and server both run the same active hot generation, so they execute
  the same movement policy.
- With the server adopting the validated client movement, the server position
  matches the client and `d` returns to ~0.

### Actual Behavior

- The client was pinned to hot generation 10 while the server was on ~76.
- The server's hot worker attempted generations 77..83 and every attempt
  returned `result:"retry"`.
- A cold source (`src/live-code/live-behavior.cpp`) was edited during the live
  session, producing `HOT_RELOAD_BOUNDARY_VIOLATION` and
  `cold_restart_pending`.

### Why This Is Bad

- Different generations execute different movement/authority code. The client's
  generation predated the input/adopt policy modules, so the server could not
  adopt the client's validated movement; the two states diverged permanently.
- `mpGenerationWorldAllowed` gates snapshot consumption and input sending on a
  matching generation, so the mismatch can silently suppress the entire
  client/server world interaction.

### Specification

- `docs/specs/movement/movement.md`: client and server call the same movement
  implementation; the server validates and replicates; the client prediction and
  the server authority must agree.
- `AGENTS.md`: the running `mimita.exe` must remain alive during normal
  development; movement policy must be hot-reloadable; only a genuinely new
  kernel primitive may require a cold change.

### Wrong Code / Wrong State

- Cold files were edited while a live session was running
  (`src/live-code/live-behavior.cpp`, plus other cold bridge files).
- The hot worker retried the same failing hash repeatedly instead of stopping
  once and reporting a single, clear boundary/restart condition.
- Generation mismatch did not force a visible, blocking state; the session kept
  running with two different generations.

### Confirmed Cause

- Client/server hot generation mismatch (10 vs 76) combined with
  `cold_restart_pending` and a hot-build retry loop (`result:"retry"`),
  preventing both peers from converging on one generation.

### Evidence

- `logs/features/live-code/2026-09-16/live_events_20260916_212040.jsonl`
  - `{"process":"client", ... "type":"generation_mismatch","generation":10,
     "result":"client_only","remote_generation":78}`
  - `{"process":"client"/"server", ... "type":"cold_restart_pending",
     "file":"src/live-code/live-behavior.cpp"}`
- `logs/features/live-code/2026-09-16/live_events_20260916_212049.jsonl`
  - `compile_started` generations 77..83 with `result:"retry"`.
- `logs/09-16-2026/Network_log_172049.txt`
  - `[SERVER] ... p2 pos=(-625.5,106.1,2351.9) vel=(0.0,0.0,0.3) onGround=0`.

### Permanent Prevention

- Never edit files listed in `hot-modules.json` `cold` during a live session; a
  cold change requires relinking the EXE and restarting.
- Before testing movement, confirm the client and server are on the same active
  generation.
- Hot generation mismatch and bootstrap decisions are exposed as a hot policy
  (`net.generation-policy`) so they can be changed live and can never silently
  wedge the session.
- Keep all movement/spawn/input/lifecycle/reconcile decisions in hot modules.

### Remaining Acceptance

- Relaunch both peers from the same EXE; verify equal generations and that the
  server position tracks the client (`d` ~ 0).
