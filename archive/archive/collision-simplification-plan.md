# Collision Pipeline Analysis and Simplification Plan

## 1. Entry Point

```
physics-mini.cpp → doCollisions() → doGLBTriangleCollisions() (for GLB worlds)
                  physics-collision-dispatch.cpp:89
                  OR
                  block-based legacy path (non-GLB worlds)
```

### `doCollisions()` (dispatch.cpp:89)
- Saves frameStart, calls `recoverInvalidPlayerCollisionState`
- If world has collision mesh: calls `doGLBTriangleCollisions()` then returns immediately
- Otherwise: runs the legacy block-based collision (AABB sweep + depenetration + snap)

---

## 2. Full Pipeline Order (GLB path)

Every phase within `doGLBTriangleCollisions()` (glb-main.cpp:32), called from `doCollisions()`:

```
                    doGLBTriangleCollisions()
                           │
  ┌────────────────────────┼────────────────────────────┐
  │  A. doGLBSweepSlide   │  Main movement              │
  │     (5 iterations)     │  sweep-slide.cpp:30         │
  │                        │                             │
  │  B. Batched Depenetration  Capsule position fix      │
  │     (4 iterations)     │  glb-main.cpp:59            │
  │                        │                             │
  │  C. Resweep Remaining  │  Leftover movement          │
  │     (1 pass)           │  glb-main.cpp:105           │
  │                        │                             │
  │  D. doGroundSnap       │  Snap to floor              │
  │                        │  glb-safety.cpp:18          │
  │                        │                             │
  │  E. doFloorRecovery    │  Lift out of floor          │
  │                        │  glb-safety.cpp:119         │
  │                        │                             │
  │  F. doBodyWeaponCollision  Limbs + weapons           │
  │     (3 passes)         │  glb-body.cpp:194           │
  │                        │                             │
  │  G. Emergency Stuck    │  Radial search escape       │
  │                        │  glb-main.cpp:167           │
  │                        │                             │
  │  H. Final Velocity     │  Non-walkable contacts      │
  │     Projection         │  glb-main.cpp:262           │
  │                        │                             │
  │  I. Stuck Position     │  Detection + logging        │
  │     Tracking           │  glb-main.cpp:290           │
  │                        │                             │
  │  J. doRotationSafety   │  solveBatchedCorrection     │
  │                        │  glb-safety.cpp:161         │
  │                        │                             │
  │  K. doFinalSafety      │  solveBatchedCorrection     │
  │                        │  glb-safety.cpp:215         │
  └────────────────────────┴────────────────────────────┘
```

### Phase-by-phase analysis

---

### Phase A: `doGLBSweepSlide` — Main Movement (sweep-slide.cpp:30)

**Why it exists:** This is THE primary movement phase. It sweeps the player capsule through the world, finds the earliest collision, slides along surfaces.

**What it does:**
1. Gather candidates with `gatherGLBTriangles(cap, totalMove)` plus body sample extras
2. Up to 5 iterations:
   - `capsuleTriangleSweep` on all candidates → earliest hit + TOI group
   - Move player to hit: `p.pos += stepMove`
   - **Step-up**: for wall-like normals (|z| < 0.2), try to lift player
   - **Seam transition**: for non-walkable hits near feet, scan for walkable normal
   - **Depenetration**: `p.pos += depen * SURFACE_SLOP`, z zeroed for 0<z<0.7
   - **Projection**: `remainingMove` projected against all hit normals
   - **8-pass slide solver**: additional projection against recovery contacts
   - `applyCollisionContact` for each hit

**State modified:** `p.pos`, `p.vel.z`, `groundedThisFrame`, `remainingMove`

**Assumptions:**
- The earliest hit is the most important one
- Step-up should only happen for near-vertical normals (|z| < 0.2)
- Wall depenetration should be purely horizontal (z zeroed)
- 5 iterations are enough to resolve movement

**Historical patches found in this file:**
- Step-up consistency check (requires 3/5 samples) — added to prevent stepping into gaps
- Seam transition walkable override — added as my patch for flat→slope
- `depen.z` zeroing — prevents walls from pushing player upward
- 8-pass slide solver — ensures smooth sliding along multiple surfaces

