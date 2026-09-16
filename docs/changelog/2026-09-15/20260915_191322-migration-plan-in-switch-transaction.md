# Migration plan validated + committed inside the switch transaction

- EST timestamp: 2026-09-15 19:13:22 EDT (UTC 2026-09-15T23:13:22Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--switch-transaction-selftest` 11/11; full suite 38/38)

## Report
- SUBSYSTEM: distributed hot-generation — switch-boundary migration transaction.

- SWITCH TRANSACTION MIGRATION
  - candidate plan storage: `HotReloadSystem` holds `candidatePlan_` bound to the
    pending candidate; the peer supplies it via `setCandidateMigrationPlan`
    (`multiplayer-tick.cpp` calls it after `prepareMigration`). It is not kept in
    unrelated test/debug storage.
  - exact F -> G validation: before any commit, `tryActivateCandidate` builds the
    authoritative local plan from the loaded candidate's declared schemas vs LIVE
    stored versions, cross-checks the peer plan, and runs
    `validateSwitchTransaction` (`active == plan.from`, `candidate == plan.to`).
  - no-op behavior: an unchanged-schema switch still produces an explicit
    `NoMigrationRequired` plan and is validated (source identity checked); only an
    initial load (no active source) skips the plan requirement.
  - commit ordering: validate plan -> register candidate migrations -> atomic
    `applySchemaUpdate` (inside `GenericRuntime::activate`) -> publish G. F is
    retired only after success.
  - active generation publication: `active_ = candidate` last; the plan is reset
    as consumed.

- ATOMICITY
  - can G execute against F state? No — the schema update runs inside activate
    (before publication) and G dispatch only exists after `active_ = candidate`.
  - can F execute against G state? No — `applySchemaUpdate` stages migrated blobs
    and returns false WITHOUT mutating on failure, and it runs before the
    registration commit.
  - activation failure behavior: migration/registration failure leaves F code AND
    F state intact (last-good); the candidate is retired.

- STALE PLAN PRODUCTION TEST: active becomes X, plan is F->G ->
  `validateSwitchTransaction == PlanStale`; no state mutation. PASS.
- MISSING PLAN TEST: real switch with no plan -> `PlanMissing`; no publication. PASS.
- ENTITY ID PRESERVATION: same EntityId across the atomic migration+publication
  (value preserved, new field initialized). PASS.
- LAST-GOOD MIGRATION FAILURES: prepare failed -> no READY; stale plan -> no
  activation; commit failure -> F state/version intact; load/validation failure ->
  F stays active. PASS.
- MIGRATION SWITCH SEMANTICS COMPLETE? **Yes** for the local switch transaction.
  Stop expanding migration architecture.

- LATE JOIN: **missing** (bootstrap state / cache miss / hit / gating).
- LATE JOIN WITH PENDING H: not implemented.
- LATE JOIN DURING COMMITTED SWITCH: not implemented.
- GENERATION CHANGES DURING BOOTSTRAP: not implemented.
- LATE JOIN FAILURE BEHAVIOR: not implemented.
- FULL GAME-LOOP TRANSACTION: not done.
- SAME SESSION / ENTITY IDS: not asserted.
- RAW CPP LIVE PROOF / TWO-PROCESS PROOF: none.
- BAD BUILD / BAD VERIFY / BAD MIGRATION LAST-GOOD: unit-covered; no live process.

- DISTRIBUTED CODE GENERATION COMPLETE ENOUGH? **No** — switch-transaction
  migration is now explicit and atomic; late join, the full production-loop
  transaction, and live proofs remain.

- NEXT: implement late join (GENERATION_BOOTSTRAP, cache miss/hit, pending-H and
  committed-switch joins, input/snapshot gating), then the full game-loop
  transaction; then distributed resources.

- LIVE-PROOF DEBT: late join, full-loop transaction, two-process/raw-cpp.

## Files changed
- `src/hot-reload/switch-transaction.h`: switch-boundary validation.
- `src/hot-reload/switch-transaction-selftest.{h,cpp}`: selftest suite.
- `src/hot-reload/hot-reload-system.{h,cpp}`: candidate plan storage,
  `buildMigrationPlan`, `registerDynamicMigrations`, validation in
  `tryActivateCandidate`, `lastSwitchRejection`.
- `src/network/multiplayer-tick.cpp`: bind the prepared plan to the candidate.
- `src/game/game-cli.cpp`: `--switch-transaction-selftest`.
- docs.

## Next (auto-selected)
Late join generation bootstrap: server advertises active generation + manifest;
client acquires/verifies/loads/activates G before world participation, with cache
miss/hit, pending-H, committed-switch, and generation-change-during-bootstrap
semantics; then the full production-loop F->G transaction.
