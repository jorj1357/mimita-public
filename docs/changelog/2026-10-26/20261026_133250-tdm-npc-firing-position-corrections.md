# TDM NPC firing and repeated position-correction issue intake

- Recorded: 2026-10-26T13:32:50Z (2026-10-26 09:32:50 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and changes

Recorded two new user reports in both gameplay issue records:

- `docs/specs/open-gameplay-issues.md:1-14` and
  `docs/specs/open-gameplay-issues-human.md:9-22` now describe the TDM report:
  after the player kills one NPC, surviving NPCs reportedly stop shooting at
  the player. This remains unreproduced; no cause is claimed. The summary
  `NPC combat` rows at lines 414 and 225 now include the report and recommend
  comparing target acquisition and legal/accepted shots before and after the
  first NPC death.
- The same new-intake blocks record repeated position corrections during
  ordinary high-speed movement far from map geometry: the player reports
  snapping each tick to a point above the general map area, slowing from about
  250 m/s toward 10 m/s, then being able to descend without more snapping.
  Summary rows at lines 433 and 243 include this report. The entry explicitly
  distinguishes it from the older journal evidence for a correction after an
  explicit teleport into blocking geometry. The new report is not reproduced
  and no cause is established.

## Review and evidence

- Consulted `docs/ROUTER.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/specs/movement/movement.md`,
  `docs/specs/networking/networking.md`,
  `docs/architecture/collision/collision.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/skills/documentation-checker-v1.md`.
- No source behavior was changed and no runtime was available for reproduction.
  The entry requests comparison of target acquisition, shot decisions/results,
  predicted/accepted positions and velocities, correction reason/magnitude,
  and collision contacts.
- `git diff --check`: `PASS`.
- Build and gameplay tests: `NOT_APPLICABLE` for this documentation-only
  intake. Live reproduction and human acceptance remain open.

## Existing working-tree state preserved

- Existing modifications to `config/accounts/default.json`,
  `config/gamemodes/masterlist/gamemode-masterlist.json`,
  `config/gamemodes/tdm.json`, `config/movement.json`, and the gameplay issue
  documents were preserved. This task only added issue text to the two issue
  documents.
- The existing untracked changelogs
  `docs/changelog/2026-10-26/20261026_042100-tdm-invisible-players.md` and
  `docs/changelog/2026-10-26/20261026_133120-competitive-no-headless-avatar-group.md`
  were left untouched. This record is the single changelog for this additional
  repository-touching request.
- No files were committed.

## Human review still needed

Reproduce the TDM NPC behavior after the first player kill and collect the
relevant NPC targeting and shot evidence. Separately reproduce high-speed
movement beyond map geometry and collect client/server position, velocity,
correction, and collision evidence before selecting a fix.
