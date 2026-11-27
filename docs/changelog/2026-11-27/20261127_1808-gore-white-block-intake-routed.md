# Gore white-block intake: cross-check, stand down, ownership repair

- Timestamp: `2026-11-27T18:08:00Z`
- Agent: `rocket-league-settle`
- Context: `ctx-6c27dcec41a4`
- Issue: `issue-48c90c86187d` (Gore hit debris spawns white blocks instead of body debris)
- Result: `ROUTED` (no code change kept, no build, no runtime claim)

## Intake and triage cross-check

The high-priority user context asked for the white blocks that appear on gore hits
to be removed and for the desired behavior to be written into a spec or issue.

Cross-checking the issue record showed this had already been triaged by
`rocket-league-drive` while this session was starting:

- Leading cause: gore bone shards render as flat near-white boxes. `spawnGoreEffect`
  sets `part.box=true` / `replayType="gore_bone"` and `part.color` from
  `gore.boneShards.color=[0.9,0.85,0.7]`, but it sets only `part.scale` and never
  `part.halfSize`. The `effect.box` render path draws `effect.halfSize`, whose
  default is `0.1 m`, so each bone shard ignores its configured `0.02-0.085 m`
  size (`src/effects/effect-part-blood.cpp`, `src/effects/effect-part.h`,
  `src/effects/effect-part-render.cpp`).
- `blood.spray.debris` and `HitEffects.worldDebris` are both disabled in config,
  so neither is this source.
- Desired behavior was already recorded in
  `docs/specs/visuals/blood-bulletholes-viscera.md` (chunks must honor configured
  color/size and never draw as untextured default-white or default-sized boxes)
  and in `docs/specs/gamemodes/bloodtest.md` acceptance item 15.
- Ownership was routed to `body-rig-ads-cont`, and this agent was told to stand
  down rather than duplicate the work.

## Actions taken

1. Reverted the duplicate in-progress edit to
   `src/effects/effect-part-blood.cpp` (`part.halfSize=vec3(part.scale)` plus a
   bounded `gore.chunk.spawn` trace). The file is clean at `HEAD`.
2. Removed the now-invalid pending build note `e0070a55abad` so the next build
   is not annotated with a change that no longer exists.
3. Restored `issue-48c90c86187d` owner to `body-rig-ads-cont`.
4. Repaired the issue's stale paths: removed nonexistent `config/effects.json`
   and added `config/impact_decals.json` and the viscera spec.
5. Acknowledged `ctx-6c27dcec41a4` and replied to the routing message with the
   stand-down and the trace suggestion (`gore.chunk.spawn` gated by
   `gHitFxTraceEnabled`) so the routed owner can satisfy the journal-predicate
   acceptance item.

## Evidence and limits

Source inspection only. No build was requested (a shared game-DLL build was
already in progress under CPU pressure) and no runtime or visual proof is
claimed. The gore white-block issue remains open and unproven pending the
routed owner's fix plus a pinned Bloodtest hit journal and human screenshot.
