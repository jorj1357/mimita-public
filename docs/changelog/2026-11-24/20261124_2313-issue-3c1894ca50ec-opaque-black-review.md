# issue-3c1894ca50ec Opaque-Black Pass-Through Review

# Task

- Task ID: review high-priority user context ctx-e298491ee9e1 (wake-3f9a0ed7ebc8)
- Owner: issue-review-sweeper (review; task `opaque-black-shader-trace` remains owned by bloodtest-quality-triage)
- Focus: "still the black colors are treated as pass thru dont look at them" tied to issue-3c1894ca50ec
- Summary: Reviewed routing and readiness (ready, no missing fields), inspected the shader/material/render path, and falsified the texture-alpha-at-black hypothesis against the shipped Bloodtest assets. Appended an evidence suggestion to the issue and asked the task owner to accept the remaining proximity-fade/viewmodel state-leak lead.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T23:13:38Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted work. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only coordination/issue records. No code, build, or runtime change.

# Verification performed (read-only)

- Inspected `shaders/basic.frag`, `shaders/basic.vert`, `src/render/render-world-mesh.cpp`, `src/entities/player-render.cpp`, `src/render/render-player.cpp`, `src/combat/weapon-viewmodel.cpp`, `src/combat/weapon-model-cache.cpp`, `src/map/map-loader-gltf-prims.cpp`, `src/map/map-loader-material.cpp`, and `config/camconfig.json` plus `config/gamemodes/bloodtest.json`.
- Parsed the GLB JSON and embedded PNGs for the exact Bloodtest assets: `assets/objects/weapons/mimita-ak47-v1.glb`, `mimita-judgement-v1.glb`, `mimita-bat-v1.glb`; maps `mimita-duels-map-v3`, `atdm`, `funworld`, `dust2cyberiav4`; cosmetic `sword1.glb`/`sword2.glb`; and all `assets/objects/**/*.glb`.

# Findings

- No shader branch in `basic.frag` keys on RGB black; black is discarded only through texture alpha (`finalAlpha = texel.a * proximityAlpha * uOpacity`, `finalAlpha <= 0` discard at basic.frag:330-334) or `uAlphaCutoff` (0 for world) or body holes.
- FALSIFIED for shipped Bloodtest assets: every Bloodtest weapon texture and every embedded Bloodtest map texture sampled has 0 transparent pixels (alpha=255) and no material declares `alphaMode: BLEND/MASK`, so texture alpha cannot explain the report on current assets.
- OBSERVED latent state leak: the local body render sets `uCameraProximityFadeEnabled` from `CamConfig` (player-render.cpp:360-361) and the restore block (691-698) does not reset it; `WeaponViewModel::render` (weapon-viewmodel.cpp:606-644) never sets `uCameraProximityFadeEnabled`, `uAlphaCutoff`, or `uOpacityEnabled`, and runs after `renderPlayer` (engine-tick-render.cpp:627 then 651). With camconfig proximityFade start=2m/end=3m, basic.frag:322-334 discards the held weapon within 2m regardless of its opaque texture.
- INFERENCE (not proven): the user-visible "pass-through" may be camera proximity fade (plus stale alpha/blend state) applied to the held/world weapon, not a black color-key.
- UNKNOWN: measured camera-to-weapon distance at the user's repro and whether disabling proximityFade removes the symptom.

# Exact implementation changes

- None. Append-only issue/coordination records only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or test invoked.
- Review artifact: suggestion appended to issue-3c1894ca50ec; issue-linked message sent to bloodtest-quality-triage (requires response); ctx-e298491ee9e1 acknowledged.
- Next owned check (owner): pinned Bloodtest run holding the bat and Judgment revolver, journal `camera.proximity_fade.*` plus screenshots with proximityFade enabled vs disabled.

# Human acceptance

- Visual review: not performed. No behavior proof claimed.
