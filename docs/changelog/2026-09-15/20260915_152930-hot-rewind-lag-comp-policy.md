# Hot rewind / lag-compensation policy (net.rewind)

- EST timestamp: 2026-09-15 15:29:30 EDT (UTC 2026-09-15T19:29:30Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--rewind-policy-selftest` 6/6; full suite 30/30)

## Report
- SUBSYSTEM: rewind / lag-compensation policy.
- REAL SHIPPING PATH MIGRATED: `estimateServerRewindTick` (server-players.cpp)
  dispatches `net.rewind`; the returned target feeds hitscan (server-attack.cpp)
  and the projectile fire-view tick (server-projectiles.cpp).
- COLD POLICY OWNER REMOVED: the rewind-target math + max-rewind clamp are no
  longer solely cold (now fallback when no hot handler is active).
- HOT POLICY OWNER ADDED: `net.rewind` (`modules/rewind-policy.cpp`).
- GENERIC PAYLOAD: `GameRewindPolicyV1` (current/command/accepted ticks, history
  bounds, measured latency, interpolation delay, compensation, maxRewind,
  historyAvailable, attacker/target/current generation ids -> handled/allow/
  targetTick/clamped/reject/interpolate). No Player, Npc, Weapon, Rocket,
  snapshot, or connection pointers.
- GENERIC HISTORICAL QUERY STATUS: **not done** — `getPlayerPoseAtTick` /
  `getNpcPoseAtTick` remain parallel player/NPC history APIs; converging them to
  `historicalState(EntityId, T)` is the next rewind sub-slice.
- PLAYER/NPC GENERICITY: the policy is type-agnostic (EntityId + numbers only);
  the underlying history lookup is still player/NPC-specific.
- EXPLOSION-REWIND SUPPORT: architecture ready, not integrated. No
  rocket-specific infrastructure added. The generic historical-state substrate is
  the intended path; the known gap (explosion victim pose not fully rewound)
  remains.
- GENERATION AWARENESS: explicit — attacker/target/current mismatch returns a
  conservative reject (cold then evaluates at the current authoritative tick).
  Ids are 0 until distributed generation wiring.
- COLD MECHANISM REMAINING: history ring/storage, sample insertion, timestamps,
  raw sample retrieval, numeric interpolation of stored transforms, raycast/
  collision execution, entity lookup.
- COMPATIBILITY FALLBACK: cold rewind-target math runs when no hot handler is
  registered (last-good).
- SELFTEST PROVEN: command tick inside history, latency compensation,
  interpolation-delay compensation, max-rewind clamp, generation-mismatch
  reject, deterministic.
- REAL PATH PROVEN: yes. LIVE HOT-EDIT PROVEN: no.
- CONCURRENCY BOUNDARY STATUS: respected (movement + presentation files
  untouched).
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Rewind timing/latency-comp/clamp/
  sample-selection policy bugs: **no** (hot). History buffer/collision
  implementation: yes (cold by design).
- REWIND CATEGORY COMPLETE? **Mostly** — the policy is hot; the generic
  `historicalState(EntityId,T)` and explosion rewind remain.
- DISTRIBUTED-GENERATION WORK STARTED? No (next milestone).
- NEXT COLD OWNER: distributed hot generation delivery (logical generation id/
  hash vs platform artifact hash, READY/SWITCH), then optional generic
  historical-state + explosion rewind.

## Files changed
`src/hot-reload/hot-rewind.h` (new),
`src/hot-reload/modules/rewind-policy.cpp` (new),
`src/network/rewind-policy-selftest.{h,cpp}` (new),
`src/network/server-players.cpp` (`estimateServerRewindTick` dispatch + fallback),
`src/game/game-cli.cpp`, `src/hot-reload/hot-modules.json`; docs + this changelog.

## Next (auto-selected)
Distributed hot generation delivery: define logical generation identity vs
platform artifact hash, a generation manifest (ABI/schema/package hashes +
per-platform descriptors), and the phase protocol BUILD→VALIDATE→ANNOUNCE→
ACQUIRE→VERIFY→READY→SCHEDULE_SWITCH→SWITCH→RETIRE_OLD with a shared switch tick.
