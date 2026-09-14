# Movement groundwork: MovementStateV1 + physics.moveCapsule capability

- EST timestamp: 2026-09-13 21:43:20 EDT (UTC 2026-09-14T01:43:20Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / BOOTSTRAP_BLOCKED` (`mimita.exe --server` pid 26248 running)

## Steps 1 & 2 of making built-in movement hot

### Step 1 — POD movement state (`src/hot-reload/game-api.h`)
- `MOVEMENT_STATE_VERSION = 1` and `MovementStateV1 { position[3], velocity[3], yaw,
  radius, halfHeight, sizeScale, grounded, collided }` — a stable plain-data
  projection the kernel can read from / write back into the real player.

### Step 2 — `physics.moveCapsule` capability
- `GameMoveCapsuleFn` + `GameplayContextV1.moveCapsule` (append-only).
- `src/live-code/live-behavior.cpp`: `capMoveCapsule` builds a `RigidBody` capsule
  from `MovementStateV1`, integrates gravity, runs the kernel world collision
  (`collideWithWorld` + `depenetrateWorld`), and writes the resolved
  position/velocity/grounded/collided back — a real, allocation-light
  capsule-vs-world solve that hot movement can call without cold collision code.
- Activated the existing (previously unused) `LiveBehavior::setDispatchWorld`
  seam: `simulate-tick.cpp` publishes the world each tick so the movement/query
  capabilities have world access.

## Evidence

- `-fsyntax-only` clean: `src/live-code/live-behavior.cpp`,
  `src/sim/simulate-tick.cpp`.
- Cold build **not run**: `mimita.exe --server` (pid 26248) is running. No process
  killed. A parallel torture test also still holds an intentional syntax error in
  `src/hot-reload/modules/banana-system-renamed.cpp`, so a full hot-package link
  would fail until that test concludes.

## Next (step 3 onward)

3. Reimplement the built-in movement step in `movement-system.cpp` as a hot
   `movement.main` system using `readComponent` + `moveCapsule`, and flip the
   kernel to prefer the hot step when present (fallback to the built-in).
4. Parity + determinism tests: N-tick bit/state comparison vs the built-in step,
   no-allocation guard, fixed 60 Hz.
5. Fold free-fly into `movement.main` as a mode (create mode branch) rather than
   a separate override.
6. Runtime proof: edit `movement-system.cpp` live and see walking/jumping change
   with the same EXE/world/EntityIds.

## Files

Changed: `src/hot-reload/game-api.h`, `src/live-code/live-behavior.cpp`,
`src/sim/simulate-tick.cpp`.
