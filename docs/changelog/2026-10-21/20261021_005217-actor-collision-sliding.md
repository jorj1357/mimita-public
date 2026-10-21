# Actor collision slide and corpse diagnosis

- Result: `INCOMPLETE_BUILD; HUMAN_REVIEW_PENDING`.
- Time: 2026-10-21T00:52:17Z; display time 2026-10-20 20:52:17 EDT (America/New_York).
- Branch: `20261011zt`.
- HEAD at start: `f6d6eeb2`.
- No commit or deployment was made. The already running 2706 game/server were left running.

## User goal

Explain why actors can feel sticky, clarify the actor-collision contract, change contacts to slide along a surface while preserving tangential movement, keep corpses from acting as horizontal walls, and add bounded diagnostics that identify contact pairs and NPC stuck state.

## Changes

### `docs/specs/npc-behavior/npc-behavior.md`

- Lines 223-239 add the plain-language no-pinning rule: discard only movement into a contact surface, preserve same-tick sideways movement and tangential velocity, use the same response on client/server, and keep work at fixed 60 Hz.
- Lines 256-261 specify that corpses can provide top support but cannot block horizontal movement or NPC routes.
- Lines 263-267 define bounded contact event contents and the extra movement context for NPC stuck events.
- Before, the actor section said: `Actors must not pass through one another.` The dead actor rule said: `Other actors can collide with it.`
- After, living actors remain solid to one another; a corpse can support an actor from above but does not block horizontal movement.

### `src/physics/movement/actor-pair-collision.cpp` and `.h`

- `resolvePair` (around lines 401-625) previously placed both actors at the collision-time positions, losing their remaining movement for that tick. It now projects only the into-surface portion away, keeps the tangent remainder, corrects final overlap once, and retains tangential velocity.
- Valid top support retains horizontal and upward remainder for the supported actor. Dead actor contacts that are not top support restore requested endpoints, allowing pass-through.
- Added `actor.collision.contact`, capped to one event per pair per second and ten events per second per thread. Fields include actor IDs/kinds, dead/static flags, contact time and normal, requested/resolved positions, movement remainder, velocities, and response.
- The public resolver now accepts a source and tick so client and server records can be distinguished.
- Before, the call ended with `ActorPairCollisionStats* outStats = nullptr);`. After, it accepts `const char* source = "unknown"` and `std::uint32_t tick = 0`.

### `src/sim/simulate-tick.cpp` and `src/network/server-npcs.cpp`

- The two resolver calls now identify their source as `client` or `server` and pass the current simulation tick.
- `server-npcs.cpp` already contained unrelated user changes. Only the resolver call arguments described here are part of this task; all other existing edits were preserved.

### `src/npc/npc.cpp`

- Existing `npc.stuck`, `npc.stuck-recovery`, and `npc.stuck-episode` records now include velocity and support actor key; stuck/recovery records also include movement intent.
- Before, stuck records did not include these movement/support fields. After, each event includes the values at the existing detection/recovery points.

## Evidence and validation

- Inspected the current actor-pair solver, client/server resolver call sites, NPC stuck-event owner, actor collision spec, and current dev-loop status.
- The old code path confirms that an actor sweep stopped movement at first contact and discarded the rest of that tick. The code also marks dead actors as static colliders. This supports the suspected sticky-contact/corpse-blocker causes, but does not prove which contact caught the player's character in a live session.
- The existing live journal is `logs/2026-10-21/20261021_001302/events.jsonl` from build 2706. It contains repeated NPC stuck and recovery records and aggregate collision profiles. Some server samples show NPC/NPC contacts; sampled profiles show no player/NPC contacts. Existing events do not identify specific contact pairs or corpse state, so they cannot establish the user's exact culprit.
- `git diff --check` passed for the files changed in this task before the final line-wrap adjustment; no code tests were added or run.
- The canonical dev-loop build request did not produce a new executable. It encountered `static assertion failed: SnapshotPacket exceeds client receive buffer` (`sizeof(SnapshotPacket)=16952`, receive buffer 16384) in the already modified `src/network/packets.h`. The dev-loop still reports build `building` despite that compiler diagnostic; no success is claimed for this requested build.
- Running client/server remain on build 2706; the previously published build 2708 is not running this patch. No fresh runtime journal exists for these changes, so source/build/live behavior are separate and live behavior remains unverified.
- No human gameplay acceptance, deployment, or commit is claimed.

## Review state

The implementation needs a successful canonical dev-loop build and a live run of the real Juggernaut scenario before its movement behavior can be accepted. Inspect the new `actor.collision.contact` pair records alongside NPC stuck/recovery records to establish whether a player, living NPC, or corpse is responsible.
