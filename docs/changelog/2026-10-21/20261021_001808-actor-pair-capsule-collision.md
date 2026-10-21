# Actor-pair capsule collision first slice

- Status: PASS_WITH_HUMAN_REVIEW
- Time UTC: 2026-10-21T00:18:08Z
- Display time: 2026-10-20 20:18:08 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD at start: `a3901adb` (spec/ragdoll work)
- HEAD now: `0b5145b2` (concurrent actor-collision commit made while this
  session was active; this session did not create it)
- Work type: gameplay collision implementation, config, server authority,
  client reconciliation, diagnostics, runtime investigation

## Pre-existing and concurrent work

At session start the working tree already had edits in `config/aimbody.json`,
`config/analytics.json`, `docs/jorj-docs/20261020plan.md`,
`docs/specs/gamemodes/juggernaut.md`,
`docs/specs/npc-behavior/npc-behavior.md`, `src/entities/aimbody-config.*`,
and `src/ragdoll/ragdoll-mode.*`. Those were preserved.

During the session another writer committed `0b5145b2` and continued changing
graphics/video/duel/actor files and `config/actor-collision.json`. This
changelog describes the actor-collision state in the working tree, including
those concurrent edits where they touched the same files. No commit was made
by this session.

## Implemented behavior

- One actor-neutral capsule-pair owner:
  `src/physics/movement/actor-pair-collision.cpp`.
- Players, NPCs, and monsters are all actors. Dead actors remain in the same
  pair path as one ground-level lengthwise capsule and are static colliders.
- Swept spatial-grid broadphase uses each actor's previous and current swept
  AABB. Pairs are sorted/deduplicated and resolved once per fixed tick.
- Narrowphase sweeps relative tick motion and substeps/ bisects the first
  capsule contact, preventing fast actors from tunneling across grid cells.
- Flat-top support registers only for a non-static upper actor, carries the
  upper actor by the support's actual displacement, and hands support velocity
  back once when support ends.
- Hard downward impacts do not become support; the contact normal sends the
  lower actor downward through the shared external-impulse path.
- Push response uses impact speed and contact directness:

  ```text
  u = clamp((impact_speed - min_impact_speed) /
            (max_impact_speed - min_impact_speed), 0, 1)
  angle = min_angle_scale +
          (1 - min_angle_scale) * pow(directness, angle_curve_exponent)
  knockback_speed = max(weak_hit_distance / weak_hit_motion_seconds,
                        max_knockback_speed * pow(u, speed_curve_exponent))
                    * angle
  ```

  The result is added to `externalImpulse`; position correction is only
  depenetration, not a teleport-to-distance response.
- `config/actor-collision.json` owns per-actor-preset capsule radius/height,
  dead capsule length, push tuning, support cutoff, broadphase cell size, and
  candidate cap. Profile lookup uses the existing actor preset ID, then
  `default`.
- Config validation rejects a whole candidate, keeps the last valid data, and
  applies a valid reload at the next fixed tick. Load succeeds as revision 1
  and live edits reached revision 2/3 in the server journal.
- Server authority: `simulateSharedNpcs` runs the pass after player/NPC
  movement and damage, before NPC map rebuild. Changed actors receive a world
  collision recheck. Player snapshot history is recorded after the pass.
- Client reconciliation: `NET_STATE_ACTOR_COLLISION` is set on the authoritative
  snapshot when the pass changed the player. The local client applies the
  server transform/velocity and logs `actor.collision.reconcile`.
- Bounded diagnostics: one `actor.collision.profile` event per second with
  actor count, candidate/narrow pairs, contacts, supports, pushes, pair-kind
  counts, broadphase/collision ms, frame ms, and config revision.

## Files changed by this session

- `config/actor-collision.json` - valid default/scalar tuning, boss dead
  capsule length, pair/support tuning.
- `src/config/actor-collision-config.cpp` - default initialization before
  first load, path resolution through executable parents, defensive profile
  fallback, bounded load/error diagnostics.
- `src/physics/movement/actor-pair-collision.h/.cpp` - pair-kind counters and
  profile/contact classification; solver body was already present in the
  concurrent commit.
- `src/sim/simulate-tick.cpp` - pair-kind fields in the client profile event.
- `src/network/server-npcs.cpp` - pair-kind fields in the server profile event;
  authoritative pass and client/server signature were already in the
  concurrent commit.
- `docs/specs/npc-behavior/npc-behavior.md` - section 6 current implementation
  status updated to describe the implemented authoritative path, measured
  evidence, and open two-client/per-limb acceptance.
- This changelog.

