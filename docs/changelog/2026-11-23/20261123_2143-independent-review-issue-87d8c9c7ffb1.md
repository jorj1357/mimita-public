# Independent review: Payload cart collidability in Arcade mode (issue-87d8c9c7ffb1)

**Date:** 2026-11-23 UTC
**Agent:** issue-review-sweeper
**Wake:** `wake-ba365e6b1b58` (scheduled issue review)
**Issue:** `issue-87d8c9c7ffb1` — "Payload cart is not collidable in Arcade mode"

## Result

Read-only bounded review: no source edits, no build, no run, no lease, no close,
no owner claim. Appended review id 302 (verdict `unproven`); prior review text
preserved. Issue remains `in_review`, task `payload-cart-arcade-contact` still
open and owned by the inactive `payload-arcade-collision-fix`.

## Findings

- Fix identity is stable and current: `physics-collision-glb-sweep-slide.cpp`
  sha256 `6b5fd3cec1d3deab...`, `physics-types.h` sha256 `f3f5ca679e7e59c3...`;
  both match proof `task-proof-7d953b610754` and every recent build snapshot,
  including gen-570 (`game-dll-20261123T191346937240Z-35f07c`) and gen-571
  (`game-dll-20261123T200924195125Z-7401c3`). `build/current.json` = generation
  571, status ok, published 2026-11-23T20:23:13Z.
- Still zero runtime evidence: `rg` over all logs found 0
  `collision.arcade_entity.sweep` occurrences.
- Newest post-fix run on generation 571 is
  `logs/2026-11-23/20261123_205333` (probe gen 571, players spawned), but map
  `dust2cyberiav4` / mode `funcounterstrike`; cart=0, arcade_entity=0.
- Current live server `20261123_192224` (PID 10164) loaded generation 570, map
  `mimita-duels-map-v3`, mode `bloodtest`, npcs=0; no cart/arcade_entity events.
  It does not count (wrong scenario, not newest gen).
- No server journal has loaded `funworld3` since 2026-11-13 (pre-fix). Payload
  cart events (`payload.cart-spawned`) exist only in 11-12/11-13 logs. The
  first review's pre-fix journal `logs/2026-11-21/20261121_144350` no longer
  exists (rotated).
- Proof `task-proof-7d953b610754` (DB id 40): status=`runtime_proven`,
  result=`unproven`, build_id='', game_generation=0, run_id='', journal_path='',
  event=`collision.arcade_entity.sweep`, field_predicate=`entity_id`. Mismatch
  persists across all three reviews.
- `dev-loop --status`: lifecycle=`command_rejected`, build=`published_stale`
  (latest_source=4), run=`RUNNING 20261123_192224` uptime ~02:10.

## Process review

Repeated delay confirmed: ~49 h and generations 519→571 after the prior review,
a post-fix generation loads and serves players, yet the required Payload
scenario is never selected and the owning agent is inactive. One proposal
(claimable pinned-scenario build-to-proof next item, with measurable targets and
a gen-571 replay experiment) was appended to the existing
`issue-b396205aaecf`, and a coordination message was sent to
`build-runtime-subagents`. No duplicate issue created.

## References

- `docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md`
- `build/current.json`
- `logs/2026-11-23/20261123_205333/server-01/events.jsonl`
- `logs/2026-11-23/20261123_192224/server-01/events.jsonl`
- `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md`
