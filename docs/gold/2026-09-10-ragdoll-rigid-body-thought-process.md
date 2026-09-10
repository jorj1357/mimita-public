// 2026-09-10T16:12:00Z
/* purpose
* preserve the working thought process that produced the physics-driven ragdoll
* record which parts of the specification and documentation actually helped
* name the human's model-choice hypothesis so it can be tested later
* this file does NOT claim complete ragdoll acceptance
* this file does NOT claim model choice alone caused the result
* this file does NOT replace the authoritative ragdoll or physics specifications
*/

# Gold thought process: making the ragdoll behave like a real physical object

- Timestamp: `2026-09-10T16:12:00Z` (2026-09-10 12:12 EDT)
- Scope: ragdoll mode physics owner, solver reuse, and specification use
- Result: the ragdoll moved from a position-only approximation to a shared
  rigid-body/constraint/contact simulation; human report at 12:12 was
  "its doing much better now, this behavior is amazing".
- Related changelog:
  `docs/changelog/2026-09-10/20260910_155338-ragdoll-mode-rigid-body-core.md`
- Related specs:
  `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`,
  `docs/specs/moving-physical-objects/moving-physical-objects.md`,
  `docs/architecture/collision/collision.md`

## The thought process

1. Classify first. The router puts movement/physics/collision tasks on the
   movement spec and the collision architecture document, and the ragdoll
   spec's own §60 says to strengthen the generalized physical-object primitives
   before writing ragdoll-specific behavior. That ordering is the whole reason
   the fix became a reusable `RigidBody` core instead of more special-case
   limb code.
2. Trace before blaming the binary. The old solver never applied torque,
   corrected only the child side of a joint, and resolved collision before
   constraints. Those three facts explain every reported symptom at once:
   capsules stuck on world Z, limbs orbiting the torso, and parts pulled into
   surfaces. Find the first missing step in the input-to-output chain.
3. Separate the owner. `RagdollModeSystem` was doing integration, joints,
   collision, grabs, camera, and rendering in one file. The fix moved the
   generic physics into `src/physics/physical-body.*` and left the ragdoll as a
   composition layer. One concept, one owner.
4. Reuse the existing world collision meaning. Instead of writing a second
   collision path, the core calls the existing `capsuleTriangleSweep`,
   `capsuleTriangleContact`, `collectCapsuleRecoveryContacts`, and
   `appendChunkTrianglesForAABB`. The fixed 60 Hz rule and the "use the cached
   broadphase" rule in the collision document are what made that safe.
5. Make limits config, not code. Mass, iteration count, damping, grab
   compliance, cone limits, and the exit hop went into the hot-reloadable
   `config/ragdoll.json` so tuning stays with the human.

## Did the full specification help, and which parts?

Yes, it helped materially. The most valuable parts, in order:

1. The informal intent notes in
   `docs/specs/ragdoll-retrograd/spec writing 9 8 2026 1102 est.md`.
   Concrete statements such as "plrOrigin is the auth root", "torso is attached
   1:1 with that", "arms and head matter, legs and torso are gravity limp",
   "0.5 m grab grace", and "preserve velocity" removed guesswork. These were the
   single most useful input.
2. The RAG-001..045 invariants, especially RAG-006 (physically simulated parts),
   RAG-008 (world collider per part), RAG-010 (self-collision), RAG-011 (no
   tunneling), and §6 "No limb should merely teleport to its target transform".
   They worked as an acceptance checklist.
3. `docs/specs/moving-physical-objects/moving-physical-objects.md` §7, §13, and
   §33-36. The vocabulary of mass, force, torque, angular momentum, constraints,
   and compliance is what produced a general solver rather than per-limb code.
4. `docs/architecture/collision/collision.md`. The fixed 60 Hz rule and the
   cached-broadphase rule directly shaped the collision call pattern.
5. `AGENTS.md` rules "one concept has one owner" and "prefer deleting code",
   which justified sharing primitives instead of duplicating them.

## What could be clearer

- The ragdoll spec is long and mixes v1 with future goals. A short "v1 in one
  page" summary at the top would shorten the read.
- The local body coordinate convention is not written down. Which local axis is
  forward, up, and side, and whether config offsets are parent-local or world,
  had to be inferred. The spec even carries a "todo explain ragdoll like
  retrograd" at line 1.
- The relationship to the moving-physical-object primitives is a file-path TODO
  at line 7 rather than an explicit list.
- Self-collision wording lists `left arm <-> torso`, but directly jointed
  capsules overlap at the joint by construction. It is unclear whether the
  intent is shrink-at-joint or skip-direct-pairs for v1.
- The intended solve order between constraints and collision is not stated.

## Model-choice hypothesis (human)

The human's hypothesis: the agent model used is a real factor. Earlier ragdoll
work used `mimo v2.5` because it cost less; this session used
`deepseek v4.1 flash`, and the result was better. The human frames this as a
cost-versus-effectiveness balance rather than a claim that a spendier model is
always correct, and says today's outcome supports the hypothesis.

Recording rules for this claim:

- It is the human's hypothesis, not a controlled result.
- The falsifiable test is to solve the same task, from the same uncommitted
  starting point, with the other model and compare the resulting behavior and
  evidence.
- This session's observable support: one coherent diagnosis tied all reported
  symptoms to three concrete solver defects, the change was made in the shared
  owner, and it built clean on the first attempt.

## What helped me the most

The single most useful thing was the human's own written intent, because it
turned "make it real" into testable statements. After that, the generalized
physical-object vocabulary and the collision architecture rules. The model
matters for how quickly those constraints are connected, but the constraints
themselves came from the repository.

## Still unverified

- Visual and gameplay acceptance of the ragdoll.
- Collision solidity at the extremes (still under active fixes).
- Multiplayer, replay, and damage-hurtbox separation.
