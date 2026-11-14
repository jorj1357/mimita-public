# Hotness contract, state domains, and live-evolution spec docs

Session date: 2026-11-14 10:46 local. Scope: append-only documentation pass on
the shared checkout. No runtime behavior changed.

## What changed

New docs:

- `docs/specs/hotreload/status.md` — current hot/cold matrix, current
  generation/probe/static-gate snapshot, networking migrated/resident lists,
  risks, and next phases.
- `docs/specs/hotreload/hotness-contract.md` — no-fake-hot rules, strict mode,
  canonical build/activation/path/lifetime/violation events, fallback policy,
  anti-debt system, and expansion plan.
- `docs/specs/hotreload/state-domains.md` — Option A (behavior in DLL, durable
  records resident), per-domain records, versioning/migration, and generated
  codegen rule.
- `docs/specs/hotreload/input-layer.md` — cold device/OS polling vs hot binds,
  live key rebinding now, normalized input later, and the performance-exception
  rule.

Appended sections (existing lines untouched):

- `docs/specs/hotreload/hotreload.md` — companion links, no-fake-hot summary,
  anti-debt system, Option A state ownership, input direction, the “add/remove/
  extend anything live” dream, and automation.
- `docs/specs/networking/networking.md` — packet schema split rationale/fanout,
  per-domain headers, stable IDs/tombstones, live add/remove of packet types,
  fields, handlers, and actor types, plus the C++-version answer.
- `docs/specs/debug-logging/canonical-jsonl.md` — hotness/generation event
  category, required identity fields, build/activation/path/lifetime events,
  strict-mode wording, no-secrets rule, and reading order.

Also appended a coordination note to
`docs/handoffs/hot-reload-agent-claims-20261113.md`. No `docs/ROUTER.md` edit.

## Evidence classification

- Source/docs only. No build, link, load, or runtime result is claimed here.
- Prior session evidence remains: EXE green, whole-game generation 131 loaded
  via probe, static gate 0/0 at the time of this pass. `status.md` records the
  current generation as 133 from `build/game/current.json`; that was not
  re-validated in this pass.

## Not proved

- The hotness event instrumentation does not exist yet; this pass only defines
  the contract.
- Strict mode, reject-not-fallback, shadow parity, canary, and the ledger are
  not implemented.
- No server cutover, no same-PID live edit/rollback, no render cutover.

## Next

Implement the contract in this order:

1. `HOT_RELOAD` category and path/build/activation/lifetime events; strict mode;
   reject-not-fallback for migrated packets.
2. Include the changed/added/removed/header diff in `generation.json` and
   `game.build.candidate`; require the DLL worker for game edits.
3. Server cutover behind `MIMITA_GAME_SERVER=1`, then default; move the server
   tick into the DLL; unify inbound; connection handles.
4. State-domain codegen; retire the wide snapshot.
5. Input/movement with live key-rebinding proof.
6. Combat, ragdoll, vehicle, avatar assets, spawn lifecycle.
7. Same-PID edit/activate/rollback proof; then packet split and render cutover.
