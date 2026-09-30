// 2026-09-30T15:37:00Z
/* purpose
* record the Infinite Dungeon Slayer spawn, room-count, door-config, and
* live-reload alignment completed in this session
* does NOT claim human gameplay acceptance
*/

# Task

- Summary: Make `pwsids` start the first dungeon room at a JSON-owned spawnpoint,
  keep room N at N NPCs, and make each room door's position, rotation, and size
  editable in room JSON with live reload.
- Status: CODE_COMPLETE / BUILD_VERIFIED / RUNTIME_VALIDATION_REQUIRED
- Date, time, timezone: 2026-09-30T15:37:00Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-30 11:37 EDT).
- Branch: `afad20a-rebuild`

# Finding

The `pwsids` alias already forwarded the authoritative start command, and the
existing runtime already generated room N with N procedural NPCs after the
current encounter reached zero alive NPCs. The mismatch was that initial spawn
preferred a GLB node while the door was a mode-level, axis-aligned barrier
loaded only at startup. The feature specification is broad and informal, so
the user's explicit room behavior is the implementation contract here.

# Changes

- `config/procedural-world/rooms/procedural-mimitasizing5.json`: added
  `spawnpoint.position`, `spawnpoint.rotation_degrees`, and
  `door.position`, `door.rotation_degrees`, `door.half_extents`.
- `src/procedural/procedural-world.h/.cpp`: added JSON-owned room spawn and door
  data, room-local door transform math, rotation-aware server barrier triangles,
  JSON spawn precedence over GLB discovery, and timestamp-based reload of the
  main mode JSON plus referenced room JSON files.
- `src/procedural/procedural-world.cpp`: valid JSON edits rebuild the current
  authoritative room barrier immediately and advance its state version.
- `src/procedural/procedural-world-client.cpp`: mirrors the door transform and
  rebuilds the client collider when JSON changes; replicated server state still
  owns door lock/unlock.

# JSON contract

```json
"spawnpoint": {
  "position": [-45.0, 0.0, 1.0],
  "rotation_degrees": [0.0, 0.0, 0.0]
},
"door": {
  "position": [51.0, 0.0, 30.5],
  "rotation_degrees": [0.0, 0.0, 0.0],
  "half_extents": [1.5, 13.0, 30.0]
}
```

Vectors are room-local. Rotation values are degrees. The old mode-level
`door_half_extents` remains a fallback when a room has no `door` object.

# Validation

- `python build_agent.py` returned `BUILD SUCCESS` twice; the final build
  compiled 10 translation units, skipped 487, and linked `mimita.exe`.
- JSON parsing loaded both procedural JSON files and printed the new values.
- `git diff --check` found only a pre-existing whitespace warning in
  `docs/specs/20260930plan.md`.
- The documented `devscripts/live-build.py` entry is absent from this checkout,
  so live activation was not available through that path.

# Human/runtime review still required

Run `pwsids` in the new executable and confirm room 1 spawn, one NPC, instant
unlock after its death, room 2 with two NPCs, and continued room progression.
While active, edit the room JSON door position, rotation, or half-extents and
confirm the barrier changes without a restart. This session does not claim
those visual or multiplayer observations.

# Routed documents and focused skills

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/20260930plan.md`
- `docs/specs/gamemodes/infinite-dungeon-slayer.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
