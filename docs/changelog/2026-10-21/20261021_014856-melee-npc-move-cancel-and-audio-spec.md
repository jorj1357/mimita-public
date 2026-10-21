# Melee NPC move cancellation and character audio specification

- Status: `PASS_WITH_HUMAN_REVIEW`. The Melee spec now requires NPC attacks to interrupt unfinished moves on the configured 1–30 tick start-to-start schedule and requires each NPC's equipped character move set to provide the matching animation and sounds. This session changed documentation only; code conformance and runtime behavior remain unverified.
- Time: 2026-10-21T01:48:56Z (2026-10-20 21:48:56 EDT, America/New_York).
- Branch: `20261011zt`; HEAD: `094e6e5a`.
- Pre-existing edits in the worktree were preserved.

## Exact edits

- `docs/specs/gamemodes/supersmashbrosmelee.md`, section 9 (line 127). Before, any new legal move could interrupt, with the example limited to move B being pressed. After, it explicitly says this also applies to an NPC's next selected attack and the NPC does not wait for the prior animation to finish.
- `docs/specs/gamemodes/supersmashbrosmelee.md`, section 34 (line 237). Before, `melee_attack_gap_ticks` was a post-move wait and action duration extended the start-to-start interval. After, it sets the start-to-start interval (1–30 ticks); when due, the NPC starts a new move and interrupts its current windup, active, or recovery action, as long as it has a live reachable target in attack range.
- `docs/specs/gamemodes/supersmashbrosmelee.md`, section 34 sound paragraph (line 239). Before, NPC and remote-player actions played configured move-set sounds. After, every NPC character's equipped move set explicitly supplies its own move animation and sounds; an interrupted move's remaining phase sounds are canceled and the replacement move's animation/start sound begins.
- No prior content existed for this changelog; this is a new file.

## Review and implementation boundary

- Read the Melee spec's action-cancellation, animation-authority, NPC participation, hot-reload, and move-sound sections, plus the repository time-and-filename standard.
- Specification review: `PASS`. The new NPC-specific cancellation wording aligns with the existing general Melee rule that a legal action can interrupt windup, active, or recovery. Sounds remain presentation only and use configured per-action assets.
- No tests or build were run because this was a documentation-only request.
- Known implementation gap from source inspected in the preceding investigation: the server Melee NPC action path starts a move only when its action state is idle and schedules the next start after action duration plus gap. The new spec intentionally changes the required behavior; code still needs to be updated and runtime-verified separately.