**Can another phase do this?** No — this is the primary movement phase and must exist.

---

### Phase B: Batched Depenetration (glb-main.cpp:59, 4 iterations)

**Why it exists:** After the sweep-slide has moved the player, the capsule may still be penetrating geometry (sweep only resolves along the movement direction, not sideways). This phase pushes the capsule out of any remaining penetration.

**What it does:**
1. Gather candidates at current position: `gatherGLBTriangles(cap, {0,0,0})`
2. `collectCapsuleRecoveryContacts` → sphere-triangle for all candidates
3. `solveBatchedCorrection` → Gauss-Seidel solver with 6 passes, 0.8 relaxation
4. `p.pos += correction`, clamped to ±0.5 per axis
5. `applyCollisionContact` for each contact

**State modified:** `p.pos`, `groundedThisFrame`

**Assumptions:**
- Penetration is small (sweep already moved the player)
- 4 iterations are enough to converge
- The solver handles opposing normals correctly

**Historical patches:** Likely added to fix capsule snagging on edges after the sweep resolved movement

**Can another phase do this?** Phase J (rotation safety) and Phase K (final safety) do the same thing. This is the primary depenetration phase.

---

### Phase C: Resweep Remaining (glb-main.cpp:105)

**Why it exists:** If `remainingMove` is non-zero after the sweep-slide (movement was partially blocked), this phase tries to apply it. It's a single capsule sweep that either slides along a surface or applies the full remaining movement.

**What it does:**
1. If `remainingMove` > 0.001: gather candidates with `remainingMove`
2. Single sweep → if hit: slide along, else: add full remaining move
3. `applyCollisionContact`

**State modified:** `p.pos`, `groundedThisFrame`, `remainingMove` (set to 0)

**Can another phase do this?** This is a continuation of Phase A. The 5-iteration limit in Phase A might have been hit. This is a "last chance" to apply leftover movement.

**Historical context:** Added because the 5-iteration loop in Phase A could exit early with remainingMove > 0.

---

### Phase D: `doGroundSnap` — Ground Snap (glb-safety.cpp:18)

**Why it exists:** When the player is slightly above the floor (within 0.25 units), snap them down to prevent floating. This is essential for grounded gameplay — without it, the player would hover slightly above surfaces due to floating-point imprecision and the COLLISION_SKIN.

**What it does:**
1. If `vel.z <= 1.0` (not jumping upward): gather candidates with downward expansion
2. Scan for walkable surface below feet
3. If within snap distance: `p.pos.z -= snapAmount` and `vel.z = 0` if negative
4. Post-snap: `solveBatchedCorrection` + `applyCollisionContact`

**State modified:** `p.pos.z`, `p.vel.z`, `groundedThisFrame`

**Assumptions:**
- Player should only snap if not moving upward (z velocity <= 1.0)
- Snap distance is 0.25 units
- Only walkable normals (z >= 0.80) count as ground

**Can another phase do this?** Phase E (floor recovery) does a similar thing but LIFTS the player. They're complementary.

---

### Phase E: `doFloorRecovery` — Floor Recovery (glb-safety.cpp:119)

**Why it exists:** If the capsule is below a walkable surface (e.g., fell through a crack), lift them back up. This is a safety net for rare cases where the player ends up inside the floor geometry.

**What it does:**
1. Gather static candidates
2. Find highest walkable surface near the player's horizontal center
3. If capsule bottom is below this surface: lift `p.pos.z`

**State modified:** `p.pos.z`, `p.vel.z`, `p.externalImpulse.z`, `groundedThisFrame`

**Can another phase do this?** Similar to Phase D but lifts instead of snaps. Both could be merged.

---

### Phase F: `doBodyWeaponCollisionPhase` — Body + Weapon Collision (glb-body.cpp:194)

**Why it exists:** Body parts (limbs, head, torso) and equipped weapons should collide with the world and push the player root. This phase generates sphere contacts from body part transforms and weapon collider configs, then feeds them into the same solver.

