# Devscripts inventory and cold-change guard

## Task

- Task ID: 20261116-devscripts-inventory-cold-change-guard
- Session ID: 20261116-devscripts-inventory-cold-change-guard
- Owner: opencode
- Focus: development tooling documentation and hot/cold preflight
- Status: complete for this slice
- Date, time, timezone: 2026-11-16T15:23:34Z
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Final commit: uncommitted

## Requested behavior

Document every current development script, including the planning/census tools
that remain optional report-only, and add a preflight that tells an editor
whether a changed file is hot, cold, module-owned, tooling, or unknown.

## Exact implementation changes

### `docs/operations/devscripts/script-inventory.md`

- Added a complete current inventory grouped by responsibility:
  - canonical dev loop,
  - build/ownership/codegen,
  - authoritative hot/cold gates,
  - optional report-only census/audit,
  - scenarios/performance,
  - configuration/content,
  - packaging/integrations/deferred workflows.
- Recorded purpose, inputs, outputs, cadence, authority, and suggested
  keep/combine/archive disposition.
- Explicitly marks audit/census tools as optional report-only, not Cutover
  acceptance.
- Records `live.py` as deprecated and `game_link_worklist.py` as superseded by
  actual link-input sidecars.
- Suggests future consolidation without moving any script yet.

### `docs/operations/devscripts/cold-change-guard.md`

- Defined hot/cold classes and examples.
- Documented the rule that a game-owned edit uses the game-DLL path and a
  resident/ABI edit requires an EXE decision.
- Documented how to run the guard and its exit status.

### `devscripts/cold_change_guard.py`

- Reads `build/source-manifest.json`.
- Defaults to staged, unstaged, and untracked Git paths.
- Accepts explicit paths.
- Classifies:
  - `HOT`,
  - `COLD`,
  - `COLD_ABI`,
  - `MODULE_DLL`,
  - `HEADER_REVIEW`,
  - `TOOLING`,
  - `CONFIG`,
  - `NON_PRODUCT`/`IGNORE`,
  - `UNKNOWN`.
- Default exit is nonzero for `COLD`, `COLD_ABI`, `HEADER_REVIEW`, or
  `UNKNOWN`; `--report-only` always exits zero.
- Optional JSON output via `--json`.

### `devscripts/README.md` and `docs/ROUTER.md`

- Linked the inventory and guard.
- Added the router entry for devscripts inventory/cold-change preflight.

## Validation

- `python -m py_compile devscripts/cold_change_guard.py`: passed.
- Hot check:
  `python devscripts/cold_change_guard.py src/combat/melee-client.cpp`:
  `HOT=1`, exit 0.
- Cold check:
  `python devscripts/cold_change_guard.py src/host/host-runtime.cpp`:
  `COLD=1`, exit 1.
- Report-only current-tree check ran without changing state; it correctly
  flagged resident cold changes and an unknown source (`src/npc/npc-utility-bridge.cpp`)
  against the current stale manifest.
- No build was requested for this documentation/tooling slice.

## Important limitation

The guard reads the generated source manifest. If a source was added or the
manifest is stale, the guard reports `UNKNOWN`; regenerate the manifest before
treating a clean result as authoritative.

## Next tasks

1. Use the guard before the next melee/networking/FPS edit.
2. Regenerate the source manifest and classify `src/npc/npc-utility-bridge.cpp`.
3. Remove audit/planning fields from authoritative status and keep those outputs
   report-only.
4. Expand Cutover-1 acceptance in `docs/specs/hotreload/hotreload.md` with
   real-EXE `events.jsonl` contracts.
5. Add the networking local-only/offline FAQ.
