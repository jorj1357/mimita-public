# Brute v3 rotation diagnosis

## Scope

Updated `docs/specs/avatar/avatar-editor.md` with the evidence-based diagnosis
for `mimita-brute-v3.glb` rotating around the wrong axis.

## Findings

The GLB contains a `-90` degree X root rotation, arm meshes whose long bounds
are on local Z, two independent arm-segment nodes per side with Blender
duplicate suffixes, and no leg nodes. It therefore does not satisfy the current
canonical six-part player rig. A runtime quaternion can be valid while still
being applied to the wrong semantic segment, local basis, or pivot.

The active `monsterbrute` avatar manifest points to `mimita-brute-v2.glb`, so a
test of v3 must explicitly load v3; observing `monsterbrute` alone does not
prove v3 was active.

## Documentation change

The spec now explains why Boomer v21 works, why Euler-order changes cannot fix
Brute, and why the correct future fix is complete player-only normalization or
an explicit two-segment arm rig—not another hard-coded quaternion.

## Evidence and limits

The GLB JSON and vertex accessors were inspected directly. No code or asset was
changed, and no runtime build was required for this documentation-only update.

