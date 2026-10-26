# TDM NPC death-ragdoll intake — 2026-10-26

- **Timestamp:** 2026-10-26T20:01:35Z (2026-10-26 16:01:35 EDT)
- **Branch:** `20261011zt`
- **HEAD:** `02f69bec4c1644264baf7abe2bc3f7de57696134`
- **Result:** PASS_WITH_HUMAN_REVIEW

## Change

Added a separate TDM item to `docs/specs/open-gameplay-issues.md` at line 32:

> **TDM NPC-versus-NPC kills do not show death ragdolls:** The player reports
> that when an NPC kills another NPC in TDM, no death ragdoll appears, while a
> player killing an NPC does produce one.

The entry says the expected behavior is to show a death ragdoll for either
killer type when death presentation is enabled, and recommends comparing the
lethal transition, corpse request/result, victim life identity, resolved mode
and global settings, and client presentation. It keeps the report unconfirmed
and does not assume that the TDM setting is ignored.

## Requirements and evidence

- `docs/specs/gamemodes/teamdeathmatch.md` says TDM enables death ragdolls and
  that a mode can explicitly suppress them with `ragdoll_enabled: false`.
- Current `config/gamemodes/tdm.json` contains
  `"ragdoll_enabled": true` at line 196.
- The related `docs/changelog/2026-10-24/20261024_232717-ragdoll-death-default.md`
  records that TDM's former false override was corrected. This new NPC-killer
  comparison does not establish a regression in that earlier fix.
- No TDM runtime journal or source path for this comparison was supplied. No
  regression record was added because the current report is not reproduced or
  confirmed.

## Review and validation

Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/gamemodes/teamdeathmatch.md`,
`docs/specs/gamemodes/gamemodes.md`,
`docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`,
`docs/regressions/README.md`,
`docs/regressions/2026-10-06/ragdoll-per-death-presentation-ATTEMPT-1-REG.md`,
and the related 2026-10-24 ragdoll changelog.

- Documentation/spec review: PASS_WITH_HUMAN_REVIEW. Intake distinguishes
  expected configuration from unverified runtime behavior.
- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed.
- No source, configuration, or runtime behavior changed. No build or gameplay
  run was performed.
- Other pre-existing working-tree changes were preserved.

## Human review still needed

Reproduce both NPC-kills-NPC and player-kills-NPC in the same confirmed TDM
mode/build with `ragdoll_enabled: true`. Compare authoritative lethal damage,
death transition, per-life corpse request and result, and client presentation;
then identify the first stage that differs.
