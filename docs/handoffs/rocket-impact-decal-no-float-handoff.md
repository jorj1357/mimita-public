# Rocket impact decals: no floating

**Purpose:** Give the next agent a concrete investigation and acceptance plan
for eliminating floating rocket impact marks.

**Audience:** The next MiMITA agent working in the shared checkout.

**Last inspected:** `2026-11-18T10:02:24Z`.

## Scope and authority

The user's request is: rocket impact decals must never float; when a rocket
hits an actor, do not attach the mark to that actor's body. Find the nearest
sensible world surface and place the mark there, or omit it when there is no
valid surface. Prove the decision and rendered result in the canonical
`events.jsonl` journal, then confirm the appearance in game.

The attached image is visual evidence only; it contains no additional
instructions. It shows a first-person view inside a narrow indoor passage, with
large dark star-shaped marks visible across the floor, walls, and ceiling. The
screenshot chat shows repeated NPC rocket-launcher kills, but is not tied to a
verified build or run ID. It supports the report, but cannot by itself tell
whether each mark is floating, attached to the wrong surface, or simply
oversized.

This handoff is an investigation plan. It does not claim the no-floating
acceptance has passed, change the desired behavior, or authorize an agent build
request. Follow the current shared-checkout and root dev-loop rules in
`docs/specs/dev-loop/agent-lanes-build-authority.md`.

## Does not

- Treat the screenshot or a spawn log as proof that a decal is attached.
- Treat an 8 m collision-mesh candidate as proof that it is the nearest visible
  surface.
- Change damage, rocket collision, or multiplayer authority while fixing decal
  placement.

## What is known

- The user's latest report and screenshot show the bug remains visible after
  earlier source changes. Treat the earlier attempt as unaccepted.
- `src/combat/weapon-rocket-launcher.cpp::findNearestWorldSurface` currently
  searches static world collision triangles in an 8 m box around actor impacts
  and selects the closest triangle point. This excludes actor colliders, but
  does not prove that the chosen triangle is the visible surface nearest from
  the player's view, or that collision and rendered geometry coincide.
- `doExplosion` emits `rocket.impact-mark.resolved` with hit type, target,
  triangle index, distance, anchor position, and the `spawnExplosionFx` result.
  This proves the resolver's choice only when that exact source revision is
  active; it does not prove decal rendering or visual attachment.
- `src/effects/effect-part.cpp::spawnExplosionMark` offsets a mark by 6 mm along
  its normal. `src/effects/effect-part-render.cpp` draws a textured quad offset
  from that stored position. Its existing `impact_decal.render_submitted`
  diagnostic records the submitted position, normal, basis, texture load,
  quad dimensions, alpha, and camera distance.
- The global explosion mark configuration in `config/impact_decals.json` uses
  `radius: 4.0` world units. The screenshot's unusually large dark marks may be
  a separate size/texture issue. Keep size diagnosis separate from testing
  whether the mark is physically attached to a surface.
- The inspected client journal
  `logs/2026-11-18/20261118_093933/client-01/events.jsonl` contains shockwave
  spawn and textured draw submissions. It did not contain
  `rocket.impact-mark.resolved` or decal trace records in the queried lines, so
  it does not prove mark placement.
- Latest inspected `python devscripts/dev-loop.py --status` reported
  `run=NOT RUNNING`, `game-build status=idle`, and a stale published EXE. There
  is no current run journal to use as acceptance evidence.

## Next best work

### 1. Establish the exact runtime and reproduce the reported view

1. Inspect root dev-loop status once and record the exact EXE identity, game
   generation, mode, map, process IDs, and journal paths before drawing any
   conclusion. Reuse the root loop and its current checkout only.
2. Ask the user for the same map/spot or a human-recorded route if the
   screenshot cannot be reproduced from current run information. Do not invent
   an AI-authored route and present it as the user's failure.
3. Enable the existing `hitfx_trace 1` command in the client terminal before
   firing. `config/debuglogger.json` already sets the `rendering` and `weapons`
   categories to `important`; confirm those settings are active in this run.
