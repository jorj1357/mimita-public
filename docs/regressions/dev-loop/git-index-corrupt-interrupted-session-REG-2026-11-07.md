# Git index became unreadable during an interrupted work session

Recorded: 2026-11-07

Status: OPEN — recovery was verified; cause and prevention are not verified.

Related workflow:
`docs/operations/task-completion/task-completion.md`

Related changelog:
`docs/changelog/2026-11-07/20261107_161106-git-index-repair.md`

## Observed

During work on branch `20261011zt`, Git commands that read or update the index
failed with:

```text
error: bad signature 0x00000000
fatal: index file corrupt
```

`git status`, `git ls-files --stage`, `git add .`, and `git commit` could not
proceed. The index file was 627,384 bytes and its filesystem last-write time
was 2026-11-06 19:05:06 EST. The exact time and event that damaged it are
unknown.

## Expected behavior

After a work session or computer shutdown, Git should be able to read its
index and report the repository state. An interrupted operation should not
leave the index unusable without a recoverable copy.

## Actual behavior

The index signature was four zero bytes rather than Git's expected index
signature. Git could not inspect the staged or working-tree state until the
index was reconstructed.

## Cause assessment

The user suspects the computer was closed or shut down while an AI agent was
working. That is plausible for an interrupted index write, but it is not
confirmed: no shutdown timeline, process trace, or disk evidence was collected.
The file's last-write time predates the observed failure, but does not identify
what wrote or damaged it. Do not present the shutdown hypothesis as the root
cause unless new evidence confirms it.

## Recovery

The original index was preserved as
`.git/index.corrupt-20261107_110836`. The index was rebuilt from the current
`HEAD` using `git read-tree HEAD`; Git status and diff then worked, and the
requested commit was created and pushed. Rebuilding from `HEAD` restores the
tracked baseline, but it cannot recover staged-only content that was not also
present in the working files. No evidence of lost staged-only work was
available in this occurrence.

## Prevention and follow-up

- Before shutting down, let active AI work and Git operations finish when
  possible.
- If this recurs, record which process was writing `.git/index`, the exact
  shutdown/restart timeline, and whether `.git/index.lock` existed. Do not
  delete the index or lock before preserving them and inspecting active Git
  processes.
- Consider a small wrapper or recovery note that preserves the damaged index
  before rebuilding it. Do not automate destructive cleanup of `.git` state
  without confirming no Git operation is active.
- If evidence confirms shutdown interruption, update this cause assessment and
  record the specific interruption and prevention that addressed it.

## Proof status

Recovery is confirmed: Git status, staging, commit, and push succeeded after
rebuilding the index. The initiating cause remains unverified, and no
preventative code or workflow change has been implemented.