The following pieces were present in the concurrent commit and were validated
rather than re-authored here: shared solver implementation, client fixed-tick
integration, server pass integration, `ServerPlayer` preset/support fields,
snapshot collision flag, and local reconciliation flag handling.

## Crash investigation and fix

The visible crash was an uncaught `std::out_of_range` at
`ActorCollisionConfig::profileFor()` while reading
`mData.profiles.at("default")`.

Root cause:

1. `config/actor-collision.json` was concurrently edited to an invalid
   candidate (`default.capsule_radius_m=5.0` with `capsule_height_m=3.6`, and
   the boss profile had `dead_capsule_length_m < 2 * radius`).
2. Validation correctly rejected the file, but the first-load constructor had
   left `mData` default-constructed (empty profile map) instead of holding
   built-in defaults.
3. `profileFor()` then threw inside the fixed tick and crashed the server and
   client.

Corrections:

- Initialize `mData = defaults()` before the first `load()`.
- Use `find()` and a built-in static default profile rather than `at()` in the
  fixed-tick lookup.
- Resolve the config through the executable's parent directories so dev-loop
  staging builds can still find repository `config/`.
- Restore a valid default capsule (`radius 0.7 m`, `height 3.6 m`, dead length
  `2.6 m`) and make the boss dead capsule length valid (`4.6 m`).
- After adding `ServerPlayer`/snapshot fields, a cold rebuild removed stale
  object-layout artifacts. A one-client 68-actor server then ran stably with
  the collision pass enabled.

A separate two-client-only crash remains in
`tickServerIceTransports` (`ACCESS_VIOLATION` reading `0xffffffffffffffff`
after ICE role-conflict/connectivity loss). It reproduced only with a manually
launched second client and is not in the actor-pair solver. It is reported as
an open ICE/transport issue, not claimed fixed here.

## Runtime evidence (separate from build evidence)

Builds:

- Incremental builds 2690-2691 and cold rebuild 2692 after struct changes.
- Final source build 2706: `Status: SUCCESS` from the dev loop.
- Executables under `.dev/builds/2690` through `.dev/builds/2706`.

Journals:

- `C:\mimita-v9\logs\2026-10-20\20261020_225854\events.jsonl`
  - 68 actors; server candidate pairs 86-115; collision 0.04-0.06 ms;
    NPC-vs-NPC contacts/pushes and support contacts observed.
  - Client collision 0.03 ms; client frame ms 16.5-30 in sampled windows.
  - Client `actor.collision.reconcile` events applied server collision results.
- `C:\mimita-v9\logs\2026-10-20\20261020_235159\events.jsonl`
  - 68 actors; server candidate pairs ~41-63; collision 0.03-0.05 ms.
  - Server log `[SERVER PERF] hz=59.7-60.5`, loopAvg 3.5-4.7 ms,
    loopMax 7-9.5 ms in sampled windows.
- `C:\mimita-v9\logs\2026-10-21\20261021_001302\events.jsonl`
  - Config revision 1 at startup; live edits produced revision 2 and 3.
  - 67 actors after the client left; candidate pairs 7-65; collision
    0.03-0.08 ms; NPC-vs-NPC contacts/pushes observed.

Not proven by runtime:

- Clean two-client player-vs-player blocking (second-client ICE crash).
- A dedicated dead-actor push/corpse scenario and support/carry visual
  acceptance by a human.
- 240 FPS rendering on the target machine; measured client frame times were
  about 16-30 ms in the sampled Juggernaut run, so the 240 FPS goal is not
  claimed.

## Documentation and skills

Read/reviewed: `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/npc-behavior/npc-behavior.md`,
`docs/architecture/collision/collision.md`,
`docs/architecture/collision/actor-vs-actor-collisions.md`,
`docs/specs/movement/movement.md`, `docs/specs/networking/networking.md`,
`docs/architecture/json-configuration/json-configuration.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/efficiency-checker-v1.md`,
`docs/skills/logging-checker-v1.md`.

Open documentation TODO (not edited): `docs/architecture/collision/actor-vs-actor-collisions.md:1`
still says “todo explain how actors vs actor collisions work”. Suggested
expansion: label the implemented root-capsule first slice (all pairings,
server authority, swept grid, support/carry, push tuning, dead capsule)
separately from later full-body/limb triangle collision.

## Human review still required

- Two-client player-vs-player contact in a clean session without the ICE
  transport crash.
- Live feel of push strength/direction, support carry, hard top impacts, and
  dead-corpse collision.
- Visual acceptance that capsules match the intended avatar sizes and that no
  actor passes through another in normal play.
- Real 70-actor (or the user-clarified 68-actor) Juggernaut frame-time
  acceptance against the existing 240 FPS target.