**What it does:**
1. Up to 3 passes:
   - `updateModelWorldTransforms()`
   - `recomputeWeaponCapsule()`
   - Collect BodyWeaponSpheres (body parts + weapon capsule + config colliders)
   - Sphere-triangle contact detection
   - Separate contacts: walkable, bodyPush, weaponPush
   - Walkable → `applyCollisionContact` + weapon support grounding
   - BodyPush + root capsule → `solveBatchedCorrection`
   - Velocity projection

**State modified:** `p.pos`, `p.vel`, `groundedThisFrame`, ability resets

**Can another phase do this?** No — body and weapon collision are separate from capsule collision and must be processed separately. But the solver used is the same.

**Historical context:** Body collision was originally done in the sweep-slide (via body samples passed to candidate gathering). Weapon collision was added as a separate system. They were merged into this phase.

---

### Phase G: Emergency Stuck (glb-main.cpp:167)

**Why it exists:** If the capsule is deeply penetrating geometry (> 0.05) for 3+ frames, search radially for a free position and teleport the player there.

**What it does:**
1. Check capsule penetration
2. If stuck for 3+ frames: test 13 directions at 0.05 increments
3. Find position with lowest penetration
4. If found: teleport and zero all velocity

**State modified:** `p.pos` (teleported), `p.vel` (zeroed), `p.collision.stuckFrames`

**Can another phase do this?** No. This is a last-resort escape mechanism. It should never fire during normal gameplay.

**DANGEROUS:** Teleports the player and zeroes all velocity. Should only fire when truly stuck.

---

### Phase H: Final Velocity Projection (glb-main.cpp:262)

**Why it exists:** After all position corrections, project velocity against non-walkable contacts to prevent sliding into walls.

**What it does:**
1. Gather static candidates
2. For each non-walkable contact (z <= 0.80): `projectVelocityAgainstNormal`

**State modified:** `p.vel`, `p.externalImpulse`

**Can another phase do this?** Phase A already does this in the slide solver at the end of each sweep iteration. Phase B does it via `applyCollisionContact`. This is the THIRD time velocity is projected.

**OVERLAP:** This is redundant with the projection in Phase A and Phase B's `applyCollisionContact`.

---

### Phase I: Stuck Position Tracking (glb-main.cpp:290)

**Why it exists:** Detect when the player is trying to move but not actually going anywhere (position change < 0.005 despite velocity > 0.01). Logs for debugging.

**What it does:** Detection only — no state modification.

**Can another phase do this?** This is only logging. It doesn't affect gameplay.

---

### Phase J: `doRotationSafetyPass` (glb-safety.cpp:161)