4. Reproduce one actor hit and one direct world hit in ordinary play. Preserve
   both client and server journals when the actual path involves both.

### 2. Trace one mark from impact to first rendered quad

For each reproduced mark, correlate these records in order:

1. `rocket.impact-mark.resolved`: impact class, actor kind/ID, chosen static
   world triangle, nearest distance, anchor point, and `mark_applied`.
2. `impact_decal.spawn`: decal ID, kind, stored position/normal, radius, and
   configured texture.
3. `impact_decal.render_submitted`: same decal ID, actual submitted position
   and normal, whether the texture loaded, quad width/height, alpha, and camera
   distance.

Find the first divergence. If event 1 is absent, the exercised executable is
not proving the current resolver or the impact path bypasses it. If event 1
reports a world anchor but events 2 or 3 disagree, trace the decal queue and
renderer. If all records agree but the image still floats, compare the chosen
collision triangle and decal plane with the rendered map surface from multiple
camera angles; collision triangles can be valid while the rendered surface
uses another transform or offset.

Add only the missing correlation needed to connect resolver output to decal ID
and draw submission. Keep diagnostics in `StructuredLogger` and the canonical
journals; do not add a separate file logger.

### 3. Correct the owning step, not just the symptom

- Actor colliders must never be eligible decal anchors.
- A candidate must be a real nearby world surface for this impact. The current
  8 m nearest-point search may select a distant wall, ceiling, or other side of
  a thin surface; inspect the reported candidate and tighten/restrict the query
  based on the observed geometry instead of accepting any triangle in range.
- Place the quad on the selected triangle plane with only the minimum offset
  needed to avoid z-fighting. Check normal orientation, triangle winding,
  chunk/world transforms, and any decal projection/clipping convention against
  the rendered surface.
- If no suitable world surface exists, suppress the decal. Do not fall back to
  actor body geometry or leave a free-space quad.
- Trace every rocket impact entry path (local prediction, NPC/player collision,
  world collision, and network-confirmed projectile presentation) and ensure
  they converge on the same placement owner or use equivalent evidenced
  placement behavior.
- Review mark radius and texture after attachment is correct. The current
  4-unit radius and dark star texture may make a correctly attached mark look
  like a large floating shadow; tune those separately and record the resolved
  world width in the journal.

## Acceptance: zero floating marks

Do not call this fixed based on compilation, a resolver event, or one angle.
The same-build live run must show, for every sampled rocket impact:

- actor impacts identify the actor separately and exclude its body geometry;
- any placed mark uses a nearby static world surface and has a correlated
  decal ID through spawn and render submission;
- the rendered quad's plane matches the selected visible surface, with no
  visible gap or detached mark while viewed from several angles;
- impacts with no suitable world surface create no decal;
- direct world hits remain attached at their actual world contact;
- radius/texture are visibly appropriate and do not resemble giant detached
  stains; and
- human visual review confirms no floating decals in the reproduced passage
  and in ordinary play on additional nearby geometry.

Keep source, build/generation, client/server journal, reproduction inputs, and
human visual acceptance as separate evidence. A journal event that says
`mark_applied=true` is not visual proof.

## Relevant owners and records

- Rocket impact selection: `src/combat/weapon-rocket-launcher.cpp`
- Shared explosion spawning: `src/combat/explosion-fx.cpp`
- Decal storage and anchor offset: `src/effects/effect-part.cpp`
- Decal quad generation/submission: `src/effects/effect-part-render.cpp`
- Mark size/texture: `config/impact_decals.json`
- Earlier attempt and current shockwave work: `docs/changelog/2026-11-18/20261118_064523-rocket-impact-fx-force-and-placement.md`
- Behavior review: `docs/skills/spec-behavior-review-v1.md`
- Logging review: `docs/skills/logging-checker-v1.md`
- Real-executable journal workflow: `docs/workflows/runtime-scenario-validation.md`
