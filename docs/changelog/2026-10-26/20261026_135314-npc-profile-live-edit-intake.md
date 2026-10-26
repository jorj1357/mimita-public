# NPC behavior-profile live-edit issue intake

- Task ID: 01a1787b-54ae-7393-baf1-885cff395511
- Summary: Record that edits to the balanced NPC profile appear to have no live effect, and that weapon restrictions should update across NPC roles and gamemodes.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-26T13:53:14Z (2026-10-26 09:53:14 EDT, America/New_York)
- Branch: `20261011zt`
- Base commit: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- Final commit: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd` (no commit created)

## Pre-existing changes

- The working tree was already dirty before this task, including `config/behavior-profiles.json`, both gameplay-issue documents, gamemode JSON files, NPC/gamemode source, and four untracked changelogs. These edits were preserved.
- In `config/behavior-profiles.json`, the existing uncommitted change sets non-shotgun/non-revolver visit counts to zero and changes the balanced reaction delay. This session did not modify that configuration.
- The gameplay issue documents already contained unrelated uncommitted intake and issue-table edits. Only the NPC combat row and new NPC weapon-profile intake wording were changed for this task; other edits remain pre-existing.

## Requested behavior

The player is editing `config/behavior-profiles.json`, especially the `balanced` area around line 70, to limit NPC weapons to revolver and shotgun. During play, NPCs do not appear to respond to those edits. The desired update should reach every NPC role in every gamemode.

## Specification alignment

- Current specification paths: `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/json-configuration/json-configuration.md`.
- The NPC behavior specification assigns weapon-cycle settings to behavior profiles and leaves resolved role/mode loadouts authoritative for available weapons.
- The JSON architecture says each file's reload path and application boundary must be verified before it is described as live. The new intake therefore records the observed report and desired scope without claiming a cause or current loader behavior.
- `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md`, named by the router, are absent in this checkout. The current regression index is `docs/regressions/README.md`.

## Exact documentation changes

### `docs/specs/open-gameplay-issues.md`

- New intake under “New intake — 2026-10-26” (line 35): records the revolver/shotgun profile change, the reported lack of NPC response when editing `balanced` around line 70 during play, the desired all-role/all-gamemode scope, and the need to trace config loading, reload, role resolution, and adoption by existing NPCs.
- NPC combat issue table (line 438): adds the live-edit report and its diagnostic questions; links the JSON configuration architecture and correct nested melee specification.

### `docs/specs/open-gameplay-issues-human.md`

- Mirrors the same intake at line 43 and the NPC combat issue at line 249.
- Corrected the touched row's melee specification link to `weapons/melee/melee-weapons.md`, the existing file path.

## Review and validation

- Read `docs/ROUTER.md`, `AGENTS.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/operations/task-completion/task-completion.md`, `docs/skills/documentation-checker-v1.md`, and `docs/skills/spec-behavior-review-v1.md`.
- Documentation checker: `git diff --check` passed for the two edited issue documents. The added specification links resolve to existing files.
- Existing TODO scan notes (not edited): `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:17` says “todo: write currentebhavior of how it works right now” — replace it with a concise current lifecycle and ownership description. Line 97 says “TODO: find a better long-term aiming solution” — define the desired arm/crosshair stability and an acceptance scenario. `docs/specs/networking/networking.md:1186` asks about a visual indicator for server disagreement — specify the indicator, trigger, and how it presents position uncertainty. `docs/specs/gamemodes/counterstrike.md:705` asks to add crouching — define input, movement, and release behavior before implementation. These are unrelated to this intake and remain untouched.
- No tests, build, or runtime scenario were run because this task only updates issue documentation. The hot-reload path and NPC live behavior remain unverified.

## Regression review

- Regression entry appended: no. This session records a user-reported issue without reproducing or confirming a regression.
- Related route: `docs/regressions/README.md`.

## Human acceptance

- Confirmed: the report is phrased as an observation and request, not a proven implementation defect.
- Still needed: trace the runtime loader/profile path and reproduce a balanced-profile edit in a live match; verify effects for NPCs across roles and gamemodes.

