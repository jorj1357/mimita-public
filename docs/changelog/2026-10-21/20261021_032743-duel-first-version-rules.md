# Duel first-version rules

- Result: `PASS_WITH_HUMAN_REVIEW` for specification update; implementation and runtime acceptance remain outstanding.
- Local time: 2026-10-20 23:27:43 EDT. Canonical time: 2026-10-21T03:27:43Z.
- Branch: `20261011zt`.
- HEAD before this documentation-only update: `094e6e5a`.
- Pre-existing edits: the workspace had numerous modified configuration, source, and specification files and multiple untracked changelogs. `docs/specs/gamemodes/duel.md` was itself untracked before this turn. Those unrelated changes were preserved.

## Changed file

### `docs/specs/gamemodes/duel.md`

Replaced the raw opening TODO and informal notes with a clear first-version
contract, phase table, spawn behavior, actor-preset work, owner map, unresolved
decisions, and runtime acceptance criteria. The broad Rocket Launcher Duel
idea remains future work and is separated from standard Duel.

- Before, line 1: `2026 10 20 2314 jorj todo explain`
- After, line 1: `# Duel`
- Before, design note: `first to 20 kills` / `or maybe 3 min has passed`
- After, lines 34-43: immediate respawn; first to 20 wins, otherwise a 180-second timeout is a stalemate; results last five seconds, then the 15-second intermission repeats.
- Before, spawn note: `both spawn at the same spawn point`
- After, lines 63-73: both use the same selected per-map Duel spawnpoint; exact overlap versus safe offset remains undecided because the existing JSON has `spawn_offset_radius: 5`.
- Before, variant notes mixed standard Duel and Rocket Launcher movement tuning.
- After, lines 12-16 and 75-88: Rocket Launcher movement tuning is explicitly future variant work; standard Duel needs authored actor preset assignment and an agreed avatar, movement preset, and loadout.

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/duel.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/actor-presets/actor-presets.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- The router points to `docs/doc-review-09-03-2026.md`, but that file is absent
  from this checkout; its additional review notes could not be applied.
- The documentation-checker scan found the related existing TODO at
  `docs/specs/gamemodes/gamemodes.md:324-325`: “Todo explain this 9 2 2026
  0848” and “And also the hotbar behavior”. Suggested expansion: explain how a
  weapon set limits available weapons and how its ordered entries map to the
  in-game hotbar. This unrelated note was not changed.
- Existing config `config/gamemodes/duel.json` has goal 20, respawn 0,
  `time_limit_seconds: 0`, weapon set 1, and spawn offset radius 5; it does not
  yet express the agreed 180-second timeout or 15/3/5-second phase contract.
- `config/movement/movement-source-rocket.json` currently identifies itself as
  `cs16`; it was not changed and is not treated as the standard Duel movement
  preset.

## Validation and remaining work

- Manually checked the revised phase durations, score and timeout rules,
  unresolved same-tick boundary, spawn ownership, and future-variant boundary
  against the shared gamemode and actor-preset specifications.
- No tests, build, or runtime scenario were run; this turn changed a
  specification only.
- Human decisions remain open for NPC slot filling, exact spawn spacing,
  standard actor-preset identities and loadout, maps without Duel spawnpoints,
  stalemate wording/score display, score-versus-timeout precedence on the same
  tick, and series-win tracking.
