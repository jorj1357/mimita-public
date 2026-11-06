# Avatar atlas reused a pre-atlas vertex buffer

- Recorded: `2026-10-25T00:14:05Z`
- Status: FIXED IN SOURCE; HUMAN VISUAL REVIEW PENDING

## Observed

After the Juggernaut performance work, avatar textures appeared stretched or
smeared, most visibly on the clown avatar. The problem had been fixed before
and returned after the performance pass.

## Cause

`c1fe8acd` added a combined player-body VBO keyed only by the model path.
Avatar atlas application rewrites UVs in place. If any actor using the same
model rendered before the atlas was applied, the shared buffer kept raw GLB UVs
while the draw bound the avatar atlas texture.

Runtime evidence: `logs/10-24-2026/Gameterminal_log_162810.txt` builds the
default model combined body before the clown atlas is applied, and does not
rebuild it for clown afterward.

A second defect was found in `src/avatar/cosmetic-system.cpp`: the cosmetic VAO
bound `normal` at attribute 1 and `uv` at attribute 2, opposite to
`shaders/basic.vert`.

## Fix

- Upload identity now includes atlas UV state (`renderUploadKey()`).
- Atlas application marks the player's UV state.
- A model load resets the state to raw.
- `gAvatarCanonicalizePlayerModel` is cleared when the default model is loaded.
- Cosmetic attribute locations now match the shader.

## Evidence

`logs/10-24-2026/Gameterminal_log_194210.txt:2029` reports
`combined body model=...|authored|atlas parts=6 verts=216` after the clown
avatar load. Visual acceptance is still required.
