// 2026-11-20
/* purpose
* check an issue record for preserved human evidence, ownership, steps, and proof
* this skill DOES NOT rewrite the original report
* this skill DOES NOT decide gameplay behavior
*/

# Issue triage v1

Check the issue record and its append-only document.

- Does the document contain the human report verbatim and unchanged?
- Is the source, path/line, date, and reporting agent recorded?
- Is the issue state valid (`new`, `triaged`, `assigned`, `active`,
  `in_progress`, `blocked`, `deferred`, `needs_clarification`, `in_review`,
  `done`, `cancelled`)?
- Is there one owner, and is every step a task with an owner or an explicit
  deferral?
- Are dependencies acyclic and visible?
- Is the acceptance evidence appropriate for the change class?
- Does a TODO or ambiguous report have a clarification question instead of a
  silent edit?
- Are reviews and human-review requirements recorded?
- Is the index generated from the brain rather than hand-maintained?

Report exact issue IDs, paths, and missing evidence. Do not resolve an issue by
rewriting the specification.
