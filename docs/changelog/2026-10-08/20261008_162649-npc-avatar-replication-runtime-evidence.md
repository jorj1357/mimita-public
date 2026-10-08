# NPC avatar replication runtime evidence

Date: 2026-10-08 16:26:49 -04:00
Branch: `afad20a-rebuild`
Commit: `d2e72c1a`
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Added bounded StructuredLogger diagnostics for the reported Juggernaut NPC
avatar replication failure. This session intentionally did not change avatar
application behavior; it established the live first divergence before a fix.

Pre-existing working-tree edits were preserved. The instrumentation changed
only:

- `src/avatar/avatar-atlas.cpp`
- `src/network/multiplayer-tick.cpp`
- `src/network/server-packets.cpp`

## Exact diagnostic changes

- `src/network/server-packets.cpp::buildAndSendSnapshot()` now emits
  `server.npc-avatar.snapshot-summary` every 120 server ticks with entity/NPC
  counts, empty-name count, field-capacity count, chunk count, and wire size.
  It also emits `server.npc-avatar.snapshot-send` per client at the same
  bounded cadence.
- `src/network/multiplayer-tick.cpp::mpTick()` now emits chunk rejection,
  incomplete-chunk progress, reassembly timeout, and complete reassembly
  summaries. The reassembly summary includes total entities, NPC entities, and
  empty NPC avatar names.
- `src/network/multiplayer-tick.cpp::processSnapshotEntities()` now records
  the NPC bind result, requested identity, async-pending state, avatar-instance
  binding, model state, fallback boolean, and fallback reason.
- `src/avatar/avatar-atlas.cpp::AvatarSystem::getOrLoadAvatar()` now records
  repeated requests whose cache state is not ready, including the load state.

The previous behavior was retained: an NPC bind that returns false still loads
the default model. No retry or rebind was added in this evidence pass.

## Documents and focused skills read

- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/live-development/live-development.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/regressions/regressions-v1.md`

## Build evidence

- `python build_agent.py` completed with `BUILD SUCCESS`.
- The developer loop published build `1754` at
  `C:\mimita-v9\.dev\builds\1754\mimita.exe`.
- The developer loop reported `builds_match=true` for the dedicated server and
  client.
- `git diff --check` passed; only existing CRLF conversion warnings appeared.

## Runtime evidence

Developer-loop launch:

- Launch mode: `9` (`juggernaut`)
- Map: `dust2cyberiav4`
- Server executable: `.dev/builds/1754/mimita.exe`
- Client executable: `.dev/builds/1754/mimita.exe`
- Shared journal:
  `logs/2026-10-08/20261008_202550/events.jsonl`

Observed records:

- Server snapshots contained `67` NPCs and `68` total entities.
- Server reported `npc_empty_avatar_count=0`.
- Server produced `10` chunks with maximum wire size `1180` bytes.
- Client reassembled complete `10`-chunk snapshots with all `67` NPC avatar
  names present and `npc_empty_avatar_count=0`.
- No `client.snapshot.chunk-rejected` or `client.snapshot.chunk-timeout`
  records occurred.
- NPC bind results: `121` asynchronous-pending fallbacks and `13` immediate
  successful bindings.
- Four `avatar.load.ready` events occurred; zero `avatar.load.failed` events
  occurred.
- The pending bind records explicitly contain
  `apply_returned=false`, `avatar_instance_bound=false`,
  `fallback_model=true`, and `fallback_reason=avatar_async_pending`.
- Later bind records were new actor IDs (`100067` onward), not reapplications
  to the original pending replicas. No duplicate bind actor IDs were observed.

## Finding

The server selection and replication path are not the first divergence in this
run. The first divergence is client-side asynchronous avatar application:
many NPCs receive valid avatar identities, but `getOrLoadAvatar()` returns no
ready instance while the background load is pending. The client then commits
the default model. When `avatar.load.ready` later fires, the existing NPC
replicas are not rebound. This matches the reported behavior where only a few
avatars appear correctly and most remain default white.

## Remaining work / human review

The smallest behavior repair is to reapply a ready avatar to existing NPC
replicas after async readiness, preserving the requested network identity and
avoiding permanent fallback. That repair requires a separate implementation
pass and a new build/runtime run. Human visual acceptance is still required:
confirm that the live Juggernaut client visibly shows the intended faces,
models, and cosmetics for the full NPC group after the retry path is added.
