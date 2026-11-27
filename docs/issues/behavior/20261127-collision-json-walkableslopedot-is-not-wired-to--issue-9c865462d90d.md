# issue-9c865462d90d: collision.json walkableSlopeDot is not wired to physics or movement

## Human report (verbatim, immutable)

> config/collision.json has walkableSlopeDot, but movement-config.cpp and physics-collision-core.cpp use MAX_WALKABLE_SLOPE_DOT. Wire the JSON value in or delete the dead key. Add collision.slope.config and collision.slope.classify events and prove with a real ramp traversal. (docs/handoffs/2026-11-27-body-rig-ads-damage-handoff.md item 7)


## Assignment

### 2026-11-27T07:36:40.191Z — opencode-deepseek-v4-3

Owner: opencode-deepseek-v4-3

## Agent suggestion

### 2026-11-27T07:52:48.805Z — opencode-deepseek-v4-3

Runtime evidence for config wiring (gen 786, run 20261127_074621): build 2026-11-27T07:45:59.076100Z:acbb9f..., server and client journals both contain collision.slope.config at startup with walkable_slope_dot 0.8000000119 and changed=false. I then hot-edited config/collision.json walkableSlopeDot to 0.85; client journal event COLLISION_1116 reports changed=true, previous 0.8, applied 0.8500000238; restoring 0.80 produced COLLISION_1324 changed=true applied 0.8. The value now comes from config/collision.json and feeds MAX_WALKABLE_SLOPE_DOT; movement-step falls back to the same global (movement presets can still override with a non-zero value). Proof task slope-config-wiring task-proof-5debe11dd6c1 proven. Still open: collision.slope.classify event and real ramp traversal proof; do not mark the whole item done yet. The first implementation caused a gen-785 server startup regression by constructing MovementJsonConfig from CollisionConfig::load (recursive singleton init); fixed and proven by gen-786 server.process.started.

## Review (needs-work)

### 2026-11-27T07:52:49.075Z — opencode-deepseek-v4-3

Reviewer: opencode-deepseek-v4-3

Config wiring only is proven; classify event and ramp traversal remain. Keep open.
