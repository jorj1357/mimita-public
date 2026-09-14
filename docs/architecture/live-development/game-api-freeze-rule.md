# GameAPI freeze rule

Status: ACTIVE as of 2026-09-14 (capability-provider hot pass).

## Rule

**No new `GameplayContextV1` / `GameAPI` fields for ordinary features.**

Ordinary gameplay functionality must first attempt composition using the existing
generic mechanisms:

- `System` (registered by domain + priority + id)
- `Dynamic Component` (schema-hashed byte payloads)
- `Event` (id + schemaHash, dispatched generically)
- `Relationship`
- `Resource`
- `Capability` (id + signature, resolved through `resolveCapability`)
- `Constraint`
- `Generation` (immutable candidate, atomic swap, rollback)

A future ABI (context/struct) bump must state which genuinely lower-level missing
primitive could not be represented through those mechanisms.

## Why

`GameplayContextV1::resolveCapability(host, id)` is the last generic doorway.
Kernel primitives and hot package providers are now the same kind of registry
entry: `{id, signatureId, schemaHash, callable, providerPackage,
providerGeneration}`. The kernel contains no `if (id == ...)` routing for
capabilities; it knows only ids and signatures.

## How to add a new capability (no ABI change)

1. Hot provider source: declare `GameCapabilityDescriptorV1`
   `{id = gameHash("my.cap"), signatureId = gameHash("sig.my.cap.v1"), 0,
   callable, "my.cap"}` and register it with
   `MimitaHotPackage::CapabilityRegistrar`.
2. Hot consumer source: register a requirement with
   `MimitaHotPackage::CapabilityRequirementRegistrar{gameHash("my.cap"),
   gameHash("sig.my.cap.v1")}` and resolve it per tick through
   `ctx->resolveCapability(ctx->host, gameHash("my.cap"))`, casting to the
   agreed function type.
3. Do not add a `GameplayContextV1` field. Do not add a capability enum.

Activation validates signature compatibility generically; a missing provider or a
signature mismatch rejects the candidate and the last-good generation stays
active.

## Consumer lifetime

Consumers must re-resolve `resolveCapability` at activation (in practice, each
tick) and must not cache the returned raw pointer across a generation swap. The
provider generation is observable via `GenericRuntime::capabilityProviderGeneration`
for kernel-side consumers.

## Kernel primitives have no privilege

`physics.move`, `effect.spawn`, `skeleton.apply`, and `animation.update` are
registered kernel entries with a signature (`providerPackage == 0`). They resolve
through the exact same table and mechanism as a hot `banana.launch` provider.
