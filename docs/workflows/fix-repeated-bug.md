# Workflow: Fix a Repeated or Ambiguous Bug

Use this for reports such as “damage numbers do not show up.” The report does
not need to be precise before starting.

## The agent's job

1. Read `docs/regressions/regressions-v1.md` before investigating.
2. Search that file for the same symptom or a related failure.
3. Follow the route in `docs/ROUTER.md`.
4. Read the relevant spec. Treat it as the intended behavior.
5. Find the existing owner in the code. Reuse it; do not create another copy
   of the same state or behavior.
6. Trace the chain: event happens -> data is created -> data is passed -> data
   is rendered or saved.
7. Identify the first missing or incorrect step and record the exact file,
   function, and line.
8. Make the smallest fix. Keep presentation settings in the existing JSON
   owner when the spec says they are configurable or hot-reloadable.
9. Add or improve a focused diagnostic at the owner if the failure would be
   hard to see next time. Rate-limit repeated messages.
10. Test the reported behavior and its nearby failure cases. For damage
    numbers, verify that a hit creates the number, the UI receives it, it is
    rendered, and its JSON settings still control it.
11. If code changed, build the canonical `mimita.exe` and check the build
    status. Run the required repository validation.
12. If this was a regression, append a new entry to
    `docs/regressions/regressions-v1.md`. Do not delete or rewrite old entries.
13. Report exactly what changed, whether the regression file was appended, and
    what was proven by tests/build versus what still needs human playtesting.

## State-name rule

Before adding a state variable, search for an existing variable that represents
the same idea. Prefer one clearly named source of truth over copies such as
`jumpStarted`, `jumpedLater`, `jumpedThisFrame`, and `jumpedThisTick`. If the
timing distinction is real, document the distinction and use names that state
which clock or event owns each value.

