# Impact decal image issue investigation

- Time: 2026-10-26T20:44:51Z (2026-10-26 16:44:51 EDT)
- Branch: `20261011zt`
- HEAD: unchanged during this session (see `git rev-parse HEAD` at review time)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope and existing state

Investigated the user's 2026-10-26 screenshot and the existing gameplay issue
tracker entry for distorted blood/world-impact/bullet-hole visuals. This was a
source and asset inspection only; no executable was launched, no runtime
scenario was run, and no rendering/configuration code was changed.

The worktree already had edits in `config/impact_decals.json` and multiple
other config files, plus the untracked changelog
`20261026_203144-juggernaut-team-avatars.md`. These pre-existing edits were
preserved. The `config/impact_decals.json` working copy has bullet-hole
`textureScale: 7.0`, radius `0.2`, bullet-hole lifetime 30 s, world-crack
diameter 2.0 world units, lifetime 30 s, and decal caps 8192. These are local
settings, not confirmed runtime-loaded settings.

## Findings

- The current textured decal batch maps the complete PNG onto an equal-sided
  surface quad, enables alpha blending, tests depth, and disables depth writes
  (`src/effects/effect-part-render.cpp::drawTexturedHitParticles()` and the
  `SurfaceDecal` render loop). The previous world-crack full-image-per-narrow-
  segment source defect was removed in the 2026-10-21 fix.
- `spawnBulletImpact()` places each bullet mark at `position + normal * 0.01`
  and stores radius from config. At the current local settings,
  `radius * textureScale` makes a 1.4-unit half-width quad (2.8 units across)
  before mode scaling. Repeated stamps with long lifetime and high cap can
  overlap extensively. This is a plausible contributor, not a proven cause of
  the screenshot.
- World cracks now use a complete square stamp with configured diameter 2.0
  units; the prior strip/repeated-UV cause is not in this source path.
- `HitEffects::onHit()` also emits world/entity impact spheres, blood spray,
  and other hit effects. Those have different render paths and can appear as
  geometry-like shards independently of the textured decal PNGs.
- The tracked PNG previews (`holesbullet4.png`, `worldcrack3.png`, `sblood3.png`)
  did not establish an asset-only explanation. The present once-per-second
  render summary reports blood and crack aspects, but not bullet-hole count or
  per-hit draw identity; it cannot distinguish crowded stamps from transient
  hit effects in this screenshot.
- No current live journal or executable identity was supplied with the
  screenshot. Cause remains unconfirmed.

## Tracker update

Updated `docs/specs/open-gameplay-issues.md:990`, the existing rank-7 visual
issue. Before:

> | 7 | Bullet-hole visuals still do not look accurate to the intended appearance; bullet-hole rendering remains visually incorrect despite earlier shape work. | Latest user report (2026-10-26); not visually verified in the current session. Reproduce and compare the in-game marks with the intended reference before deciding whether the renderer, decal placement, or source art needs adjustment. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md) |

After: the row now includes blood/world-impact marks, the source-backed square
quad and blending facts, the local scale/lifetime/cap values as plausible
overlap contributors, the separate transient effect paths, missing diagnostic
coverage, and the next runtime reproduction needed to identify a first
divergence.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/visuals/blood-bulletholes-viscera.md`,
  `docs/specs/open-gameplay-issues.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Reviewed prior investigations/fix records
  `docs/changelog/2026-10-21/20261021_092533-blood-crack-stretch-trace.md`
  and `docs/changelog/2026-10-21/20261021_093539-blood-crack-shape-fix.md`.
- `docs/regressions/regressions-v1.md` and
  `docs/doc-review-09-03-2026.md`, selected by the router, are absent from this
  checkout. No regression was confirmed, so no regression entry was added.
- Spec/behavior review: source confirms equal-sided full-image stamp geometry;
  no spec/source contradiction established. The screenshot appearance is
  unresolved without current runtime evidence and human visual comparison.

## Validation and remaining evidence

- Source paths and the tracker were reviewed; local impact texture files were
  visually inspected.
- No code changed, so no build was run. No runtime was disturbed or launched.
- Next evidence: use the dev loop and a fresh executable/journal per the runtime
  scenario workflow; reproduce a single world impact and a single entity hit,
  inspect live draw/effect records, then repeat impacts to test overlap. Human
  comparison should determine whether the wrong pixels come from PNG decals or
  transient hit geometry before choosing a renderer, placement, asset, or
  configuration fix.
