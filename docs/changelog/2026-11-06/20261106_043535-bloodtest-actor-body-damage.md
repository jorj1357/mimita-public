# Bloodtest actor body damage and replication

Time created: 2026-11-06T04:35:35Z (2026-11-05 23:35:35 EST)

## Scope

Implemented the shared actor body-part damage slice for Bloodtest: six-part
authoritative health, part identity through hitscan/contact/projectiles,
snapshot replication, confirmed-damage part state, procedural skin/meat/bone
rendering, part removal, and force-scaled blood presentation.

World Editor work in the same checkout was left to the other agent.

## Source and configuration changes

### New shared owner

- `src/entities/actor-body.h`
- `src/entities/actor-body.cpp`
- `config/actor-body.json`

The shared state covers head, torso, left arm, right arm, left leg, and right
leg. Each part starts at 100 health. Skin is above two thirds, meat at or below
two thirds, bone at or below one third, and the part is destroyed at zero.
Head or torso destruction is lethal; arm or leg destruction removes the part
without adding a separate persistent limb entity.

### Authoritative damage

- `src/network/server.h` — actor body state, six-part request fields, part result
  fields, and six-part NPC body samples.
- `src/network/server-damage.cpp` — applies part damage with ordinary actor
  health, forces death for destroyed head/torso, classifies unknown contact
  points, and records part state in damage events.
- `src/network/server-contact.cpp` and `.h` — carries/classifies six-part
  identity and excludes destroyed parts from hit volume.
- `src/network/server-attack.cpp` — carries the exact hitscan part through
  retrace and claims, excludes destroyed parts from hit boxes.
- `src/network/server-body-template.cpp` — maps template names to shared parts.
- `src/network/server-npcs.cpp` — maps NPC physical part names to shared parts
  and preserves part state across the per-tick mirror rebuild.
- `src/network/server-physical-contact.cpp` — routes player physical-contact
  part state into the confirmed-damage path.
- `src/combat/weapon-execution.h` and `.cpp` — propagates six-part identity
  through hitscan pellets and aggregates.

### Replication

- `src/network/packets.h` — adds six part-health bytes to snapshots, part fields
  to confirmed-damage events, and bumps the protocol from 51 to 52.
- `src/network/snapshot-chunks.cpp` — copies part health through compact
  snapshots.
- `src/network/server-players.cpp` — fills player part health.
- `src/network/server-npcs.cpp` — fills NPC part health.
- `src/network/multiplayer-context.h`, `multiplayer-tick.cpp`, and
  `multiplayer-reconcile.cpp` — apply local and remote part health.

### Presentation

- `src/entities/player.h` — stores replicated body state and per-part inner
  layer meshes.
- `src/entities/player-loader.cpp` — builds meat and bone inner shells from the
  existing part meshes.
- `src/entities/player-render.cpp` — draws only the active layer and hides
  destroyed parts.
- `src/effects/hit-effects.h` and `.cpp` — carries a blood amount scale for
  destroyed-part hits.
- `src/network/confirmed-damage-presentation.cpp` — maps confirmed part identity
  and destroyed state into hit presentation.
- `src/effects/effect-part.h` and `effect-part-blood.cpp` — per-decal radius
  falloff: larger near the impact/cone center, smaller toward the edges.

### Weapons

- `config/weapons/bloodtest_revolver/config.json` — explicit world-impact mass
  `0.008`, diameter `0.008`, speed `260`, penetration `0.06`.
- `config/weapons/bloodtest_shotgun/config.json` — explicit world-impact mass
  `0.002`, diameter `0.006`, speed `320`, penetration `0.03`.

`uses_physics_projectile` remains non-authoritative metadata. Actual routing is
still `behavior_type: projectile` to the shared projectile-rifle family.

### Documentation

- `docs/specs/gamemodes/bloodtest.md` — updated to describe implemented part
  health, lethal head/torso destruction, removable limbs, procedural layers,
  replication, and force-scaled blood.
- `docs/regressions/dev-loop/build-superseded-stale-REG-2026-11-06.md` — recorded
  the build-supersession diagnosis. The index is
  `docs/regressions/dev-loop/README.md` Entry 22.

