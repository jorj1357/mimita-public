# 2026-09-27 — Hot reload was fixed by returning to reviewable increments

Status: `GOLD / REFERENCE` — process lesson and implementation direction from
the hot-reload work between approximately 2026-09-11 and 2026-09-24.

## The outcome

The hot-reload issue was fixed: gameplay behavior can now be moved through the
intended hot boundary and observed without treating every behavior adjustment
as a cold executable replacement.

The important lesson is that the final architecture was not the result of
asking AI to make many changes at once. The productive part began when each
change was reviewed, traced, built, activated, and observed before the next
change was started.

## What went wrong during the first attempt

The initial implementation process became an AI spam loop:

1. A large behavior goal was stated.
2. Many edits were generated quickly.
3. New helpers, paths, flags, and fallback code accumulated.
4. The running behavior was not checked after every small change.
5. Duplicate ownership and junk implementation became harder to see.

That process can produce a lot of code while reducing confidence. More code,
more logs, and more abstractions do not prove that the running EXE is using the
intended hot owner.

The failure was therefore partly architectural and partly procedural. The
hot boundary needed to be real, but the work also needed a review gate after
each behavior slice.

## The corrected method

The right method is:

> Keep the whole vision large, but implement and verify it in very small,
> independently understandable increments.

For every increment:

1. State the user-visible behavior in one sentence.
2. Identify the current owner and the intended hot owner.
3. Trace the existing input-to-output path before editing.
4. Change the smallest owner that can produce the behavior.
5. Remove duplicate or abandoned paths instead of layering another fallback.
6. Build the relevant hot module or use the required JSON reload path.
7. Observe the unchanged running EXE/world/session.
8. Compare the result with the acceptance sentence.
9. Record proof, remaining uncertainty, and the next slice.

If the result is not understood, stop and review it before adding more code.

## What “fixed” means here

The fix is not merely that a DLL compiled. The useful hot-reload contract is:

- the process-lifetime EXE and session remain alive;
- gameplay policy and behavior can change in the hot module or through its
  documented JSON boundary;
- the running system applies the new generation at a safe boundary;
- the same input can be traced to the changed owner;
- the human can observe the result without restarting the world;
- cold rebuilds remain reserved for the generic kernel, ABI, loader, or other
  process-lifetime mechanism changes.

This is consistent with the hot/cold boundary record in
`docs/gold/2026-09-22-hot-cold-combat-boundary.md` and the behavior-parity
method in `docs/gold/2026-09-23-afad20a-behavior-parity-hot-reload-journey.md`.

## Review gates that must stay in the loop

Before accepting each new hot-reload slice, ask:

- Is there already an owner for this behavior?
- Did this change create a second source of truth?
- Is the new code policy, tuning, presentation, or kernel mechanism?
- Does the running EXE actually load and apply the changed generation?
- Can the result be seen or measured without a cold restart?
- What is the smallest proof that distinguishes this fix from a stale binary?
- What code can now be deleted because the new owner replaces it?

The most important gate is ownership. If two files both decide the same
behavior, stop and resolve the owner before adding more features.

## Likely next blocker: hardcodedness

The next predicted blocker is hardcoded behavior or presentation that remains
inside C++ after the hot boundary exists. This is a risk, not yet a confirmed
regression.

Examples to investigate carefully:

- GUI positions, colors, labels, sizes, and visibility rules hardcoded in C++;
- gameplay values duplicated in C++ and JSON;
- mode-specific branches that bypass the selected JSON definition;
- fallback constants that silently override the active hot policy;
- one visual transform used by rendering while a separate constant is used by
  collision or networking.

The answer is not to move every line into JSON. First identify whether the
value is policy, tuning, presentation, or a kernel mechanism. Then give it one
owner, keep validation and last-valid fallback explicit, and prove that reload
changes the visible or authoritative result.

## Practical next step

For the next hot-reload task, choose one narrow slice such as one movement
decision, one collision response, one effect, or one GUI element. Write its
acceptance sentence first. Review the current owner. Make one change. Observe
it live. Only then move to the next slice.

The large vision remains the guide; the small increments are the safety
system.

## References

- `docs/gold/2026-09-21-hot-reload-progress-and-v206-parity.md`
- `docs/gold/2026-09-22-hot-cold-combat-boundary.md`
- `docs/gold/2026-09-23-afad20a-behavior-parity-hot-reload-journey.md`
- `docs/architecture/live-development/live-development.md`
- `docs/features/live-code-development/live-code-development.md`
