```md
# Regression Records

Use this folder only for confirmed behavior regressions discovered through human review or playtesting.

Normal AI work history belongs in:

`docs/changelog/`

Do not use regression records as a general changelog.

---

## Directory Structure

Regression records are organized by the ISO 8601 calendar date on which the regression was first recorded.

Format:

```text
docs/regressions/
└── YYYY-MM-DD/
    └── three-word-summary-REG.md
```

Example:

```text
docs/regressions/
└── 2026-09-09/
    └── spawn-velocity-feature-REG.md
```

Rules:

- Create a new `YYYY-MM-DD` directory when needed.
- Do not append unrelated regressions to one shared regression document.
- Each independently tracked regression gets its own `*-REG.md` file.
- The filename should be a short, descriptive, approximately three-word summary.
- Use lowercase kebab-case.
- A regression file is not append-only. Existing text may be corrected, reorganized, expanded, or deleted when appropriate.
- Repeated failures of the same underlying regression stay in the same regression file.

---

## Time Format

All timestamps must follow:

`docs/architecture/time-and-formatting/time-and-formatting.md`

Every regression file must contain:

```text
Time created: <ISO 8601 timestamp>
Time last updated: <ISO 8601 timestamp>
```

Example:

```text
Time created: 2026-09-09T07:33:21Z
Time last updated: 2026-09-09T07:41:53Z
```

`Time created` never changes.

`Time last updated` must be updated whenever the regression file is materially changed.

---

## Regression Status

The current regression status must appear near the top of the file.

Allowed states:

### `UNRESOLVED`

Use when the regression is currently reproducible and no confirmed solution exists.

Example:

```text
Status: UNRESOLVED
```

### `ATTEMPTED FIX (n)`

Use after a fix has been attempted but has not yet met the requirements for a confirmed solution.

Increment `n` for each distinct attempted fix.

Example:

```text
Status: ATTEMPTED FIX (1)
```

Then:

```text
Status: ATTEMPTED FIX (2)
```

An attempted fix does not count as a solution merely because code was changed or automated tests passed.

### `SOLUTION AS OF <timestamp>`

Use only after the behavior has been human reviewed and confirmed working at least once.

Example:

```text
Status: SOLUTION AS OF 2026-09-09T11:41:53Z
```

This status records when the current known-good solution was confirmed.

---

## Regression Lifecycle

A regression file persists for the lifetime of that specific problem.

Do not create a new regression file merely because a previously solved regression breaks again.

If the same regression returns:

1. Keep the existing regression file.
2. Record that the previous solution was known to work.
3. Record when and how the behavior regressed again.
4. Add a new regression entry.
5. Continue numbering attempted fixes for the new occurrence.
6. Record a new `SOLUTION AS OF <timestamp>` when the new occurrence is human-confirmed fixed.

A single regression document may therefore contain multiple confirmed solutions over time.

Example lifecycle:

```text
Regression occurrence 1
→ ATTEMPTED FIX (1)
→ ATTEMPTED FIX (2)
→ SOLUTION AS OF 2026-09-09T11:41:53Z

Regression occurrence 2
→ behavior broke again after a later change
→ ATTEMPTED FIX (1)
→ SOLUTION AS OF 2026-09-14T18:22:10Z
```

Attempt numbers are scoped to each regression occurrence.

---

## Required Regression Information

Every regression occurrence must document:

- observed date and time;
- expected behavior;
- actual behavior;
- why the actual behavior is bad;
- exact specification used;
- exact wrong code;
- exact corrected code;
- confirmed cause;
- fix;
- proof; and
- related changelog file.

Do not claim a cause is confirmed unless there is evidence establishing that cause.

Do not claim a solution is confirmed until it has been human reviewed and observed working at least once.

---

## Recommended File Format

```md
# <Regression Name>

Time created: <ISO 8601 timestamp>
Time last updated: <ISO 8601 timestamp>

Status: UNRESOLVED

Related specification:
`<exact specification path>`

Related changelog:
`<exact changelog path>`

---

## Regression Occurrence 1

### Observed

Time:
`<ISO 8601 timestamp>`

### Expected Behavior

<Exact expected behavior.>

### Actual Behavior

<Exact observed behavior.>

### Why This Is Bad

<Why this violates the intended behavior or creates a user/system problem.>

### Specification

`<exact specification path>`

Relevant requirement:

<Exact requirement or concise reference to it.>

### Wrong Code

File:

`<exact source path>`

```<language>
<exact wrong code>
```

### Confirmed Cause

<Confirmed technical cause.>

Evidence:

<Evidence proving this cause.>

### Attempted Fix 1

Time:
`<ISO 8601 timestamp>`

Change:

<What was changed and why.>

Result:

<What happened after the attempt.>

### Corrected Code

File:

`<exact source path>`

```<language>
<exact corrected code>
```

### Fix

<Final technical explanation of the fix.>

### Proof

Human review:

<What a human tested or observed.>

Automated proof:

<Tests, commands, logs, assertions, recordings, or other evidence.>

### Solution

`SOLUTION AS OF <ISO 8601 timestamp>`

<Concise description of the confirmed working solution.>

---

## Regression Occurrence 2

Add this section only if the same regression returns after a previously confirmed solution.

### Previous Solution

Previously confirmed:

`SOLUTION AS OF <ISO 8601 timestamp>`

The previous solution was known to work until:

`<event/change/time if known>`

### Observed

...

### Expected Behavior

...

### Actual Behavior

...

### Confirmed Cause

...

### Attempted Fix 1

...

### Solution

`SOLUTION AS OF <new ISO 8601 timestamp>`
```

---

## Core Rule

Regression records answer:

> What behavior was confirmed broken, why was it broken, what exact change fixed it, and what evidence proves that the fix worked?

Changelog records answer:

> What work was performed?

Keep those two purposes separate.
```