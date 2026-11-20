# Payload cart transform diagnostics

Date: 2026-11-20
Issue: `issue-cf9cd77eb056`
Task: `payload-cart-transform-telemetry`

## Change

`src/network/server-gamemode.cpp` emits a bounded `payload.cart-trace` event from the authoritative Payload tick after cart collision resolution. It is rate-limited to at most one event per monotonic wall-clock second, with up to 64 NPCs and a truncation flag. Each event records map/mode/tick identity, cart entity, position, size, movement delta/velocity, route distance, push/rollback/wait state, and the 4x4 cart transform as column vectors. Same-tick NPC fields include origin position, world and cart-local offsets, velocity, size scale and capsule dimensions, grounded/stable-ground/world-contact flags, ground normal, support entity, role, and health. Existing `payload.cart-spawned` and transition-only `payload.cart-state` events now also include transform/position/velocity context.

A whole-game DLL build note was queued before the compiler input was saved (`0a828294bcc0`). `git diff --check` passed. The build attempt active at the time, `game-dll-20261120T152655407656Z-5e6e30`, froze revision `6de4984` / source generation 9; its source snapshot was checked and does not contain `payload.cart-trace`. The source was edited after that snapshot (source generation 15 and later). At 15:56:25Z the root dev-loop was still processing generation 405; 22 translation units had completed and no build completion/publication event existed. The diagnostics have not yet been compiled or observed in a runtime journal.

## Runtime evidence still required

After a candidate containing this exact source publishes, run one pinned Payload/FunWorld3 scenario through the root dev-loop, verify `payload.cart-trace` in that run's server `events.jsonl`, reconstruct an NPC world position from its cart-local position and transform, and request `screenshot_at_tick` with a matching sidecar. Compare build/generation/run/tick identities. The existing TDM run `20261120_152743` was left untouched; no extra runtime was started while the root build and another agent's scenario were active.

The symptom of a visibly floating NPC above the cart is not proven or fixed by this diagnostic. The event is intended to establish the actor/cart/support relationship needed to reproduce and diagnose it. Task proof remains unverified pending pinned runtime evidence.
