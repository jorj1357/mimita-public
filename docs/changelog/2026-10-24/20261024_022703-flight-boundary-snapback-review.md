# Flight boundary snapback investigation

- Local timestamp: 2026-10-23 22:27:03 EDT
- UTC timestamp: 2026-10-24T02:27:03Z
- Branch: `20261011zt`
- HEAD before and after: `2103321b012f546c07bb418add26269c66bbace0`
- Result: `PASS_WITH_HUMAN_REVIEW` for identifying the correction trigger from source and the active runtime journal; no implementation change was made.
- Scope: explain repeated server corrections while using developer flight and moving beyond the map's authored bounds.

## Finding

The active dev-loop was already running build `2978` on `payload1`, run `20261024_021456`. Its journal records 1,090 `movement.server-position-corrected` events, all with reason `out-of-bounds`. They span `2026-10-24T02:15:33.697Z` through `2026-10-24T02:22:10.109Z`. A representative report at server tick 1898 had reported X `-184.908 m` and accepted X `-186.103 m`; later events repeatedly clamp X to `-186.103 m`. This identifies the map X bound as the trigger in this run, not a detected wall crossing.

`makeMovementValidationConfig` in `src/network/movement-validation.cpp:361-380` adopts the loaded world's bounds. `validateClientMovementReport` checks them at `:679-687` and clamps an out-of-bounds report. `handleInputPacket` in `src/network/server-packets.cpp:1130-1165` records and sends each nonzero correction. `mpReconcileLocalPlayer` in `src/network/multiplayer-reconcile.cpp:197-205` applies that correction independently of ordinary drift thresholds. Therefore `hard_snap_distance: 999.0` does not prevent this forced correction; that separation is also specified in `docs/specs/networking/networking.md:68-93`.

Developer flight bypasses local world collision in `src/physics/physics-mini.cpp:206-224`, but movement reports still pass through the server bounds and geometry validation. The server's flight integration is in `src/network/server-players.cpp:1009-1025`. This mismatch explains why flight can keep moving locally while the server keeps returning the player to its boundary. A separate possible contributor is that local flight multiplies speed by 3 while dash is held (`physics-mini.cpp:217`), while the server flight branch uses only the configured multiplier; the journal's correction reason does not implicate that mismatch in this incident.

## Pre-existing work and changes

The working tree was already dirty across gameplay, networking, configuration, and documentation files. Relevant pre-existing edits were preserved: `docs/specs/networking/networking.md` changes countdown rules; `src/network/server-players.cpp` changes actor-preset reporting and Payload respawn protection; `src/network/server-packets.cpp` clears actor-preset state on disconnect. No local diff was present in `src/network/movement-validation.cpp`, `src/network/multiplayer-reconcile.cpp`, or `src/physics/physics-mini.cpp`.

This investigation added only this changelog. Before: the file did not exist. After: this exact investigation record was added.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/networking/networking.md`, `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/workflows/fix-repeated-bug.md`, `docs/regressions/README.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`; the server's bounds correction conflicts with the observed expectation for developer flight to continue beyond map bounds.
- The runtime journal already contained `versioninfo.executed`, build identity `2978:95f913e79db1a426742caec8e2ca5621196448a57fbde2317dc2bdb82844182c`, and the real server correction events. The dev-loop status confirmed this run was still active and current, so it was not restarted or interrupted.
- The `docs/workflows/fix-repeated-bug.md` reference to `docs/regressions/regressions-v1.md` does not resolve in this checkout; the `docs/regressions/` directory contains dated entries and a README. No regression entry was added because this review did not establish when the behavior changed.

## Validation and remaining review

- Source review: passed; correction order and client application were traced.
- Build/tests: not run by this investigation. Existing dev-loop evidence identifies build `2978` as current and published.
- Runtime evidence: the existing active `payload1` run confirms repeated `out-of-bounds` corrections; no new scenario was launched.
- Human visual/feel acceptance: not separately performed. The user-reported jitter matches the repeated correction sequence in the journal.
- No source or configuration behavior was changed. The code change needed to honor the expressed flight expectation is to exempt server-authorized flight reports from map-bound and blocking-geometry correction while retaining lifecycle, sequence, finite-value, and catastrophic-position checks; the local/server faster-flight multiplier should also be aligned. This remains unimplemented pending a live, bounded verification on the dev-loop executable.
