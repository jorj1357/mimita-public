# NPC wall containment reset loop

Time created: 2026-11-03T18:52:13Z
Time last updated: 2026-11-03T18:52:13Z

Status: ATTEMPTED FIX (2)

Correction 2026-11-03: the template body-collider experiments described below
were removed. Headless NPCs now use the shared capsule movement path and the
bounded ground clamp, matching the Nov 2 behavior. This record remains at
ATTEMPTED FIX until a runtime journal proves NPCs stay out of the reported
closed block and the 60 Hz/tick budget is clean. See
`docs/regressions/master/collisions-REG.md` entries 16–18.

## Failure

`devscripts/scenarios/npc_wall_containment_v1.json` fails after template NPC
body collision was introduced. Over the 30 second soak the run recorded
1,955 `npc.pose.reset` events and 116 `npc.pose.penetration` events. Most
resets are `no_valid_pose`; final penetration ranges from `0.03 m` to
`1.45 m`.

The scenario expects `npc.pose.reset` and `npc.pose.penetration` to be absent
while NPCs are contained beside a closed block.

## Cause and attempted corrections

The headless server builds NPC body collision from
`MimitaNet::standardPlayerBodyTemplate()`. The first fallback implementation
centered every box at the bone origin, ignoring the template's mesh-center
offset. It also seeded `previousWorldTransform` with a root-local matrix before
the first world-transform update.

Attempted fix 1: build each box around
`inverse(restTransform) * offset` instead of the bone origin.

Attempted fix 2: after the first `updateModelWorldTransforms`, copy the real
world transform into `previousWorldTransform` for every part.

Both fixes are in `src/network/server-npcs.cpp`. The scenario still fails, so
the remaining cause is not proven.

## Evidence

- Failing run: `logs/2026-11-03/20261103_184934/events.jsonl`.
- Build: `2026-11-03T18:49:25.936727Z`.
- First penetration sample: actor 1000, leftLeg/rightLeg, floor triangle 3911,
  `0.0203 m` penetration.
- Reset reasons: 2,220 `no_valid_pose`, 65 `last_clear`, 21 `safe_point`,
  7 `post_actor_pair`.
- The same run also shows `performance.server-tick-window` NPC maxima of
  `100-156 ms`, so this is both a containment failure and a 60 Hz regression.

## Next diagnostic

Trace one `no_valid_pose` actor across the tick: final-pose contacts, safe-point
candidates, world triangles, and the actor-pair correction that placed it
there. Do not silence the scenario or raise the reset threshold as the fix.
