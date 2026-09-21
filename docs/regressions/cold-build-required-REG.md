# Cold Build Required

Time created: 2026-09-20T15:11:13Z
Time last updated: 2026-09-20T15:11:13Z

Status: COLD-BUILD DEBT

This persistent record tracks every intentional cold build and the work needed
to move that boundary into the live-reload path. A cold build is not by itself
a user-visible behavior regression.

Related specification:
`docs/features/live-code-development/live-code-development.md`

Related workflow:
`docs/operations/build-and-exe/build-and-exe.md`

Related changelog:
`docs/changelog/2026-09-20/`

---

## Cold-build occurrence template

Copy this section for each intentional cold build. Keep all occurrences in this
file; do not create a new file for the next cold build.

### Cold-build occurrence N

Time:
`<UTC ISO 8601 timestamp>`

Related changelog:
`<exact changelog path>`

Reason the cold build was required:

<What result the session needed and why a live build was insufficient.>

Exact cold owner or boundary:

File/function/ABI/runtime mechanism:
`<exact path and owner>`

Why this is still cold:

<Evidence that the changed behavior cannot currently cross the live boundary.>

Result needed from the cold build:

<What the new executable was needed to prove or enable.>

Hot-boundary change needed:

<Smallest code or architecture change that would make this work live-reloadable.>

Migration/falsification step:

<Next test that could prove the cold dependency is removable.>

Build result:

`<SUCCESS | BLOCKED | NOT RUN>`

Human review:

<What remains unverified by source/build evidence.>
