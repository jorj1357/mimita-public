# Blood and world mark texture investigation

- Time: 2026-10-21T09:20:49Z (2026-10-21 05:20:49 EDT)
- Branch: `20261011zt`
- HEAD: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Updated the general gameplay issue inbox with the new screenshot and investigated
why blood/world-crack PNGs appear as squares, bars, or shards. This session made
documentation changes only. The worktree had extensive unrelated pre-existing
changes and deletions; none were reverted or otherwise modified here.

## Changed files

### `docs/specs/open-gameplay-issues.md`

- Lines 66-75: the screenshot list and issue summary were updated.
- Line 84: the blood/world-impact issue now states the intended full PNG/alpha
  behavior, identifies the confirmed world-crack UV/geometry defect, and keeps
  the blood symptom open pending runtime reproduction.

Before, lines 66-72:

> These are player reports from a play session. They are recorded as observed or
> requested behavior, not as confirmed causes. The three attached screenshots
> (`codex-clipboard-63358e38-80a0-4b99-aa7e-97e1d38bbb0c.png`,
> `codex-clipboard-b16badb8-8ceb-493a-978f-d3f90dc1c125.png`, and
> `codex-clipboard-9d0cc63b-bd0d-47fb-8adb-63f1f399f40b.png`) show blood and
> impact marks appearing as clipped, blocky bars or shards with heavy overlap;
> the rendering cause is unknown.

After, lines 66-75:

> These are player reports from a play session. They are recorded as observed or
> requested behavior, not as confirmed causes. The four attached screenshots
> (`codex-clipboard-63358e38-80a0-4b99-aa7e-97e1d38bbb0c.png`,
> `codex-clipboard-b16badb8-8ceb-493a-978f-d3f90dc1c125.png`,
> `codex-clipboard-9d0cc63b-bd0d-47fb-8adb-63f1f399f40b.png`, and
> `codex-clipboard-5204e8e3-66ee-4680-bcac-3f8335af5b04.png`) show blood and
> impact marks appearing as clipped, blocky bars or shards with heavy overlap.
> Source inspection confirms that world cracks currently map the complete PNG to
> each narrow procedural crack segment, which stretches/repeats its image into
> strips. Blood and crack PNGs both have alpha channels, and the shared stamp
> renderer loads RGBA and enables alpha blending. The blood screenshot symptom
> still needs a focused runtime reproduction to identify its cause.

Before, line 79:

> | Blood and world-impact marks | Effects should appear consistently when any actor hits another actor, including NPC-to-player hits. Marks should keep their complete, intended shape without clipping or turning into bars/shards. | User-reported; screenshots attached above; cause and exact effect/texture policy are unknown. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md), [`weapons/weapons.md`](weapons/weapons.md) |

After, line 84:

> | Blood and world-impact marks | Effects should appear consistently when any actor hits another actor, including NPC-to-player hits. Blood and world-crack marks should preserve the complete authored PNG shape and alpha, never appear as square stamps, clipped chunks, repeated strips, or bars/shards. | User-reported; four screenshots attached above. Source-confirmed crack rendering defect: `spawnWorldCracks()` creates a chain of narrow segment quads, and the renderer maps the full 0–1 texture UV range onto every segment, repeating and squeezing the complete crack PNG. The blood renderer maps the full image to one quad with RGBA blending; its reported blocky appearance is not explained by this path and remains for live reproduction. Corrective direction: draw the crack PNG as one complete surface-aligned stamp, or define deliberate segment-specific UVs; preserve transparency and verify blood separately in the real game. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md), [`weapons/weapons.md`](weapons/weapons.md); code owners: `src/effects/effect-part.cpp::spawnWorldCracks()`, `src/effects/effect-part-render.cpp::appendTexturedQuad()` / crack decal batching, `src/world/texture-store.cpp::TextureStore::getPath()` |

## Investigation evidence

- `config/impact_decals.json:22` selects `assets/textureshq/sblood3.png`;
  line 160 selects `assets/textureshq/worldcrack3.png`.
- Both PNG files are 256x256 RGBA and have nonzero alpha. `loadTexture()` in
  `src/map/texture.cpp` requests four channels and uploads `GL_RGBA`;
  `drawTexturedHitParticles()` in `src/effects/effect-part-render.cpp` enables
  `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` blending.
- `EffectPartSystem::spawnWorldCracks()` in `src/effects/effect-part.cpp`
  creates multiple narrow crack-segment decals. The crack branch in
  `src/effects/effect-part-render.cpp` builds a narrow rectangle per segment,
  then calls `appendTexturedQuad()`, which maps UVs from 0 to 1 over that entire
  rectangle. Each segment therefore compresses and repeats the full crack PNG.
  This explains the repeated strip/shard appearance visible in the attached
  screenshot. It is a source-path finding; no code fix was made in this task.
- Blood is drawn as one complete textured quad per particle/decal with alpha
  blending. This does not explain the reported blood blocks. Source data and
  texture alpha alone cannot confirm what happened in the screenshot; a fresh
  game reproduction and live journal are still needed.

## Documents and review

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/specs/visuals/blood-bulletholes-viscera.md`,
  `docs/specs/weapons/weapons.md`,
  `docs/architecture/code-ownership/code-ownership.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/workflows/runtime-scenario-validation.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Router references `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md`, but neither file exists in this checkout.
- The documentation checker asks for a repository-wide TODO scan. Existing
  relevant TODOs remain untouched per the router:
  `docs/specs/visuals/blood-bulletholes-viscera.md:2` says `jorj todo explain`; suggested expansion: make
  explicit whether all blood/decal stamps must preserve the authored image's
  full aspect and alpha. `docs/specs/weapons/weapons.md:1` says
  `2026 10 20 1655 jorj todo explain`; suggested expansion: state the intended
  combat rules currently left unclear in that spec. The router and checker
  themselves also contain matching instructional TODO wording, not feature
  TODOs.

## Validation and remaining evidence

- The issue inbox is untracked in this checkout, so `git diff --check` cannot
  validate its contents; both changed Markdown files were manually inspected.
- No code changed, so no build was run. No live game or runtime journal was
  inspected for this screenshot; the existing running game was left untouched.
- Human visual review remains required after a fix. The next implementation
  should render the crack PNG as one complete surface stamp or author deliberate
  segment UVs, then reproduce both the crack and blood symptoms in the real game.
