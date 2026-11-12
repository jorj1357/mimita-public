# Plan whole-game DLL ownership cutover

Updated the hot reload migration plan to match the requested whole-game scope: trace all eligible sources first, then switch ordinary game-source ownership to one `game.dll` in a single reviewed cutover. The NPC path is retained as a loader/continuity acceptance probe, not the final migration scope.

- Clarified that the all-file scan is only a static triage map; semantic state, callback/job quiescence, external resource, ABI, and safe-unload review must be completed per eligible file/dependency.
- Changed Phase 2 from a gradual source ownership migration to a complete one-owner manifest and atomic target ownership cutover after preparatory runtime/loader work and proof.
- Added explicit CSV fields for semantic lifetime status, persistent-state owner, callback/job quiescence, external-resource lifetime, unload evidence, and cutover group. Current entries are marked not reviewed/unproven.
- Read and followed `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and the routed hot-reload/build-authority documents.

Validation: regenerated the CSV from the existing source inventory, confirmed the Python scanner parses, and checked inventory/document consistency. No build, test, compiler, or game launch was run. The full semantic trace and DLL cutover remain unimplemented; current source ownership and runtime behavior are unchanged.