**Why it exists:** After all movement and collision, check if the capsule is penetrating geometry (presumably from rotation changing the capsule's footprint) and push out.

**What it does:**
1. Gather static candidates
2. `collectCapsuleRecoveryContacts`
3. If contacts exist: `solveBatchedCorrection` → `p.pos += correction`
4. `applyCollisionContact` for each

**State modified:** `p.pos`, `groundedThisFrame`

**OVERLAP with Phase B:** Does the EXACT same thing as the batched depenetration (Phase B) — gather + collect + solve + apply. The only difference is this runs later.

**Historical context:** Added to handle capsule penetration caused by player rotation changing the capsule's world-space footprint.

---

### Phase K: `doFinalSafetyPass` (glb-safety.cpp:215)

**Why it exists:** One final depenetration pass to catch any remaining penetration. Only runs if penetration > 0.01 (COLLISION_SKIN * 0.5).

**What it does:**
1. Gather static candidates
2. `collectCapsuleRecoveryContacts`
3. If `maxPen > 0.01`: `solveBatchedCorrection` → `p.pos += correction`
4. Velocity projection for non-walkable contacts

**State modified:** `p.pos`, `p.vel`

**OVERLAP with Phase B and Phase J:** This is the THIRD depenetration pass using the exact same algorithm.

**Could potentially be removed** — if Phase B and Phase J converge, this should never fire.

---

## 3. DUPLICATE `gatherGLBTriangles` CALLS

| Location | Context | Move Vector |
|----------|---------|-------------|
| sweep-slide.cpp:47 | Initial gather | `totalMove` |
| sweep-slide.cpp:94 | Per-iteration sweep | `remainingMove` |
| glb-main.cpp:63 | Batched depen (4x) | `{0,0,0}` |
| glb-main.cpp:111 | Resweep | `curMove` |
| glb-safety.cpp:30 | Ground snap | `{0,0,-0.25}` |
| glb-safety.cpp:89 | Post-snap correction | `{0,0,0}` |
| glb-body.cpp:~38 | Body-weapon (3x passes) | `{0,0,0}` with `appendChunkTrianglesForAABB` |
| glb-main.cpp:173 | Emergency stuck | `{0,0,0}` |
| glb-main.cpp:264 | Final velocity projection | `{0,0,0}` |
| glb-safety.cpp:167 | Rotation safety | `{0,0,0}` |
| glb-safety.cpp:222 | Final safety | `{0,0,0}` |

**estimated:** 15-20+ calls per frame.

---

## 4. DUPLICATE `solveBatchedCorrection` CALLS

| Location | Context |
|----------|---------|
| glb-main.cpp:74 | Batched depen (4x max) |
| glb-safety.cpp:97 | Post-snap correction |
| glb-body.cpp:98 | Body-weapon pass |
| glb-safety.cpp:197 | Rotation safety |
| glb-safety.cpp:242 | Final safety |

**estimated:** 7-10 calls per frame.

---

## 5. DUPLICATE `applyCollisionContact` CALLS

| Location | Context |
|----------|---------|
| sweep-slide.cpp:365 | After every sweep iteration |
| glb-main.cpp:86 | After every depen iteration (4x max) |
| glb-main.cpp:139 | Resweep hit |
| glb-safety.cpp:106 | Post-snap correction |
| glb-body.cpp:78+98 | Body-weapon pass |
| glb-safety.cpp:209 | Rotation safety |

**estimated:** 10-15 calls per frame.

---

## 6. IDEAL PIPELINE

Based on first principles (not current code):

```
┌─────────────────────────────────────────────┐
│            ONE FRAME OF COLLISION           │
│                                             │
│  1. GATHER CANDIDATES (once per frame)      │
│     └─ Gather all triangles near player      │
│        (capsule AABB + margin)               │
│                                             │
│  2. COMPUTE MOVE VECTOR                     │
│     └─ totalMove = (vel + impulse) * dt     │
│                                             │
│  3. SWEEP + DEPENETRATE (unified loop)      │
│     └─ Sweep capsule along remainingMove     │
│     └─ Move to earliest hit                  │
│     └─ Project remainingMove against         │
│        hit normals (using walkable normal    │
│        if hit is a seam/backface)            │
│     └─ If stuck with remainingMove > 0:      │
│        solveBatchedCorrection on current     │
│        position contacts                     │
│     └─ Repeat until remainingMove ~0         │
│        or iteration limit                     │
│                                             │
│  4. GROUND SNAP                              │
│     └─ Find walkable surface below feet      │
│     └─ Snap down if within distance          │
│                                             │
│  5. FINAL DEPENETRATION (one pass)          │
│     └─ Check capsule penetration             │
│     └─ solveBatchedCorrection if needed      │
│     └─ projectVelocityAgainstNormal for      │
│        non-walkable contacts                 │
│                                             │
│  6. BODY/WEAPON COLLISION                   │
│     └─ Generate body+weapon spheres          │
│     └─ Sphere-triangle contacts              │
│     └─ solveBatchedCorrection with root      │
│     └─ Apply grounding / support             │
│                                             │
│  7. STUCK DETECTION + RECOVERY              │
│     └─ If deeply stuck for 3+ frames:       │
│        radial search for escape              │
│                                             │
└─────────────────────────────────────────────┘
```

### Key simplifications over current code:

1. **One gather per frame** (not 15+). The same triangle set can be reused for sweep and all depenetration passes. The sweep needs different gather geometry (capsule expanded by move), but depenetration and ground snap can share a single static gather.

2. **Merge sweep and depenetration into one loop.** Current code has 5 sweep iterations in Phase A + 4 depenetration iterations in Phase B + 1 resweep in Phase C. These can be one unified loop: sweep until hit, then depenetrate the new position, then sweep again.

3. **Remove duplicate safety passes (J, K).** If the unified sweep+depenetration loop converges (remainingMove ~0 and penetration ~0), there's nothing for the safety passes to do. They should be removed or converted to assertions.

4. **Remove the separate projection phases (H).** Velocity projection against non-walkable contacts should happen inside `applyCollisionContact` which already does it. The standalone projection in Phase H is redundant.

5. **Merge ground snap + floor recovery (D, E).** Both look for walkable surfaces near the player's feet. Ground snap pushes down, floor recovery lifts up. They share the same candidate gathering and classification logic.

---

## 7. CONCRETE DELETE PLAN

### Files to keep (with rationale):

| File | Kept because |
|------|-------------|
| `physics-collision-glb-sweep-slide.cpp` | Core sweep logic must exist |
| `physics-collision-glb-contact.cpp` | Sphere-triangle contact primitives |
| `physics-collision-glb-sweep.cpp` | Sweep primitives (capsuleTriangleSweep) |
| `physics-collision-glb-setup.cpp` | Candidate gathering (`gatherGLBTriangles`) |
| `physics-collision-core.cpp` | `solveBatchedCorrection`, `applyCollisionContact`, `applyTouchResets` |
| `physics-collision-glb-body.cpp` | Body + weapon collision (separate logic) |
| `physics-collision-glb-safety.cpp` | Ground snap + floor recovery (keep, but merge) |
| `physics-collision-glb-main.cpp` | Orchestrator |

### Files to merge or delete:

| File | Action | Reason |
|------|--------|--------|
| `physics-collision-glb-slide.cpp` | DELETE | Duplicate of sweep-slide.cpp |
| `physics-collision-glb-depen.cpp` | DELETE | Logic is inline in glb-main.cpp already |
| `physics-collision-glb-recovery.cpp` | DELETE | Duplicate of glb-safety.cpp |
| `physics-collision-glb-phases.cpp` | DELETE | Merged safety + stuck + overlap — use glb-main.cpp instead |
| `physics-collision-glb.cpp` | DELETE (1 function: `gatherGLBTrianglesForSphere` — merge into setup.cpp) |

### Phases to remove:

| Phase | Reason |
|-------|--------|
| Resweep remaining (C) | Merge into unified sweep+depen loop |
| Final velocity projection (H) | Redundant — `applyCollisionContact` already handles this |
| Rotation safety (J) | Depenetration already runs after all transforms |
| Final safety (K) | Same as J — pure duplicate |
| Emergency stuck (G) | Keep but simplify — remove velocity zeroing |
| Overlap warning | Detection only, keep |

---

## 8. IMPLEMENTATION ORDER

### Step 1: Merge safety passes
- Move `doGroundSnap` and `doFloorRecovery` into a single `glbSafetyPhase` function
- Remove `doRotationSafetyPass` and `doFinalSafetyPass`
- Move their logic into the main depenetration loop (if penetration persists after 4 iterations, it's a bug, not something a safety pass can fix)

### Step 2: Remove duplicate files
- Delete `physics-collision-glb-slide.cpp` (unused)
- Delete `physics-collision-glb-depen.cpp` (logic already inline in main)
- Delete `physics-collision-glb-recovery.cpp` (duplicate of safety)
- Delete `physics-collision-glb-phases.cpp` (merged version with wrong constants)
- Move `gatherGLBTrianglesForSphere` into setup.cpp

### Step 3: Unify sweep + depenetration loop
- Replace the current 5-iteration sweep + 4-iteration depen + resweep with a single loop
- Each iteration: sweep, slide, depenetrate, repeat
- Keep the ground snap and body-weapon phases separate

### Step 4: Reduce gather calls
- Cache the static-position gather once and reuse it for all depenetration passes
- The sweep still needs its own gather (with move vector)

### Step 5: Simplify contact classification
- Extract one function that takes a normal and returns: walkable/wall/ceiling/backface
- Replace all scattered `normal.z < MAX_WALKABLE_SLOPE_DOT` checks with calls to this function
