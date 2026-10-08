# Juggernaut Fighter camera preference fix

Date: 2026-10-08 15:44:27 -04:00 (EST)
Branch: `afad20a-rebuild`

## Result

Fighters in Juggernaut mode no longer force a perspective. The actual
Juggernaut remains forced first person through `juggernaut_arcade`; Fighters
may use either first or third person and may change it with the normal camera
command.

## Cause and correction

- `config/actor-presets/juggernaut_fighter.json`: the pre-existing working-tree
  value was `"force_perspective": true`; it is now restored to
  `"force_perspective": false`.
- `src/network/community-match-client.cpp`, `CommunityMatchClient::onState()`
  around lines 503-510: when the shared mode preset had already applied the
  Juggernaut first-person camera, switching to the replicated Fighter preset
  changed the preset ID without restoring the saved user camera choice. The
  role transition now calls `resetActorPreset()` before applying the local
  role preset.

## Specification and focused review

- Read `docs/ROUTER.md` and routed gamemode, networking, runtime-validation,
  build, task-completion, live-development, and behavior-review documents.
- `docs/specs/gamemodes/juggernaut.md` requires normal Fighter camera values;
  the Juggernaut role owns the forced first-person policy.
- `docs/skills/spec-behavior-review-v1.md`: PASS for the requested role-scoped
  behavior; live visual acceptance remains required.

## Validation

- `git diff --check`: PASS.
- Forced affected object recompilation and relink: PASS; 1 source compiled,
  548 skipped, linker completed successfully.
- Fresh executable: `mimita-20261008T154500-juggernaut-camera.exe`.
- `--actor-preset-selftest`: loaded the active presets and printed
  `juggernaut_arcade first_person=1` and `juggernaut_fighter first_person=0`,
  but returned FAIL because the existing unrelated Counter-Strike movement
  policy assertion is out of sync. This is not counted as a passing test.
- `--versioninfo`: PASS. Journal:
  `logs/10-08-2026/20261008_154412/events.jsonl`.

## Human review still needed

Using the fresh executable in a real Juggernaut session, join the Fighter team,
switch with the normal first/third-person command, and confirm several state
packets do not overwrite the choice. Repeat as the actual Juggernaut and
confirm first person remains enforced. Existing MiMITA processes were left
running and were not restarted or terminated.

## Pre-existing work

Unrelated edits in the working tree were preserved, including the existing
configuration, networking, NPC, death-presentation, ragdoll, and prior
changelog changes visible before this session.
