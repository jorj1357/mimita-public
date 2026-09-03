9 3 2026
/* purpose
* review recent code for avoidable performance cost and confusing implementation patterns
* identify the exact owner, data flow, and evidence for each finding
* reduce duplicate concepts that can cause agents to invent conflicting solutions
* this skill DOES NOT replace focused profiling, tests, or human gameplay acceptance
*/

# Efficiency Checker v1

Use this skill after recent code changes or when an agent claims a performance,
clarity, or maintainability problem. Inspect the actual diff and surrounding
owners before making recommendations. Do not invent a rewrite from symptoms.

## Review priorities

### 1. Expensive work in hot paths

Look for work inside per-frame, fixed-tick, per-entity, per-body-part, or
per-projectile loops that can scale with the whole world.

Flag code that:

- scans every triangle, entity, or player when a cached, spatial, or filtered
  query already exists;
- allocates, copies, sorts, or builds temporary containers repeatedly;
- repeats the same lookup or broadphase gather for several callers;
- performs file, network, database, shader, or other blocking work on gameplay
  or render threads;
- runs collision, damage, or physics outside the required fixed tick.

For each finding, estimate the scaling: `per frame`, `per tick`, `per entity`,
and worst-case world size. Distinguish a proven cost from a possible cost.

### 2. Duplicate concepts and names

Search the changed area and its callers for variables, fields, functions, and
config keys that represent the same state. Examples include `jumpedNow`,
`jumpedThisFrame`, `jumpedThisTick`, `jumpIntent`, and `jumpFlag` all being
used for one jump decision.

Flag this when names obscure whether a value is an input, intent, transient
event, current state, or authoritative result. Recommend one canonical owner
and vocabulary, then list which names are aliases, stale remnants, or genuinely
different concepts. Do not merge names merely because they look similar.

### 3. Duplicate solutions and ownership drift

Search before proposing new helpers, state variables, caches, packet types, or
render paths. Flag recent code that solves an existing problem a second way,
routes one concept through multiple owners, or adds a workaround without
explaining why the existing owner cannot be reused.

Prefer, in order: reuse the existing function, expand it, generalize it, and
only then add a new function. Prefer deleting duplicate code when behavior is
equivalent.

## Evidence rules

For every finding, include:

1. exact file and line range;
2. the relevant code path and caller frequency;
3. observed or logically demonstrated risk;
4. the smallest corrective direction;
5. confidence: `confirmed`, `likely`, or `needs profiling`.

Never claim that a function scans the whole world unless the implementation or
call chain proves it. Never claim a variable is duplicated without tracing its
reads and writes. If evidence is incomplete, say what check would confirm it.

## Required output

Report findings in severity order:

`[P0/P1/P2/P3] title — file:line`

Then provide `Evidence`, `Why it matters`, `Smallest fix`, and `Confidence`.

Finish with `No finding` for each checked category that had no supported issue.

Do not implement changes automatically unless the user separately requests a
fix. Keep the review focused on recent work and preserve unrelated edits.
