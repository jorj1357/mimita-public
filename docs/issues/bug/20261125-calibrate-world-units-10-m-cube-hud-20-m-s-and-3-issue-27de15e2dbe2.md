# issue-27de15e2dbe2: Calibrate world units: 10 m cube, HUD 20 m/s, and 3.6 m capsule disagree

## Human report (verbatim, immutable)

> unit scale i think was 1.0 in blender so 10x10 in blender = 10 meters irl but in game 20 m/s edge to edge= 1 sec so mabye my speed is 2x'd and im going 10m/s actually ... maybe 10x10 in blender is 20x20 meters irl? ... 3.6m capsuel ehight


## Agent suggestion

### 2026-11-25T21:30:48.847Z — resource-profiler

Source trace for the unit mismatch (read-only; no code change).

OBSERVED
- config/actor-collision.json:17 (all presets) capsule_height_m=3.6; src/physics/config.h:45 PLAYER_HEIGHT=3.6; src/avatar/character-manifest.h:7 height=3.6. The same 3.6 value is simultaneously the world-unit capsule and a 1.8 m human -> 2 WU = 1 m.
- src/npc/npc.cpp:62-67 already encodes this: kNpcMetersPerWorldUnit=0.5f, comment says capsule 3.6 WU = 1.8 m and a 20-unit block crosses in 1 s at 20 unit/s.
- docs/specs/npc-behavior/npc-behavior.md:1008-1017 documents the same 2:1 convention as provisional and says correct it only with a measured player probe.
- src/engine/engine-tick-ui-game-hud.cpp:350-353 prints raw glm::length(player.vel) with label 'Speed: %.2f m/s'. The HUD labels world-units/second as m/s, so the reported 20 is 20 WU/s.
- config/movement/movement-default.json:39-40 ground_speed=20.0, air_speed=20.0 (FCS default). config/movement/movement-deliberate.json:27-34 contradicts by saying 'read one world unit as roughly one metre' and max_speed 1.0 in m/s.

INFERENCE
- Crossing a span in 1 s at 20 WU/s means the block measured 20 WU across = 10 m under 2:1. Under 2:1 the user is moving ~10 m/s, not 20 m/s, and the 3.6 'm' capsule is really 1.8 m. This supports the user's 'speed is 2x'd, actually 10 m/s' reading and rejects '10x10 Blender = 20x20 m IRL' unless the import is 1:1 and the reported speed is doubled instead.

UNKNOWN / SMALLEST NEXT CHECK
- Exact GLB POSITION min/max and Blender import/object scale for the block; whether the cube object scale was 2.0; exact map/run and movement preset used. Those decide between 'import 2x' and 'speed label 2x'.
- Probe: pin one run that logs player position + elapsed time + raw WU/s across the known block and dumps the block GLB bounds, then compute WU/m independently. No global scale change should precede that probe.

## Agent suggestion

### 2026-11-25T21:38:45.960Z — build-wait-reviewer

2026-11-25 (build-wait-reviewer) - GLB bounds resolve the "import 2x vs speed label 2x" unknown (read-only; no code change).

OBSERVED (exact GLB bounds, node TRS applied, computed locally)
- assets/entity/player/default/mimita-char-no-animations-v4.glb: overall world height 3.3881 units (min z -1.6616, max z 1.7264).
- assets/maps/mimita metric sizing.glb:
  - Cube.002 and Cube.003..Cube.007 are each 20.0 x 20.0 x 2.0 world units (unit mesh [-1,1] with node scale 10).
  - Cube floor is 200 x 200 x 2 units (node scale 100).
  - Cube.001 is 2.0 x 2.0 x 3.6 units (a 1 x 1 x 1.8 m reference under 0.5 m/unit).
  - mimitachar reference is 0.88 x 1.616 x 3.4 units.
- Runtime config: capsule 3.6 units (config/actor-collision.json:17, src/physics/config.h:45), matching the 3.39-unit character.

INFERENCE
- Authored content is consistently 2 world units = 1 m. The 3.39 u character and 3.6 u capsule both represent a 1.8 m human; the metric map's "10x10" block is 20x20 units = 10x10 m, and its floor is 100x100 m.
- Crossing 20 units at 20 units/s takes 1.0 s = 10 m at 10 m/s. Movement is not 2x too fast; the HUD label is wrong because src/engine/engine-tick-ui-game-hud.cpp:351-354 prints |velocity| (units/s) as "m/s".
- The user's "I'm actually going 10 m/s" is correct; "10x10 Blender = 20x20 m IRL" is not (it is 20x20 units = 10x10 m).

UNKNOWN / NEXT
- Need one pinned run to confirm the same ratio live (travel distance/time + HUD text). Then a human decision: (a) keep 2 WU/m and convert HUD/metric-facing labels to metres (minimal safe change), or (b) rescale authored content to 1 WU/m (scope-changing asset migration; do not start before approval).
- Overlaps issue-feef64cab2a7; this issue now holds the decisive asset evidence, so do not duplicate the GLB trace there.

## Agent discussion and suggestions

### 2026-11-25T21:40:03.780Z — build-wait-reviewer

Message: `msg-89873e392405` (question)

Subject: GLB bounds resolve the unit question

Sharing decisive asset evidence on issue-27de15e2dbe2 (read-only). mimita-char-no-animations-v4.glb is 3.3881 world units tall; its capsule is 3.6. assets/maps/mimita metric sizing.glb has 20x20x2-unit '10x10' blocks, a 200x200x2-unit floor (100x100 m), a 2x2x3.6-unit reference block, and a 3.4-unit mimitachar. So authored content is consistently 2 WU = 1 m; the 1 s crossing is 20 units = 10 m at 10 m/s. The HUD's raw-units-as-m/s label is the defect, not movement speed. Proposed smallest next check: one pinned run logging travel distance/time and HUD text to confirm the ratio live; then human decides whether to keep 2 WU/m and fix labels or rescale assets. Do you agree, or do you see a different discriminator?
