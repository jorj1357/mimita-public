// 2026-10-01T21:28:37Z (display: 2026-10-01 17:28:37 EDT)
/* purpose
* Record the pass that fixed the sustained 2 fps after shooting: fracture no
* longer replays the whole cut history (Decompose the cached running solid),
* fracture is disconnected-only by default with a cooldown and budget, and cut
* batching + commented config.
*/

# Task

- Summary: kill the sustained post-shoot dip (fracture), make fracture rare and
  budgeted, batch cuts, comment the config.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`.
- Result states: build PASS; 5 selftests PASS; full-auto drain bounded headlessly.

# Evidence (`logs/10-01-2026/20261001_170721/events.jsonl`)

Windows ~612-644: fps 2, `max_entity_physics ≈ max_destruction ≈ 480-550 ms`
every window, while actor physics <= 5 ms and rendering <= 1 ms. Sustained ~30 s
after shooting, then recovered.

Cause: `boolean-mesh.cpp booleanDecomposePieces` replayed the entire cut history
from the base and then `Decompose()`d it, and `decomposePieces` passed all cuts.
The unbalanced-support heuristic (comOffsetFraction 0.08) fired on ordinary
holey crates, so `applyFracture` ran nearly every tick while the queue drained.

# Changes

- `src/impact/boolean-mesh.{h,cpp}`: added `booleanDecomposeIncremental(sessionId,
  base)` which decomposes the cached running solid (no history replay); the
  full-replay path remains as a fallback. Extracted `decomposeManifold`.
- `src/impact/destructible-geometry.{h,cpp}`:
  - `decomposePieces` uses the incremental decompose.
  - `FractureTuning::unbalancedEnabled` (default false) gates the
    unbalanced-support branch; `fractureCooldownTicks` (30) + `hasFractured` /
    `lastFractureTick` on the record.
  - `countSurfaceComponents` uses an `unordered_map` + FNV hash.
- `src/impact/impact-system.cpp` `flushPendingCuts`: checks the ms budget
  *before* each entity; processes entities that have a pending fracture flag;
  applies the per-entity fracture cooldown.
- `config/destructible-world.json`: every field commented; `maxCutsPerEntityPerTick`
  = 4; `fracture.unbalancedEnabled` = false; `fracture.fractureCooldownTicks` = 30.
- `src/impact/destructible-world-config.cpp`: parse `unbalancedEnabled`,
  `fractureCooldownTicks`.
- `src/impact/destructible-selftest.cpp`: test 17 opts into
  `unbalancedEnabled` (the heuristic is now opt-in).
- `src/physics/physical-entity.cpp`: perf selftest gains a full-auto drain
  measurement.

# Measured effect

- Full-auto (200 cuts on an already-hole crate, cap 4/flush): avg 0.26 ms,
  max 0.86 ms per flush (was ~500 ms/tick sustained for ~30 s).
- Moving holey crates ~2.6 ms/tick (TARGET 4 ms MET); settled ~0.025 ms/tick.

# Reasoning

- Spec section 45 (do not block the frame on heavy work) and 44 (budgets).
- The user's directive: fracture only on real disconnection; config drives cut
  batching; fragments get no special logic (stronger collisions instead).

# Files changed

- `src/impact/boolean-mesh.{h,cpp}`
- `src/impact/destructible-geometry.{h,cpp}`, `impact-system.cpp`,
  `destructible-world-config.cpp`, `destructible-selftest.cpp`
- `src/physics/physical-entity.cpp`
- `config/destructible-world.json`
- `docs/regressions/2026-10-01/physical-objects-collision-REG.md` (Attempt 10)

# Pre-existing (not authored this session)

`config/accounts/default.json`, `config/analytics.json`,
`config/movement/movement-source.json`, `docs/specs/20261001plan.md`,
`src/devtools/dev-log-commands.cpp` were already modified; left untouched.

# Validation

- Build: `python build.py build-only` -> success.
- All five self-tests PASS.
- Cold-build debt: appended `Cold-build occurrence 44`.

# Human review still needed

- Shoot holes / full-auto and confirm the sustained 2 fps is gone and no single
  shot spikes.
- Retest the cylinder (hole-side bounce) and fragments (phasing/floor).

# Not done (open)

- Deep depenetration (stuck/through-floor/player phasing); stronger fragment
  collisions; irregular-geometry contact normals; config-triangle weapon
  hitboxes; overlay word wrap.
