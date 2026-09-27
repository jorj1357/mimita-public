// 2026-09-16T18:10:00Z
/* purpose
* record the movement-authority fix that eliminated the spawn lock and the
* forward/back rubberband, and confirm the direction is the right one
* capture what remains as tuning, not a redesign
* this file DOES NOT replace docs/regressions/regressions-v1.md
* this file DOES NOT rewrite historical regressions
*/

# Regression: server did not track the player; forward/back rubberband (FIX DIRECTION CONFIRMED, TUNING REMAINS)

Status: FIX WORKING — TUNING REMAINS

## Regression Occurrence 1

### Observed

- The server never reflected the player's movement; the server position stayed
  near the spawn while the client moved.
- After the generation-mismatch fix, movement worked but with a stutter:
  forward ~0.9 m, back ~0.05 m, forward ~0.5 m, back ~0.1 m. The server position
  error could not exceed roughly 2 m.
- Jump did not work at all (dash, down-dash, freeze, and walking did).

### Expected Behavior

- The server authoritative state follows the player's validated movement.
- No repeated correction/rubberband during ordinary movement.
- Holding jump jumps whenever a jump resource is available; touching the world
  resets jump resources (wall climbing by contact).

### Actual Behavior

- `net.reconcile` returned a smooth/snap correction for any divergence above
  0.5 units, and the cold client hard-applied the server state every 250 ms,
  producing the forward/back stutter and the ~2 m cap.
- The server movement-validation policy passed through `computedDecision`,
  correcting/rejecting large-per-tick movement.
- `movement.main` set `jumpBufferSeconds = 0.0f`, and the shared jump policy only
  fires when the intent timer is `> 0`, so no jump ever fired.

### Why This Is Bad

- The player could not traverse; every attempt was pulled back.
- The jump ability was effectively missing.

### Specification

- `docs/specs/movement/movement.md`: one shared movement implementation; client
  prediction and server authority; held-jump behavior; universal contact reset
  for movement abilities; vertical velocity preserved; no waiting on movement.
- `AGENTS.md`: movement policy hot-reloadable; only a genuinely new kernel
  primitive may require a cold change.

### Wrong Code

- `src/hot-reload/modules/reconcile-policy.cpp` — returned
  `GAME_RECONCILE_APPLY_SMOOTH_ONCE` / `APPLY_SNAP` for divergence.
- `src/hot-reload/modules/rocket-behavior.cpp` — movement-validation handler set
  `decision = computedDecision` (no override).
- `src/hot-reload/modules/movement-system.cpp` — `jp.jumpBufferSeconds = 0.0f`.

### Confirmed Cause

- Correction policy that fought the client, plus a zero jump buffer.

### Fix Applied (this direction works)

- Hot `reconcile-policy.cpp`: position-divergence corrections are DISABLED
  (returns `MODE_NONE` / `APPLY_NONE`); the server adopts the client's validated
  movement, so there is nothing to converge.
- Hot `rocket-behavior.cpp`: movement-validation decision forced to Accept
  (`decision = 0`), with a restore comment showing how to bring back
  `computedDecision`.
- Hot `movement-system.cpp` / `actor-movement-system.cpp`: jump buffer now comes
  from `hot-actor-movement.h` (`kJumpBufferMode`: 0 = seconds default, 1 = ticks;
  `kJumpBufferSeconds` / `kJumpBufferTicks`). Held jump with auto-bhop jumps
  whenever a jump resource is available; contact resets the resource.
- Cold bridge: `GameReconcileV1.reserved` flag bits (`ALLOW_POSTGAP`,
  `ALLOW_SNAP`) are honored by `multiplayer-reconcile.cpp`, so the post-gap and
  catastrophic snaps are hot-controlled; both default off.

### Remaining Tuning

- Jump buffer value/mode (seconds vs ticks) — tune live in `hot-actor-movement.h`.
- Reconcile thresholds and whether to re-enable any gentle convergence — in
  `reconcile-policy.cpp`.
- Server acceptance breadth — in `rocket-behavior.cpp`.

### Permanent Prevention

- Keep every movement decision in hot modules; cold files hold mechanism only.
- Never edit files listed in `hot-modules.json` `cold` during a live session.
- Verify client and server are on the same active generation before judging
  movement.
- The physics files (`move-capsule.cpp`, `movement-step.cpp`, `physics-mini.cpp`,
  `movement-validation.cpp`) are now tracked as cold so a silent live edit is
  reported.

### Evidence

- `logs/09-16-2026/Server_log_*.txt` — `server authoritative player state ...
  vel=(0.0,0.0,0.0)`, `[SERVER MOVEMENT DECISION] ... decision=correct`.
- Selftests: `--reconciliation-policy-selftest` updated to expect no divergence
  correction; `--movement-parity-selftest` and `--air-movement-parity-selftest`
  pass with the shared movement function.

### Remaining Acceptance

- Live: jump works; travel is unbounded; no rubberband; wall-contact climbing.
