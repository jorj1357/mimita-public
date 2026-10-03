# Seeded procedural chunk streaming

- UTC: 2026-10-03T03:59:54Z
- Display timezone: America/New_York
- Display time: 2026-10-02 23:59:54 EDT
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Summary

Expanded Infinite Dungeon Slayer's repeated-room generator into a bounded,
seeded chunk layout. The active mode now uses 1,000 m chunks, 100 m block
slots, and 1–4 deterministic authored blocks per chunk. Server and client use
the same pure seed-plus-coordinate hash, while the server remains authoritative
for gameplay and room progression.

## Pre-existing work preserved

The worktree already contained edits to `config/accounts/default.json`,
`config/aimbody.json`, `config/analytics.json`, `config/camconfig.json`,
`config/collision.json`, `config/healthbar.json`, `config/movement.json`,
`config/weapons.json`, and the untracked
`config/movement/movement-source-fast.json`. Those files were not changed by
this session.

## Exact implementation

- `config/procedural-world.json:13-20`: added
  `streaming_enabled`, `chunk_size`, `block_spacing`,
  `min_blocks_per_chunk`, `max_blocks_per_chunk`,
  `stream_radius_chunks`, and `loaded_room_radius`.
- `src/procedural/procedural-world.h:108-181`: added the mode policy fields
  and the shared `proceduralChunkHash` /
  `proceduralChunkBlockPositions` contract.
- `src/procedural/procedural-world.cpp:151-159`: loads the new JSON policy.
  `:314-390` replaces implicit process RNG with a SplitMix64-style
  seed/coordinate hash and unique 100 m slot selection.
  `:424-475` adds server chunk materialization and bounded room/chunk rebuilds.
  `:650`, `:687`, and `:719` use the bounded rebuild at startup, config reload,
  and room transitions.
- `src/procedural/procedural-world-client.cpp:97-128` reconstructs the same
  chunk blocks locally. `:198-226` rebuilds only at replicated state-version
  boundaries and mirrors the server's bounded room/chunk window.

Old behavior was a straight-line append of every generated room and no use of
the seed for placement. New behavior is a bounded window containing the lobby,
nearby room slots, and the seed-selected chunk blocks; the old room lifecycle,
network state, NPC authority, and teleport contract remain in place.

## Documents and focused skills

Read and applied:

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/procedural-infinite-world/procedural-infinite-world.md`
- `docs/specs/performance/performance.md`
- `docs/architecture/live-development/live-authored-world.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`

Specification review result: the written spec requires deterministic seeded
client reconstruction, server authority, and performance under many players;
the new path satisfies those source-level contracts. Human wording about exact
block size and chunk spacing was ambiguous, so the implementation chose the
requested example values (100 m slots inside 1,000 m chunks) as JSON-owned
defaults.

Efficiency review result: generation is no longer performed per render frame
or per fixed gameplay tick. The only rebuild boundary is startup, valid config
reload, or a room transition. The loaded room/chunk counts are bounded by
`loaded_room_radius` and `stream_radius_chunks`. No supported finding remains
in the changed path; runtime profiling is still needed for actual GLB cost.

## Validation

- `python -m json.tool config/procedural-world.json`: passed.
- `git diff --check`: passed; only normal CRLF conversion warnings were shown.
- `python build_agent.py`: `Status: SUCCESS`, build log timestamp
  `2026-10-02 23:59:21`; edited procedural object files were rebuilt.

## Human review still needed

Run `pwsids` in the newly built executable and verify visually that two clients
with the same server state see identical blocks, that changing the seed changes
their layout, that room transitions remove old geometry instead of growing
memory forever, and that very fast movement does not outrun the streamed
window. Multiplayer ICE/network acceptance and frame-time profiling remain
unproven by this source/build validation.
