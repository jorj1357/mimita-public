# Cleanup branch plan

- UTC timestamp: 2026-09-25T01:38:23Z
- Display timezone: America/New_York
- Branch: `20260924cleanup`
- Result: PASS_WITH_HUMAN_REVIEW

## Scope

Created the requested cleanup branch from the existing working tree without
discarding, stashing, or modifying pre-existing changes.

## Evidence

- Initial branch: `8292026stash` tracking `origin/8292026stash`.
- Existing modified and untracked files were preserved.
- New branch verified by `git status --short --branch` as
  `## 20260924cleanup`.
- Read `docs/ROUTER.md`, `docs/doc-review-09-03-2026.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/code-ownership/code-ownership.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.

## Initial findings

The repository already states a one-owner and one-source-of-truth direction,
but the documentation review identifies incomplete or conflicting areas,
including collision, movement, networking authority, JSON hot-reload inventory,
player/NPC ownership, terminal commands, moderation layers, and generic
function direction. The working tree also contains substantial pre-existing NPC
lifecycle and hot-reload work that must be audited before cleanup decisions.

## Proposed next phase

1. Inventory active code, active configuration, specifications, deprecated files,
   and archive material.
2. Build an ownership matrix: concept, current owner, duplicate owners,
   callers, specification, runtime proof, and recommended action.
3. Classify each item as keep, consolidate, rewrite, archive, or delete;
   deletion requires evidence and human agreement where behavior may change.
4. Establish a spec-first workflow: define the desired feature contract and
   acceptance evidence before aligning implementation.
5. Execute cleanup in small reviewed slices, validating source/build/runtime
   claims separately.

## Human review still needed

The user should decide which repository categories are essential, historical,
experimental, or disposable before any deletion or broad consolidation. No
cleanup deletion or behavior change was performed in this session.

## Follow-up inventory

Added `docs/cleanup/20260924-repository-inventory.md` with the initial
confidence-rated repository map, hot-runtime architectural objective, risk
signals, and proposed audit order. The inventory found approximately 4,031
recursive files, including generated and dependency material; it does not yet
claim that all of those files belong in the AI's active working context.

Added `docs/cleanup/20260924-ai-working-beliefs.md`, a simple review of the
tradeoff between fast implementation and cleanup. It recommends small working
slices with an owner and acceptance check, rather than either polishing
everything first or adding unlimited code without ownership records.

Added `docs/cleanup/human-ai-working-guide.md`, distilled from the user's
provided programming lessons. It establishes that the human chooses intent
and acceptance while the AI discovers and explains ownership, and provides a
small feature-tracking card for future work.

Added `docs/cleanup/20260924-actor-network-chain-audit.md` with the first
source-level audit of actor, entity, player, NPC, server, networking, and hot
runtime ownership. It identifies the legacy concrete world and newer generic
world living together, recommends consolidation around EntityId/components
and one actor lifecycle owner, and explicitly marks old paths as unsafe to
delete until redirected and tested.
