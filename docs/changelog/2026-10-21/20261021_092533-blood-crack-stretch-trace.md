# Blood and crack texture stretch trace

- Time: 2026-10-21T09:25:33Z (2026-10-21 05:25:33 EDT)
- Branch: `20261011zt`
- HEAD: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Scope and existing state

Traced how blood and world-crack textures are spawned and rendered after the
user supplied two additional screenshots. Updated the existing gameplay issues
inbox. This session changed documentation only. The checkout already had
numerous unrelated modified and deleted files and untracked work; those changes
were preserved.

## Changed files

### `docs/specs/open-gameplay-issues.md`

- Lines 66-79 add the two new screenshot filenames and summarize the confirmed
  sources of variable stretching.
- Line 84 updates the issue with the blood and crack code paths and a corrective
  direction.

Before, lines 66-75:

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

After, lines 66-79:

> These are player reports from a play session. They are recorded as observed or
> requested behavior, not as confirmed causes. The six attached screenshots
> (`codex-clipboard-63358e38-80a0-4b99-aa7e-97e1d38bbb0c.png`,
> `codex-clipboard-b16badb8-8ceb-493a-978f-d3f90dc1c125.png`,
> `codex-clipboard-9d0cc63b-bd0d-47fb-8adb-63f1f399f40b.png`,
> `codex-clipboard-5204e8e3-66ee-4680-bcac-3f8335af5b04.png`,
> `codex-clipboard-1d93ad7b-2665-4f18-8c7b-420e912524e6.png`, and
> `codex-clipboard-cec704f2-58e0-454c-9d53-8cb2415c12e9.png`) show blood and
> world-impact marks appearing as clipped, blocky bars, stretched shapes, or
> shards with heavy overlap. Source inspection found two intentional sources of
> variation: blood spray billboards get a random 0.7–1.3 width/height stretch,
> and world cracks use a random chain of narrow segments while mapping the full
> crack PNG to every segment. Both can change the authored PNG's shape; cracks
> also repeat it into strips. The PNGs contain alpha, and the shared stamp
> renderer loads RGBA and enables alpha blending. This does not explain every
> large red/transparent shape visible in the screenshots; non-textured hit
> effects and tracers also have separate render paths. No screenshot-matching
> runtime journal was found, so the exact live draw sequence remains unconfirmed.

Before, line 84:

> | Blood and world-impact marks | Effects should appear consistently when any actor hits another actor, including NPC-to-player hits. Blood and world-crack marks should preserve the complete authored PNG shape and alpha, never appear as square stamps, clipped chunks, repeated strips, or bars/shards. | User-reported; four screenshots attached above. Source-confirmed crack rendering defect: `spawnWorldCracks()` creates a chain of narrow segment quads, and the renderer maps the full 0–1 texture UV range onto every segment, repeating and squeezing the complete crack PNG. The blood renderer maps the full image to one quad with RGBA blending; its reported blocky appearance is not explained by this path and remains for live reproduction. Corrective direction: draw the crack PNG as one complete surface-aligned stamp, or define deliberate segment-specific UVs; preserve transparency and verify blood separately in the real game. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md), [`weapons/weapons.md`](weapons/weapons.md); code owners: `src/effects/effect-part.cpp::spawnWorldCracks()`, `src/effects/effect-part-render.cpp::appendTexturedQuad()` / crack decal batching, `src/world/texture-store.cpp::TextureStore::getPath()` |

After, line 84:

