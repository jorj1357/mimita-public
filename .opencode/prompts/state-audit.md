# State Audit Prompt

Audit the codebase for duplicate state variables.

1. Search for all boolean/flag variables related to grounded state.
2. Search for all boolean/flag variables related to dash state.
3. Search for all boolean/flag variables related to jump state.
4. For each group, identify which variable is the source of truth.
5. Report competing sources of truth.

Use the duplicate-state-checker skill.
