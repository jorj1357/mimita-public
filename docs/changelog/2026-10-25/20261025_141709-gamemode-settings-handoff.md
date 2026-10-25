# Gamemode settings masterlist contributor handoff

- Result: PASS_WITH_HUMAN_REVIEW (documentation-only handoff; implementation decisions and runtime behavior remain open).
- Timestamp: 2026-10-25 14:17:09 EDT / 2026-10-25 18:17:09 UTC.
- Branch: `20261011zt`.
- HEAD before this documentation change: `cfae20a54ad4f9c623e043ca553a181278c48719`.
- Pre-existing worktree edits preserved: `config/collision-lod.json`, `src/config/collision-lod-config.cpp`, `src/config/collision-lod-config.h`, `src/map/map-loader-collision.cpp`, `src/map/map-loader-collision.h`, `src/physics/physics-types.h`, `src/world/world.cpp`, `src/world/world.h`, and untracked `src/map/map-static-union.cpp` / `src/map/map-static-union.h`. No code or configuration was changed for this handoff.

## Files and exact documentation change

- `docs/specs/open-gameplay-issues.md`, heading `Gamemode settings masterlist and validation handoff — 2026-10-25` (inserted at line 180): before the insertion, the presentation discussion was followed directly by `## New intake — 2026-10-21`. The new section now sits between them. It records the desired central masterlist path, shared presentation defaults and per-mode overrides, named avatar groups, preservation of existing per-mode numeric choices, the open synchronization/generation rule, strict loader/validator requirements, the malformed-file/default-mode issue, in-game path-specific error tips, rate-limited `events.jsonl` diagnostics, and the still-unverified TDM trail report. It is a contributor handoff, not a claim that these changes exist.
- `docs/changelog/2026-10-25/20261025_141709-gamemode-settings-handoff.md`: this record.

## Reasoning and evidence

The handoff follows the user's request for one central definition and a complete, validated set of gamemode JSON options, while retaining each mode's existing values. It keeps a key design choice explicit: a central master plus generated mode files needs to store current per-mode values before generation; hand-edited mode files can instead be validated without silent rewrites. The handoff also includes the requested contributor-wide in-game and journal error reporting and TDM trail investigation.

Repository evidence noted in the handoff:

- `src/gamemode/gamemode.cpp` owns directory loading, parsing, and reload behavior. Existing inspection found inconsistent rejection of unknown keys and wrong types. Initial load must report explicit parse/load success instead of treating a default-initialized `duel` ID as proof of success; reload should retain last-known-good settings.
- `src/config/player-visuals-config.cpp` owns shared player visuals and already emits structured apply/reject events, so the contributor should reuse existing configuration and notification owners.
- The current report that TDM does not show a trail has not yet been traced to a verified first failing runtime step.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/regressions/README.md`, and `docs/skills/documentation-checker-v1.md`.
- The router-requested `docs/doc-review-09-03-2026.md` was not present in the checkout. This missing review document is recorded as a workflow warning; it did not block this focused handoff.
- Documentation checker: PASS for the new section's purpose, audience, open status, and repository-relative paths. The handoff clearly distinguishes requested behavior from verified implementation.
- Existing TODOs were left untouched per the router instruction. `docs/specs/open-gameplay-issues.md:122` says `2026 10 17 1211 jorj todo add: the  blood and world imapct effects dont show if the npc hittin gme`; suggested replacement: convert it into a dated, readable report that identifies the expected NPC-to-player hit effects and links the effect owner. `docs/specs/open-gameplay-issues.md:139` says `ALSO TODO EVERY MAP NEEDS TO AHVE SPAWNPOINTS IN THE JSON AND I CAN SET WHATS ACTIVE/INACTIVE`; suggested replacement: state the desired per-map spawn-point schema and activation behavior, then link the map/spawn owner.

## Validation and remaining review

- `git diff --check`: PASS for whitespace in the changed handoff. Git printed line-ending warnings for pre-existing edited source files; those files were not modified by this task.
- No tests or build were run because this change only adds documentation and the user asked not to change code.
- Human/contributor review remains needed for the generation-versus-validation ownership decision, the exact avatar group membership and setting schema, and runtime proof of the TDM trail and error notice/journal path.
