// 2026-11-14
/* purpose
* define what belongs in a current specification
* keep specifications static and keep live facts in generated JSON
* separate desired behavior from changelogs, status, and temporary coordination
* this file DOES NOT define game behavior
* this file DOES NOT replace the specification and behavior review skill
* this file DOES NOT require a separate status Markdown document
*/

# Specification Writing v1

Use this skill when creating or revising a project specification.

## A specification contains

- purpose and audience;
- desired behavior and user-visible outcomes;
- invariants and hard rules;
- ownership and boundaries;
- data and configuration contracts;
- failure behavior and rollback;
- acceptance evidence;
- non-goals;
- links to related specifications and generated artifacts.

## A specification does not contain

- current generation or build numbers;
- current source or file counts;
- "currently working", "still broken", or session progress;
- daily plans, next-step lists, or agent assignments;
- temporary claims or lock state;
- hand-maintained status tables.

Those belong in generated JSON, changelogs, or a temporary handoff scratchpad.

## Authority rules

1. A specification states what should exist. Code that disagrees is wrong unless
   the specification is deliberately revised.
2. Do not silently rewrite a specification to match code.
3. Append corrections when an older decision is superseded; archive old material
   rather than leaving competing current claims.
4. One concept has one specification owner.
5. Generated status is not a specification. Changelogs are not specifications.
6. A specification change must state the decision, the affected owner, and the
   acceptance consequence.

## Review

For contradictions between a specification and code, use
`docs/skills/spec-behavior-review-v1.md`. For document clarity and routing, use
`docs/skills/documentation-checker-v1.md`.
