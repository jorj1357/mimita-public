// 2026-11-20
/* purpose
* check a self-improvement or capability proposal for evidence and safe boundaries
* this skill DOES NOT grant authority to change policy or production
* this skill DOES NOT replace human review
*/

# Self-improvement v1

For an `improvement` or `capability` issue, check:

- the observed friction or missing capability is stated;
- the exact evidence (build ID, timing, run, journal, path/line) is linked;
- the human assumption being challenged is explicit;
- the proposed change and its expected metric effect are measurable;
- the smallest safe experiment and rollback are described;
- risks, dependencies, owner, and reviewers are recorded;
- the proposal does not silently change `AGENTS.md`, build authority, specs,
  production, or packet/gameplay behavior;
- evidence for and against has been collected by at least one other agent where
  practical;
- human approval is recorded before closing a change to shared authority;
- the result is measured and the outcome is appended to the issue doc.

Report the verdict, missing evidence, and the exact human decision still needed.
