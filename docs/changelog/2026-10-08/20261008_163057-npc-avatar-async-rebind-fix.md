# NPC avatar asynchronous rebind fix

Date: 2026-10-08 16:30:57 -04:00
Branch: `afad20a-rebuild`
Base commit: `d2e72c1a`
Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Implemented the fix identified by the previous live Juggernaut journal. In
`src/network/multiplayer-tick.cpp::processSnapshotEntities()`, an existing NPC
replica now retries its replicated avatar when all of these conditions hold:

- the snapshot contains a non-empty avatar identity;
- the replica still has the same requested identity;
- no avatar instance is currently bound; and
- `AvatarSystem::isAvatarLoadPending()` reports that the cache is ready.

The retry uses the existing `AvatarSystem::applyAvatarToPlayer()` owner. It
does not create a second loading system or alter server authority. Each retry
emits `client.npc.avatar.rebind` with the actor, avatar, application result,
instance binding, model state, and fallback state.

## Evidence used

The preceding run at
`logs/2026-10-08/20261008_202550/events.jsonl` showed 67 NPCs with valid
replicated names, 121 initial `avatar_async_pending` fallbacks, four later
`avatar.load.ready` events, and no reapplication to the original replicas.

## Build and launch evidence

- Developer-loop build `1755` published successfully from source generation 35.
- Developer-loop Juggernaut launch used matching build `1756` for both server and
  client. Build identity:
  `0b0bbc5911ce0fc852d0af3363836d5ad84de669c7b11dc7f1b12a68b649d36b`.
- Mode: `9` (`juggernaut`); map: `dust2cyberiav4`.
- Runtime journal:
  `logs/2026-10-08/20261008_202851/events.jsonl`.
- `git diff --check` passed; only pre-existing CRLF conversion warnings were
  reported.

## New runtime result

- Initial NPC binds: `134`.
- Initial async-pending fallbacks: `121`.
- Immediate successful binds: `13`.
- Avatar loads ready: `4`.
- Avatar loads failed: `0`.
- Successful existing-replica rebinds: `9`.
- Rebind failures: `0`.
- Snapshot chunk rejection/timeouts: `0`.

The new records show the intended sequence, for example:

`avatar.load.ready` for `abusivegirlheadless4` ->
`client.npc.avatar.rebind` for actor `100046` with
`apply_returned=true`, `avatar_instance_bound=true`, and
`fallback_model=false`.

The same sequence was observed for `headlessredux2`,
`abusivegirlheadless`, and `angel`. This proves the previously permanent
fallback can now transition to the intended avatar when the async asset is
ready.

## Remaining review

The run prepared only four distinct avatar assets during the observed window;
the remaining assets stayed pending and did not fail. Full-roster visual
acceptance is still required to confirm every Juggernaut NPC eventually shows
the intended model, face, and cosmetics. The developer-loop Juggernaut run
was left active for that review.

Focused documents and skills used this session remain those recorded in the
preceding diagnostic changelog, plus the routed runtime-scenario and logging
requirements.
