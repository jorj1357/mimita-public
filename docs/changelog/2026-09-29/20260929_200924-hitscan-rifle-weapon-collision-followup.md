# Hitscan rifle and weapon collision follow-up

- time: 2026-09-29 20:09:24 EDT
- status: PASS_WITH_BUILD_AND_RUNTIME_PENDING
- scope: weapon collision ownership, JSON collider examples, swept capsule behavior, and a new networked hitscan rifle

## Changed

- `config/weaponcollisions.json` now documents one working example for each supported collider style: single capsule, multiple capsules, explicit spheres, sampled capsule spheres, generated sphere chains, and swept boxes.
- Spy Knife boxes are enabled as the active JSON collider source. JSON capsule colliders now preserve the previous tick's endpoints, restoring continuous swept collision instead of resetting to a static overlap test.
- Added `hitscan_rifle` as a separate weapon definition using the projectile rifle model/viewmodel and automatic 60 Hz fire timing, while selecting the hitscan execution family and normal network mode.
- Registered the new weapon on both sides and mapped its legacy compatibility family to revolver so the existing generic `AttackRequest` gate accepts it before server-authoritative hitscan validation.
- Added the rifle to the stable weapon set and gave it a matching collision entry.

## Evidence

- PASS: JSONC parsing for `config/weaponcollisions.json`, `config/weapons.json`, and `config/weaponsets.json`.
- PASS: `git diff --check`.
- INVESTIGATED: the server already validates generic hitscan requests by dynamic weapon definition ID, cooldown, per-tick shot limit, world trace, and rewound target pose. The new rifle uses that path; it does not use the OP/admin revolver's `client_only` mode.
- NOT RUN: full compile/link and live multiplayer acceptance. The active `mimita.exe` process is using `.dev/builds/0496/mimita.exe`, and this checkout has no live-build script that can update these combat/network sources without risking the running session.
- NOT CLAIMED: visual collision behavior, multiplayer latency behavior, or in-game rifle availability until a fresh approved build is launched and tested.

## Follow-up acceptance

1. Build a fresh client/server from this worktree after the current running session is safe to replace.
2. Equip the stable set's Hitscan Rifle and verify that each shot produces one generic attack request and one authoritative result, with no projectile prediction or client-only route.
3. With Spy Knife collider diagnostics visible, verify the JSON boxes stop the player at a wall and apply the expected bounce/rollback at the wall edge.
