# Blender Computer Use gold reference

- Result: `PASS_WITH_HUMAN_REVIEW` — added a reusable explanation of visible computer control and the separate MCP scene tools; no Blender project content was edited.
- Time: `2026-10-29T16:22:45Z` (2026-10-29 12:22:45 EDT, America/New_York).
- Branch and starting repository commit: `20261011zt`, `8cc796f87dc46636ccefccce024572586d4497d8`.
- No source commits were created.

## Changes

- Added `docs/gold/20261029_162106-blender-computer-use-mcp-gold.md` (new 70-line file). It explains Computer Use versus MCP, lists tool names found in the checked-out source, records the Blender UI activation and connection evidence, explains the remaining Codex reload step, documents safe-mode and telemetry settings, and proposes a read-first workflow for 100 separately organized and labeled floors.
- The `get_scene_info` entry was corrected to avoid claiming it directly reports Blender collection membership. The source's `SCENE_FIELDS` and object summary code report object hierarchy and transforms; collection membership can instead be queried with `execute_blender_code`.
- Documented the actual observed UI limitation: a viewport coordinate changed the active object and left an unsaved marker. No mesh geometry was changed or saved. The reference recommends checking the screen after each action and restoring state before saving.
- At task start the root worktree already showed `m devscripts/blender-ai` and the untracked `docs/changelog/2026-10-29/` directory from prior Blender setup work. Inside the existing upstream checkout, `integrations/codex/plugins/mcp-for-blender/.mcp.json` was already modified to enable safe mode and disable telemetry. These prior changes were preserved. No other root worktree changes were present at the start.
- During final validation, nine additional deletions appeared under `docs/features/` (`README.md`, `TEMPLATE.md`, `camsway-realisticish/camsway.md`, `gameplay-primitives/gameplay-primitives.md`, `live-code-development/live-code-development.md`, `live-code-development/live-collaboration-feature.md`, `movement/movement.md`, `spawnvelocity/spawnveloc.md`, and `vehicles/vehicles.md`). Their origin is unknown; they were not part of this documentation task and were left untouched.

## Reasoning and validation

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, and the existing gold UI/network examples.
- The routed `docs/doc-review-09-03-2026.md` file does not exist in this checkout; no replacement was assumed.
- Inspected upstream README and source owners in `devscripts/blender-ai/src/blender_mcp/server.py` and `devscripts/blender-ai/src/blender_mcp/blender_scripts.py`; confirmed `get_addon_status`, `get_scene_info`, `execute_blender_code`, `look`, asset search/import, 3D generation, and viewport app tool names and descriptions.
- Ran the required docs TODO scan. No TODO-marked documents were changed. Active spec notes are recorded for follow-up in the final response; archived TODOs and historical changelog quotations were left untouched.
- Documentation review: the new reference has an audience, purpose, and explicit limits; its repository-relative source paths exist. No tests or builds were relevant.
- Human review still needed: the newly configured MCP tools must appear in a refreshed Codex session before any scene inventory or floor edit can be claimed.

## TODO review notes

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:17` — “todo: write currentebhavior of how it works right now”. Suggested expansion: document current ownership and the live ragdoll flow first, then separately state desired death impulse and aiming behavior.
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:34` — “TODO: WHY AIMING IN WITH THE RAGDOLL ARM IS JITTERING THE CORSSHAIR WONT SIT STILLLLLL ARGHHHH”. Suggested expansion: capture reproduction steps, expected crosshair stability, the responsible aim/arm behavior, and a measurable acceptance scenario.
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:97` — “TODO: find a better long-term aiming solution”. Suggested expansion: define how the physical arm and weapon should settle, under which movement/aim conditions, and how runtime review proves it.
- `docs/specs/gamemodes/juggernaut.md:107` — “jorj todo: the issue is”. Suggested expansion: split the following friendly/enemy outline and heavy-shotgun audio reports into separate expected/actual behavior notes with their owning presentation/audio areas.
- `docs/specs/gamemodes/juggernaut-duel.md:226` — “Networking TODO: give role-based score-limit modes one consistent weapon-set”. Suggested expansion: promote the following paragraph's owner choice and authoritative Duel acceptance steps into the specification, then remove the redundant TODO label.

