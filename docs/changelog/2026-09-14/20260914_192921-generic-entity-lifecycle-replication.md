# Generic entity create/destroy replication

- EST timestamp: 2026-09-14 19:29:21 EDT (UTC 2026-09-14T23:29:21Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--dynamic-replication-selftest` 23/23 +
  full suite 14/14)

## What this proves
A completely new entity created by the server after startup can appear,
receive arbitrary dynamic components/relationships, and disappear on clients
through one generic lifecycle record in the existing replication envelope. No
per-feature entity packet, struct, or codec; no EXE knowledge of the entity's
conceptual kind.

## 1. Identity audit
`EntityId` packs `generation(16)|realm(4)|domain(8)|legacyId(32)`; the registry
lookup key is `(realm, domain, legacyId)` and does not include generation. Rare
stale aliasing was possible if a recreated key differed only by generation. Added
`EntityRegistry::adopt(EntityId)` (used by the client apply): it registers the
exact packed id and, if a different generation already owns the key, retires the
old identity first so generations cannot alias.

## 2. Generic lifecycle record
`EntityLifecycleRecord { op CREATE/DESTROY, changeVersion, entity (packed id),
generation }` appended as an optional section of the existing
`PACKET_DYNAMIC_COMPONENT` envelope. One path for every entity kind.

## 3. Server create/replicate path
`serverReplicateDynamicComponents` now sends, per client and in order:
CREATE for newly relevant entities (entities with a replicated component or
relationship, plus explicit `serverReplicateEntity` marks), then schema
descriptors/components, then relationships; DESTROY is derived by absence from
`EntityRegistry` for entities already sent. Lifecycle records ride the first
batch so CREATE always precedes state.

## 4. Client create path
`dynamicReplicationApply` processes lifecycle first: CREATE adopts the entity
shell; DESTROY destroys it, erases its components/relationships, and retires the
id. No switch on entity/component/relationship kind.

## 5. Destroy + stale safety
A retired entity id rejects subsequent component/relationship records, so old
data cannot resurrect a destroyed entity. Duplicate CREATE/DESTROY are
idempotent; a recreated key with a new generation adopts the new identity.

## 6. Ordering
CREATE -> schema/COMPONENT ADD/UPDATE -> RELATIONSHIP ADD/UPDATE -> DESTROY,
handled within the envelope; when a packet carries lifecycle records, component
and relationship records for entities not yet known are dropped (applied on the
next sync), so UPDATE-before-CREATE is safe.

## 7–8. Proof
`--dynamic-replication-selftest` (real DLL) PASS 23/23, including: client learns
entities via generic CREATE; component/relationship add/update/remove;
mismatched payload rejected; v1->v2 schema migration and last-good on failure;
duplicate CREATE, destroy + state clear, stale UPDATE after DESTROY rejected,
and id reuse with a new generation. The real gameplay component
`HotProjectileState` uses the same lifecycle + component path (its entity gets a
generic CREATE and its state arrives generically). Full suite PASS 14/14.

## 9. Presentation
Not addressed; the entity exists in client runtime state only.

## 10. Typed lifecycle bridge
Not removed this pass. Player/NPC/`ServerProjectile` still use their typed
lifecycles; the runtime hot-projectile entity is the first real consumer of the
generic lifecycle. Removing one typed bridge is the next step.

## Falsifications covered
duplicate CREATE, stale CREATE/id reuse (generation), UPDATE before CREATE,
DESTROY twice, UPDATE after DESTROY, client join after entity exists (server
sends CREATE on first relevant tick).

## Honest limits
- The live server + two-client run was not performed (no multiplayer harness);
  the selftest exercises the real encode/decode/apply path and the server
  integration runs each fixed step.
- Entity-only lifecycle (no component/relationship) requires an explicit
  `serverReplicateEntity` mark; automatic marking of every created generic
  entity is not wired.
- No typed lifecycle (player/NPC/projectile) migrated yet.
- Tree co-edited by the gameplay agent; relationship replication in the envelope
  was concurrent and is integrated and green.

## Files changed
`src/ecs/entity-registry.{h,cpp}` (`adopt`), `src/network/dynamic-replication.{h,cpp}`
(lifecycle record, encode/decode/apply/collect, server lifecycle diffing,
`serverReplicateEntity`), `src/network/dynamic-replication-selftest.cpp`
(lifecycle + falsification checks), `src/network/multiplayer-tick.cpp` (client
lifecycle apply), docs + this changelog.

## Next
Remove one typed lifecycle bridge; migrate a Player/NPC state slice into generic
components; then hot snapshot/relevance and prediction/interpolation policy.
