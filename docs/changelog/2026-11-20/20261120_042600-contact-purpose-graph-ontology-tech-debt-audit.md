// 2026-11-20
/* purpose
* record the spec-only unified contact contract, nodes/connections ontology, and tech-debt audit
* preserve the boundary that no networking migration was implemented
* this file DOES NOT authorize packet, ABI, or gameplay changes
*/

# Contact purpose contract, nodes/connections ontology, and tech-debt audit

# Task

- Task: graph-coordination
- Issues: issue-93bd29a36d24, issue-75220121d993, issue-49ded3edbe47,
  issue-7d3dcfd7433b, issue-bbc291c180f8
- Date: 2026-11-20T04:26:00+00:00
- Status: source/spec only; no networking or gameplay code changed

# Requested behavior

Unify all weapon contacts behind purpose/damage flags in specification:
collision vs damage vs trigger/presentation, then the damage family. Do not
migrate networking code yet. Expand the project model to nodes and connections,
where space, time, 3D, code, and agent context are projections. Add a recurring
duplicate/hardcoding/weird-name/unused-file/workflow audit that leaves findings
for review.

# Contact authority

`docs/specs/networking/contact-authority.md` now defines:

- `ContactPurpose` (`Collision`, `Damage`, `Trigger`, `Presentation`);
- `ContactKind` (`Hitscan`, `Projectile`, `Melee`, `Explosion`, `Deathball`,
  `WeaponClash`, `Environment`);
- `DamageKind` (`None`, `Hitscan`, `MeleePhysical`, `ProjectileDirect`,
  `Explosion`, `Deathball`, `Environment`);
- one claim queue, one wire batch, one server resolver, one damage sink;
- legacy adapters for SpyKnife, Godball, NPC-damage, melee, hitscan, projectile,
  and explosion paths;
- a spec-only migration sequence requiring human pre-review.

No packet layout, handler, resolver, damage, or gameplay code was changed.

# Nodes and connections

Added `docs/specs/architecture/nodes-and-connections.md`:

- canonical nodes, typed connections, attributes, provenance, and versions;
- no inherent space/time; space and time are projections;
- 3D is a projection of spatial nodes/edges, not a second world;
- live projection switching is read-only and must preserve identity;
- code/dependency/build/test structure can be projected as a graph;
- agent context is a bounded subgraph with evidence/confidence;
- C++/graph-native experiments are phased research, not an engine rewrite;
- safety rules prevent a second source of truth and silent view mutations.

Added the route in `docs/ROUTER.md` and the generic-owner/tech-debt gate in
`AGENTS.md`.

# Tech-debt audit

Added `devscripts/tech-debt-audit.py`:

- reads source/scripts/docs; reports candidates only;
- detects special contact paths/packets, direct health writes, weapon-name
  branches in shared paths, stale TODOs, and large hand-maintained files;
- does not edit code, create issues, or delete files.

Initial scan: 1,537 files, 122 candidates:
- 38 special contact paths;
- 14 direct health writes (review host/serialization exemptions);
- 4 weapon-name branches;
- 63 stale TODOs;
- 3 large files.

Added the scanner reference and generic-owner review to
`docs/specs/dev-loop/agent-self-improvement.md`.

# Launch follow-up

No launch code was changed in this task. Sent the playable definition and stage
requirements to the active `launch-feedback` agent:
connected + mode active + valid local player/spawn generation + accepted
movement input, with per-stage p50/p95 and the current 60s
`server.local-gameplay-ready` timeout measured before changing the timeout.

# Validation

- `python -m py_compile devscripts/tech-debt-audit.py` passed.
- Scanner executed read-only and produced the candidates above.
- No networking build or runtime proof was claimed.
- No 99.99% contact claim was made.

# Next

- Review issue-93bd29a36d24 and approve/reject the contact migration design.
- Turn high-value scanner candidates into focused tech-debt issues with owners.
- Let `launch-feedback` produce the launch timeline and first-divergence report.
- Expand the graph spec only after review; do not implement a graph engine from
  this changelog.

# Reverse

- Revert the touched spec/AGENTS/ROUTER paths; remove
  `devscripts/tech-debt-audit.py` and this changelog if not wanted.