> | Blood and world-impact marks | Effects should appear consistently when any actor hits another actor, including NPC-to-player hits. Blood, bullet holes, and world cracks should preserve the complete authored PNG shape and alpha, with only uniform size changes and in-plane rotation; they should never become square stamps, clipped chunks, repeated strips, or randomly stretched. | User-reported; six screenshots attached above. Source-confirmed crack defect: `spawnWorldCracks()` creates random narrow segment chains, and the renderer maps the full 0–1 texture UV range onto every segment, repeating/squeezing the complete PNG. Source-confirmed blood variation: `spawnBloodEffect()` assigns each textured spray particle a randomized width/height ratio (`stretch = 0.7–1.3`); size also varies by force. Corpse spray calls the same function, so hit and death blood share this variation, though each spawn gets different random samples. Blood surface decals use a uniform quad and random in-plane rotation. Corrective direction: preserve each PNG's aspect ratio; use uniform blood particle scale; render a world-crack image once as a complete surface stamp or use intentional segment-specific UVs. The screenshots also contain large red/transparent geometry whose source is not identified. No matching runtime journal was found for room `B5DSC72`, so correlate a fresh capture with the actual draw events before claiming all visual artifacts are traced. | [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md), [`weapons/weapons.md`](weapons/weapons.md); code owners: `src/effects/hit-effects.cpp::HitEffects::onHit()`, `src/effects/effect-part-blood.cpp::EffectPartSystem::spawnBloodEffect()`, `src/ragdoll/ragdoll-mode.cpp::RagdollModeSystem::sprayCorpseBlood()`, `src/effects/effect-part.cpp::EffectPartSystem::spawnWorldCracks()`, `src/effects/effect-part-render.cpp::appendTexturedQuad()` / crack decal batching |

## Source trace

- `HitEffects::onHit()` sends world-hit events to `spawnWorldImpact()`,
  `spawnWorldCracks()`, and `spawnBulletImpact()`; entity-hit events go to
  `spawnBloodEffect()` (`src/effects/hit-effects.cpp:100-131`). The large
  world-hit sphere is not one of the PNG decals and has its own effect render
  path.
- Blood spray sets `particle.stretch` from `0.7` through `1.3` at
  `src/effects/effect-part-blood.cpp:345`. `effect-part-render.cpp:605-606`
  multiplies one billboard dimension by that value, making this a deliberate
  randomized non-uniform image scale. Blood surface stains use equal in-plane
  dimensions; rotation alone does not distort their aspect ratio.
- NPC corpse spray calls the same `spawnBloodEffect()` at
  `src/ragdoll/ragdoll-mode.cpp:2470`, so the blood particle aspect variation
  applies to both ordinary hit spray and corpse spray. Separate force-based
  particle size variation is also intentional and is uniform in both axes.
- `EffectPartSystem::spawnWorldCracks()` chooses a random segment count, length,
  and turn (`src/effects/effect-part.cpp:386-411`). The crack renderer creates a
  narrow quad per segment and `appendTexturedQuad()` maps the complete UV range
  0–1 across each quad (`src/effects/effect-part-render.cpp:657-677`). This both
  repeats the full PNG and changes its aspect ratio. The random chain makes the
  result differ between impacts.
- Both configured PNGs are 256x256 RGBA with nonzero alpha. The texture loader
  uploads RGBA, and the stamp renderer uses source-alpha blending. The observed
  repeated strip and random aspect mechanisms therefore come from geometry and
  scale choices, rather than the PNG being converted into an opaque square.
- A repository search found no `events.jsonl` containing room `B5DSC72`. The
  visible replay tick `126484` cannot be tied to a journal or exact executable
  from available evidence.

## Documents and review

- Read the router, the blood/effects specification, weapon specification,
  relevant code ownership and JSON configuration rules, runtime scenario
  validation workflow, time/filename rules, and the documentation and behavior
  review skills.
- The router points to `docs/doc-review-09-03-2026.md` and
  `docs/regressions/regressions-v1.md`; both are absent in this checkout.
- Existing TODO at `docs/specs/visuals/blood-bulletholes-viscera.md:2` reads
  `jorj todo explain`. Suggested expansion: state that authored blood and
  world-impact PNG aspect and alpha must be preserved, with uniform scaling and
  rotation allowed. Left untouched per the router's TODO instruction.

## Validation and remaining work

- Both changed Markdown files were checked for trailing whitespace in the newly
  edited sections; none was found. The inbox has older pre-existing trailing
  whitespace in raw intake notes, left untouched.
- No code changed, so no build was run. No runtime reproduction was available;
  the screenshot room was absent from the local journal set and the active game
  was left undisturbed.
- Human review is still needed. Fix the known scale/UV causes, then confirm in a
  live run that hit blood, death blood, bullet holes, and world cracks preserve
  their intended PNG shape across repeated impacts. The large red/transparent
  geometry requires its own visual/source correlation.
