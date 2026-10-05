# Counter-Strike NPC targeting and wall recovery

## Request

Investigate why Counter-Strike NPCs behaved differently from Sandbox: some did
not attack the human, NPC-vs-NPC behavior was unclear, and actors repeatedly
walked into walls or doorways. Record that the team-spawn issue appeared fixed.

## Findings and changes

- Counter-Strike already enables `rage2`, `opposite_team`, and both human/NPC
  targets.
- The active profile scoring path made Counter-Strike differ from Sandbox's
  nearest-hostile behavior. `opposite_team` now uses nearest hostile selection
  while Rage2 remains the combat tuning profile.
- Human team checks now use the authoritative `matchTeams` roster before the
  possibly stale player mirror.
- Stuck NPCs choose a locally open direction before jumping and requesting a
  repath.
- The human playtest reported the earlier cross-team spawn issue as fixed.

## Evidence

- Build: `python build_agent.py` returned BUILD SUCCESS and relinked the EXE.
- `--counterstrike-acceptance-selftest`: PASS.
- `--npc-targeting-selftest`: PASS.
- `--spawn-tag-selftest`: PASS.
- No post-fix live match was observed by this session.
