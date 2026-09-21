# Deterministic hot weapon probe

## Outcome

Added a deterministic weapon probe to the existing `--hot-combat-selftest`.
It reuses the real hot tool claim, hot attachment state, `Player`, and
`recomputeWeaponCapsule` path. It does not create a second weapon collision
system and it does not emit hot-reload lifecycle records.

The probe covers five named phases:

- `idle`
- `aim`
- `reload`
- `melee`
- `wall_contact`

Each phase writes a `WEAPONS` `weapon.probe_phase` event containing the hot
pose, collision grip/tip, radius, and validity. The existing transform probe
also aggregates the five samples.

## Runtime evidence

- Build: `mimita-20260921T102706.exe`
- Command: `--hot-combat-selftest`
- Exit code: `1` because unrelated pre-existing animation phase assertions
  still fail in the larger hot-combat suite.
- New weapon probe assertion: **PASS**
- JSONL: `logs/2026-09-21/20260921_142733/events.jsonl`
- Records: five `weapon.probe_phase` events and one
  `weapon.transform_probe.summary` event.
- All five samples reported `hot_claim_written=true`,
  `hot_attachment_written=true`, `collision_valid=true`, and
  `collision_capsule_mode=true`.
- The summary reported `hot_transform_used=true` and
  `tip_minus_hot_muzzle=[0,0,0]`.

## Honest limitation

`wall_contact` currently labels a deterministic pose sample. It does not yet
construct a world triangle and run `doBodyWeaponCollisionPhase`, so this proves
hot pose-to-collider alignment, not physical wall blocking. The next focused
step is a real fixed-tick wall fixture that records contact, correction, and
slide response through the existing cached collision path.

## Cold/hot boundary

The self-test and probe calls are in cold-linked owners, so this session needed
a timestamped cold link. The probe data shape and phase policy should later move
behind the generic hot collision result envelope; then only the stable logger
bridge and collision mechanism remain cold.

