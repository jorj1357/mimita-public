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

9 9 2026 1302 est jorj - this  todo explain how this is like
bc that relates to the thing where , writing a spec for waht u want to implement is muc much better than 
just wriitng about ti and making it up as  u go aloing with the AI 
so write out desird behavior frist and then get to implementing 
planning planning then implement
measure 10x  cut 1x 
For the 1x for the 1x 
