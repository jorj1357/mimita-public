# Collision Legacy Inventory

Time: `2026-09-29T15:55:26Z`

## Request

Inventory all collision/movement `TODO-DELETE` and related legacy or parallel
owners without deleting code, so future consolidation can be planned against
real callers and runtime boundaries.

## Change

Added:

`docs/architecture/collision/collision-legacy-inventory-2026-09-29.md`

The inventory groups the 67 matching source comments into logical owners and
records active, fallback-only, NPC-only, remote/ragdoll/test, diagnostic, and
apparently uncalled status. It also records additional legacy state-sync,
movement-wrapper, render-mesh fallback, and physical-entity migration bridges
that do not all carry `TODO-DELETE` labels.

No source code was deleted or behavior changed.

## Evidence

- Repository-wide `TODO-DELETE` search was performed under `src`.
- Caller search was performed for the main collision owners and helpers.
- `config/collision.json` currently enables `actorTriangleSolver`.
- The inventory identifies the local-player early-return boundary and the
  remaining legacy consumers/blockers.
