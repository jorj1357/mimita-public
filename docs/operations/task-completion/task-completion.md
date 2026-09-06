// 09 03 2026, 15 40
/* purpose
* define how an AI session proves its work is ready for human review
* require the right documents, focused skills, validation, and final changelog
* keep source evidence separate from visual or gameplay acceptance
* this file DOES NOT define gameplay behavior
* this file DOES NOT replace the detailed specifications
* this file DOES NOT require the deprecated overseer.py script
*/

# Task Completion

## Completion flow

1. Read `AGENTS.md` and `docs/ROUTER.md`.
2. Read the routed specification before changing behavior.
3. Read related architecture and workflow documents.
4. Inspect current code, git state, and pre-existing changes.
5. Compare code with the specification and record disagreements.
6. Make the smallest correct change.
7. Run the focused skills required by the route.
8. Run relevant tests, focused Markdown skills, checks, and the canonical build when code changed.
9. Confirm expected outputs exist.
10. Write exactly one changelog file immediately before completion.
11. Report what was proven and what still needs human review.

## Review authority

`overseer.py` is deprecated and is not required for new work. The maintained
review system is specification review, focused Markdown skills, relevant tests,
build evidence, runtime evidence, and human acceptance where required.

Use these result states: `PASS`, `PASS_WITH_HUMAN_REVIEW`, `NEEDS_SPEC_DECISION`,
`BLOCKED`, and `NOT_APPLICABLE`.

## Focused skills

Skills are the focused review documents in `docs/skills/`. Use the smallest set
that covers the task and record each exact path in the changelog. They are the
maintained, task-specific review system. Record each skill's result, findings,
unresolved warnings, and evidence still required from human review.

## Changelog requirement

Every AI session that touches the repository creates one file at:

`docs/changelog/mm-dd-yyyy/mm-dd-yyyy-three-word-summary-hh-mm-ss.md`

Write it at the end of the session. It must describe the final state and 
include the EST timestamp, branch, commits, pre-existing edits, exact files and
lines, exact old and new content, reasoning, documents, skills, validation, and
human review still needed. Use function or heading names as well as line
numbers because later edits can move line numbers.

## Regression requirement

Only confirmed behavior breaks belong in `docs/regressions/`. Human playtesting
may discover one. The AI may recommend one. Each entry must link its changelog
and include expected behavior, actual behavior, specification, wrong code,
corrected code, cause, fix, and proof. The regression record is append-only.

## Human review

Builds and focused checks prove source or executable facts. They do not prove
that the game feels correct or that a visual result is acceptable. State human
playtesting separately and never call it complete when it was not performed.
