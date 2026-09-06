// 09 06 2026, 12 00
/* purpose
* define stable feature-centered tracking for goals, evidence, and acceptance
* connect specifications, owners, tests, logs, changelogs, and regressions
* keep immutable session history separate from current feature status
* this file DOES NOT duplicate implementation history
* this file DOES NOT replace source code or specifications
* this file DOES NOT mark human acceptance automatically
*/

# Feature records

Each feature has one stable record at
`docs/features/<feature-name>/<feature-name>.md`. The record is the current
index. It links to specifications, implementation owners, tests, logs,
changelogs, regressions, and human acceptance.

Immutable session history remains under:

`docs/changelog/mm-dd-yyyy/<session-changelog>.md`

Feature tests and raw evidence belong under:

`tests/features/<feature-name>/`
`logs/features/<feature-name>/`

Do not copy full changelog history into feature records. Link to it instead.
