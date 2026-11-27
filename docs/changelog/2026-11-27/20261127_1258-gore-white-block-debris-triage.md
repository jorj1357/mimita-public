# Gore white-block debris: root-cause triage, ownership, spec acceptance

- Task/agent: `rocket-league-drive` (wake `wake-4380cf0ec56e`, context `ctx-6c27dcec41a4`)
- Date, time, timezone: 2026-11-27T12:58-05:00
- Status: triage and documentation complete; runtime/visual proof pending (no live run at intake)
- Linked issue: `issue-48c90c86187d`

## Requested behavior

The user reports opaque white blocks spawning when they are hit or hit another
player, calls them gore-related debris visuals, and wants them gone. They asked
that the "no white blocks" requirement be written into the spec or issues.

## Findings

Leading hypothesis (source-only, medium-high confidence): Bloodtest gore bone
shards render as flat near-white cubes.

- `src/effects/effect-part-blood.cpp:525-531` sets `part.box = true` and
  `part.replayType = "gore_bone"` for bone chunks.
- `config/impact_decals.json:135-150` gives `boneShards.color` `[0.9,0.85,0.7]`
  and no `texture`.
- `src/effects/effect-part-render.cpp:616-618` routes `effect.box` to
  `DebugVis::drawFilledBox` and ignores `texturePath`.
- `src/debug/debug-visuals-render.cpp:333-359` emits 12 flat-colored triangles
  per box.

Secondary defect in the same branch: `gore_organ` sets both `box=true` and a
`texturePath`, but the box path ignores the texture.

Ruled out as the current source: `blood.spray.debris` is disabled (the
2026-10-07 beige-cube debris path the user remembers from about a month ago),
`hitfx.worldDebris.enabled=false`, and `WeaponHitFxConfig::debrisFor()` has no
callers.

## Changes

- `docs/specs/gamemodes/bloodtest.md`: added acceptance item 15 requiring gore
  chunk debris to use its configured flesh/bone/organ material, color, and size;
  no untextured near-white block; bone shards never draw as flat white boxes.
- `issue-48c90c86187d`: appended the full root-cause analysis, fix direction, and
  evidence gap; assigned to the active damage/gore owner `body-rig-ads-cont`.
- Notified `body-rig-ads-cont` (owner request) and stood down the other agents
  woken for the same context (`rocket-league-input-fix`, `rocket-league-settle`)
  to avoid duplicate work.
- Acknowledged `ctx-6c27dcec41a4`.

The desired behavior was already added to
`docs/specs/visuals/blood-bulletholes-viscera.md:289-293` by the intake agent
that filed the issue; this session added the matching Bloodtest acceptance item.

## Evidence and limits

- No compiler, build, or runtime was started. The game-DLL build was failing on
  a changed-source preflight (`src/engine/engine-tick-camera.cpp`) and a live run
  was not active, so no runtime proof is claimed.
- Remaining proof: a pinned Bloodtest run (in-game hits or `agent.corpseprobe`)
  with a screenshot showing the actual white blocks and the matching
  `bloodtest.blood-effect.applied` / debris render records, then a human visual
  check after the render fix.
- Fix options recorded on the issue: render textured gore chunks through the
  textured path even when `box` is set; give bone shards a real bone material and
  move their color away from near-white; add one rate-limited
  `gore.debris.render` event for proof.
