# Presentation DLL failure containment and persistent goal checkpoint

- Task: continue the active C++ edit-to-live latency goal; clarify optional presentation-DLL failure handling; preserve long-running goal continuity.
- Session ID: work-ccd55d6007c3
- Agent: `cpp-latency-failure-containment`
- Date/time: 2026-11-22 (America/New_York)
- Status: investigation and issue handoff recorded; no implementation change in this activation.

## Findings

- The user’s “failure like that” refers to the optional `build_game_dll.py` presentation DLL attempted during the authoritative EXE build, tracked by `issue-ae248907f1f1`.
- `build.py` already fails open for this optional artifact. A failed helper records its outcome and cooldown marker, then the required EXE build continues. Unchanged failures are skipped for 30 minutes; `MIMITA_RETRY_PRESENTATION_DLL=1` forces a retry.
- Prior root-dev-loop evidence on the issue: an optional DLL failure taking 21.616s did not fail the EXE request; the identical subsequent request skipped the helper in 0.0s and the EXE request succeeded. Required EXE compile/link correctness remains fail-closed.
- The optional failure fingerprint includes the listed direct sources, `game-api.h`, build scripts, toolchain, and compiler. `build_game_dll.py` builds two C++ sources without a dependency file. It remains unknown whether all transitive headers are represented; a relevant header edit might not invalidate the skip fingerprint. The issue now records this as the next discriminating test.
- The general gold behavior already exists in `docs/gold/20261030_033224-agent-session-handoff-gold.md`: “Long-running goals survive turns and session resets” and “Optional tools and accelerators fail open.” It directs agents to continue when the user says “continue,” carry exact checkpoints, and keep optional failures separate from required goal/build results. No duplicate gold document was added.
- A generic candidate game-DLL compile/link failure is a different boundary: reject that coherent candidate, retain the last successful generation, and keep the dev-loop/runtime available. Do not substitute stale objects for changed sources.

## Goal checkpoint

- Persistent goal remains active: reduce C++ edit-to-live latency toward 10 seconds under measured live Juggernaut/AI pressure while preserving correct caching, MinGW fallback, root dev-loop authority, and runtime proof.
- Latest measured changed-source attempt remains generation 479: 85.019s attempt / 85.735s dev-loop wait; one edited TU compiled in 24.145s, the GNU linker took 20.914s over 644 object inputs, and publish took 0.508s. A warm no-op took 11.928s. These are prior-activation measurements, not results from a new build in this activation.
- The shared server PID 26748 and run `20261122_011031` were left untouched. No client or additional game process was launched. No new CPU or build measurement was made here.

## Coordination and follow-up

- Added the failure-boundary clarification and fingerprint risk to `issue-ae248907f1f1` and the persistent-goal checkpoint to `issue-91e3384b219b`.
- Corrected the earlier generic failure note on `issue-136d4553ff92` to identify the presentation-DLL issue as the primary context for this user request.
- Next: test a relevant transitive-header change against the optional-DLL retry fingerprint through the root dev-loop, then fix it only if that test demonstrates a missed invalidation; continue warm no-op and changed-TU latency optimization with exact stage and runtime evidence.
- Proof status: build failure containment has prior successful EXE build evidence; transitive dependency invalidation remains unknown. The generation-479 NPC tuning effect and <=10s edit-to-live target remain unproven.
