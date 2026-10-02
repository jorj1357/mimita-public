# Square Big Shotgun Spread

Time UTC: 2026-10-02T01:35:55Z
Display timezone: America/New_York
Branch: afad20a-rebuild
Commit: working tree; no commit created

## Result

Status: PASS_WITH_HUMAN_REVIEW

Changed Big Shotgun spread generation from the server's circular seeded
distribution plus a separate local grid to one shared square-pattern owner.
The new `squareSpread` policy uses `spread` as the full angular side length:
each pellet is placed within equal horizontal and vertical limits around aim.
With the current Big Shotgun `spread: 50.0`, the square reaches approximately
25 degrees on each side of the aim direction. The normal shotgun remains on
its existing pattern because `squareSpread` is enabled only for Big Shotgun.

## Code changes

- `src/combat/pellet-pattern.h`: added the shared `squareSpread` policy flag.
- `src/combat/pellet-pattern.cpp`: added deterministic square-grid generation;
  100 pellets form a 10x10 pattern.
- `src/combat/weapon-execution.cpp`: reads `custom_params.squareSpread` for
  authoritative shared hitscan generation.
- `src/combat/weapon-fire-hit.cpp`: local prediction now calls the same shared
  generator instead of maintaining a second grid implementation.
- `config/weapons.json`: enabled `squareSpread: 1.0` only for `big_shotgun`.
  The existing user-tuned `spread: 50.0` was preserved.

## Validation

- Square-spread JSON assertions: PASS.
- `git diff --check` on the changed spread files: PASS.
- Existing dev-loop completed successfully and published
  `C:\mimita-v9\.dev\builds\0872\mimita.exe`.
- Two existing `mimita.exe` processes remained running and were not stopped,
  restarted, killed, relinked, or replaced.

Build proof is separate from human visual proof. The attached screenshot was
used as evidence of the requested shape; in-game confirmation is still needed
to judge the final visual size at the current `spread: 50.0` tuning.

## Documentation locations for the no-cold-build rule

The active rule is stated in:

- `docs/operations/build-and-exe/build-and-exe.md:9-17`
- `docs/architecture/live-development/live-development.md:14-43`
- `docs/architecture/live-development/live-development.md:115-118`
- `docs/operations/build-and-exe/build-and-exe.md:129-131`
- `docs/gold/2026-09-12-live-jsonl-ai-observability.md:143-149`
- `docs/operations/task-completion/task-completion.md:63-67`
- `docs/regressions/README.md:332-360`
- `docs/architecture/live-development/hot-audio-contract.md:9`

Archived documents may repeat the historical rule, but the paths above are
the current operational sources.

## Pre-existing work

All unrelated worktree edits and prior changelogs were preserved. This session
added only the square-spread implementation/config changes and this changelog.
