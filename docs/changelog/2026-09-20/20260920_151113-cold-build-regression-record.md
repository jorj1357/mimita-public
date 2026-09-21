# Cold-build regression record policy

Time: 2026-09-20T15:11:13Z

## Final state

Documented a dedicated persistent cold-build debt record. Future intentional
cold builds append occurrences to:

`docs/regressions/2026-09-20/cold-build-required-REG.md`

The policy distinguishes cold-build migration debt from confirmed user-visible
behavior regressions. Independent behavior regressions continue to use their
own files, while repeated occurrences of the same problem remain in the same
file.

## Changed files

- `docs/regressions/README.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/regressions/2026-09-20/cold-build-required-REG.md`

## Evidence

- Read the project router and routed documentation/build/regression guidance.
- Confirmed the existing build workflow distinguishes cold builds from live
  builds and prohibits replacing a running executable.
- Confirmed the existing regression guidance did not require a cold-build
  debt entry after every cold build.
- Documentation-only change; no executable build was required.

## Human review still needed

Please review whether the dedicated record should remain under the regression
folder or move to a separate build-debt folder. The current placement follows
the requested regression workflow.
