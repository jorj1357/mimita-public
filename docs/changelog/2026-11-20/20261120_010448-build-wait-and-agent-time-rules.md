# Build wait and agent time rules

## Task

- Agent: `root-build-wait-efficiency`
- Date/time: 2026-11-20 01:04 EST
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `ce0009d5b2983f462d1d17dd640c4e66769c2a23`
- Existing shared-checkout changes: present across unrelated files before this task; left untouched.

## Request

The user asked for a rule that build/test waits expected to exceed one minute become issues and trigger investigation, for agent work/thinking time to be tracked for efficiency, and for repeated reports to reuse existing issues rather than repeat work.

## Existing issue records

- `issue-e044a931c54b` already tracks avoidable build/test waits and has the `wait-instrument` task. The new wait threshold requirement was appended as a user quote and issue suggestion; no duplicate wait issue/task was created.
- `issue-89c11130189a` already tracks agent work-time and phase latency. The user's request was appended as a quote and issue suggestion; no duplicate time tracking issue was created.

## Changes

- `docs/specs/dev-loop/change-to-evidence-latency.md`: added observable agent wall-clock phase timing, separated external/tool/build waits from agent work, and prohibited claiming elapsed activation time as internal model inference time without a reliable platform measurement.
- Added an immediate investigation trigger when an agent estimates or observes a build/test wait over 60 seconds, including wait/phase/blocker evidence, issue creation or linkage, and independent review while investigating.
- Added duplicate-aware intake: search existing issues, append evidence and continue open tasks for the same request/root cause, and avoid repeating completed work.

## Evidence and limits

- Documentation checker instructions were read; the changed specification retains its audience, purpose, and authority boundaries, and its changes align with the existing wait logging and build-friction rules.
- `git diff --check -- docs/specs/dev-loop/change-to-evidence-latency.md` passed. No build or runtime validation applies to this documentation-only change.
- Live dev-loop status reported no active build worker at review time (`game-build status=idle`); the earlier proposed five-minute wait was no longer active.
- This records timing requirements; it does not claim the tooling already captures internal model inference time or automatically creates all issues. The implementation work remains under the existing issue tasks.
