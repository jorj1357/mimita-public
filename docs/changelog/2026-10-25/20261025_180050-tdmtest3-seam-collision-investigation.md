# tdmtest3 seam collision investigation

- **Time UTC:** `2026-10-25T18:00:50Z`
- **Display time:** `2026-10-25 14:00:50 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `0f33025c`
- **Result:** `FINDINGS_NEED_HUMAN_DECISION`

## Request

Investigate why close/touching block geometry in `assets/maps/tdmtest3.glb`
does not act as one surface, why the actor catches on invisible faces, and why
the problem is noticed while firing. Replay
`devscripts/scenarios/user/scenario-20261025_174241-tdm2.jsonl` on tdmtest3,
apply `healthme 999999`, and inspect the live `events.jsonl`.

## Runtime evidence

- Build: `3214`, source generation `16`,
  `C:\mimita-v9\.dev\builds\3214\mimita.exe`.
- Run: `20261025_175420_01`.
- Journal:
  `C:\mimita-v9\logs\2026-10-25\20261025_175420_01\events.jsonl`.
- Launch mode: `30` (`team_deathmatch`, `tdmtest3`, `--weapon-set 1`,
  `--no-npcs`), launched through `python devscripts/dev-loop.py`.
- `healthme 999999` completed at client tick `831`.
- `scenario.playback.started` at `2026-10-25T17:54:40.823Z`, frame count `4858`.
- An earlier attempt was rejected with `scenario.playback.rejected`
  `reason=weapon_set_mismatch`; the retry with weapon set `1` played.
- Playback emitted a divergence/snap near frames `120-186` because the source
  recording contains a death/respawn jump from about
  `[1571.9, -1854.5, 2343.8]` to `[1526.1, -1858.5, 2416.4]`. This is a
  recording-state artifact, not a collision finding.
- Build/link and the runtime path are separate claims. Visual feel and final
  acceptance remain for the human.

## First seam failure

`collision.solve.summary`, client tick `1534`,
`2026-10-25T17:54:52.169Z`:

```text
position        [1554.67, -1845.52, 2349.55]
velocity_before [-9.78, 22.91, 0.47]
velocity_after  [-9.78, 0.04, 1.21]
contact         triangle 600, leftLeg
contact_point   [1554.64, -1845.62, 2347.96]
normal          [-0.00, -1.00, 0.09]
surface_normal  [0.00, -1.00, 0.00]
penetration     0.1129
```

The actor was crossing a flush floor seam at `y = -1845.62`; its northward
`y` velocity was reduced from `22.91` to `0.04` by a hidden vertical face.

Parsed GLB geometry:

- triangle `70`/`71` = top face of `Cube.6700`, normal `+Z`,
  `z = 2347.96`.
- triangle `610` = top face of `Cube.6765`, normal `+Z`, `z = 2347.96`.
  The two tops are coplanar and flush.
- triangle `600` = south face of `Cube.6765` at `y = -1845.62`,
  `z 2337.96..2347.96`, normal `-Y`.
- triangles `64`/`65` = north face of `Cube.6700` at the same plane,
  normal `+Y`. They are coincident hidden faces with triangle `600`.

So the visible union is one flat 40 x 20 floor, but the collision mesh still
contains the invisible internal wall between the two blocks. The rounded
`MOVEMENT_FEATURE_SMOOTHNESS = 0.1` shell turns that internal top edge into a
small physical bump.

## Scale in this map

A static-mesh check of `tdmtest3.glb` found:

- `1368` render triangles;
- `230` coplanar, opposing, overlapping face pairs between different objects;
- `243` unique triangles involved in those internal-face pairs.

During the recorded replay, `51` sampled solve summaries contained contacts on
those internal triangles:

- weapon `19`, leftLeg `15`, rightArm `7`, rightLeg `6`, head `2`,
  leftArm `2`.

Examples with large response changes:

- tick `2520`, triangle `272`, `[-7.16, -3.46, 39.33]` to
  `[-26.40, 2.28, 0.85]`.
- tick `3202`, triangle `1356`, `y` velocity `26.67` to `-9.34`.
- tick `3249`, triangle `785`, `y` velocity `29.61` to `-10.37`.
- tick `1673`, triangle `71` (`weapon` part, invisible revolver box):
  floor contact while the actor was about `2.3 m` above it.

## Why contacts are not merged

- `mergeContactsByNormal` (`src/physics/movement/actor-triangle-solver.cpp:184`)
  merges only contacts with normal dot `>= 0.95` from the same entity. A floor
  (`+Z`) and a seam wall (`-Y`) have dot `0`, so they never merge.
- `removeTouchingFaceSeams` (`:234`) only handles nearly opposite normals
  (`dot <= -0.98`). It does not cover a walkable top plus a perpendicular
  hidden side face.
- `collapseCloseFeatureContacts` (`:281`) has a step-up seam rule, but it
  requires horizontal separation `<= 0.2 m` (2 x feature smoothness). The
  measured floor-to-wall contact separation at tick `1534` was about `0.27 m`,
  so the rule did not fire.
- `docs/architecture/collision/collision.md:134-136` states explicitly:
  contact merging prevents repeated sharp bounces, "but this is not a
  topological face weld." That is the current limitation.
- There is no load-time union/CSG pass that deletes internal faces between
  touching or overlapping authored blocks.

## Why firing correlates

The revolver adds a large self-impulse on every shot:
`src/combat/weapon-system.cpp:900-902` applies
`player.externalImpulse -= dir * firingRecoilStrength;` and the default is
`20.0`. Rapid revolver fire is therefore the main movement force in this
recording. It drives the actor at speed across the hidden seams; the collision
bug is not a separate weapon-collision code path.

A contributing factor: the revolver's invisible collision box is
`half_size = [1.02, 0.22, 0.22]` (about `2.04 m` long) in
`config/weaponcollisions.json:32-33`. While the recorded shots aim about
`27 deg` downward, the box repeatedly contacts the floor and seam faces
(`actor_part=weapon`), adding more stop/bounce behavior.

## Files changed

- Added this investigation changelog.
- Added dev launch mode `30` (`tdmtest3 collision repro`) in
  `devscripts/dev-launch-modes.json` so the same map/weapon-set setup is
  repeatable.
- Temporarily set the collision logger to `verbose` for the run, then restored
  `config/debuglogger.json` to `important`. The restored file has no diff.
- No gameplay collision source was changed.

## Recommended next action (not implemented)

1. Remove internal faces in the static map collision mesh at load time: detect
   coincident coplanar opposing faces between different source objects and
   delete the overlapping internal region (a box-union / CSG step), then rebuild
   the world tree. This matches the requested "touching objects are one object"
   behavior and removes the source of the rounded hidden wall.
2. As a narrower fallback, add an edge-adjacency seam rule that drops a vertical
   contact when its top edge is shared with a walkable contact and the vertical
   face lies below that walkable plane. Do not simply raise the `0.2 m`
   horizontal threshold; that can suppress real walls near the feet.
3. Shrink the revolver collision box toward the actual model length so aiming
   down while firing does not drag a 2 m invisible box across the floor.

Runtime acceptance for any fix must reuse this exact recording on tdmtest3 and
show that the tick `1534` velocity drop and the internal-face contacts above
no longer occur.