## Validation evidence

### Source/config checks

```text
python devscripts/gamemode_json_sync.py --check
  PASS: masterlist=1 definitions=36 errors=0

python devscripts/weapon_json_sync.py --check
  PASS: masterlist=1 definitions=36 errors=0
```

### Build evidence

A changed `server-damage.cpp` compile error was found and corrected during this
session. The canonical build then published:

```text
Build artifact: 2026-11-06T04:33:32.276873Z
Build ID:       2026-11-06T04:33:32.276873Z:4e1ea8eb9a8ef8493214c01931b85d98fc8b2761e70f5a88295454fbde260fd7
Source generation: 9
Result: published_current
```

The direct build log ended with `Nothing changed` after the object was rebuilt,
and `mimita.exe` was updated at 23:33 local.

A silent compile failure before that was caused by low disk space: the system
drive had only about 113 MB free. Removing generated `build/ccache` and three
superseded `.dev/builds` artifacts freed generated build space; the canonical
build then published successfully. See
`docs/regressions/dev-loop/build-superseded-stale-REG-2026-11-06.md`.

### Runtime evidence

Partial only. An earlier mode-51 launch through the dev-loop started Bloodtest
with two clients and:

- resolved `gamemode.match.configured` with `mode=bloodtest`, `weapon_set_id=33`
- resolved both player loadouts to:
  `["swordsword","bloodtest_shotgun","nothing","bloodtest_revolver"]`

The shared dev-loop run was then switched to a sandbox World Editor run by the
other agent, so the following remain unverified in the live journal:

- part damage reaching meat/bone/destroyed thresholds
- head/torso lethal destruction
- limb removal and hitbox removal
- replicated part state on two clients
- force-scaled blood and decal size falloff
- projectile count, muzzle origin, and effective direct damage

No runtime acceptance is claimed.

### Human review

Pending. Visual layer appearance, part disappearance, blood splatter shape,
weapon scale, movement, and block-edge traversal still need human observation.

## Known limits and follow-up

- Bloodtest avatar/model presentation and movement were verified in run
  `20261106_071418`; part-damage threshold/detachment runtime verification
  remains a separate pending item.
- `uses_physics_projectile` remains a parsed-but-unconsumed field; do not use it
  as evidence of physical routing.
- No persistent physical limb entity is spawned for a destroyed part; the part
  is removed and the client presents force-scaled blood/gore.
- Part-health config is loaded from `config/actor-body.json`; hot-reload of that
  file is not yet verified.

## Follow-up — avatar fallback / green capsule fix

The same session also fixed the green-capsule client state reported during
Bloodtest testing.

Root causes:

- `src/render/render-player.cpp` set `mLazyLoadRequested=true` but only queued a
  model load when `avatarInstance` was null. A bound avatar instance without a
  queued model stayed in the fallback collision capsule forever.
- The new procedural meat/bone layer generation ran `gTextures.getPath` from the
  async model-load thread, which could prevent model finalization.
- Failed avatar loads were not retried after parse/atlas failure. Remote avatar
  files were written directly rather than atomically.

Fixes:

- Always queue the avatar model path (or default body) when a player has an
  avatar but no model.
- Build meat/bone layer meshes on the main thread after sync load, cache hit, or
  async model finalization.
- Add bounded avatar-load retries with backoff, exact failure reasons, atomic
  remote avatar writes, and default-body fallback when a model is missing or
  produces no body parts.

Runtime evidence:

```text
Build: 2026-11-06T07:13:46.557145Z
Run:   20261106_071418
client.actor.render_state: model_loaded=true body_mesh_count=6
avatar.pose.live: parts=[head,leftArm,leftLeg,rightArm,rightLeg,torso]
agent.move: (13.23,-120.38,87.81) -> (27.94,-146.25,87.90)
```

Regression record:
`docs/regressions/gameplay/avatar-model-fallback-REG-2026-11-06.md`
(README Entry 33).

The earlier part-damage runtime acceptance items remain separate and are not
claimed as verified by this avatar fix.
