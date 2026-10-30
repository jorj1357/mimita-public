# Rocket Duel and Juggernaut Horde intake

- Result: `PASS_WITH_HUMAN_REVIEW` (documentation-only intake; implementation was not requested).
- Timestamp: `2026-10-30T03:20:06Z` (UTC; client display date is 2026-10-29 America/New_York).
- Branch: `20261011zt`.
- Commits: no commits created; HEAD was not changed.
- Pre-existing edits: preserved all tracked/untracked changes present before this task, including modified config/spec files, `devscripts/blender-ai`, and these changelogs: `20261030_013700-gamemode-resolver-npc-combat-approach.md`, `20261030_021741-arm-ragdoll-reuse.md`, `20261030_030042-vehicle-creator-mode-specs.md`, and `20261030_031642-melee-contact-modes.md`.

## Final state

Added a new latest-first intake section to `docs/specs/open-gameplay-issues.md` titled “New intake — 2026-10-30: Rocket Launcher Duel and Juggernaut Horde.” It records:

- A Rocket Launcher-only Duel for one local player and one NPC, using the existing weapon set ID `3` subject to role loadout resolution.
- A Juggernaut Horde proposal: four Juggernauts, Fighter instant respawns, no Juggernaut respawns, full role-maximum healing on Juggernaut kills, Fighters win by eliminating all Juggernauts, and Juggernauts win after 18,000 authoritative 60 Hz ticks.
- The open Fighter and Juggernaut roster details, current shared owners, and the likely missing split-respawn/timeout-winner behavior.
- Explicit status that this is intake only and does not authorize runtime code changes.

The existing `kill_heals` boolean restores a player killer to their role-resolved max HP in `creditKill` within `src/network/server-gamemode.cpp`; the inspected path does not heal NPC killers. Generic `last_team_standing` exists in the same file, and `config/weaponsets.json` defines ID `3` as Rocket Launcher only. No gameplay implementation, spec, or config was changed.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/duel.md`, `docs/specs/gamemodes/juggernaut.md`, `docs/specs/weapons/weapons.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Router reference `docs/doc-review-09-03-2026.md` is missing from the checkout; noted as a documentation warning. `docs/regressions/regressions-v1.md` is also missing; this task is not a regression report.
- Source/config inspection confirmed the existing Rocket Launcher weapon-set ID and the current boolean kill-heal / generic elimination behavior. The Horde timeout result and team-specific respawn policy remain implementation questions.
- Documentation diff was reviewed. No build, tests, live runtime, or gameplay acceptance were run because this was documentation-only intake and no implementation was authorized.
- Human review is still needed to settle the exact Fighter roster and whether the local player must always be on the Fighter team. Full implementation and live match validation remain future work.
