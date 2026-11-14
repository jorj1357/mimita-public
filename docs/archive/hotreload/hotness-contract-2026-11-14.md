# Hotness contract (2026-11-14)

Purpose: make it impossible to have **fake hot**. Every runtime path must prove
which code ran, in the exact process `events.jsonl`.

This file is normative. The event names/fields below are the canonical contract
for hot-reload proof. The `events.jsonl` file format itself is defined in
`docs/specs/debug-logging/canonical-jsonl.md`.

## Rules (strict for all)

1. **Every runtime path journals where it ran.**
   Packets, input, render, jobs, callbacks, activation, rollback:
   `path=dll|resident`, `generation`, `reason`.
2. **Silent fallback is a defect.**
   A migrated packet must not fall back to resident. If it cannot be handled in
   the DLL, it is rejected and logged at `Errors`. Resident is allowed only for
   genuinely cold mechanisms (sockets, device, OS, loader, barrier) and must be
   named in the hotness ledger.
3. **Build identity must match.**
   EXE build ID, DLL build ID, revision, and code hash must agree. A mismatch
   refuses activation and logs `game.hotness.stale_dll`.
4. **No secrets.**
   Events contain paths, ids, hashes, counts, reasons. Never tokens, passwords,
   join tickets, progression tickets, or packet payload contents.
5. **Hot is proven, not assumed.**
   A feature is hot only when normal play's journal shows `path=dll` for it,
   with shadow parity, canary, and rollback evidence.
6. **A feature's whole causal chain must be classified.**
   A bug fix must name every owner in its input-to-output chain and whether each
   is hot or cold. If a cause is cold, the fix is not a hot-fix claim.

## Strict mode

`hotness.mode=strict` is the default in development.

- Any violation is `Errors`.
- Activation is refused when the candidate or identity is invalid.
- The dev run exits nonzero on a violation.
- The process aborts only for `game.lifetime.use-after-unload`.
- `warn` mode exists only for an explicitly listed migration window.

## Canonical events

Build (the live git diff):

```text
game.build.candidate
  generation, revision, dll, code_hash, exe_pid, exe_build_id
  compiled, skipped, compile_s, link_s
  changed[], added[], removed[], renamed[]
  changed_headers[], header_dependents[], changed_owners[]
```

Activation:

```text
game.generation.activated
  old_generation, new_generation, old_dll, new_dll
  old_code_hash, new_code_hash
  boundary, quiesce_ok, activation_ms
  state_schema_old, state_schema_new, migration
  exe_pid, unchanged_pid
game.generation.rolled_back reason
game.generation.activate_failed reason old_generation_kept=true
game.hotness.stale_dll exe_build_id dll_build_id reason
```

Runtime paths:

```text
game.transport.path type player path=dll|resident generation [fallback_reason]
game.transport.dispatch type peer path=dll|resident   (verbose; off by default)
game.render.path path=dll|legacy subpath=render|menu|map-editor generation frames
game.input.path path=dll|resident|direct generation
game.server.path path=dll|resident generation
game.hotness.summary tick dll resident resident_types render_path input_path server_path
game.render.thread-violation op action=refused thread=worker
presentation.framebuffer_probe center_r center_g center_b non_black_pixels viewport_w viewport_h png
performance.frame-budget total_ms budget_ms=4 over_ms top_scopes
performance.npc-budget id total_ms think_ms path_ms combat_ms collision_ms
```

The frame budget is 4 ms or less every frame (240 FPS minimum) regardless of
NPC count or scene. `performance.frame-budget` fires on any frame over 4 ms;
`performance.npc-budget` fires when an NPC exceeds 4 ms and is rate-limited to
one record per second.

Lifetime:

```text
game.lifetime.job.registered generation owner
game.lifetime.job.drained generation
game.lifetime.callback.registered generation owner
game.lifetime.quiesce.timeout generation remaining
game.lifetime.unload.blocked generation reason
game.lifetime.use-after-unload generation owner
```

Violations:

```text
game.hotness.violation
  kind=resident-fallback|stale-dll|build-mask|identity-mismatch|use-after-unload
  type, generation, exe_build_id, dll_build_id, reason
```

## Anti-debt system

1. **Generated hotness ledger.** Derived from the dispatch switch, source
   owners, link worklist, and runtime journals. No hand-written truth.
2. **Decision shadow.** The DLL computes a decision without applying it; the
   resident path applies; the host compares and logs only divergence.
3. **Canary edit.** One sentinel change per batch; the journal must show the new
   sentinel with the same EXE PID.
4. **Single owner per batch.** Once parity passes, delete or disable the resident
   case. A long-lived dual path is debt.
5. **Domain state + codegen.** No hand-edited wide snapshot; no field clobber.
6. **Build-mask gate.** A game-source change must produce a successful DLL
   candidate. EXE-only is `Errors`.
7. **Lifetime tags.** Every job/callback/resource carries a generation; the
   barrier drains/joins; unload only on `Ok`.
8. **Static gate is necessary, not sufficient.** 0/0 only proves no banned raw
   patterns.
9. **Causal-chain rule.** See rule 6.

## Expansion later

- Add `frame`, `tick`, `thread`, `capability_versions`, `state_schema`,
  `migration_id`, `feature_scope`, and `owner`.
- Add a per-feature hotness score: how much of its causal chain is hot.
- Add shadow-divergence events and a generated ledger snapshot.
- Add a `hotness` terminal command and an in-game indicator
  (`HOT dll gen 134` / `COLD resident`).
- Add per-domain path events for combat, movement, vehicle, avatar, UI.
- Add build-input events for headers and owner changes.

## Fallback policy

- Reject, do not silently fall back.
- A migrated packet has one of: `Handled`, `Rejected`, `Cold`.
- `Rejected` is a protocol/state error and is logged.
- `Cold` is only for a genuinely cold mechanism, named in the ledger.
- Every fallback reason is a fixed string; "unknown" is not allowed.

## Runtime gate (2026-11-14)

`devscripts/server_hotness_probe.py` is the first executable hotness gate. It
launches the published EXE plus the current generation, drives a real PING, and
fails unless the journal proves DLL packet handling and DLL server calls. Its
`--swap` mode requires a distinct-module generation swap and a post-swap PONG.
Use `python devscripts/live.py --once --verify-server` to pair the build and the
runtime proof.

## Hot client gate (2026-11-14)

`server_hotness_probe.py --client --chat <text>` is the required client/server
gate: it launches a real client, requires a hot join (`players>=1`), requires
the chat request to appear as `game.transport.path` with `path=dll`, and
requires zero resident packet handling and zero server failures. Add `--swap`
to require a distinct-module generation swap while the client stays connected.

## Behavior-changing swap gate (2026-11-14)

`GameServerV1.runtimeRevision` is the behavior revision of a generation and is
reported as `runtime_revision` in `game.hotness.summary`. The behavior-swap gate
starts a server on an older generation, joins a client, chats, swaps to a new
generation with a different `runtimeRevision`, and requires the new revision plus
a still-connected client afterwards. A gameplay-visible delta should be added to
this gate once one is chosen.
