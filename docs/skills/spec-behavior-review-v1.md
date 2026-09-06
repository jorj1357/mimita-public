// 09 06 2026, 12 00
/* purpose
* compare specifications, implementation, tests, and observed behavior
* identify disagreements without silently changing desired behavior
* turn unclear human-authored notes into explicit decisions or wording proposals
* this skill DOES NOT rewrite specifications to match code
* this skill DOES NOT decide product intent for the human
* this skill DOES NOT replace runtime or visual acceptance
*/

# Specification and Behavior Review v1

Use this skill for behavior-changing tasks, specification work, regressions,
and reviews where written requirements may disagree with code or tests.

## Review order

1. Read the routed current specification.
2. Identify the implementation owner and exact code path.
3. Inspect focused tests, configuration, and relevant logs.
4. Compare requested, specified, implemented, and observed behavior.
5. Check related specifications for contradictions.
6. Preserve informal historical wording, but do not treat unclear wording as a
   precise requirement.

## Required finding format

```markdown
## Finding

- Severity: blocker | high | medium | low
- Type: spec-code disagreement | spec-spec conflict | unclear wording | missing acceptance
- Specification:
- Exact quoted requirement:
- Code path:
- Actual behavior:
- Expected behavior:
- Evidence:
- Recommended wording or implementation action:
- Human decision required:
```

## Informal writing rule

Human-authored informal notes remain historical context. If meaning is unclear,
quote the note, explain the ambiguity, and label the item `NEEDS_SPEC_DECISION`
or propose clearer wording. Do not silently convert shorthand into a new rule.

## Completion rule

The changelog must record this skill path, result, findings, unresolved warnings,
and whether runtime or human acceptance is still required.
