# C++ latency warm no-op and failure boundaries

- Agent: `cpp-latency-digest-cache`
- Work session: `work-d5aee5a84882`
- Issue: `issue-136d4553ff92`
- Date: 2026-11-22
- Checkout: `C:\mimita-v9`, branch `checkpoint/one-checkout-condensation-20261111`

## Work and evidence

Requested a fresh game-DLL attempt only through the root dev-loop after confirming there was no active build and generation 490 matched the current game-DLL revision. Request `2026-11-22T12:38:36.734Z-30055315` completed successfully as no-op, attempt `game-dll-20261122T123836930053Z-601e42`.

- Attempt wall time: 6.890 seconds; dev-loop wait: 6.969 seconds.
- Result: 642 skipped, 0 compiled, 0 linked; generation 490 retained.
- Worker: 2.547 seconds; worker first output at 3.406 seconds.
- Toolchain setup: 0.469 seconds; object-cache scan: 0.140 seconds; no-op decision: 0.016 seconds.
- Initial source coherence: 1.812 seconds for 643 sources.
- Existing game PID 26748 and run `20261122_011031` stayed alive. This was no-op verification, not a new live gameplay test.
- Detailed evidence: `.dev/build-timings/game-dll-20261122T123836930053Z-601e42.json` and `.jsonl`.

## Failure containment

The optional presentation-DLL helper already fails open: its failure is recorded, the primary EXE build continues, and an unchanged failed fingerprint is suppressed for 30 minutes. The existing evidence is in issue `issue-ae248907f1f1` and `docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md`.

Required game-DLL compile, link, coherence, and publication failures must remain visible as failed candidate attempts and leave the last good generation current. Marking a failed candidate successful could activate stale or mixed code. Safe containment means keeping the prior generation usable and allowing a later corrected candidate to retry; it does not mean suppressing a required failure.

## Next work and limits

The measured changed-source path remains 80.609 seconds to attempt completion and 81.797 seconds to runtime activation for a comment-only canary, not a behavior proof. Prior measurements identify source snapshot construction (~13 seconds) and synchronous pruning (~11.248 seconds for one snapshot) as removable critical-path costs. The pruning call is in `devscripts/dev_loop/game_build.py` before worker launch. Open coordination conflicts `conflict-28c39cc357c3` and `conflict-bf022f236b9a` cover the build driver and worker; reconcile them before editing those files. No stress-profile test or compiler experiment was run in this checkpoint.

The persistent goal remains active. The handoff guidance in `docs/gold/20261030_033224-agent-session-handoff-gold.md` already requires continuing after a “continue” message, carrying forward verified evidence, and not treating optional accelerator failures as goal completion.
