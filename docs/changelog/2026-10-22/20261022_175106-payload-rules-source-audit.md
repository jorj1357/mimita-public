# Payload rules and source reuse audit

- Canonical timestamp: 2026-10-22T17:51:06Z
- Display time: 2026-10-22 13:51:06 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD: `85336642`
- Result: PASS_WITH_HUMAN_REVIEW

## Scope and pre-existing changes

Updated `docs/specs/gamemodes/payload.md` with the user's clarified cart speed, contest, overtime, session-statistics rules, and an evidence-based audit of current implementation owners. This session changed no source code. The Payload spec was already modified before this work; its existing design edits were retained. Other pre-existing working-tree edits were left untouched, including `docs/specs/open-gameplay-issues.md`, `docs/architecture/accessibility/accessibility.md`, renderer/network files, configuration files, and earlier changelogs. Exactly one changelog was added for this session.

## Files and exact changes

### `docs/specs/gamemodes/payload.md`

- Sections 4 and 5, lines 94-172: resolved overtime and cart movement rules. Replaced the prior vague overtime wording with a 10-second/600-tick timer; each tick with a living Team A actor intersecting the authoritative box refreshes the full timer, including while contested. Removed the older "recently pushed or touched" phrasing so it cannot be read as a different overtime test. A defender in the box sets movement to zero. Attackers add 0.2 m/s each, capped at six attackers/1.2 m/s; per-tick distance is speed divided by 60. Added sample distances and noted that HUD time is displayed in seconds.
- Section 14, line 440: retained the already-updated rule that server shutdown ends the match and discards process-only statistics, and that v1 does not send Payload session statistics through the generic persistent match-result path.
- Section 21, lines 587-605: added the new source-inspection reuse audit and implementation work list. Before this session, there was no source-audit section. The new audit identifies shared server gamemode tick/state, bomb-only objective state, MapConfigRegistry's point-only axis-aligned query, PhysicalEntitySystem and its replication as a candidate, TeamBrain's bomb/site context, the generic persistence enqueue path, and the need to trace packet size and receive/apply paths before adding replicated fields.

Exact prior wording in the working tree before this session's final precision edit:

> `- If the timer reaches zero, overtime may keep the round running.`
> `- Every qualifying offensive interaction can refresh the overtime window.`
> `- Exactly which interactions refresh overtime requires a precise definition.`
> `advance cart along its configured route by speedMetersPerSecond / 60 meters`

Exact final additions include:

> `If at least one living Team A actor overlaps the box on that tick, refresh the overtime countdown to its configured full duration. This happens on every qualifying tick, even if a defender is contesting the box and preventing movement.`
> `At 60 ticks per second, one attacker moves the cart about **0.0033 m per tick**; five attackers move it about **0.0167 m per tick**; six or more move it **0.02 m per tick**. The server stores overtime as an integer tick count; the HUD presents the remaining time in seconds.`

## Source findings

- `src/network/server-gamemode.cpp:140` returns a function-static `ServerGamemodeState`; `serverGamemodeTick` is the existing authoritative mode tick. Shared lifecycle/state is reusable.
- `src/network/server-gamemode.cpp:4523` onward has generic timeout/result handling; the generic timeout currently does not implement Payload overtime.
- `src/game/objective-state.cpp:9` parses Bomb only; its self-test at line 66 expects `payload` to map to `None` today.
- `src/gamemode/map-config.cpp:56` implements `mapEntityContainsPoint`; this is a point query and cannot alone satisfy actor-body intersection with a rotated cart box.
- `src/physics/physical-entity.h:112` exposes `PhysicalEntitySystem`, including `moveKinematic` and `advanceKinematics`; physical-entity networking exists in `src/network/physical-entity-replication.cpp` and `src/network/multiplayer-physical-entities.cpp`. This is a strong candidate for cart body, carrying, and replication, but the Payload route/rules integration is not present or runtime-proven.
- `src/network/server-gamemode.cpp:4368` calls `PersistenceQueue::enqueueMatchResult`; `src/persistence/persistence-queue.cpp:137` turns that into `requestSave("MATCH_END")`. Payload's temporary session counters must avoid this generic completion path.
- `src/network/packets.h:74` sets `MAX_GAME_DATAGRAM_BYTES` to 1200; this audit does not claim packet expansion is safe. Payload fields and serialization/receive/apply paths need a separate size-aware trace.

These are source inspection findings only. No build, tests, live server, or gameplay acceptance was run because only documentation changed.

## Documents and focused review

Read and followed:

- `docs/ROUTER.md` and its routed Payload, networking, entity editor, NPC, collision, configuration, and task-completion references.
- `docs/specs/gamemodes/payload.md`.
- `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW: made user-specified requirements explicit; code disagreements and unverified runtime integration are labeled.
- `docs/skills/documentation-checker-v1.md` — PASS_WITH_HUMAN_REVIEW: checked links/wording and scanned `docs/` for TODO wording. Existing note at `docs/specs/gamemodes/payload.md:1` reads `2026 10 06 1607 jorj todo - make it like`. Per `docs/ROUTER.md`, leave it untouched and suggest labeling it historical context or replacing it with a concise design-origin note.
- `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/operations/task-completion/task-completion.md`.

The routed review expected `docs/doc-review-09-03-2026.md`, which is absent from this checkout; recorded as an unresolved documentation warning.

## Validation and remaining review

- `git diff --check -- docs/specs/gamemodes/payload.md`: PASS.
- Source paths and referenced functions were checked with focused repository search: PASS for source existence; this does not prove runtime behavior.
- No tests/build/runtime exercise: NOT_APPLICABLE for this documentation-only change.
- Human review/playtesting remains needed for box dimensions/rotation, the route and rollback behavior, overtime presentation, NPC priorities/path following, and HUD seconds formatting. Configuration key names and the active loader must be selected before implementation. Host migration remains future work.
